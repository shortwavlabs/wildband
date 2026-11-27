// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Stephane P. Pericat
//
// When may the plugin's radio receiver touch the network? docs/p1/PLUGIN-RECEIVER-LIFECYCLE.md
// Only once audio has been rendered in real time, without a break, for a while: scanning, validating, loading a
// project, restoring state and offline rendering never qualify. Pure logic, no JUCE, message thread only; the audio
// thread feeds it through atomics (see Plugin.cpp).
#pragma once

#include <cstdint>

namespace wildband
{

class ReceiverGate
{
public:
    enum class State { Dormant, Armed, Active, Suspended };

    static constexpr double kActivateAfterSeconds = 1.5;   // continuous real-time rendering needed
    static constexpr double kMaxGapSeconds = 0.5;          // a longer gap between callbacks restarts that window
    static constexpr double kSuspendAfterSeconds = 5.0;    // no callbacks for this long while active: suspend
    static constexpr double kMinRatio = 0.5, kMaxRatio = 2.0;   // rendered audio time / wall time

    void prepared (bool nonRealtime) noexcept { state = nonRealtime ? State::Dormant : (state == State::Active ? State::Active : State::Armed); resetWindow(); }
    void released() noexcept { state = State::Dormant; resetWindow(); }

    /** Message thread, regularly. `nowSeconds`: wall clock. `renderedSeconds`: total audio rendered so far (monotonic),
        `lastCallbackSeconds`: wall-clock time of the latest audio callback (negative if none yet). */
    State update (double nowSeconds, double renderedSeconds, double lastCallbackSeconds, bool nonRealtime) noexcept
    {
        if (state == State::Dormant) return state;
        if (nonRealtime) { if (state == State::Active) state = State::Suspended; resetWindow(); return state; }

        const bool rendering = lastCallbackSeconds >= 0.0 && nowSeconds - lastCallbackSeconds <= kMaxGapSeconds;
        if (state == State::Active)
        {
            if (lastCallbackSeconds < 0.0 || nowSeconds - lastCallbackSeconds > kSuspendAfterSeconds) { state = State::Suspended; resetWindow(); }
            return state;
        }
        if (! rendering) { resetWindow(); return state; }                 // ARMED or SUSPENDED, waiting for real-time audio
        if (windowStart < 0.0) { windowStart = nowSeconds; windowRendered = renderedSeconds; return state; }
        const double wall = nowSeconds - windowStart, audio = renderedSeconds - windowRendered;
        if (wall >= kActivateAfterSeconds)
        {
            const double ratio = audio / wall;
            if (ratio >= kMinRatio && ratio <= kMaxRatio) state = State::Active;
            else resetWindow();                                           // much faster (validator) or slower than real time
        }
        return state;
    }

    State current() const noexcept { return state; }
    bool active() const noexcept { return state == State::Active; }
    static const char* name (State s) noexcept { static const char* n[] = { "DORMANT", "ARMED", "ACTIVE", "SUSPENDED" }; return n[(int) s]; }

private:
    void resetWindow() noexcept { windowStart = -1.0; windowRendered = 0.0; }
    State state = State::Dormant;
    double windowStart = -1.0, windowRendered = 0.0;
};

} // namespace wildband
