// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Stephane P. Pericat
//
// Headless check of the BUILT plugin bundles (docs/p1/PLUGIN-ARCHITECTURE.md, PA-04 and PA-06). Not a DAW: it loads the
// real AU and VST3 binaries through JUCE's plugin-hosting code and drives them directly. It says nothing about how
// Logic, Live or any other host behaves.
//
//   wildband_plugin_check vst3=/path/WILDBAND.vst3        (the AU is looked up as aumu/Wldb/SwLb in the system)
#include <juce_audio_utils/juce_audio_utils.h>

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
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::String vst3;
    for (int i = 1; i < argc; ++i) if (juce::String (argv[i]).startsWith ("vst3=")) vst3 = juce::String (argv[i]).fromFirstOccurrenceOf ("=", false, false);
    juce::AudioPluginFormatManager fm;
    juce::addDefaultFormatsToManager (fm);
    if (vst3.isNotEmpty()) exercise (fm, "VST3", vst3);
    exercise (fm, "AudioUnit", "AudioUnit:Synths/aumu,Wldb,SwLb");
    std::printf ("%d checks, %d failed\n", checks, failures);
    return failures;
}
