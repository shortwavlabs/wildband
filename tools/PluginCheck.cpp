// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Stephane P. Pericat
//
// Headless check of the BUILT plugin bundles (docs/p1/PLUGIN-ARCHITECTURE.md, PA-04 and PA-06). Not a DAW: it loads the
// real AU and VST3 binaries through JUCE's plugin-hosting code and drives them directly. It says nothing about how
// Logic, Live or any other host behaves.
//
//   wildband_plugin_check vst3=/path/WILDBAND.vst3        (the AU is looked up as aumu/Wldb/SwLb in the system)
//   wildband_plugin_check vst3=... lifecycle=/path/server.log fixtures=<dir> [control=1]
//        RECEIVE checks RX-10..RX-25 (docs/p1/PLUGIN-RECEIVE.md) against tools/stream_test_server.py started with
//        --log, with WILDBAND_TEST_STATIONS pointing at it. control=1 runs only the RX-11 scenario and expects
//        connections (the positive control, for a build without RECEIVE). Run through tools/run_plugin_lifecycle_check.sh.
//   wildband_plugin_check vst3=<older build> dumpstate=<dir>
//        writes that build's VST3 state and the installed AU's state (how tests/fixtures/pre-receive-state was made)
//
// The RECEIVE gesture is a MIDI controller: this harness sets WILDBAND_TEST_RECEIVE_CC (unless already set) before
// loading a plugin, which maps that controller to RECEIVE inside the plugin, as LEARN would. The mapping never
// activates anything by itself; a press of the controller (127 then 0) toggles RECEIVE.
#include <juce_audio_utils/juce_audio_utils.h>
#include <AudioToolbox/AudioToolbox.h>

#include <cmath>
#include <cstdio>

namespace
{
int failures = 0, checks = 0;
int receiveCc = 102;
/** The RECEIVE gesture: the mapped controller pressed and let go, at the start of a block. */
juce::MidiBuffer press()
{
    juce::MidiBuffer m;
    m.addEvent (juce::MidiMessage::controllerEvent (1, receiveCc, 127), 0);
    m.addEvent (juce::MidiMessage::controllerEvent (1, receiveCc, 0), 1);
    return m;
}
void check (bool ok, const juce::String& what) { ++checks; failures += ! ok; std::printf ("  %s  %s\n", ok ? "pass" : "FAIL", what.toRawUTF8()); }

std::unique_ptr<juce::AudioPluginInstance> load (juce::AudioPluginFormatManager& fm, const juce::String& format, const juce::String& id, double fs, int block)
{
    juce::OwnedArray<juce::PluginDescription> found;
    for (auto* f : fm.getFormats()) if (f->getName() == format) f->findAllTypesForFile (found, id);
    if (found.isEmpty()) return nullptr;
    juce::String error;
    auto p = fm.createPluginInstance (*found[0], fs, block, error);
    if (p == nullptr) std::printf ("  load %s failed: %s\n", format.toRawUTF8(), error.toRawUTF8());
    return p;
}

void exercise (juce::AudioPluginFormatManager& fm, const juce::String& format, const juce::String& id)
{
    std::printf ("%s\n", format.toRawUTF8());
    auto p = load (fm, format, id, 48000.0, 512);
    check (p != nullptr, "loads");
    if (p == nullptr) return;

    // PA-04: the instrument contract
    check (p->getTotalNumInputChannels() == 0 && p->getTotalNumOutputChannels() == 2, "no audio input, stereo output (" + juce::String (p->getTotalNumInputChannels()) + " in, " + juce::String (p->getTotalNumOutputChannels()) + " out)");
    check (p->acceptsMidi(), "accepts MIDI");
    check (p->getPluginDescription().isInstrument, "described as an instrument");
    juce::StringArray names;
    for (int i = 0; i < std::min (10, p->getParameters().size()); ++i) names.add (p->getParameters()[i]->getName (32));
    // The order a host sees is part of the contract with saved projects. VST3: declaration order. AU: JUCE sorts by
    // hashed parameter ID (then by version hint), and Logic addresses AU parameters by that index - so this order is
    // frozen here, and any parameter added later must carry a version hint above 1 so that it lands after these.
    const auto expected = format == "VST3" ? juce::String ("TUNE,PITCH,SHAPE,TONE,COUPLE,DRIFT,FEEDBACK,MIX,LEVEL,STATIC") : juce::String ("MIX,TONE,TUNE,DRIFT,LEVEL,PITCH,SHAPE,COUPLE,STATIC,FEEDBACK");
    check (names.joinIntoString (",") == expected, "the 10 parameters in the frozen hosted order: " + names.joinIntoString (", "));
    const int extra = p->getParameters().size() - 10;
    if (format == "VST3")   // JUCE adds a bypass parameter and, because VST3 has no MIDI CC events, hidden parameters that carry CCs (so MIDI learn works)
        check (extra == 1 + 16 * 130 || extra >= 1, juce::String (extra) + " further parameters added by JUCE's VST3 wrapper (bypass and MIDI CC emulation)");
    else
        check (extra == 0, juce::String (extra) + " further parameters");

    // PA-06 (a): in real time for 6 s, so the station really connects: static while it does, then the broadcast
    {
        p->setRateAndBufferSizeDetails (48000.0, 512);
        p->prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi = press();          // RECEIVE: without it the plugin never connects
        float early = 0, late = 0;
        bool finite = true;
        const double t0 = juce::Time::getMillisecondCounterHiRes();
        for (int b = 0; b < (int) (6.0 * 48000 / 512); ++b)
        {
            buffer.clear();
            p->processBlock (buffer, midi);
            midi.clear();
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) { const float v = buffer.getSample (c, i); finite = finite && std::isfinite (v); (b * 512 < 48000 * 3 ? early : late) = std::max (b * 512 < 48000 * 3 ? early : late, std::abs (v)); }
            const double due = t0 + 1000.0 * (b + 1) * 512 / 48000.0;
            while (juce::Time::getMillisecondCounterHiRes() < due) juce::MessageManager::getInstance()->runDispatchLoopUntil (1);
        }
        check (finite && early > 0.0f && late > 0.0f && std::max (early, late) <= 0.891f, "6 s in real time at 48 kHz: sound in the first half (static and/or broadcast, peak " + juce::String (early, 3) + ") and in the second (peak " + juce::String (late, 3) + "), under the ceiling");
    }

