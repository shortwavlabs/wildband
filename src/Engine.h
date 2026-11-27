// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Stephane P. Pericat
// WILDBAND P0 engine. No JUCE, no allocation after prepare(), no locks: driven by the audio callback only.
// Signal path: radio -> "detune" ring modulator -> [patched audio source] -> six tuned, damped delay-line resonators
// with a saturator in each loop and a shared coupling path -> mix -> safety clip. Which control signal reaches which
// resonator input is decided by the patch (Patch.h); COUPLE is the depth of all of them.
// Design notes and the control map: docs/p0/ARCHITECTURE.md
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

#include "Dissolve.h"
#include "Patch.h"

namespace wildband
{

constexpr float kPi = 3.14159265358979f;

enum Param : int { kTune, kPitch, kShape, kTone, kCouple, kDrift, kFeedback, kMix, kLevel, kStatic, kNumParams };   // new parameters go at the end
inline constexpr const char* paramNames[kNumParams] = { "TUNE", "PITCH", "SHAPE", "TONE", "COUPLE", "DRIFT", "FEEDBACK", "MIX", "LEVEL", "STATIC" };
inline constexpr float paramDefaults[kNumParams] = { 0.0f, 0.33f, 0.4f, 0.6f, 0.4f, 0.2f, 0.55f, 0.7f, 0.5f, 0.5f };

enum class Mode : int { Engine, Dry, Reference };

inline float clamp01 (float v) noexcept { return std::min (1.0f, std::max (0.0f, v)); }
inline float onePoleCoeff (double seconds, double fs) noexcept { return (float) (1.0 - std::exp (-1.0 / (seconds * fs))); }

// ---------------------------------------------------------------------------------------------------------------------
// TUNE is a dial over station slots, not an RF frequency. Each slot owns an equal arc; the middle 40 % of an arc is
// clean reception, and toward the edges `offCentre` rises to 1, which the engine turns into ring modulation.
struct Dial { int station; float offCentre; };
inline Dial dial (float tune, int numStations) noexcept
{
    if (numStations <= 1) return { 0, 0.0f };
    const float pos = clamp01 (tune) * (float) numStations;
    const int i = std::min (numStations - 1, (int) pos);
    const float d = std::abs (pos - (float) i - 0.5f) * 2.0f;
    return { i, clamp01 ((d - 0.4f) / 0.6f) };
}
inline float dialCentre (int station, int numStations) noexcept { return ((float) station + 0.5f) / (float) std::max (1, numStations); }

// ---------------------------------------------------------------------------------------------------------------------
// Environment: every source publishes the same three bounded, already-smoothed values. Only the microphone exists in
// P0; the other slots are placeholders for sources that are NOT implemented. A source that stops publishing goes stale
// and reads as neutral (0), which the engine then glides to.
struct EnvFrame { float level = 0.0f, activity = 0.0f, brightness = 0.0f; };
enum EnvSource : int { kEnvMicrophone, kEnvWeather, kEnvBluetoothRssi, kEnvWifi, kEnvExternalRf, kNumEnvSources };

class EnvBus
{
public:
    void publish (EnvSource s, EnvFrame f, int64_t nowMs) noexcept
    {
        auto& slot = slots[(size_t) s];
        slot.level = clamp01 (f.level); slot.activity = clamp01 (f.activity); slot.brightness = clamp01 (f.brightness);
        slot.stampMs = nowMs;
    }
    void withdraw (EnvSource s) noexcept { slots[(size_t) s].stampMs = -1; }
    bool fresh (EnvSource s, int64_t nowMs) const noexcept
    {
        const auto t = slots[(size_t) s].stampMs.load();
        return t >= 0 && nowMs - t < staleMs;
    }
    /** The strongest fresh value of each feature; neutral when nothing is fresh. */
    EnvFrame read (int64_t nowMs) const noexcept
    {
        EnvFrame f;
        for (int s = 0; s < kNumEnvSources; ++s)
            if (fresh ((EnvSource) s, nowMs))
            {
                f.level = std::max (f.level, slots[(size_t) s].level.load());
                f.activity = std::max (f.activity, slots[(size_t) s].activity.load());
                f.brightness = std::max (f.brightness, slots[(size_t) s].brightness.load());
            }
        return f;
    }
    static constexpr int64_t staleMs = 1500;

private:
    struct Slot { std::atomic<float> level { 0 }, activity { 0 }, brightness { 0 }; std::atomic<int64_t> stampMs { -1 }; };
    std::array<Slot, kNumEnvSources> slots;
};

/** Microphone -> control features. The audio itself goes nowhere else. */
class RoomAnalyser
{
public:
    void prepare (double fs) noexcept
    {
        aEnergy = onePoleCoeff (0.05, fs); aSplit = onePoleCoeff (1.0 / (2.0 * kPi * 2000.0), fs);
        aAttack = onePoleCoeff (0.001, fs); aRelease = onePoleCoeff (0.03, fs); aSlow = onePoleCoeff (0.4, fs);
        activityDecayPerSample = (float) std::exp (-1.0 / (0.25 * fs));
        eTotal = eHigh = lp = fast = slow = activity = 0.0f;
    }

