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

} // namespace wildband