    // PA-06 (b): faster than real time at three rates, with automation and MIDI (no new audio arrives this fast)
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        p->releaseResources();
        p->setRateAndBufferSizeDetails (fs, 256);
        p->prepareToPlay (fs, 256);
        juce::AudioBuffer<float> buffer (2, 256);
        juce::MidiBuffer midi;
        float peak = 0;
        bool finite = true;
        const int blocks = (int) (fs * 3 / 256);
        for (int b = 0; b < blocks; ++b)
        {
            midi.clear();
            if (b % 40 == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 48 + (b / 40) % 24, (juce::uint8) 100), 10);
            if (b % 25 == 0) p->getParameters()[(b / 25) % 10]->setValue ((float) ((b * 37) % 100) / 100.0f);   // automation, including LEVEL and FEEDBACK
            buffer.clear();
            p->processBlock (buffer, midi);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 256; ++i) { const float v = buffer.getSample (c, i); finite = finite && std::isfinite (v); peak = std::max (peak, std::abs (v)); }
            if (b % 64 == 0) juce::MessageManager::getInstance()->runDispatchLoopUntil (1);      // the station follows TUNE on the message thread
        }
        check (finite && peak <= 0.891f, "rendered 3 s faster than real time at " + juce::String (fs / 1000.0, 1) + " kHz with automation and MIDI: finite, peak " + juce::String (peak, 3) + " (ceiling 0.89)");
    }

    // PA-06: state round-trips and holds no audio
    for (auto* q : p->getParameters()) q->setValue (0.25f);
    p->getParameters()[3]->setValue (0.8f);
    juce::MemoryBlock state;
    p->getStateInformation (state);
    auto other = load (fm, format, id, 48000.0, 512);
    if (other != nullptr)
    {
        other->setStateInformation (state.getData(), (int) state.getSize());
        bool same = true;
        for (int i = 0; i < 10; ++i) same = same && std::abs (other->getParameters()[i]->getValue() - p->getParameters()[i]->getValue()) < 1.0e-4f;
        check (same, "state restores every parameter in a second instance");
    }
    // (the hosting layer wraps the plugin's own XML in each format's container - a VST3 state block, an AU class-info
    // plist - so only its size and its round trip are checked here)
    check (state.getSize() < 2048, "state as the host stores it is " + juce::String ((int) state.getSize()) + " bytes (settings only; no audio could fit)");
    p->releaseResources();
}
// ---- receiver lifecycle, counted at the server ----------------------------------------------------------------------
juce::File serverLog;
struct Conn { int opens = 0, closes = 0; double firstOpen = -1, lastClose = -1; };
Conn connections (double since)      // connections the server logged since `since` (epoch seconds)
{
    Conn c;
    for (const auto& line : juce::StringArray::fromLines (serverLog.loadFileAsString()))
    {
        const auto w = juce::StringArray::fromTokens (line, " ", "");
        if (w.size() < 3) continue;
        const double t = w[1].getDoubleValue();
        if (t < since) continue;
        if (w[0] == "open") { ++c.opens; if (c.firstOpen < 0) c.firstOpen = t; }
        if (w[0] == "close") { ++c.closes; c.lastClose = t; }
    }
    return c;
}
double epoch() { return (double) juce::Time::currentTimeMillis() / 1000.0; }
void pump (double seconds) { const double until = juce::Time::getMillisecondCounterHiRes() + seconds * 1000.0; while (juce::Time::getMillisecondCounterHiRes() < until) juce::MessageManager::getInstance()->runDispatchLoopUntil (5); }