    EnvFrame process (const float* x, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const float s = std::isfinite (x[i]) ? x[i] : 0.0f;
            lp += aSplit * (s - lp);
            const float high = s - lp;
            eTotal += aEnergy * (s * s - eTotal);
            eHigh += aEnergy * (high * high - eHigh);
            const float mag = std::abs (s);
            fast += (mag > fast ? aAttack : aRelease) * (mag - fast);
            slow += aSlow * (mag - slow);
            const float onset = clamp01 ((fast / (slow + 1.0e-3f) - 2.0f) * 0.25f);
            activity = std::max (onset, activity * activityDecayPerSample);
        }
        EnvFrame f;
        const float db = 10.0f * std::log10 (eTotal + 1.0e-12f);
        f.level = clamp01 ((db + 60.0f) / 50.0f);          // -60 dBFS .. -10 dBFS RMS
        f.activity = f.level > 0.05f ? activity : 0.0f;    // no onsets out of the noise floor
        f.brightness = f.level > 0.05f ? clamp01 (1.5f * std::sqrt (eHigh / (eTotal + 1.0e-12f))) : 0.0f;
        return f;
    }

private:
    float aEnergy = 0, aSplit = 0, aAttack = 0, aRelease = 0, aSlow = 0, activityDecayPerSample = 0;
    float eTotal = 0, eHigh = 0, lp = 0, fast = 0, slow = 0, activity = 0;
};

// ---------------------------------------------------------------------------------------------------------------------
/** Deterministic stand-in for a broadcast: pings, noise bursts and rests. Labelled LOCAL TEST SOURCE in the UI; it is
    never presented as live radio. Also the fixed input of the DSP tests. */
class TestSource
{
public:
    void prepare (double sampleRate) noexcept { fs = sampleRate; seed = 0x1234567u; pos = 0; env = 0; phase = 0; kind = 0; lp = 0; }
    float next() noexcept
    {
        if (pos-- <= 0)
        {
            pos = (int) (fs * 0.125) * (1 + (int) (rand() % 3));
            kind = (int) (rand() % 4);                                   // 0 rest, 1/2 ping, 3 noise burst
            static constexpr float notes[5] = { 110.0f, 164.81f, 220.0f, 277.18f, 329.63f };
            freq = notes[rand() % 5] * (kind == 2 ? 2.0f : 1.0f);
            env = kind == 0 ? 0.0f : 1.0f;
        }
        env *= (float) std::exp (-1.0 / (0.08 * fs));
        phase += 2.0f * kPi * freq / (float) fs;
        if (phase > 2.0f * kPi) phase -= 2.0f * kPi;
        const float noise = (float) (int32_t) rand() * (1.0f / 2147483648.0f);
        lp += 0.2f * (noise - lp);
        return 0.25f * env * (kind == 3 ? lp * 2.0f : std::sin (phase)) + 0.01f * lp;
    }

private:
    uint32_t rand() noexcept { seed = seed * 1664525u + 1013904223u; return seed; }
    double fs = 48000;
    uint32_t seed = 0;
    int pos = 0, kind = 0;
    float env = 0, phase = 0, freq = 110, lp = 0;
};

// ---------------------------------------------------------------------------------------------------------------------
/** Tuner static: SYNTHESIZED noise that fills the real wait for a station (docs/p0/TUNER-ACCEPTANCE.md). It is not
    received radio, RF or interference from anywhere; only its timing comes from the world (the receiver's state).
    Band-passed noise whose centre wanders slowly, a little hiss, and sparse crackle. Deterministic, no allocation,
    zero-mean, never above 0.9. */
