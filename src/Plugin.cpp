// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Stephane P. Pericat
//
// WILDBAND as a plugin (AU, VST3): first proof of the shared architecture, docs/p1/PLUGIN-ARCHITECTURE.md.
// A thin juce::AudioProcessor around the same Instrument the standalone uses. The host owns the audio device, MIDI
// and state. An instrument: no audio input. No custom editor yet (the host shows its generic one), no ROOM, no patch
// bay state. This binary declares no microphone use and contains no capture path.
// The radio receiver only touches the network once the plugin is genuinely being played in real time - never while
// a host scans, validates, loads or restores it, nor during offline renders: docs/p1/PLUGIN-RECEIVER-LIFECYCLE.md
#include <juce_audio_processors/juce_audio_processors.h>

#include "Instrument.h"
#include "ReceiverGate.h"

namespace wildband
{

class WildbandProcessor final : public juce::AudioProcessor, private juce::Timer
{
public:
    WildbandProcessor() : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
    {
        // Host parameters mirror the instrument's knobs. Their IDs, and the order hosts see them in, are part of the
        // contract with saved projects and automation. AU hosts see them sorted by hashed ID (JUCE's AU wrapper), and
        // Logic addresses them by that index: the hosted order is frozen by tools/PluginCheck.cpp. Never rename,
        // remove or re-hint one; a parameter added later gets version hint 2 (or higher) so it sorts after these.
        for (int i = 0; i < kNumParams; ++i)
        {
            const float initial = i == kTune ? dialCentre (0, kNumStations) : paramDefaults[i];
            addParameter (hostParams[(size_t) i] = new juce::AudioParameterFloat (juce::ParameterID { juce::String (paramNames[i]).toLowerCase(), 1 }, paramNames[i],
                                                                                  juce::NormalisableRange<float> (0.0f, 1.0f), initial));
        }
        for (int i = 0; i < kNumStations; ++i) stations[(size_t) i] = kDefaultStations[i];
        // Test aid (inert unless set): WILDBAND_TEST_STATIONS="url1,url2,..." replaces the default stations, so a
        // local server can count connections. It changes only where a station request would go, never when.
        if (const char* test = std::getenv ("WILDBAND_TEST_STATIONS"))
        {
            const auto urls = juce::StringArray::fromTokens (test, ",", "");
            for (int i = 0; i < kNumStations && i < urls.size(); ++i) stations[(size_t) i] = { "test " + juce::String (i + 1), urls[i].trim() };
        }
        startTimerHz (30);
    }
    ~WildbandProcessor() override
    {
        stopTimer();
        instrument.radio.tune ({});     // cancel any connection now; Radio's destructor then joins its thread
    }

    // ---- audio
    void prepareToPlay (double sampleRate, int samplesPerBlock) override
    {
        instrument.prepareForHost (sampleRate, samplesPerBlock);
        renderedSamples = 0;            // (lastCallbackMs is kept: a host re-preparing a live plugin must not look like silence)
        rate = sampleRate;
        lifecycle.push ({ Event::Prepared, isNonRealtime() });
    }
    void releaseResources() override { lifecycle.push ({ Event::Released, false }); }
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        return layouts.getMainInputChannelSet().isDisabled() && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        for (int i = 0; i < kNumParams; ++i)        // host automation -> the instrument (MIDI learn writes the instrument too; see timerCallback)
        {
            const float v = hostParams[(size_t) i]->get();
            if (! juce::exactlyEqual (v, lastHost[(size_t) i].load (std::memory_order_relaxed)))
            {
                lastHost[(size_t) i].store (v, std::memory_order_relaxed);
                instrument.params[(size_t) i].store (v, std::memory_order_relaxed);
            }
        }
        for (const auto meta : midi) instrument.handleIncomingMidiMessage (nullptr, meta.getMessage());
        instrument.renderForHost (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
        // what the receiver gate needs to know (the message thread decides; nothing here touches the network)
        renderedSamples.store (renderedSamples.load (std::memory_order_relaxed) + buffer.getNumSamples(), std::memory_order_relaxed);
        lastCallbackMs.store (juce::Time::getMillisecondCounterHiRes(), std::memory_order_relaxed);
        nonRealtimeNow.store (isNonRealtime(), std::memory_order_relaxed);
    }

