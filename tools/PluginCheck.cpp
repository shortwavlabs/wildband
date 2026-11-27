// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Stephane P. Pericat
//
// Headless check of the BUILT plugin bundles (docs/p1/PLUGIN-ARCHITECTURE.md, PA-04 and PA-06). Not a DAW: it loads the
// real AU and VST3 binaries through JUCE's plugin-hosting code and drives them directly. It says nothing about how
// Logic, Live or any other host behaves.
//
//   wildband_plugin_check vst3=/path/WILDBAND.vst3        (the AU is looked up as aumu/Wldb/SwLb in the system)
//   wildband_plugin_check vst3=... lifecycle=/path/server.log
//        receiver lifecycle checks RL-10..RL-17 (docs/p1/PLUGIN-RECEIVER-LIFECYCLE.md) against tools/stream_test_server.py
//        started with --log, with WILDBAND_TEST_STATIONS pointing at it. Run through tools/run_plugin_lifecycle_check.sh.
#include <juce_audio_utils/juce_audio_utils.h>
#include <AudioToolbox/AudioToolbox.h>

#include <cmath>
#include <cstdio>

namespace
{
int failures = 0, checks = 0;
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
        juce::MidiBuffer midi;
        float early = 0, late = 0;
        bool finite = true;
        const double t0 = juce::Time::getMillisecondCounterHiRes();
        for (int b = 0; b < (int) (6.0 * 48000 / 512); ++b)
        {
            buffer.clear();
            p->processBlock (buffer, midi);
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

/** Renders `seconds` of audio; `paced` = in real time (pumping the message loop meanwhile), else as fast as possible. */
float render (juce::AudioPluginInstance& p, double seconds, bool paced, double fs = 48000.0)
{
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    float peak = 0;
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    const int blocks = (int) (seconds * fs / 512);
    for (int b = 0; b < blocks; ++b)
    {
        buffer.clear();
        p.processBlock (buffer, midi);
        if (b > blocks * 2 / 3) peak = std::max (peak, buffer.getMagnitude (0, 512));
        if (paced) { const double due = t0 + 1000.0 * (b + 1) * 512 / fs; while (juce::Time::getMillisecondCounterHiRes() < due) juce::MessageManager::getInstance()->runDispatchLoopUntil (1); }
        else if (b % 64 == 0) juce::MessageManager::getInstance()->runDispatchLoopUntil (1);
    }
    return peak;
}

void lifecycle (juce::AudioPluginFormatManager& fm, const juce::String& format, const juce::String& id)
{
    std::printf ("%s receiver lifecycle\n", format.toRawUTF8());
    double t = epoch();
    for (int k = 0; k < 3; ++k) { auto p = load (fm, format, id, 48000.0, 512); if (p != nullptr) { (void) p->getParameters().size(); (void) p->getName(); } pump (0.4); }
    pump (1.0);
    check (connections (t).opens == 0, "RL-10 scan-like (construct, query, destroy) x3: " + juce::String (connections (t).opens) + " connections");

    t = epoch();
    { auto p = load (fm, format, id, 48000.0, 512); p->prepareToPlay (48000.0, 512); pump (2.0); p->releaseResources(); pump (0.5); }
    check (connections (t).opens == 0, "RL-11 prepare and release without rendering: " + juce::String (connections (t).opens) + " connections");

    t = epoch();
    { auto p = load (fm, format, id, 48000.0, 512); p->prepareToPlay (48000.0, 512); render (*p, 4.0, false); pump (2.5); p->releaseResources(); }
    check (connections (t).opens == 0, "RL-12 4 s rendered as fast as possible (as validators do), then 2.5 s idle: " + juce::String (connections (t).opens) + " connections");

    t = epoch();
    {
        auto p = load (fm, format, id, 48000.0, 512);
        p->setNonRealtime (true);              // reaches a VST3 as its offline process mode...
        if (format == "AudioUnit")             // ...but JUCE's AU hosting does not forward it, so set the AU's own property as Logic does for a bounce
            if (auto* client = p->getAudioUnitClient()) if (auto au = client->getAudioUnitHandle()) { const UInt32 one = 1; AudioUnitSetProperty (au, kAudioUnitProperty_OfflineRender, kAudioUnitScope_Global, 0, &one, sizeof (one)); }
        p->prepareToPlay (48000.0, 512);
        render (*p, 4.0, true);
        p->releaseResources();
    }
    check (connections (t).opens == 0, "RL-13 offline rendering, paced in real time, 4 s: " + juce::String (connections (t).opens) + " connections");

    juce::MemoryBlock saved;
    { auto p = load (fm, format, id, 48000.0, 512); p->getParameters()[0]->setValue (0.6f); p->getStateInformation (saved); }
    t = epoch();
    { auto p = load (fm, format, id, 48000.0, 512); p->setStateInformation (saved.getData(), (int) saved.getSize()); pump (2.5); }
    check (connections (t).opens == 0, "RL-14 restore a saved state, then 2.5 s: " + juce::String (connections (t).opens) + " connections");

    // RL-15..17 with one instance
    auto p = load (fm, format, id, 48000.0, 512);
    p->prepareToPlay (48000.0, 512);
    t = epoch();
    const float peak = render (*p, 4.0, true);
    auto c = connections (t);
    check (c.opens == 1 && c.firstOpen - t >= 1.5 && c.firstOpen - t <= 2.5 && peak > 0.0f,
           "RL-15 real-time rendering: " + juce::String (c.opens) + " connection(s), opened " + juce::String (c.firstOpen - t, 2) + " s after rendering began; sound in the last 1.3 s (peak " + juce::String (peak, 3) + ")");
    const double stopped = epoch();
    pump (7.0);
    c = connections (t);
    check (c.closes == c.opens && c.lastClose - stopped <= 6.0, "RL-16 rendering stops: connection closed " + juce::String (c.lastClose - stopped, 2) + " s later (" + juce::String (c.opens) + " opened, " + juce::String (c.closes) + " closed)");
    double t2 = epoch();
    render (*p, 3.0, true);
    check (connections (t2).opens == 1, "RL-16 rendering resumes in real time: " + juce::String (connections (t2).opens) + " new connection");
    const double released = epoch();
    p->releaseResources();
    pump (1.5);
    c = connections (t2);
    // limit raised from 1.0 to 1.5 s when the 0.5 s release grace period was added (RL-21): grace + one timer tick +
    // the time the test server takes to notice a closed socket (it only finds out on its next paced write)
    check (c.closes == c.opens && c.lastClose - released <= 1.5, "RL-17 releaseResources while active: closed " + juce::String (c.lastClose - released, 2) + " s later (0.5 s grace period; limit 1.5 s)");
    p->prepareToPlay (48000.0, 512);
    t2 = epoch();
    render (*p, 3.0, true);
    const double destroyed = epoch();
    p.reset();
    pump (1.5);
    c = connections (t2);
    check (c.opens == 1 && c.closes == 1 && c.lastClose - destroyed <= 1.0, "RL-17 destroyed while active: closed " + juce::String (c.lastClose - destroyed, 2) + " s later; open now: " + juce::String (c.opens - c.closes));
    // RL-20 / RL-21: a host re-preparing a live plugin
    {
        auto q = load (fm, format, id, 48000.0, 512);
        q->prepareToPlay (48000.0, 512);
        render (*q, 2.5, true);
        double t3 = epoch();
        q->prepareToPlay (48000.0, 512);                        // same rate, e.g. a buffer-size change
        render (*q, 2.5, true);
        auto c3 = connections (t3);
        check (c3.opens == 0 && c3.closes == 0, "RL-20 prepareToPlay again while live (still rendering): " + juce::String (c3.opens) + " opened, " + juce::String (c3.closes) + " closed");
        t3 = epoch();
        q->releaseResources();
        q->prepareToPlay (48000.0, 512);
        render (*q, 3.0, true);
        c3 = connections (t3);
        check (c3.closes == 0 && c3.opens == 0, "RL-21 release + prepare back to back while live (a host re-initialising): stream kept (" + juce::String (c3.opens) + " opened, " + juce::String (c3.closes) + " closed)");
        q.reset();
        pump (1.0);
    }

    // RL-22: teardown while a connection is still being made (station 3 points at a server that never answers)
    {
        auto q = load (fm, format, id, 48000.0, 512);
        for (auto* prm : q->getParameters()) if (prm->getName (16) == "TUNE") prm->setValue ((2.0f + 0.5f) / 6.0f);
        q->prepareToPlay (48000.0, 512);
        double t3 = epoch();
        render (*q, 2.5, true);
        const bool connecting = connections (t3).opens == 1;
        double t0 = juce::Time::getMillisecondCounterHiRes();
        q->releaseResources();
        const double releaseMs = juce::Time::getMillisecondCounterHiRes() - t0;
        q->prepareToPlay (48000.0, 512);
        render (*q, 2.5, true);
        t0 = juce::Time::getMillisecondCounterHiRes();
        q.reset();
        const double destroyMs = juce::Time::getMillisecondCounterHiRes() - t0;
        check (connecting && releaseMs < 100.0 && destroyMs < 1000.0, "RL-22 mid-connect to a silent server: release returned in " + juce::String (releaseMs, 1) + " ms, destruction in " + juce::String (destroyMs, 1) + " ms");
    }

    const auto all = connections (0);
    // (connections to the silent server are never logged as closed: its handler is asleep, not reading)
    (void) all;
    int answered = 0, answeredClosed = 0;
    for (const auto& line : juce::StringArray::fromLines (serverLog.loadFileAsString()))
    {
        if (line.contains ("/silent")) continue;
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
    for (int i = 1; i < argc; ++i)
    {
        const juce::String a (argv[i]);
        if (a.startsWith ("vst3=")) vst3 = a.fromFirstOccurrenceOf ("=", false, false);
        if (a.startsWith ("lifecycle=")) log = a.fromFirstOccurrenceOf ("=", false, false);
    }
    juce::AudioPluginFormatManager fm;
    juce::addDefaultFormatsToManager (fm);
    if (log.isNotEmpty())
    {
        serverLog = juce::File (log);
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
