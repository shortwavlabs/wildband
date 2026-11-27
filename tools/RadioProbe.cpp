// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Stephane P. Pericat
// G1 evidence tool: tunes the real Radio class to each URL given, plays it into nowhere at real-time pace (this thread
// stands in for the audio callback) and reports what happened. Usage: wildband_radio_probe [--seconds N] [--rate HZ] url...
// With two or more URLs it also measures switching between them.
#include "../src/Radio.h"

#include <cmath>
#include <cstdio>

using wildband::Radio;

int main (int argc, char** argv)
{
    double seconds = 10.0, fs = 48000.0;
    juce::StringArray urls;
    for (int i = 1; i < argc; ++i)
    {
        const juce::String a (argv[i]);
        if (a == "--seconds" && i + 1 < argc) seconds = juce::String (argv[++i]).getDoubleValue();
        else if (a == "--rate" && i + 1 < argc) fs = juce::String (argv[++i]).getDoubleValue();
        else urls.add (a);
    }

    Radio radio;
    radio.setOutputRate (fs);
    const int block = 480;
    std::vector<float> l ((size_t) block), r ((size_t) block);
    std::printf ("| URL | result | detail | first audio | played | RMS dBFS | peak | underruns | reconnects |\n|---|---|---|---|---|---|---|---|---|\n");

    for (const auto& url : urls)
    {
        const int under0 = radio.underruns.load(), re0 = radio.reconnects.load();
        radio.tune (url);
        const double t0 = juce::Time::getMillisecondCounterHiRes();
        double firstAudio = -1, sum = 0, peak = 0;
        juce::int64 played = 0, blocks = 0;
        auto last = Radio::State::Idle;
        for (;;)
        {
            const double now = juce::Time::getMillisecondCounterHiRes() - t0;
            if (now > seconds * 1000.0 || (radio.state() == Radio::State::Failed && now > 300)) break;
            radio.read (l.data(), r.data(), block);
            for (int i = 0; i < block; ++i)
            {
                const double v = l[(size_t) i];
                if (radio.state() == Radio::State::Playing) { sum += v * v; ++played; peak = std::max (peak, std::abs (v)); }
            }
            if (firstAudio < 0 && radio.state() == Radio::State::Playing) firstAudio = now;
            if (radio.state() != last) { last = radio.state(); std::fprintf (stderr, "  %7.0f ms  %s  %s\n", now, Radio::name (last), radio.detail().toRawUTF8()); }
            ++blocks;
            const double due = t0 + 1000.0 * (double) (blocks * block) / fs;       // pace like a device callback
            if (const double ahead = due - juce::Time::getMillisecondCounterHiRes(); ahead > 0) juce::Thread::sleep ((int) ahead);
        }
        std::printf ("| %s | %s | %s | %s | %.1f s | %.1f | %.2f | %d | %d |\n", url.toRawUTF8(), Radio::name (radio.state()), radio.detail().toRawUTF8(),
                     firstAudio < 0 ? "never" : (juce::String (firstAudio / 1000.0, 2) + " s").toRawUTF8(), (double) played / fs,
                     played > 0 ? 10.0 * std::log10 (sum / (double) played + 1e-12) : -120.0, peak, radio.underruns.load() - under0, radio.reconnects.load() - re0);
        std::fflush (stdout);
    }
    return 0;
}