    // ---- description
    const juce::String getName() const override { return "WILDBAND"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }
    bool hasEditor() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    // ---- state: settings only. No audio is ever stored (and there is none to store). The host parameters are the
    // record: they are what the host automated and what MIDI learn has been mirrored into (timerCallback), and they
    // are current even if the host saves without having called processBlock since the last change.
    void getStateInformation (juce::MemoryBlock& destination) override
    {
        juce::XmlElement xml ("WILDBAND");
        xml.setAttribute ("version", 1);
        for (int i = 0; i < kNumParams; ++i) xml.setAttribute (juce::String (paramNames[i]).toLowerCase(), (double) hostParams[(size_t) i]->get());
        copyXmlToBinary (xml, destination);
    }
    void setStateInformation (const void* data, int size) override
    {
        const auto xml = getXmlFromBinary (data, size);
        if (xml == nullptr || ! xml->hasTagName ("WILDBAND")) return;
        for (int i = 0; i < kNumParams; ++i)
        {
            const auto id = juce::String (paramNames[i]).toLowerCase();
            if (! xml->hasAttribute (id)) continue;
            const float v = clamp01 ((float) xml->getDoubleAttribute (id));
            instrument.params[(size_t) i] = v;
            lastHost[(size_t) i] = v;
            hostParams[(size_t) i]->setValueNotifyingHost (v);   // range is 0..1, so normalised = plain
        }
    }

    Instrument instrument;      // public for the headless check

private:
    /** Message thread. The station follows TUNE once it has rested for a quarter of a second, as in the standalone;
        Radio::tune takes a lock, so it is never called from processBlock. */
    void timerCallback() override
    {
        // ---- receiver lifecycle (docs/p1/PLUGIN-RECEIVER-LIFECYCLE.md)
        const bool wasActive = gate.active();            // before release/prepare events can change it
        Event e;
        const double nowS = juce::Time::getMillisecondCounterHiRes() / 1000.0, lastS = lastCallbackMs.load() / 1000.0;
        while (lifecycle.pop (e)) { if (e.type == Event::Prepared) gate.prepared (e.nonRealtime); else gate.released (nowS); }
        gate.update (nowS, (double) renderedSamples.load() / rate, lastS, nonRealtimeNow.load());
        if (wasActive && ! gate.active()) { instrument.radio.resume ({}); if (tuned >= 0) suspendedStation = tuned; tuned = -1; }   // suspend / release: cancel the stream

        // A knob moved by MIDI learn (the instrument's own controller map) is mirrored into its host parameter, so the
        // host sees it and saves it - unless the host has moved that parameter itself since processBlock last looked.
        for (int i = 0; i < kNumParams; ++i)
        {
            const float mine = instrument.params[(size_t) i].load(), host = hostParams[(size_t) i]->get();
            if (std::abs (mine - host) > 1.0e-5f && juce::exactlyEqual (host, lastHost[(size_t) i].load()))
            {
                lastHost[(size_t) i] = mine;
                hostParams[(size_t) i]->setValueNotifyingHost (mine);
            }
        }

        if (! gate.active()) return;                     // not being played in real time: no station request, no network
        const int wantedNow = dial (instrument.params[kTune].load(), kNumStations).station;
        const auto now = juce::Time::getMillisecondCounter();
        if (wantedNow != wanted) { wanted = wantedNow; wantedSince = now; }
        if (wanted != tuned && now - wantedSince > 250)
        {
            // Coming back to the station that was playing before a suspension, a release or construction is not a retune:
            // it buffers silently. Moving TUNE to another station while live is one, with tuner static.
            const bool resuming = wanted == suspendedStation || suspendedStation == kNever;
            tuned = wanted;
            suspendedStation = -1;
            if (resuming) instrument.radio.resume (stations[(size_t) tuned].url);
            else instrument.radio.tune (stations[(size_t) tuned].url);
        }
    }

    // audio thread -> message thread, for the receiver gate
    std::atomic<juce::int64> renderedSamples { 0 };
    std::atomic<double> lastCallbackMs { -1.0 };
    std::atomic<bool> nonRealtimeNow { false };
    double rate = 48000.0;
    struct Event { enum Type { Prepared, Released } type = Prepared; bool nonRealtime = false; };
    struct EventQueue   // prepare/release may come from any host thread; the timer drains it. Bounded; a full queue keeps the newest
    {
        void push (Event e) noexcept { const juce::SpinLock::ScopedLockType l (lock); if (count == 8) { head = (head + 1) % 8; --count; } items[(head + count++) % 8] = e; }
        bool pop (Event& e) noexcept { const juce::SpinLock::ScopedLockType l (lock); if (count == 0) return false; e = items[head]; head = (head + 1) % 8; --count; return true; }
        juce::SpinLock lock; Event items[8]; int head = 0, count = 0;
    } lifecycle;
    ReceiverGate gate;

public:
    const char* receiverState() const noexcept { return ReceiverGate::name (gate.current()); }   // for the headless check

private:
    std::array<juce::AudioParameterFloat*, kNumParams> hostParams {};
    std::array<std::atomic<float>, kNumParams> lastHost {};
    std::array<Station, kNumStations> stations;
    static constexpr int kNever = -2;     // never connected yet: the first connection is a start, not a retune
    int wanted = -1, tuned = -1, suspendedStation = kNever;
    juce::uint32 wantedSince = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WildbandProcessor)
};

} // namespace wildband

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new wildband::WildbandProcessor(); }