class StaticSource
{
public:
    static constexpr float kPeak = 0.9f;
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        seed = 0x9E3779B9u; counter = 0; s1 = s2 = crackle = 0.0f; wander1 = 0.0f; wander2 = 1.7f;
        crackleDecay = (float) std::exp (-1.0 / (0.0004 * fs));
        crackleChance = (float) (9.0 / fs);                                    // about nine crackles a second
        level = 0.62f * (float) std::sqrt (fs / 48000.0);                      // keeps the band's loudness the same at every sample rate
    }
    float next() noexcept
    {
        if ((counter++ & 31) == 0)                                             // the band's centre drifts between about 0.5 and 3.2 kHz
        {
            wander1 += 32.0f * 2.0f * kPi * 0.13f / (float) fs; if (wander1 > 2.0f * kPi) wander1 -= 2.0f * kPi;
            wander2 += 32.0f * 2.0f * kPi * 0.31f / (float) fs; if (wander2 > 2.0f * kPi) wander2 -= 2.0f * kPi;
            const float centre = 1300.0f * std::exp2 (1.3f * (0.6f * std::sin (wander1) + 0.4f * std::sin (wander2)));
            g = std::tan (kPi * std::min (centre, (float) fs * 0.2f) / (float) fs);
            a1 = 1.0f / (1.0f + g * (g + kDamping));
        }
        const float white = uniform();
        const float v1 = a1 * (s1 + g * (white - s2)), v2 = s2 + g * v1;       // TPT state-variable band-pass
        s1 = 2.0f * v1 - s1;
        s2 = 2.0f * v2 - s2;
        if (0.5f * (uniform() + 1.0f) < crackleChance) crackle = 0.9f * uniform();
        crackle *= crackleDecay;
        return std::min (kPeak, std::max (-kPeak, level * v1 + 0.05f * white + 0.5f * crackle));
    }

private:
    float uniform() noexcept { seed = seed * 1664525u + 1013904223u; return (float) (int32_t) seed * (1.0f / 2147483648.0f); }   // -1 .. 1
    static constexpr float kDamping = 1.0f / 1.4f;
    double fs = 48000;
    uint32_t seed = 0, counter = 0;
    float s1 = 0, s2 = 0, g = 0, a1 = 0, crackle = 0, crackleDecay = 0, crackleChance = 0, wander1 = 0, wander2 = 0, level = 0;
};

// ---------------------------------------------------------------------------------------------------------------------
struct Controls
{
    float p[kNumParams] = {};
    Mode mode = Mode::Engine;
    float detune = 0.0f;   // Dial::offCentre
    float gap = 0.0f;      // Radio::tuneGap: 0 = the station is playing, 1 = the receiver is fetching the one just asked for
    int request = 0;       // Radio::request: changes with every station request, so each one gets its own static
    int voice = 0;            // LAB comparison (docs/p0/VOICE-RESEARCH.md): 0 = Resonator (the instrument), 1 = Dissolve, 2 = Dissolve -> Resonator
    int strikeResponse = 0;   // LAB comparison (docs/p0/STRIKE-RESPONSE.md): 0 = Held (the existing behaviour), 1 = Fast, 2 = Organic
    EnvFrame room;         // EnvBus::read
    RoutingPlan plan = RoutingPlan::fromGraph (Graph::defaultPatch());
};

