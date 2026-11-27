// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Stephane P. Pericat
// WILDBAND voice experiment D2, "Dissolve": a rolling granular delay. The incoming sound leaves a trail that runs from
// a clear echo (TRACE, dissolve = 0) to a cloud of overlapping fragments (CLOUD, dissolve = 1).
//
// Written new for WILDBAND after the idea described in docs/AFTERIMAGE-DSP-REUSE.md. It shares no code with
// AFTERIMAGE: there is no capture, no selection, no frozen slot, no FFT. Its only memory is a fixed two-second stereo
// history that is overwritten continuously, read only by the taps below, cleared by reset(), and never copied
// anywhere else. Experimental: docs/p0/VOICE-RESEARCH.md. No JUCE, no allocation after prepare().
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace wildband
{

class Dissolve
{
public:
    static constexpr int kGrains = 24;
    static constexpr double kHistorySeconds = 2.0;

    struct Params
    {
        float time = 0.35f;      // seconds behind "now" that the trail is read (0.03 .. 1.6)
        float dissolve = 0.5f;   // 0 = one clear echo, 1 = cloud
        float spray = 0.3f;      // how far grains scatter in time and across the stereo field
        float regen = 0.4f;      // how much of the output is written back into the history (0 .. 0.95)
        float damp = 0.5f;       // one-pole coefficient of the low-pass in that feedback path (1 = open)
        float drive = 1.0f;      // input gain
    };

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        size = 1;
        while (size < (int) (fs * kHistorySeconds) + 8) size <<= 1;
        for (auto& h : history) h.assign ((size_t) size, 0.0f);
        aTime = (float) (1.0 - std::exp (-1.0 / (0.06 * fs)));
        reset();
    }

    /** Forgets everything it has heard. */
    void reset() noexcept
    {
        for (auto& h : history) std::fill (h.begin(), h.end(), 0.0f);
        for (auto& g : grains) g.remaining = 0;
        w = 0; countdown = 0; seed = 0x51ED270Bu; delay = -1.0f; clearAt = -1;
        last = {}; lowpassed = {};
    }

    /** Clears the history a piece at a time (for use on the audio thread, where clearing it all at once would be a
        long call). Returns true once nothing of what it heard is left. Call reset() instead when time does not matter. */
    bool forgetSome (int samples) noexcept
    {
        if (clearAt < 0) { clearAt = 0; for (auto& g : grains) g.remaining = 0; last = {}; lowpassed = {}; countdown = 0; }
        const int end = std::min (size, clearAt + samples);
        for (auto& h : history) std::fill (h.begin() + clearAt, h.begin() + end, 0.0f);
        clearAt = end;
        if (clearAt < size) return false;
        clearAt = -1; w = 0; seed = 0x51ED270Bu; delay = -1.0f;
        return true;
    }

    size_t memoryBytes() const noexcept { return sizeof (*this) + 2 * (size_t) size * sizeof (float); }

    void process (float inL, float inR, float& outL, float& outR, const Params& p) noexcept
    {
        const int mask = size - 1;
        const float target = std::min (1.6f, std::max (0.03f, p.time)) * (float) fs;
        delay = delay < 0.0f ? target : delay + aTime * (target - delay);                 // the echo time glides, tape-like
        const float dissolve = clamp01 (p.dissolve), spray = clamp01 (p.spray);

        // ---- write: the input plus a saturated, low-passed share of what just came out
        const float regen = std::min (0.95f, std::max (0.0f, p.regen)), damp = std::min (1.0f, std::max (0.01f, p.damp));
        const float in[2] = { inL * p.drive, inR * p.drive };
        for (size_t ch = 0; ch < 2; ++ch)
        {
            lowpassed[ch] += damp * (last[ch] - lowpassed[ch]);
            history[ch][(size_t) w] = in[ch] + std::tanh (regen * lowpassed[ch]);
        }

        // ---- TRACE: one coherent tap
        const float pos = (float) w - delay + (float) size;
        const int i0 = (int) pos;
        const float frac = pos - (float) i0;
        float trace[2];
        for (size_t ch = 0; ch < 2; ++ch)
            trace[ch] = history[ch][(size_t) (i0 & mask)] + frac * (history[ch][(size_t) ((i0 + 1) & mask)] - history[ch][(size_t) (i0 & mask)]);

        // ---- CLOUD: short grains read from around the same place, denser and more scattered as it dissolves
        const float overlap = 2.0f + 6.0f * dissolve;
        const int length = std::max (64, (int) ((0.18f - 0.11f * dissolve) * (float) fs));
        if (dissolve > 0.0f && --countdown <= 0)
        {
            countdown = std::max (1, (int) ((float) length / overlap));
            for (auto& g : grains)
                if (g.remaining <= 0)
                {
                    const float scatter = delay * 0.6f * spray * uniform() + 0.15f * (float) fs * spray * std::abs (uniform());
                    g.delay = std::min (size - 8, std::max (8, (int) (delay + scatter)));
                    g.length = g.remaining = length;
                    const float pan = 0.5f + 0.5f * spray * uniform();
                    g.gainL = std::sqrt (1.0f - pan) * 1.41421356f;
                    g.gainR = std::sqrt (pan) * 1.41421356f;
                    break;
                }
        }
        float cloud[2] = { 0.0f, 0.0f };
        for (auto& g : grains)
            if (g.remaining > 0)
            {
                const float x = 1.0f - (float) g.remaining / (float) g.length, window = 4.0f * x * (1.0f - x);
                const auto at = (size_t) ((w - g.delay + size) & mask);
                cloud[0] += window * g.gainL * history[0][at];
                cloud[1] += window * g.gainR * history[1][at];
                --g.remaining;
            }
        // overlapping parabolic windows sum to about 2/3 of the overlap when the grains agree, and to about its
        // square root when they are scattered: this keeps the cloud near the echo's level either way
        const float agree = 1.5f / overlap, scattered = 1.2f / std::sqrt (overlap), norm = agree + spray * (scattered - agree);

        last[0] = (1.0f - dissolve) * trace[0] + dissolve * norm * cloud[0];
        last[1] = (1.0f - dissolve) * trace[1] + dissolve * norm * cloud[1];
        outL = last[0];
        outR = last[1];
        w = (w + 1) & mask;
    }

private:
    struct Grain { int delay = 0, length = 1, remaining = 0; float gainL = 1, gainR = 1; };
    static float clamp01 (float v) noexcept { return std::min (1.0f, std::max (0.0f, v)); }
    float uniform() noexcept { seed = seed * 1664525u + 1013904223u; return (float) (int32_t) seed * (1.0f / 2147483648.0f); }   // -1 .. 1, deterministic

    double fs = 48000;
    int size = 0, w = 0, countdown = 0, clearAt = -1;
    uint32_t seed = 0;
    float delay = -1.0f, aTime = 0;
    std::array<std::vector<float>, 2> history;
    std::array<Grain, kGrains> grains;
    std::array<float, 2> last {}, lowpassed {};
};

} // namespace wildband