/** Renders `seconds` of audio from every instance in turn; `paced` = in real time (pumping the message loop meanwhile),
    else as fast as possible. `first[i]` is MIDI for instance i's first block. Returns the peak of the last third. */
float render (std::vector<juce::AudioPluginInstance*> ps, double seconds, bool paced, std::vector<juce::MidiBuffer> first = {}, double fs = 48000.0)
{
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer none;
    float peak = 0;
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    const int blocks = (int) (seconds * fs / 512);
    for (int b = 0; b < blocks; ++b)
    {
        for (size_t i = 0; i < ps.size(); ++i)
        {
            buffer.clear();
            auto& midi = b == 0 && i < first.size() ? first[i] : none;
            ps[i]->processBlock (buffer, midi);
            none.clear();
            if (b > blocks * 2 / 3) peak = std::max (peak, buffer.getMagnitude (0, 512));
        }
        if (paced) { const double due = t0 + 1000.0 * (b + 1) * 512 / fs; while (juce::Time::getMillisecondCounterHiRes() < due) juce::MessageManager::getInstance()->runDispatchLoopUntil (1); }
        else if (b % 64 == 0) juce::MessageManager::getInstance()->runDispatchLoopUntil (1);
    }
    return peak;
}
float render (juce::AudioPluginInstance& p, double seconds, bool paced, juce::MidiBuffer first = {}) { return render ({ &p }, seconds, paced, { first }); }

void setOffline (juce::AudioPluginInstance& p, const juce::String& format, bool offline = true)
{
    p.setNonRealtime (offline);           // reaches a VST3 as its offline process mode...
    if (format == "AudioUnit")            // ...but JUCE's AU hosting does not forward it, so set the AU's own property as Logic does for a bounce
        if (auto* client = p.getAudioUnitClient()) if (auto au = client->getAudioUnitHandle()) { const UInt32 v = offline ? 1 : 0; AudioUnitSetProperty (au, kAudioUnitProperty_OfflineRender, kAudioUnitScope_Global, 0, &v, sizeof (v)); }
}
/** One 512-sample block with this MIDI, and no message-loop pumping: the plugin's timer cannot run in between. */
void oneBlock (juce::AudioPluginInstance& p, juce::MidiBuffer midi)
{
    juce::AudioBuffer<float> buffer (2, 512);
    buffer.clear();
    p.processBlock (buffer, midi);
}
juce::MidiBuffer panicMidi() { juce::MidiBuffer m; m.addEvent (juce::MidiMessage::allSoundOff (1), 0); return m; }
juce::File fixtures;        // states saved by the PR #17 build (no RECEIVE mapping fields): tests/fixtures/pre-receive-state
juce::MemoryBlock fixture (const juce::String& format)
{
    juce::MemoryBlock mb;
    fixtures.getChildFile (format == "VST3" ? "vst3.state" : "au.state").loadFileAsData (mb);
    return mb;
}
void tuneTo (juce::AudioPluginInstance& p, int slot) { for (auto* prm : p.getParameters()) if (prm->getName (16) == "TUNE") prm->setValue (((float) slot + 0.5f) / 6.0f); }
juce::String str (const Conn& c) { return juce::String (c.opens) + " opened, " + juce::String (c.closes) + " closed"; }
double msSince (double t0) { return juce::Time::getMillisecondCounterHiRes() - t0; }