class Engine
{
public:
    static constexpr int kLines = 6;
    static constexpr float kCeiling = 0.89f;   // about -1 dBFS; the output never exceeds it
    static constexpr int kControlInterval = 16;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        lineSize = 1;
        while (lineSize < (int) (fs / 18.0) + 8) lineSize <<= 1;
        for (auto& l : lines) l.assign ((size_t) lineSize, 0.0f);
        refSize = (int) (fs * kRefDelaySeconds) + 4;
        for (auto& d : refDelay) d.assign ((size_t) refSize, 0.0f);
        aParam = onePoleCoeff (0.015, fs); aFreq = onePoleCoeff (0.025, fs); aMode = onePoleCoeff (0.03, fs);
        aMod = onePoleCoeff (0.02, fs / kControlInterval);
        tunerStatic.prepare (fs);
        dissolve.prepare (fs);
        gapSeconds = 0.0f;
        staticMuted = false;
        strikeRelease = (float) std::exp (-1.0 / (2.0 * fs / kControlInterval));
        strikeReleaseFast = (float) std::exp (-1.0 / (0.25 * fs / kControlInterval));
        strikeReleaseOrganic = (float) std::exp (-1.0 / (1.5 * fs / kControlInterval));
        aEaseUp = onePoleCoeff (0.35, fs / kControlInterval);
        aEaseDown = onePoleCoeff (1.2, fs / kControlInterval);
        aDc = onePoleCoeff (1.0 / (2.0 * kPi * 5.0), fs);
        aFast = onePoleCoeff (0.002, fs); aFastRel = onePoleCoeff (0.06, fs); aSlow = onePoleCoeff (0.3, fs);
        reset();
    }

    /** PANIC: clears all audio state, and keeps the tuner static silent until the current retune has finished or the
        player asks for another station. */
    void panic() noexcept
    {
        reset();
        staticMuted = true;
    }

    /** Clears all audio state; controls keep their values. Safe on the audio thread. */
    void reset() noexcept
    {
        staticGain = 0.0f;
        dissolve.reset();
        dissolveHeard = false;
        for (auto& l : lines) std::fill (l.begin(), l.end(), 0.0f);
        for (auto& d : refDelay) std::fill (d.begin(), d.end(), 0.0f);
        lp = {}; dc = {}; svfA = {}; svfB = {}; refLp = {};
        w = refW = 0; tick = 0; ringPhase = refLfo = 0.0f; rFast = rSlow = 0.0f;
        mod = {};
        strikeSeen = strikeEase = 0.0f;
        for (int i = 0; i < kLines; ++i) driftPhase[(size_t) i] = 0.7f * (float) i;
        first = true;
    }

    /** `radioL/R`: the live source. `local`: the test source (mono). Which of them is heard is up to the patch. */
    void process (const float* radioL, const float* radioR, const float* local, float* outL, float* outR, int n, const Controls& c) noexcept
    {
        if (first) { snapControls (c); first = false; }

        // LAB voice housekeeping. Dissolve's history is temporary: once the voice is deselected and has faded out, what
        // it heard is erased (in pieces, so no single callback pays for it), and it can never be replayed later.
        const bool labOn = c.voice == 1 || c.voice == 2;
        if (! labOn && voiceW[1] <= 1.0e-4f && voiceW[2] <= 1.0e-4f) { if (dissolveHeard) dissolveHeard = ! dissolve.forgetSome (16384); }
        else if (labOn && dissolveHeard && voiceW[1] <= 1.0e-4f && voiceW[2] <= 1.0e-4f && ! dissolveRunning) { dissolve.reset(); }   // re-selected mid-erase: finish at once
        dissolveRunning = labOn || voiceW[1] > 1.0e-4f || voiceW[2] > 1.0e-4f;
        bool bad = false;
        float peak = 0.0f;

        for (int s = 0; s < n; ++s)
        {
            if (tick-- <= 0) { tick = kControlInterval - 1; updateTargets (c); }
            smooth();

            // ---- radio, with the TUNE "detune" ring modulator. While the receiver is fetching a station, synthesized
            // static takes the broadcast's place HERE, at the radio input, so it is detuned, patched and resonated
            // like one. (Nothing is added when there is no static, so STATIC at 0 leaves every sample as it was.)
            float xl = sanitize (radioL[s]), xr = sanitize (radioR[s]);
            if (staticGain > 1.0e-6f || staticT > 0.0f)
            {
                const float hiss = staticGain * tunerStatic.next();
                xl += hiss; xr += hiss;
            }
            ringPhase += 2.0f * kPi * (20.0f + 380.0f * detune * detune) / (float) fs;
            if (ringPhase > 2.0f * kPi) ringPhase -= 2.0f * kPi;
            const float ring = (1.0f - detune + detune * std::cos (ringPhase)) * (1.0f - 0.4f * detune);
            xl *= ring; xr *= ring;

            // ---- radio-derived control (PRD P0-04): slow level and transient activity
            const float mag = 0.5f * std::abs (xl + xr);
            rFast += (mag > rFast ? aFast : aFastRel) * (mag - rFast);
            rSlow += aSlow * (mag - rSlow);

            // ---- the cable into resonator.audio_in (cross-faded, so plugging and pulling it never clicks)
            const float loc = sanitize (local[s]);
            xl = srcW[0] * xl + srcW[1] * loc;
            xr = srcW[0] * xr + srcW[1] * loc;

            // ---- LAB voice experiment: Dissolve, a rolling granular delay on the same excitation. With the resonator
            // selected (voice 0) none of this runs and every sample below is exactly what it was.
            const bool lab = dissolveRunning;
            float dl = 0.0f, dr = 0.0f, exL = xl, exR = xr;
            if (lab)
            {
                dissolveHeard = true;
                dissolveNow.dissolve += aParam * (dissolveP.dissolve - dissolveNow.dissolve);      // smoothed like every other control
                dissolveNow.spray += aParam * (dissolveP.spray - dissolveNow.spray);
                dissolveNow.regen += aParam * (dissolveP.regen - dissolveNow.regen);
                dissolveNow.drive += aParam * (dissolveP.drive - dissolveNow.drive);
                dissolveNow.time = dissolveP.time;                                                 // (glides inside Dissolve)
                dissolveNow.damp = damp;
                dissolve.process (xl, xr, dl, dr, dissolveNow);
                exL = xl + voiceW[2] * (dl - xl);                          // voice 2: the grains, not the radio, excite the resonators
                exR = xr + voiceW[2] * (dr - xr);
            }

            // ---- resonators
            std::array<float, kLines> v;
            float sum = 0.0f;
            for (int i = 0; i < kLines; ++i)
            {
                const auto k = (size_t) i;
                const float delay = std::max (3.0f, (float) fs / freq[k] - (1.0f - damp) / damp); // minus the damping filter's delay
                lp[k] += damp * (read (lines[k], delay) - lp[k]);          // frequency-dependent damping
                const float y = lp[k] - dc[k];                            // 5 Hz high-pass: no DC latch-up above unity gain
                dc[k] += aDc * y;
                v[k] = y;
                sum += y;
            }
            float wetL = 0.0f, wetR = 0.0f;
            for (int i = 0; i < kLines; ++i)
            {
                const auto k = (size_t) i;
                // coupling matrix I - couple * (2/N) * ones: eigenvalues 1 and 1 - 2*couple, so it never adds energy
                const float mixed = v[k] - couple * (2.0f / kLines) * sum;
                lines[k][(size_t) w] = ((i & 1) ? exR : exL) * excite + std::tanh (gain * mixed);
                ((i & 1) ? wetR : wetL) += v[k];
            }
            w = (w + 1) & (lineSize - 1);
            wetL *= wetGain; wetR *= wetGain;
            if (lab)                                                         // voice 1: Dissolve alone takes the resonators' place
            {
                wetL = (1.0f - voiceW[1]) * wetL + voiceW[1] * dl;
                wetR = (1.0f - voiceW[1]) * wetR + voiceW[1] * dr;
            }

            // ---- reference chain: an ordinary resonant filter into a feedback delay, for A/B listening
            float refL = 0.0f, refR = 0.0f;
            {
                refLfo += 2.0f * kPi * 0.3f / (float) fs;
                if (refLfo > 2.0f * kPi) refLfo -= 2.0f * kPi;
                const float fc = std::min ((float) fs * 0.2f, rootHz * std::exp2 (pc[kDrift] * std::sin (refLfo)));
                const float g = std::tan (kPi * fc / (float) fs), kq = 1.0f / (0.7f + 11.0f * pc[kShape]);
                const float a1 = 1.0f / (1.0f + g * (g + kq));
                const float in[2] = { xl, xr };
                float* out[2] = { &refL, &refR };
                for (size_t ch = 0; ch < 2; ++ch)
                {
                    const float delayed = refDelay[ch][(size_t) refW];
                    refLp[ch] += damp * (delayed - refLp[ch]);
                    const float x = in[ch] + std::tanh (0.9f * pc[kFeedback] * refLp[ch]);
                    const float v1 = a1 * (svfA[ch] + g * (x - svfB[ch])), v2 = svfB[ch] + g * v1; // TPT state-variable filter
                    svfA[ch] = 2.0f * v1 - svfA[ch];
                    svfB[ch] = 2.0f * v2 - svfB[ch];
                    const float filtered = 0.5f * (v2 + kq * v1);                                    // low-pass + band-pass
                    refDelay[ch][(size_t) refW] = filtered;
                    *out[ch] = filtered + 0.6f * delayed;
                }
                refW = (refW + 1) % refSize;
            }

            // ---- mix, level, safety clip
            const float mix = pc[kMix], level = 2.0f * pc[kLevel] * pc[kLevel];
            const float l = level * (modeW[1] * xl + (1.0f - modeW[1]) * (1.0f - mix) * xl + mix * (modeW[0] * wetL + modeW[2] * refL));
            const float r = level * (modeW[1] * xr + (1.0f - modeW[1]) * (1.0f - mix) * xr + mix * (modeW[0] * wetR + modeW[2] * refR));
            outL[s] = kCeiling * std::tanh (l / kCeiling);
            outR[s] = kCeiling * std::tanh (r / kCeiling);
            bad = bad || ! std::isfinite (outL[s]) || ! std::isfinite (outR[s]);
            peak = std::max (peak, std::max (std::abs (outL[s]), std::abs (outR[s])));
        }

        if (bad) // cannot happen by construction (every state is bounded); if it ever does, fail silent rather than loud
        {
            reset();
            std::fill (outL, outL + n, 0.0f);
            std::fill (outR, outR + n, 0.0f);
            faults.fetch_add (1);
            peak = 0.0f;
        }
        meterRadioLevel = radioLevel(); meterRadioActivity = radioActivity(); meterOut = peak; meterStatic = staticGain;
    }

    float lineFrequency (int i) const noexcept { return freq[(size_t) i]; }
    float strikeBackoff() const noexcept { return strikePlugged; }
    size_t dissolveMemoryBytes() const noexcept { return dissolve.memoryBytes(); }     // 0..1: how far the steady excitation is backed off (tests, LAB)
    std::atomic<int> faults { 0 };
    std::atomic<float> meterRadioLevel { 0 }, meterRadioActivity { 0 }, meterOut { 0 }, meterStatic { 0 };

