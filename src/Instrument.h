// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Stephane P. Pericat
//
// WILDBAND's instrument core: everything the audio and MIDI threads touch - engine, radio receiver, controller maps,
// timing counters. No audio device and no window: the standalone app (Main.cpp) and the plugin (Plugin.cpp) are two
// shells around this one class. docs/p1/PLUGIN-ARCHITECTURE.md
#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

#include "Engine.h"
#include "Radio.h"

namespace wildband
{

struct Station { juce::String name, url; };
// Starting points only: public stream addresses of independent broadcasters. Nothing of theirs is bundled, and a
// public stream is not a licence to rebroadcast (docs/p0/RADIO-COMPATIBILITY.md).
const Station kDefaultStations[] = {
    { "SomaFM Groove Salad", "https://ice2.somafm.com/groovesalad-128-mp3" },
    { "KEXP Seattle", "https://kexp.streamguys1.com/kexp160.aac" },
    { "WFMU Freeform", "http://stream0.wfmu.org/freeform-128k" },
    { "BBC World Service", "http://stream.live.vc.bbcmedia.co.uk/bbc_world_service" },
    { "Radio Paradise", "https://stream.radioparadise.com/aac-128" },
    { "SomaFM SF 10-33", "https://ice2.somafm.com/sf1033-128-mp3" } };
constexpr int kNumStations = (int) std::size (kDefaultStations);

// ---------------------------------------------------------------------------------------------------------------------
/** Everything the audio and MIDI threads touch. The UI talks to it through atomics only. */
class Instrument final : public juce::AudioIODeviceCallback, public juce::MidiInputCallback
{
public:
    Instrument()
    {
        for (int i = 0; i < kNumParams; ++i) params[(size_t) i] = paramDefaults[i];
        params[kTune] = dialCentre (0, kNumStations);
        for (auto& c : ccMap) c = -1;      // no controller is mapped until MIDI LEARN maps one: no guessed defaults
        for (auto& n : noteMap) n = -1;
        plans.publish (RoutingPlan::fromGraph (Graph::defaultPatch()));
    }

    // ---- shared state
    std::array<std::atomic<float>, kNumParams> params;
    std::atomic<int> mode { 0 };
    PlanMailbox plans;                                      // written by the patch bay (message thread), read by the callback
    std::atomic<int> strikeResponse { 0 };                  // LAB: 0 Held (existing), 1 Fast, 2 Organic
    std::atomic<int> voice { 0 };                           // LAB: 0 Resonator (the instrument), 1 Dissolve, 2 Dissolve -> Resonator
    std::atomic<bool> roomEnabled { false }, panic { false }, stallOnce { false };   // stallOnce: --measure positive control only
    // Station navigation can be mapped too. Targets from kNumParams up are actions, not knobs: PREVIOUS, NEXT, then one
    // per station slot. A controller fires an action when it rises through half way; a mapped note fires it on note-on
    // (and then no longer sets PITCH). The UI thread performs the action (navSteps / presetRequest).
    enum Action : int { kActionPrev = kNumParams, kActionNext, kActionPreset, kNumTargets = kActionPreset + kNumStations };
    std::array<std::atomic<int8_t>, 128> noteMap;           // note number -> Action, -1 = unmapped
    std::atomic<int> navSteps { 0 }, presetRequest { -1 };
    std::array<std::atomic<int8_t>, 128> ccMap;             // controller number -> Param or Action, -1 = unmapped
    std::atomic<int> learnTarget { -1 }, midiEvents { 0 };
    std::atomic<bool> mapDirty { false };
    std::atomic<float> roomLevel { 0 }, roomActivity { 0 }, roomBrightness { 0 };
    std::atomic<double> sampleRate { 0 };
    std::atomic<int> blockSize { 0 };

    Radio radio;
    Engine engine;

    // ---- callback timing (the instrument itself; host-time gaps are the dropout evidence, as AFTERIMAGE found
    // JUCE's xrun counter does not move on this macOS)
    struct Perf
    {
        static constexpr int kBins = 400;                   // 5 us each: 0 .. 2 ms, last bin = everything above
        std::array<std::atomic<uint32_t>, kBins> histogram;
        std::atomic<uint64_t> callbacks { 0 }, lateCallbacks { 0 };
        std::atomic<double> maxUs { 0 }, sumUs { 0 }, maxGapMs { 0 };
        void reset() { for (auto& b : histogram) b = 0; callbacks = 0; lateCallbacks = 0; maxUs = 0; sumUs = 0; maxGapMs = 0; }
        double percentileUs (double p) const
        {
            uint64_t total = 0, run = 0;
            for (auto& b : histogram) total += b.load();
            for (int i = 0; i < kBins; ++i) { run += histogram[(size_t) i].load(); if ((double) run >= p * (double) total) return (i + 1) * 5.0; }
            return kBins * 5.0;
        }
    } perf;

    // ---- audio device
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override
    {
        const double fs = device->getCurrentSampleRate();
        engine.prepare (fs);
        local.prepare (fs);
        radio.setOutputRate (fs);
        sampleRate = fs;
        blockSize = device->getCurrentBufferSizeSamples();
        lastHostNs = 0;
        perf.reset();
    }
    void audioDeviceStopped() override { sampleRate = 0; }

    /** Message thread: the latest values from the WILDBAND Room helper. They go stale, and so neutral, by themselves. */
    void publishRoom (EnvFrame frame) { env.publish (kEnvMicrophone, frame, (int64_t) juce::Time::getMillisecondCounter()); }
    void withdrawRoom() { env.withdraw (kEnvMicrophone); }

