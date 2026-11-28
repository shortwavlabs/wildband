// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Stephane P. Pericat
//
// WILDBAND as a plugin (AU, VST3): docs/p1/PLUGIN-ARCHITECTURE.md. A thin juce::AudioProcessor around the same
// Instrument the standalone uses. The host owns the audio device, MIDI and state. An instrument: no audio input. No
// ROOM, no patch bay state. This binary declares no microphone use and contains no capture path.
//
// The radio receiver touches the network only after an explicit RECEIVE - the editor's button, or a MIDI control the
// user mapped to it - and only while the host processes in real time. RECEIVE is runtime state: off at construction,
// never saved or restored, revoked by RECEIVE off, state restore, destruction and PANIC. docs/p1/PLUGIN-RECEIVE.md
#include <juce_audio_processors/juce_audio_processors.h>

#include "Instrument.h"
#include "ReceiverControl.h"

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
        // RECEIVE is deliberately NOT a host parameter: a parameter would be saved, restored and automatable.
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
        // Test aid (inert unless set): WILDBAND_TEST_RECEIVE_CC=<n> maps controller n to RECEIVE, as if the user had
        // used LEARN, so a headless harness can make the gesture. It maps; it does not activate.
        if (const char* cc = std::getenv ("WILDBAND_TEST_RECEIVE_CC")) mapping.restore (juce::String (cc).getIntValue(), -1);
        startTimerHz (30);
    }

    ~WildbandProcessor() override
    {
        stopTimer();
        revoke();       // RECEIVE off, stream cancelled now; Radio's destructor then joins its thread
    }

    // ---- RECEIVE. Any thread except the audio thread. An explicit setting supersedes MIDI toggles still pending.
    void setReceive (bool on)
    {
        const juce::ScopedLock sl (receiverLock);
        requests.discardToggles();
        control.setReceive (0, on);
        if (! on) cancelLocked();
    }
    bool receiving() const { const juce::ScopedLock sl (receiverLock); return control.receiving (0); }
    void panic()
    {
        instrument.panic = true;        // the engine silences itself on the next audio block
        revoke();
    }
    /** For the editor: what the receiver is doing, in one line. */
    juce::String receiverStatus() const
    {
        const juce::ScopedLock sl (receiverLock);
        if (! control.receiving (0)) return "RECEIVE off - no network";
        if (! connected) return "RECEIVE on - waiting for the host to play (no connection)";
        return juce::String (Radio::name (instrument.radio.state())) + "  " + (tuned >= 0 ? stations[(size_t) tuned].name : juce::String());
    }
    void armReceiveLearn() noexcept { mapping.armLearn(); }
    bool learningReceive() const noexcept { return mapping.learning(); }
    juce::String receiveMapping() const
    {
        if (mapping.cc() >= 0) return "CC " + juce::String (mapping.cc());
        if (mapping.note() >= 0) return "note " + juce::String (mapping.note());
        return "no MIDI control";
    }

    // ---- audio. prepare / release may come from any host thread: they only store atomics, which the timer reads.
    // (lastCallbackMs survives a prepare, so a host re-preparing a live plugin does not look like silence.)
    void prepareToPlay (double sampleRate, int samplesPerBlock) override
    {
        instrument.prepareForHost (sampleRate, samplesPerBlock);
        preparedOffline.store (isNonRealtime());
        prepared.store (true);
    }
    void releaseResources() override
    {
        releasedAtMs.store (juce::Time::getMillisecondCounterHiRes());
        prepared.store (false);
    }
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
        const bool offline = isNonRealtime();
        uint32_t toggles = 0;
        bool panicSeen = false;
        for (const auto meta : midi)
        {
            const auto m = meta.getMessage();
            if (receiveMidi (m, offline, toggles, panicSeen)) continue;   // RECEIVE learn / toggle and PANIC: requests only
            instrument.handleIncomingMidiMessage (nullptr, m);
        }
        requests.publishBlock (toggles, panicSeen);     // once per block: PANIC in a block outranks every toggle in it
        instrument.renderForHost (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
        lastCallbackMs.store (juce::Time::getMillisecondCounterHiRes(), std::memory_order_relaxed);
        offlineNow.store (offline, std::memory_order_relaxed);
    }

    // ---- description
    const juce::String getName() const override { return "WILDBAND"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    // ---- state: settings only. No audio is ever stored (and there is none to store). The host parameters are the
    // record: they are what the host automated and what MIDI learn has been mirrored into (timerCallback). The RECEIVE
    // MIDI mapping is configuration and is saved; RECEIVE itself never is.
    void getStateInformation (juce::MemoryBlock& destination) override
    {
        juce::XmlElement xml ("WILDBAND");
        xml.setAttribute ("version", 1);
        for (int i = 0; i < kNumParams; ++i) xml.setAttribute (juce::String (paramNames[i]).toLowerCase(), (double) hostParams[(size_t) i]->get());
        xml.setAttribute ("receiveCc", mapping.cc());
        xml.setAttribute ("receiveNote", mapping.note());
        copyXmlToBinary (xml, destination);
    }
    void setStateInformation (const void* data, int size) override
    {
        // Restoring a state always turns RECEIVE off, discards MIDI toggles still pending, and cancels any request,
        // including one in flight. The RECEIVE mapping is replaced by the state's; a state without mapping fields
        // (saved before RECEIVE existed, or not a WILDBAND state) leaves no mapping. Either way LEARN is cancelled.
        revoke();
        const auto xml = getXmlFromBinary (data, size);
        const bool ours = xml != nullptr && xml->hasTagName ("WILDBAND");
        mapping.restore (ours ? xml->getIntAttribute ("receiveCc", -1) : -1, ours ? xml->getIntAttribute ("receiveNote", -1) : -1);
        if (! ours) return;
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
    // ---- audio thread: RECEIVE learn, a mapped control's press, and PANIC (CC 120, All Sound Off). It only counts;
    // processBlock publishes the block's result once. While the host renders offline, the mapped control is consumed
    // but never counts and LEARN does not capture, so offline MIDI playback can neither grant RECEIVE now nor leave an
    // authorisation behind for a later real-time pass. PANIC counts offline too: it only ever takes RECEIVE away.
    bool receiveMidi (const juce::MidiMessage& m, bool offline, uint32_t& toggles, bool& panicSeen) noexcept
    {
        if (m.isController() && m.getControllerNumber() == 120) { panicSeen = true; return true; }
        if (! offline && (m.isController() || m.isNoteOn())
            && mapping.captureLearn (m.isController() ? m.getControllerNumber() : -1, m.isNoteOn() ? m.getNoteNumber() : -1))
        {
            lastReceiveCcValue = 127;               // the move that taught it does not also toggle it
            return true;
        }
        if (m.isController() && m.getControllerNumber() == mapping.cc())
        {
            const int v = m.getControllerValue();
            if (! offline && v >= 64 && lastReceiveCcValue < 64) ++toggles;
            lastReceiveCcValue = v;
            return true;
        }
        if (m.isNoteOn() && m.getNoteNumber() == mapping.note()) { if (! offline) ++toggles; return true; }
        if (m.isNoteOff() && m.getNoteNumber() == mapping.note()) return true;
        return false;
    }

    void revoke()
    {
        const juce::ScopedLock sl (receiverLock);
        requests.discardToggles();      // toggles made before the revocation must not re-grant RECEIVE after it
        control.revokeAll();
        cancelLocked();
    }
    void cancelLocked()          // receiverLock held
    {
        if (connected) instrument.radio.resume ({});
        connected = false;
        tuned = -1;
    }

    /** Message thread, 30 Hz. Decides, under receiverLock, whether the receiver holds a connection; issues the station
        requests. Radio::tune / resume take Radio's own lock and are never called from processBlock. */
    void timerCallback() override
    {
        // a knob moved by MIDI learn (the instrument's own controller map) is mirrored into its host parameter, so the
        // host sees it and saves it - unless the host has moved that parameter itself since processBlock last looked
        for (int i = 0; i < kNumParams; ++i)
        {
            const float mine = instrument.params[(size_t) i].load(), host = hostParams[(size_t) i]->get();
            if (std::abs (mine - host) > 1.0e-5f && juce::exactlyEqual (host, lastHost[(size_t) i].load()))
            {
                lastHost[(size_t) i] = mine;
                hostParams[(size_t) i]->setValueNotifyingHost (mine);
            }
        }

        const juce::ScopedLock sl (receiverLock);
        // MIDI requests are taken under the same lock that restore, explicit on/off and PANIC hold, so a toggle can
        // never be applied after a revocation that came later. A PANIC is applied before any toggle that followed it.
        if (ReceiveRequests::applyTo (control, requests.take()))
        {
            instrument.panic = true;
            cancelLocked();
        }

        ReceiverControl::HostSnapshot h;
        h.prepared = prepared.load();
        h.offline = preparedOffline.load() || offlineNow.load();
        h.lastCallback = lastCallbackMs.load() / 1000.0;
        h.releasedAt = releasedAtMs.load() / 1000.0;
        control.updateHost (juce::Time::getMillisecondCounterHiRes() / 1000.0, h);

        const bool want = control.shouldConnect (0);
        if (! want) { if (connected) cancelLocked(); return; }

        const int wantedNow = dial (instrument.params[kTune].load(), kNumStations).station;
        const auto now = juce::Time::getMillisecondCounter();
        if (! connected)
        {
            // RECEIVE was just granted, or the host has come back: connect at once to the station under TUNE. Not a
            // retune, so it buffers without tuner static.
            tuned = wanted = wantedNow;
            wantedSince = now;
            connected = true;
            instrument.radio.resume (stations[(size_t) tuned].url);
            return;
        }
        if (wantedNow != wanted) { wanted = wantedNow; wantedSince = now; }
        if (wanted != tuned && now - wantedSince > 250)    // TUNE moved to another station and rested: a deliberate retune, with static
        {
            tuned = wanted;
            instrument.radio.tune (stations[(size_t) tuned].url);
        }
    }

    // host threads / audio thread -> timer (atomics only)
    std::atomic<bool> prepared { false }, preparedOffline { false }, offlineNow { false };
    std::atomic<double> lastCallbackMs { -1.0 }, releasedAtMs { -1.0 };
    ReceiveRequests requests;                           // MIDI toggles and PANIC, published once per block (ReceiverControl.h)
    ReceiveMapping mapping;                             // the control mapped to RECEIVE, and LEARN (ReceiverControl.h)
    int lastReceiveCcValue = 0;                         // audio thread only

    // receiver decision state: under receiverLock (never taken by the audio thread)
    juce::CriticalSection receiverLock;
    ReceiverControl control;
    bool connected = false;
    int wanted = -1, tuned = -1;
    juce::uint32 wantedSince = 0;

    std::array<juce::AudioParameterFloat*, kNumParams> hostParams {};
    std::array<std::atomic<float>, kNumParams> lastHost {};
    std::array<Station, kNumStations> stations;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WildbandProcessor)
};

