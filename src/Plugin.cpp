// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Stephane P. Pericat
//
// WILDBAND as a plugin (AU, VST3): first proof of the shared architecture, docs/p1/PLUGIN-ARCHITECTURE.md.
// A thin juce::AudioProcessor around the same Instrument the standalone uses. The host owns the audio device, MIDI
// and state. An instrument: no audio input. No custom editor yet (the host shows its generic one), no ROOM, no patch
// bay state. This binary declares no microphone use and contains no capture path.
#include <juce_audio_processors/juce_audio_processors.h>

#include "Instrument.h"

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
        startTimerHz (30);
    }
    ~WildbandProcessor() override { stopTimer(); }

    // ---- audio
    void prepareToPlay (double sampleRate, int samplesPerBlock) override { instrument.prepareForHost (sampleRate, samplesPerBlock); }
    void releaseResources() override {}
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

        const int wantedNow = dial (instrument.params[kTune].load(), kNumStations).station;
        const auto now = juce::Time::getMillisecondCounter();
        if (wantedNow != wanted) { wanted = wantedNow; wantedSince = now; }
        if (wanted != tuned && now - wantedSince > 250) { tuned = wanted; instrument.radio.tune (stations[(size_t) tuned].url); }
    }

    std::array<juce::AudioParameterFloat*, kNumParams> hostParams {};
    std::array<std::atomic<float>, kNumParams> lastHost {};
    std::array<Station, kNumStations> stations;
    int wanted = -1, tuned = -1;
    juce::uint32 wantedSince = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WildbandProcessor)
};

} // namespace wildband

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new wildband::WildbandProcessor(); }