    void audioDeviceIOCallbackWithContext (const float* const*, int, float* const* out, int numOut, int n, const juce::AudioIODeviceCallbackContext& context) override
    {
        const auto t0 = juce::Time::getHighResolutionTicks();
        juce::ScopedNoDenormals noDenormals;
        if (panic.exchange (false)) engine.panic();      // also silences tuner static for the rest of the current retune

        Controls c;
        for (int i = 0; i < kNumParams; ++i) c.p[i] = params[(size_t) i].load (std::memory_order_relaxed);
        c.mode = (Mode) mode.load (std::memory_order_relaxed);
        c.detune = dial (c.p[kTune], kNumStations).offCentre;
        c.gap = radio.tuneGap.load (std::memory_order_relaxed);   // written by radio.read in the previous block
        c.request = radio.request();
        c.strikeResponse = strikeResponse.load (std::memory_order_relaxed);
        c.voice = voice.load (std::memory_order_relaxed);
        c.plan = plans.read();                              // whole plan or the previous whole plan, never a mixture
        const auto nowMs = (int64_t) juce::Time::getMillisecondCounter();

        for (int at = 0; at < n; at += kScratch)
        {
            const int m = juce::jmin (kScratch, n - at);
            radio.read (radioL, radioR, m);
            for (int i = 0; i < m; ++i) localBuf[i] = local.next();
            c.room = env.read (nowMs);
            float* l = numOut > 0 && out[0] != nullptr ? out[0] + at : spare;
            float* r = numOut > 1 && out[1] != nullptr ? out[1] + at : spare;
            engine.process (radioL, radioR, localBuf, l, r, m, c);
        }
        for (int ch = 2; ch < numOut; ++ch) if (out[ch] != nullptr) juce::FloatVectorOperations::clear (out[ch], n);
        roomLevel = c.room.level; roomActivity = c.room.activity; roomBrightness = c.room.brightness;

        if (stallOnce.exchange (false))   // --measure positive control: overrun this callback by three periods, on purpose
            while (juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0) < 3.0 * n / sampleRate.load()) {}
        const double us = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0) * 1.0e6;
        perf.histogram[(size_t) juce::jlimit (0, Perf::kBins - 1, (int) (us / 5.0))].fetch_add (1, std::memory_order_relaxed);
        perf.callbacks.fetch_add (1, std::memory_order_relaxed);
        perf.sumUs = perf.sumUs.load (std::memory_order_relaxed) + us;
        if (us > perf.maxUs.load (std::memory_order_relaxed)) perf.maxUs = us;
        if (context.hostTimeNs != nullptr)
        {
            const uint64_t host = *context.hostTimeNs;
            if (lastHostNs != 0 && lastN > 0)
            {
                const double gapMs = (double) (host - lastHostNs) * 1.0e-6, expectedMs = 1000.0 * lastN / sampleRate.load (std::memory_order_relaxed);
                if (gapMs > perf.maxGapMs.load (std::memory_order_relaxed)) perf.maxGapMs = gapMs;
                if (gapMs > 1.5 * expectedMs) perf.lateCallbacks.fetch_add (1, std::memory_order_relaxed);
            }
            lastHostNs = host;
            lastN = n;
        }
    }

    // ---- for a plugin host, which owns the device: the same two steps without a juce::AudioIODevice
    void prepareForHost (double fs, int block)
    {
        engine.prepare (fs);
        local.prepare (fs);
        radio.setOutputRate (fs);
        sampleRate = fs;
        blockSize = block;
        lastHostNs = 0;
        perf.reset();
    }
    void renderForHost (float* const* out, int numOut, int n) { audioDeviceIOCallbackWithContext (nullptr, 0, out, numOut, n, {}); }

    // ---- MIDI (its own thread): controllers move knobs, notes set PITCH
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m) override
    {
        if (m.isController())
        {
            const auto cc = (size_t) m.getControllerNumber();
            const int value = m.getControllerValue();
            if (const int target = learnTarget.exchange (-1); target >= 0)
            {
                unmap (target);
                ccMap[cc] = (int8_t) target;
                lastCc[cc] = 127;                                   // the move that taught it does not also fire it
                mapDirty = true;
            }
            const int target = ccMap[cc].load();
            if (target >= kNumParams) { if (value >= 64 && lastCc[cc] < 64) fire (target); }
            else if (target >= 0) params[(size_t) target] = (float) value / 127.0f;
            lastCc[cc] = (uint8_t) value;
            midiEvents.fetch_add (1);
        }
        else if (m.isNoteOn())
        {
            const auto note = (size_t) m.getNoteNumber();
            if (const int target = learnTarget.load(); target >= kNumParams)      // learning an action: a note may be its trigger
            {
                learnTarget = -1;
                unmap (target);
                noteMap[note] = (int8_t) target;
                mapDirty = true;
            }
            else if (const int action = noteMap[note].load(); action >= 0) fire (action);
            else params[kPitch] = clamp01 ((float) ((int) note - 36) / 48.0f);
            midiEvents.fetch_add (1);
        }
    }

    void unmap (int target) noexcept
    {
        for (auto& other : ccMap) if (other.load() == target) other = -1;
        for (auto& other : noteMap) if (other.load() == target) other = -1;
    }

private:
    void fire (int action) noexcept          // MIDI thread; the UI timer carries it out
    {
        if (action == kActionPrev) navSteps.fetch_sub (1);
        else if (action == kActionNext) navSteps.fetch_add (1);
        else if (action >= kActionPreset && action < kNumTargets) presetRequest = action - kActionPreset;
    }
    std::array<uint8_t, 128> lastCc {};      // MIDI thread only
    static constexpr int kScratch = 2048;
    EnvBus env;
    TestSource local;
    float radioL[kScratch], radioR[kScratch], localBuf[kScratch], spare[kScratch];
    uint64_t lastHostNs = 0;
    int lastN = 0;
};

} // namespace wildband