private:
    static constexpr double kRefDelaySeconds = 0.33;
    // SHAPE morphs the six resonator ratios through these sets (interpolated in log frequency).
    static constexpr float kRatios[4][kLines] = {
        { 1.0f, 1.004f, 0.996f, 2.003f, 0.5f, 1.007f },     // near-unison "chorus of strings"
        { 1.0f, 1.5f, 2.0f, 3.0f, 0.5f, 4.0f },             // fifths and octaves
        { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f },             // harmonic series
        { 1.0f, 2.756f, 5.404f, 8.933f, 1.47f, 3.9f } };    // inharmonic, bar/bell-like
    static constexpr float kDriftRates[kLines] = { 0.11f, 0.17f, 0.23f, 0.31f, 0.41f, 0.53f };

    static float sanitize (float x) noexcept { return std::isfinite (x) ? std::min (4.0f, std::max (-4.0f, x)) : 0.0f; }
    float radioLevel() const noexcept { return clamp01 ((20.0f * std::log10 (rSlow + 1.0e-6f) + 50.0f) / 40.0f); } // -50 .. -10 dBFS
    float radioActivity() const noexcept { return clamp01 ((rFast - rSlow) / (rSlow + 0.01f)); }

    /** 4-point Hermite read `delay` samples behind the write head (delay >= 3). */
    float read (const std::vector<float>& line, float delay) const noexcept
    {
        const float pos = (float) w - delay + (float) lineSize;
        const int i0 = (int) pos;
        const float t = pos - (float) i0;
        const int mask = lineSize - 1;
        const float ym = line[(size_t) ((i0 - 1) & mask)], y0 = line[(size_t) (i0 & mask)];
        const float y1 = line[(size_t) ((i0 + 1) & mask)], y2 = line[(size_t) ((i0 + 2) & mask)];
        const float c1 = 0.5f * (y1 - ym), c2 = ym - 2.5f * y0 + 2.0f * y1 - 0.5f * y2, c3 = 0.5f * (y2 - ym) + 1.5f * (y0 - y1);
        return ((c3 * t + c2) * t + c1) * t + y0;
    }

    /** Control rate: every modulation is computed here from the knobs, the radio followers and the room. */
    void updateTargets (const Controls& c) noexcept
    {
        for (int i = 0; i < kNumParams; ++i) pt[i] = clamp01 (c.p[i]);
        detuneT = clamp01 (c.detune);
        for (int m = 0; m < 3; ++m) modeT[m] = (int) c.mode == m ? 1.0f : 0.0f;

        // ---- tuner static: only while the receiver reports a gap, and never for longer than 8 s (full for 4 s, then
        // fading), so a station that never arrives does not hiss for ever. PANIC mutes it until the gap has closed.
        // Each new request starts the clock again (and lifts a PANIC mute): hopping onward from a station that never
        // arrived must not find the static already spent.
        const float gapIn = clamp01 (c.gap);
        if (gapIn < 0.01f || c.request != lastRequest) { gapSeconds = 0.0f; staticMuted = false; lastRequest = c.request; }
        else gapSeconds += (float) kControlInterval / (float) fs;
        staticT = staticMuted ? 0.0f : gapIn * pt[kStatic] * clamp01 (2.0f - gapSeconds * 0.25f);

        // ---- the patch: each input reads the output plugged into it, scaled by that cable's signed depth, or nothing.
        // The plan comes from another thread, so every index and depth is checked here.
        const float cv[kNumCvOuts] = { radioLevel(), radioActivity(), clamp01 (c.room.level), clamp01 (c.room.activity), clamp01 (c.room.brightness) };
        const int audioSource = c.plan.sourceOf (kResAudioIn);
        srcT[0] = audioSource == kRadioAudioOut ? 1.0f : 0.0f;
        srcT[1] = audioSource == kLocalAudioOut ? 1.0f : 0.0f;
        for (int in = kResPitchIn; in < kNumPorts; ++in)
        {
            const int from = c.plan.sourceOf (in);
            const bool plugged = from >= kFirstCvOut && from < kFirstInput;
            auto& m = mod[(size_t) (in - kFirstInput)];
            const float depth = plugged ? validDepth (c.plan.depthOf (in)) : 0.0f;
            m += aMod * (depth * (plugged ? cv[from - kFirstCvOut] : 0.0f) - m);     // signed; glides when a cable or its depth moves
            if (in == kResExciteIn)
            {
                // How much the steady excitation backs off. P0 backed off whenever a cable was in STRIKE, even from a
                // source producing nothing, so a dead source dimmed the instrument. Now the back-off needs a source
                // that has actually been active lately: `strikeSeen` follows the source's recent peak (instant up,
                // about 2 s down) and counts as fully present from 0.1. A source that reads zero has no influence.
                const float source = plugged ? cv[from - kFirstCvOut] : 0.0f;
                float presence;
                if (c.strikeResponse == 1)            // B, Fast: short memory, gentle knee - the steady excitation breathes back between hits
                {
                    strikeSeen = std::max (source, strikeSeen * strikeReleaseFast);
                    if (strikeSeen < 1.0e-3f) strikeSeen = 0.0f;
                    strikeEase = strikeSeen;
                    presence = std::min (1.0f, strikeSeen * 3.0f);
                }
                else if (c.strikeResponse == 2)       // C, Organic: swells in, eases out, no hard edge
                {
                    strikeSeen = std::max (source, strikeSeen * strikeReleaseOrganic);
                    if (strikeSeen < 1.0e-3f) strikeSeen = 0.0f;
                    const float target = std::min (1.0f, strikeSeen * 3.0f);
                    strikeEase += (target > strikeEase ? aEaseUp : aEaseDown) * (target - strikeEase);
                    if (strikeEase < 1.0e-4f && target == 0.0f) strikeEase = 0.0f;
                    presence = strikeEase * strikeEase * (3.0f - 2.0f * strikeEase);
                }
                else                                  // A, Held: the existing behaviour, unchanged
                {
                    strikeSeen = std::max (source, strikeSeen * strikeRelease);
                    if (strikeSeen < 1.0e-3f) strikeSeen = 0.0f;
                    strikeEase = std::min (1.0f, strikeSeen);
                    presence = std::min (1.0f, strikeSeen * 10.0f);
                }
                strikePlugged += aMod * (std::max (0.0f, depth) * presence - strikePlugged);
            }
        }
        auto cvIn = [this] (int in) { return mod[(size_t) (in - kFirstInput)]; };
        const float cpl = pt[kCouple], drift = pt[kDrift];

        rootHz = 440.0f * std::exp2 ((36.0f + 48.0f * pt[kPitch] - 69.0f) / 12.0f);     // C2 .. C6
        const float shape = clamp01 (pt[kShape] + 0.35f * cpl * cvIn (kResShapeIn)) * 3.0f;
        const int set = std::min (2, (int) shape);
        const float frac = shape - (float) set;
        const float dt = (float) kControlInterval / (float) fs;
        // (the positive branch is written exactly as P0 wrote it, so +100 % stays bit-identical)
        const float driftCv = cvIn (kResDriftIn), driftRate = driftCv >= 0.0f ? (1.0f + 3.0f * cpl * driftCv) : 1.0f / (1.0f - 3.0f * cpl * driftCv);
        for (int i = 0; i < kLines; ++i)
        {
            const auto k = (size_t) i;
            // DRIFT: six slow phases, each bending its neighbour's; the DRIFT input makes them run up to 4x faster,
            // or with negative depth up to 4x slower (never backwards).
            driftPhase[k] += 2.0f * kPi * dt * kDriftRates[k] * (0.05f + 1.5f * drift) * driftRate;
            if (driftPhase[k] > 2.0f * kPi) driftPhase[k] -= 2.0f * kPi;
        }
        for (int i = 0; i < kLines; ++i)
        {
            const auto k = (size_t) i;
            const float ratio = std::exp ((1.0f - frac) * std::log (kRatios[set][k]) + frac * std::log (kRatios[set + 1][k]));
            const float cents = 200.0f * drift * drift * std::sin (driftPhase[k] + 0.8f * std::sin (driftPhase[(k + 1) % kLines]))
                              + 100.0f * cpl * cvIn (kResPitchIn);                        // PITCH input: up to a semitone sharp, or flat
            freqT[k] = std::min (std::min (6000.0f, (float) fs * 0.2f), std::max (20.0f, rootHz * ratio * std::exp2 (cents / 1200.0f)));
        }
        const float cutoff = std::max (60.0f, std::min ((float) fs * 0.45f, 200.0f * std::exp2 (6.3f * pt[kTone] + 3.0f * cpl * cvIn (kResDampIn))));   // negative depth closes it
        dampT = 1.0f - std::exp (-2.0f * kPi * cutoff / (float) fs);
        const float fb = 1.0f - pt[kFeedback];
        gainT = std::max (0.0f, std::min (1.25f, 1.1f * (1.0f - fb * fb) + 0.2f * cpl * cvIn (kResFeedbackIn))); // unity loop gain near FEEDBACK 0.7
        coupleT = 0.45f * drift;
        // STRIKE input. Positive depth: while the source is active the steady excitation backs off (in proportion to
        // the depth) and the control signal strikes the resonators. Negative depth: the steady excitation stays and
        // the signal ducks it.
        const float strike = cpl * strikePlugged;
        exciteT = std::max (0.0f, 0.5f * ((1.0f - 0.7f * strike) + 5.0f * cpl * cvIn (kResExciteIn)));
        const float g2 = gainT * gainT;
        wetGainT = 1.0f / (1.0f + 2.0f * g2 * g2 * g2);

        // ---- LAB voice: Dissolve borrows the panel. TIME <- PITCH (higher = shorter), DISSOLVE <- SHAPE, SPRAY <- DRIFT,
        // regeneration <- FEEDBACK, damping <- TONE, and the same six control inputs under the same names.
        for (int m = 1; m < 3; ++m) voiceT[m] = c.voice == m ? 1.0f : 0.0f;
        dissolveP.time = 1.6f * std::exp2 (-5.7f * pt[kPitch] - cpl * cvIn (kResPitchIn));
        dissolveP.dissolve = clamp01 (pt[kShape] + 0.5f * cpl * cvIn (kResShapeIn));
        dissolveP.spray = clamp01 (drift + 0.5f * cpl * cvIn (kResDriftIn));
        dissolveP.regen = 0.95f * pt[kFeedback] + 0.3f * cpl * cvIn (kResFeedbackIn);
        dissolveP.damp = dampT;
        dissolveP.drive = std::min (3.0f, 2.0f * exciteT);                                  // keeps loudness roughly level as the loop closes
    }

    void smooth() noexcept
    {
        for (int i = 0; i < kNumParams; ++i) pc[i] += aParam * (pt[i] - pc[i]);
        for (size_t k = 0; k < kLines; ++k) freq[k] += aFreq * (freqT[k] - freq[k]);
        for (int m = 0; m < 3; ++m) modeW[m] += aMode * (modeT[m] - modeW[m]);
        for (int m = 0; m < 2; ++m) srcW[m] += aMode * (srcT[m] - srcW[m]);
        detune += aParam * (detuneT - detune);
        staticGain += aParam * (staticT - staticGain);
        for (int m = 1; m < 3; ++m) voiceW[m] += aMode * (voiceT[m] - voiceW[m]);
        damp += aParam * (dampT - damp);
        gain += aParam * (gainT - gain);
        couple += aParam * (coupleT - couple);
        excite += aFast * (exciteT - excite);
        wetGain += aParam * (wetGainT - wetGain);
    }

    void snapControls (const Controls& c) noexcept
    {
        updateTargets (c);
        srcW[0] = srcT[0]; srcW[1] = srcT[1];
        for (int i = 0; i < kNumParams; ++i) pc[i] = pt[i];
        freq = freqT;
        for (int m = 0; m < 3; ++m) modeW[m] = modeT[m];
        detune = detuneT; damp = dampT; gain = gainT; couple = coupleT; excite = exciteT; wetGain = wetGainT;
    }

    double fs = 48000;
    int lineSize = 0, refSize = 0, w = 0, refW = 0, tick = 0;
    bool first = true;
    std::array<std::vector<float>, kLines> lines;
    std::array<std::vector<float>, 2> refDelay;
    std::array<float, kLines> lp {}, dc {}, freq {}, freqT {}, driftPhase {};
    std::array<float, 2> svfA {}, svfB {}, refLp {};
    float pc[kNumParams] = {}, pt[kNumParams] = {}, modeW[3] = {}, modeT[3] = {};
    float detune = 0, detuneT = 0, damp = 0.5f, dampT = 0.5f, gain = 0, gainT = 0, couple = 0, coupleT = 0;
    float excite = 0, exciteT = 0, wetGain = 0, wetGainT = 0, rootHz = 110;
    float ringPhase = 0, refLfo = 0, rFast = 0, rSlow = 0;
    std::array<float, kNumInputs> mod {};   // smoothed value at each resonator input (index 0, the audio input, unused)
    StaticSource tunerStatic;
    Dissolve dissolve;
    Dissolve::Params dissolveP, dissolveNow;
    bool dissolveHeard = false, dissolveRunning = false;
    float voiceW[3] = {}, voiceT[3] = {};
    float staticGain = 0, staticT = 0, gapSeconds = 0;
    int lastRequest = 0;
    bool staticMuted = false;
    float srcW[2] = {}, srcT[2] = {}, strikePlugged = 0, strikeSeen = 0, strikeRelease = 0, aMod = 0;
    float strikeEase = 0, strikeReleaseFast = 0, strikeReleaseOrganic = 0, aEaseUp = 0, aEaseDown = 0;
    float aParam = 0, aFreq = 0, aMode = 0, aDc = 0, aFast = 0, aFastRel = 0, aSlow = 0;
};

} // namespace wildband