/** RX-11: a monitored project - prepared, rendering in real time for 10 s - and nobody touches RECEIVE. */
Conn monitoredWithoutReceive (juce::AudioPluginFormatManager& fm, const juce::String& format, const juce::String& id)
{
    const double t = epoch();
    auto p = load (fm, format, id, 48000.0, 512);
    p->prepareToPlay (48000.0, 512);
    juce::MidiBuffer notes;                    // the performer plays; that is not RECEIVE
    notes.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    render (*p, 10.0, true, notes);
    p->releaseResources();
    p.reset();
    pump (1.0);
    return connections (t);
}

void lifecycle (juce::AudioPluginFormatManager& fm, const juce::String& format, const juce::String& id)
{
    std::printf ("%s RECEIVE\n", format.toRawUTF8());
    // RX-10: everything a host does without the user asking for the radio
    double t = epoch();
    for (int k = 0; k < 3; ++k) { auto p = load (fm, format, id, 48000.0, 512); if (p != nullptr) { (void) p->getParameters().size(); (void) p->getName(); } pump (0.4); }
    { auto p = load (fm, format, id, 48000.0, 512); p->prepareToPlay (48000.0, 512); pump (1.5); p->releaseResources(); pump (0.5); }
    { auto p = load (fm, format, id, 48000.0, 512); p->prepareToPlay (48000.0, 512); render (*p, 4.0, false); pump (2.0); p->releaseResources(); }
    { auto p = load (fm, format, id, 48000.0, 512); setOffline (*p, format); p->prepareToPlay (48000.0, 512); render (*p, 3.0, true); p->releaseResources(); }
    juce::MemoryBlock saved;
    { auto p = load (fm, format, id, 48000.0, 512); p->getParameters()[0]->setValue (0.6f); p->getStateInformation (saved); }
    { auto p = load (fm, format, id, 48000.0, 512); p->setStateInformation (saved.getData(), (int) saved.getSize()); p->prepareToPlay (48000.0, 512); render (*p, 2.0, true); }
    pump (1.0);
    check (connections (t).opens == 0, "RX-10 scan-like x3, prepare/release, fast rendering, offline rendering, restore + real time: " + str (connections (t)));

    const auto monitored = monitoredWithoutReceive (fm, format, id);
    check (monitored.opens == 0, "RX-11 monitored project, 10 s in real time with notes played, no RECEIVE: " + str (monitored));

    // RX-12..RX-15 with one instance
    auto p = load (fm, format, id, 48000.0, 512);
    p->prepareToPlay (48000.0, 512);
    render (*p, 1.0, true);
    t = epoch();
    const float peak = render (*p, 3.0, true, press());
    auto c = connections (t);
    check (c.opens == 1 && c.firstOpen - t <= 0.6 && peak > 0.0f, "RX-12 RECEIVE (mapped CC) while rendering: " + str (c) + ", first " + juce::String (c.firstOpen - t, 2) + " s after the gesture (limit 0.6); sound (peak " + juce::String (peak, 3) + ")");

    const double stopped = epoch();
    pump (6.5);
    c = connections (t);
    check (c.closes == c.opens && c.lastClose > 0 && c.lastClose - stopped <= 6.5, "RX-13 callbacks stop: closed " + juce::String (c.lastClose - stopped, 2) + " s later (" + str (c) + ")");
    t = epoch();
    render (*p, 2.0, true);
    check (connections (t).opens == 1, "RX-13 callbacks resume: reconnected without a new gesture (" + str (connections (t)) + ")");

    t = epoch();
    render (*p, 3.0, true, press());
    pump (1.0);
    c = connections (t);
    check (c.opens == 0 && c.closes == 1, "RX-14 RECEIVE off: closed, nothing new over 3 s of rendering (" + str (c) + ")");

    t = epoch();
    for (int k = 0; k < 5; ++k) { render (*p, 1.5, true, press()); render (*p, 1.0, true, press()); }
    pump (1.0);
    c = connections (t);
    check (c.opens == 5 && c.closes == 5, "RX-15 five on/off cycles: " + str (c));

    // RX-16: restore into an instance that is receiving
    render (*p, 1.5, true, press());
    t = epoch();
    double t0 = juce::Time::getMillisecondCounterHiRes();
    p->setStateInformation (saved.getData(), (int) saved.getSize());
    const double restoreMs = msSince (t0);
    render (*p, 3.0, true);
    pump (0.5);
    c = connections (t);
    check (c.opens == 0 && c.closes == 1 && restoreMs < 100.0, "RX-16 restore while receiving: closed, RECEIVE off, no reconnection over 3 s (" + str (c) + "; restore took " + juce::String (restoreMs, 1) + " ms)");
    render (*p, 1.0, true, press());           // RECEIVE was off: this press turns it on again
    t = epoch();
    render (*p, 1.0, true);
    check (connections (t - 1.2).opens == 1, "RX-16 after restore, a new gesture connects again (" + str (connections (t - 1.2)) + ")");

    // RX-20: PANIC (CC 120)
    {
        juce::MidiBuffer panic;
        panic.addEvent (juce::MidiMessage::allSoundOff (1), 0);
        t = epoch();
        render (*p, 3.0, true, panic);
        pump (0.5);
        c = connections (t);
        check (c.opens == 0 && c.closes == 1, "RX-20 PANIC (CC 120) while receiving: closed, RECEIVE off, nothing over 3 s (" + str (c) + ")");
    }
    p.reset();
    pump (0.5);

    // RX-17: in flight, toward a server that never answers (station slot 3 = /silent)
    for (int k = 0; k < 3; ++k)
    {
        auto q = load (fm, format, id, 48000.0, 512);
        tuneTo (*q, 2);
        q->prepareToPlay (48000.0, 512);
        render (*q, 0.5, true);
        t = epoch();
        render (*q, 1.0, true, press());
        const bool connecting = connections (t).opens == 1;
        juce::String how;
        double ms = 0;
        t0 = juce::Time::getMillisecondCounterHiRes();
        if (k == 0) { how = "RECEIVE off"; render (*q, 0.1, true, press()); }   // the gesture is a request the timer acts on: no call to time, only the outcome
        if (k == 1) { how = "restore"; q->setStateInformation (saved.getData(), (int) saved.getSize()); ms = msSince (t0); }
        if (k == 2) { how = "destroy"; q.reset(); ms = msSince (t0); }
        if (q != nullptr) render (*q, 2.5, true);
        else pump (2.5);
        check (connecting && ms < (k == 2 ? 1000.0 : 150.0) && connections (t).opens == 1,
               "RX-17 mid-connect to a silent server, then " + how + (k == 0 ? juce::String (":") : ": returned in " + juce::String (ms, 1) + " ms,") + " no later connection (" + juce::String (connections (t).opens) + " opened)");
    }

    // RX-18: the gesture during an offline render
    {
        auto q = load (fm, format, id, 48000.0, 512);
        setOffline (*q, format);
        q->prepareToPlay (48000.0, 512);
        t = epoch();
        render (*q, 4.0, true, press());
        q->releaseResources();
        pump (0.5);
        check (connections (t).opens == 0, "RX-18 RECEIVE during an offline render (paced, 4 s): " + str (connections (t)));
    }

    // RX-19: two instances
    {
        auto a = load (fm, format, id, 48000.0, 512), b = load (fm, format, id, 48000.0, 512);
        a->prepareToPlay (48000.0, 512);
        b->prepareToPlay (48000.0, 512);
        t = epoch();
        render ({ a.get(), b.get() }, 2.0, true, { press(), {} });
        check (connections (t).opens == 1, "RX-19 two instances, RECEIVE on one: " + str (connections (t)));
        t = epoch();
        render ({ a.get(), b.get() }, 2.0, true, { {}, press() });
        check (connections (t).opens == 1, "RX-19 RECEIVE on the second as well: one more (" + str (connections (t)) + ")");
        t = epoch();
        a.reset();
        render (*b, 2.0, true);
        c = connections (t);
        check (c.closes == 1 && c.opens == 0, "RX-19 destroying the first closes only its own (" + str (c) + ")");
        b.reset();
        pump (1.0);
    }

    // RX-22: PANIC and a RECEIVE toggle in the same block, from RECEIVE off: PANIC wins whatever the order in the block
    for (int order = 0; order < 2; ++order)
    {
        auto q = load (fm, format, id, 48000.0, 512);
        q->prepareToPlay (48000.0, 512);
        render (*q, 0.5, true);
        juce::MidiBuffer mb;
        const int panicAt = order == 0 ? 2 : 0, pressAt = order == 0 ? 0 : 1;
        mb.addEvent (juce::MidiMessage::controllerEvent (1, receiveCc, 127), pressAt);
        mb.addEvent (juce::MidiMessage::controllerEvent (1, receiveCc, 0), pressAt + 1);
        mb.addEvent (juce::MidiMessage::allSoundOff (1), panicAt);
        t = epoch();
        render (*q, 3.0, true, mb);
        check (connections (t).opens == 0, juce::String ("RX-22 ") + (order == 0 ? "toggle then PANIC" : "PANIC then toggle")
                                               + " in one block, from RECEIVE off: RECEIVE stays off (" + str (connections (t)) + ")");
        q.reset();
        pump (0.3);
    }

    // RX-23: a toggle still pending (the timer has not run) when PANIC or a restore arrives: it must not re-grant RECEIVE
    for (int k = 0; k < 2; ++k)
    {
        auto q = load (fm, format, id, 48000.0, 512);
        q->prepareToPlay (48000.0, 512);
        render (*q, 0.5, true);
        t = epoch();
        oneBlock (*q, press());                             // queued; no message-loop pass, so not yet applied
        if (k == 0) oneBlock (*q, panicMidi());             // PANIC in the next block
        else q->setStateInformation (saved.getData(), (int) saved.getSize());
        render (*q, 3.0, true);
        check (connections (t).opens == 0, juce::String ("RX-23 toggle pending, then ") + (k == 0 ? "PANIC in the next block" : "state restore")
                                               + ": no connection over 3 s (" + str (connections (t)) + ")");
        q.reset();
        pump (0.3);
    }

    // RX-24: the RECEIVE control played during an offline render must not leave an authorisation for real time
    {
        auto q = load (fm, format, id, 48000.0, 512);
        setOffline (*q, format, true);
        q->prepareToPlay (48000.0, 512);
        t = epoch();
        render (*q, 2.0, true, press());
        q->releaseResources();
        setOffline (*q, format, false);
        q->prepareToPlay (48000.0, 512);
        render (*q, 3.0, true);
        check (connections (t).opens == 0, "RX-24 RECEIVE pressed during an offline render, then 3 s in real time: " + str (connections (t)));
        t = epoch();
        render (*q, 2.0, true, press());
        check (connections (t).opens == 1, "RX-24 a real-time press afterwards still connects (" + str (connections (t)) + ")");
        q.reset();
        pump (0.5);
    }

    // RX-25: restoring a state saved before RECEIVE existed (PR #17 build: no mapping fields) clears the mapping
    {
        const auto old = fixture (format);
        if (old.isEmpty()) check (false, "RX-25 fixture missing: " + fixtures.getFullPathName());
        else
        {
            auto q = load (fm, format, id, 48000.0, 512);     // constructed with the test mapping (CC 102)
            q->setStateInformation (old.getData(), (int) old.getSize());
            q->prepareToPlay (48000.0, 512);
            render (*q, 0.5, true);
            t = epoch();
            render (*q, 3.0, true, press());
            check (connections (t).opens == 0, "RX-25 after restoring a pre-RECEIVE state, the formerly mapped CC does nothing (" + str (connections (t)) + ")");
            q->setStateInformation (saved.getData(), (int) saved.getSize());   // a current state carries the mapping again
            t = epoch();
            render (*q, 2.0, true, press());
            check (connections (t).opens == 1, "RX-25 restoring a current state brings the mapping back: one connection (" + str (connections (t)) + ")");
            q.reset();
            pump (0.5);
        }
    }

    int answered = 0, answeredClosed = 0;
    for (const auto& line : juce::StringArray::fromLines (serverLog.loadFileAsString()))
    {
        if (line.contains ("/silent")) continue;        // (the silent server's handler is asleep, so it never logs a close)
        if (line.startsWith ("open ")) ++answered;
        if (line.startsWith ("close ")) ++answeredClosed;
    }
    check (answered == answeredClosed, "no stream connection left open overall (" + juce::String (answered) + " opened, " + juce::String (answeredClosed) + " closed)");
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::String vst3, log;
    bool control = false, dumpOnly = false;
    if (const char* cc = std::getenv ("WILDBAND_TEST_RECEIVE_CC")) receiveCc = juce::String (cc).getIntValue();
    else setenv ("WILDBAND_TEST_RECEIVE_CC", juce::String (receiveCc).toRawUTF8(), 1);   // read by the plugin when constructed
    for (int i = 1; i < argc; ++i)
    {
        const juce::String a (argv[i]);
        if (a.startsWith ("vst3=")) vst3 = a.fromFirstOccurrenceOf ("=", false, false);
        if (a.startsWith ("lifecycle=")) log = a.fromFirstOccurrenceOf ("=", false, false);
        if (a == "control=1") control = true;
        if (a.startsWith ("fixtures=")) fixtures = juce::File (a.fromFirstOccurrenceOf ("=", false, false));
        if (a.startsWith ("dumpstate=")) fixtures = juce::File (a.fromFirstOccurrenceOf ("=", false, false)), dumpOnly = true;
    }
    juce::AudioPluginFormatManager fm;
    juce::addDefaultFormatsToManager (fm);
    if (dumpOnly)   // writes the states of whatever build is given (vst3=) and installed (AU): used once, with the PR #17 build
    {
        for (const auto& [format, fid] : { std::pair<juce::String, juce::String> { "VST3", vst3 }, { "AudioUnit", "AudioUnit:Synths/aumu,Wldb,SwLb" } })
        {
            auto p = load (fm, format, fid, 48000.0, 512);
            if (p == nullptr) { std::printf ("could not load %s\n", format.toRawUTF8()); return 1; }
            juce::MemoryBlock mb;
            p->getStateInformation (mb);
            fixtures.createDirectory();
            fixtures.getChildFile (format == "VST3" ? "vst3.state" : "au.state").replaceWithData (mb.getData(), mb.getSize());
            std::printf ("%s state: %d bytes\n", format.toRawUTF8(), (int) mb.getSize());
        }
        return 0;
    }
    if (log.isNotEmpty())
    {
        serverLog = juce::File (log);
        if (control)        // positive control: a build that connects without RECEIVE must show it here
        {
            for (const auto& [format, fid] : { std::pair<juce::String, juce::String> { "VST3", vst3 }, { "AudioUnit", "AudioUnit:Synths/aumu,Wldb,SwLb" } })
            {
                if (fid.isEmpty()) continue;
                const auto c = monitoredWithoutReceive (fm, format, fid);
                check (c.opens >= 1, format + " positive control, RX-11 scenario: " + str (c) + " (expected >= 1 for a build without RECEIVE)");
            }
            std::printf ("%d checks, %d failed\n", checks, failures);
            return failures;
        }
        if (vst3.isNotEmpty()) lifecycle (fm, "VST3", vst3);
        lifecycle (fm, "AudioUnit", "AudioUnit:Synths/aumu,Wldb,SwLb");
        std::printf ("%d checks, %d failed\n", checks, failures);
        return failures;
    }
    if (vst3.isNotEmpty()) exercise (fm, "VST3", vst3);
    exercise (fm, "AudioUnit", "AudioUnit:Synths/aumu,Wldb,SwLb");
    std::printf ("%d checks, %d failed\n", checks, failures);
    return failures;
}