// ---- temporary editor: RECEIVE, its MIDI mapping, PANIC and a status line. Not the instrument's design.
class WildbandEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit WildbandEditor (WildbandProcessor& p) : AudioProcessorEditor (p), owner (p)
    {
        receive.setButtonText ("RECEIVE");
        receive.setClickingTogglesState (true);
        receive.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffF59E0B));
        receive.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        receive.setTooltip ("Connect to the station under TUNE. Off whenever the plugin is opened, restored or panicked.");
        receive.onClick = [this] { owner.setReceive (receive.getToggleState()); };
        learn.setButtonText ("LEARN");
        learn.setTooltip ("Then press a button or move a control on your MIDI controller to make it toggle RECEIVE.");
        learn.onClick = [this] { owner.armReceiveLearn(); };
        panicButton.setButtonText ("PANIC");
        panicButton.onClick = [this] { owner.panic(); };
        for (auto* c : std::initializer_list<juce::Component*> { &receive, &learn, &panicButton, &status, &mapping, &note })
            addAndMakeVisible (c);
        note.setText ("Temporary panel. The ten sliders are in the host's parameter view.", juce::dontSendNotification);
        note.setColour (juce::Label::textColourId, juce::Colours::grey);
        setSize (460, 150);
        startTimerHz (10);
        timerCallback();
    }

private:
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff0C0A09)); }
    void resized() override
    {
        auto area = getLocalBounds().reduced (12);
        auto row = area.removeFromTop (36);
        receive.setBounds (row.removeFromLeft (130));
        row.removeFromLeft (8);
        learn.setBounds (row.removeFromLeft (80));
        row.removeFromLeft (8);
        mapping.setBounds (row.removeFromLeft (120));
        panicButton.setBounds (row.removeFromRight (80));
        area.removeFromTop (10);
        status.setBounds (area.removeFromTop (24));
        note.setBounds (area.removeFromTop (24));
    }
    void timerCallback() override
    {
        receive.setToggleState (owner.receiving(), juce::dontSendNotification);   // MIDI, restore and PANIC change it too
        status.setText (owner.receiverStatus(), juce::dontSendNotification);
        mapping.setText (owner.learningReceive() ? "move a control..." : owner.receiveMapping(), juce::dontSendNotification);
    }

    WildbandProcessor& owner;
    juce::TextButton receive, learn, panicButton;
    juce::Label status, mapping, note;
};

juce::AudioProcessorEditor* WildbandProcessor::createEditor() { return new WildbandEditor (*this); }

} // namespace wildband

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new wildband::WildbandProcessor(); }
