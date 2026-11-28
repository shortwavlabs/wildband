// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Stephane P. Pericat
//
// May a receiver hold a network connection right now? docs/p1/PLUGIN-RECEIVE.md
// Two independent parts: AUTHORISATION (the user explicitly asked receiver i to receive - only RECEIVE on/off, PANIC,
// state restore and destruction change it) and HOST (is the host processing in real time, so a connection is useful).
// A receiver connects iff it is authorised AND the host is live. Host activity never authorises anything.
// Pure logic, no JUCE, message thread only; written for several receivers (A, B).
#pragma once

#include <array>
#include <atomic>
#include <cstdint>

namespace wildband
{

class ReceiverControl
{
public:
    static constexpr int kReceivers = 2;
    static constexpr double kStartWithinSeconds = 0.5;     // the host counts as live if it called back this recently
    static constexpr double kSuspendAfterSeconds = 5.0;    // ...and stops counting after this long without a callback
    static constexpr double kReleaseGraceSeconds = 0.5;    // a release followed at once by a prepare keeps the connection

    // ---- authorisation: explicit user intent, runtime only
    void setReceive (int receiver, bool on) noexcept { if (valid (receiver)) authorised[(size_t) receiver] = on; }
    void toggleReceive (int receiver) noexcept { if (valid (receiver)) authorised[(size_t) receiver] = ! authorised[(size_t) receiver]; }
    void revokeAll() noexcept { authorised.fill (false); }                 // RECEIVE off everywhere: restore, destruction, PANIC
    bool receiving (int receiver) const noexcept { return valid (receiver) && authorised[(size_t) receiver]; }

    // ---- host activity: resource management only
    struct HostSnapshot
    {
        bool prepared = false;          // between prepareToPlay and releaseResources
        bool offline = false;           // prepared for offline, or the host says it is rendering offline now
        double lastCallback = -1.0;     // seconds, wall clock; negative if none since construction
        double releasedAt = -1.0;       // seconds, wall clock, of the latest releaseResources; negative if none
    };
    void updateHost (double now, const HostSnapshot& h) noexcept
    {
        if (h.offline) { live = false; return; }
        if (! h.prepared)
        {
            if (live && h.releasedAt >= 0.0 && now - h.releasedAt < kReleaseGraceSeconds) return;   // grace: a re-initialise
            live = false;
            return;
        }
        const bool recent = h.lastCallback >= 0.0 && now - h.lastCallback <= kStartWithinSeconds;
        if (live) { if (h.lastCallback < 0.0 || now - h.lastCallback > kSuspendAfterSeconds) live = false; }
        else live = recent;
    }
    bool hostLive() const noexcept { return live; }

    /** The decision: should receiver i hold a connection now? */
    bool shouldConnect (int receiver) const noexcept { return receiving (receiver) && live; }

private:
    static bool valid (int r) noexcept { return r >= 0 && r < kReceivers; }
    std::array<bool, kReceivers> authorised {};
    bool live = false;
};

/** RECEIVE requests from MIDI: the audio thread publishes, the message thread takes. One atomic word, so the order of
    PANIC and toggles is never lost between two atomics:
      bit 31     a PANIC (CC 120) was seen
      bits 0-30  toggles of the mapped control seen AFTER the latest PANIC
    Precedence is deterministic. A PANIC anywhere in a block discards every toggle of that block, whatever their order
    within it, and every toggle still pending from earlier blocks. Toggles in later blocks count again: they are new
    gestures. An explicit RECEIVE on/off or a state restore (discardToggles) supersedes pending toggles. A pending
    PANIC survives it, and still runs. The message thread calls take() and discardToggles() only under the lock that
    also guards ReceiverControl, so a toggle taken before a restore can never be applied after it. */
class ReceiveRequests
{
public:
    static constexpr uint32_t kPanic = 0x80000000u, kCount = 0x7fffffffu;
    struct Taken { bool panic = false; uint32_t toggles = 0; };

    /** Audio thread, once per block, after scanning the block's MIDI. Wait-free in the common cases. */
    void publishBlock (uint32_t toggles, bool panic) noexcept
    {
        if (panic) { word.store (kPanic, std::memory_order_release); return; }
        if (toggles != 0) word.fetch_add (toggles & kCount, std::memory_order_acq_rel);
    }
    /** Message thread (or any non-audio thread), under the receiver lock. */
    void discardToggles() noexcept
    {
        auto v = word.load (std::memory_order_acquire);
        while (! word.compare_exchange_weak (v, v & kPanic, std::memory_order_acq_rel)) {}
    }
    Taken take() noexcept
    {
        const auto v = word.exchange (0, std::memory_order_acq_rel);
        return { (v & kPanic) != 0, v & kCount };
    }
    /** Applies what was taken to receiver 0: a PANIC revokes first, then an odd number of later toggles turns
        RECEIVE on (or off). Returns true if a PANIC was applied. */
    static bool applyTo (ReceiverControl& control, Taken t) noexcept
    {
        if (t.panic) control.revokeAll();
        if ((t.toggles & 1u) != 0) control.toggleReceive (0);
        return t.panic;
    }

private:
    std::atomic<uint32_t> word { 0 };
};

/** The MIDI control mapped to RECEIVE, and whether LEARN is waiting for one. One atomic word, written by LEARN (audio
    thread), by a state restore and by the test aid (any non-audio thread):
      bits 0-7 controller + 1 (0 = none), bits 8-15 note + 1 (0 = none), bit 16 LEARN armed
    restore() always wins: it stores a whole new word with LEARN cleared, so a LEARN capture that has not completed
    can no longer land (captureLearn succeeds only while the LEARN bit is still set). */
class ReceiveMapping
{
public:
    void restore (int cc, int note) noexcept { word.store (pack (cc, note, false), std::memory_order_release); }
    void armLearn() noexcept
    {
        auto v = word.load (std::memory_order_acquire);
        while (! word.compare_exchange_weak (v, v | kLearn, std::memory_order_acq_rel)) {}
    }
    /** Audio thread: if LEARN is armed, map this control (cc or note, the other -1) and disarm. */
    bool captureLearn (int cc, int note) noexcept
    {
        auto v = word.load (std::memory_order_acquire);
        while ((v & kLearn) != 0)
            if (word.compare_exchange_weak (v, pack (cc, note, false), std::memory_order_acq_rel)) return true;
        return false;
    }
    bool learning() const noexcept { return (word.load (std::memory_order_acquire) & kLearn) != 0; }
    int cc() const noexcept { return (int) (word.load (std::memory_order_acquire) & 0xffu) - 1; }
    int note() const noexcept { return (int) ((word.load (std::memory_order_acquire) >> 8) & 0xffu) - 1; }

private:
    static constexpr uint32_t kLearn = 0x10000u;
    static uint32_t field (int v, int max) noexcept { return v >= 0 && v <= max ? (uint32_t) (v + 1) : 0u; }
    static uint32_t pack (int cc, int note, bool learn) noexcept { return field (cc, 119) | (field (note, 127) << 8) | (learn ? kLearn : 0u); }
    std::atomic<uint32_t> word { 0 };
};

} // namespace wildband
