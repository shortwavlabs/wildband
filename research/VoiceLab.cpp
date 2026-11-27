// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Stephane P. Pericat
// WILDBAND voice lab (docs/p0/VOICE-RESEARCH.md): renders every candidate voice over the same synthesized fixtures,
// with a WORLD cable connected and pulled, level-matched, into WAV files for quick A/B listening, and prints a table
// of cost, memory, level and how much WORLD changed the result.
//
//   wildband_voice_lab [output-directory]          (default: voice-lab)
//
// Fixtures are generated here; there are no recordings. The WORLD track is a SCRIPTED STAND-IN for a room, not a
// measurement. Candidates from other repositories are compiled from their own checkout when CMake is given
// -DTAPESTRY_DIR (Wyrd, Drift) and -DRACK_SDK_DIR (Korupt); none of their code is in this repository.
// The output is research material for listening on this machine: it is written to the build tree and not committed.
#include "../src/Engine.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <sys/stat.h>

#if WILDBAND_LAB_TAPESTRY
 #include "dsp/drif-dsp.h"
 #include "dsp/wyrd-dsp.h"
#endif
#if WILDBAND_LAB_RACK
 #include "dsp/korupt-dsp.h"
#endif

using namespace wildband;

namespace
{
constexpr double fs = 48000.0;
constexpr int seconds = 12;
constexpr size_t N = (size_t) (fs * seconds);
constexpr double kTarget = 0.0708, kSilent = 0.000316;      // -23 dBFS RMS; below -70 dBFS a render counts as silent
using Buf = std::vector<float>;
struct Stereo { Buf l, r; };

uint32_t seed = 12345u;
float rnd() { seed = seed * 1664525u + 1013904223u; return (float) (int32_t) seed * (1.0f / 2147483648.0f); }
float rnd01() { return 0.5f * (rnd() + 1.0f); }

struct Band   // two-pole band-pass (TPT state-variable)
{
    float s1 = 0, s2 = 0;
    float operator() (float x, float hz, float q) { const float g = std::tan (kPi * hz / (float) fs), k = 1.0f / q, a1 = 1.0f / (1.0f + g * (g + k)); const float v1 = a1 * (s1 + g * (x - s2)), v2 = s2 + g * v1; s1 = 2 * v1 - s1; s2 = 2 * v2 - s2; return v1 * k; }
};

// ---- fixtures -------------------------------------------------------------------------------------------------------
Buf speechLike()   // a formant voice saying nothing: syllables, vowels that move, pauses, a few noisy consonants
{
    static const float vowels[5][3] = { { 730, 1090, 2440 }, { 270, 2290, 3010 }, { 300, 870, 2240 }, { 530, 1840, 2480 }, { 570, 840, 2410 } };
    Buf x (N);
    Band f[3], hiss;
    float phase = 0, env = 0, target = 0, formant[3] = { 500, 1500, 2500 };
    int until = 0, vowel = 0, syllablesLeft = 0;
    bool consonant = false;
    seed = 7;
    for (size_t i = 0; i < N; ++i)
    {
        if ((int) i >= until)
        {
            if (syllablesLeft > 0) { --syllablesLeft; vowel = (int) (rnd01() * 4.99f); target = 0.6f + 0.4f * rnd01(); consonant = rnd01() < 0.35f; until = (int) i + (int) (fs * (0.12 + 0.14 * rnd01())); }
            else { syllablesLeft = 3 + (int) (rnd01() * 4); target = 0.0f; until = (int) i + (int) (fs * (0.3 + 0.6 * rnd01())); }
        }
        env += (target > env ? 0.002f : 0.0006f) * (target - env);
        const float f0 = 118.0f + 14.0f * std::sin (2 * kPi * 0.31f * (float) i / (float) fs) + 3.0f * std::sin (2 * kPi * 5.2f * (float) i / (float) fs);
        phase += f0 / (float) fs; if (phase >= 1) phase -= 1;
        const float glottal = 2.0f * phase - 1.0f - 0.6f * (phase < 0.1f ? 1.0f : 0.0f);
        float y = 0;
        for (int k = 0; k < 3; ++k) { formant[k] += 0.0015f * (vowels[vowel][k] - formant[k]); y += f[k] (glottal, formant[k], 9.0f) * (k == 0 ? 1.0f : k == 1 ? 0.6f : 0.3f); }
        const float burst = consonant && until - (int) i > (int) (fs * 0.09) ? hiss (rnd(), 4200.0f, 1.2f) * 0.5f : 0.0f;
        x[i] = 0.5f * env * y + env * burst;
    }
    return x;
}

Buf percussion()   // kick, snare and hat at 110 bpm
{
    Buf x (N, 0.0f);
    const double beat = 60.0 / 110.0;
    seed = 11;
    Band hat;
    for (size_t i = 0; i < N; ++i)
    {
        const double t = (double) i / fs, inBar = std::fmod (t, beat * 4), step = std::fmod (t, beat / 2);
        float y = 0;
        for (double k : { 0.0, beat * 2.5 }) { const double d = inBar - k; if (d >= 0 && d < 0.25) y += 0.9f * (float) (std::exp (-d * 18) * std::sin (2 * kPi * (45 * d + 75 / 30.0 * (1 - std::exp (-d * 30))))); }
        for (double k : { beat, beat * 3 }) { const double d = inBar - k; if (d >= 0 && d < 0.18) y += (float) std::exp (-d * 28) * (0.5f * rnd() + 0.35f * (float) std::sin (2 * kPi * 190 * d)); }
        if (step < 0.04) y += 0.25f * (float) std::exp (-step * 110) * hat (rnd(), 8000.0f, 0.8f);
        x[i] = 0.7f * y;
    }
    return x;
}

Buf music()   // Am - F - C - G: a soft pad and a plucked arpeggio
{
    static const float chords[4][3] = { { 220.0f, 261.63f, 329.63f }, { 174.61f, 220.0f, 261.63f }, { 261.63f, 329.63f, 392.0f }, { 196.0f, 246.94f, 293.66f } };
    Buf x (N);
    float ph[3] = {}, lp = 0, pluckPhase = 0;
    for (size_t i = 0; i < N; ++i)
    {
        const double t = (double) i / fs;
        const int chord = (int) (t / 1.5) % 4, note = (int) (t / 0.1875) % 3;
        float pad = 0;
        for (int k = 0; k < 3; ++k) { ph[k] += chords[chord][k] / (float) fs; if (ph[k] >= 1) ph[k] -= 1; pad += 2 * ph[k] - 1; }
        lp += 0.06f * (pad / 3 - lp);
        const double d = std::fmod (t, 0.1875);
        pluckPhase += 2.0f * chords[chord][note] / (float) fs; if (pluckPhase >= 1) pluckPhase -= 1;
        const float pluck = (float) std::exp (-d * 14) * (4.0f * std::abs (pluckPhase - 0.5f) - 1.0f);
        x[i] = 0.45f * lp + 0.35f * pluck;
    }
    return x;
}

Buf staticNoise() { StaticSource s; s.prepare (fs); Buf x (N); for (auto& v : x) v = 0.5f * s.next(); return x; }

/** SCRIPTED STAND-IN for a room: irregular bursts of activity over a slow swell. Not a measurement of anything. */
std::vector<EnvFrame> scriptedWorld()
{
    std::vector<EnvFrame> w (N / 64 + 1);
    seed = 23;
    float activity = 0;
    int next = 40;
    for (size_t k = 0; k < w.size(); ++k)
    {
        const double t = (double) k * 64 / fs;
        if ((int) k >= next) { activity = 0.6f + 0.4f * rnd01(); next = (int) k + (int) ((0.25 + 1.4 * rnd01()) * fs / 64); }
        activity *= 0.94f;
        w[k] = { (float) (0.15 + 0.65 * (0.5 - 0.5 * std::cos (2 * kPi * t / 12.0)) + 0.1 * std::sin (2 * kPi * 0.7 * t)), activity, 0.3f };
    }
    return w;
}

// ---- measuring and writing ------------------------------------------------------------------------------------------
double rms (const Stereo& s) { double a = 0; for (size_t i = 0; i < s.l.size(); ++i) a += (double) s.l[i] * s.l[i] + (double) s.r[i] * s.r[i]; return std::sqrt (a / (2.0 * (double) s.l.size())); }
float peak (const Stereo& s) { float p = 0; for (size_t i = 0; i < s.l.size(); ++i) p = std::max (p, std::max (std::abs (s.l[i]), std::abs (s.r[i]))); return p; }
bool finite (const Stereo& s) { for (size_t i = 0; i < s.l.size(); ++i) if (! std::isfinite (s.l[i]) || ! std::isfinite (s.r[i])) return false; return true; }

void writeWav (const std::string& path, const Stereo& s)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (f == nullptr) return;
    const uint32_t bytes = (uint32_t) (s.l.size() * 4), rate = (uint32_t) fs, byteRate = rate * 4, riff = 36 + bytes, fmtSize = 16;
    const uint16_t pcm = 1, channels = 2, align = 4, bits = 16;
    std::fwrite ("RIFF", 1, 4, f); std::fwrite (&riff, 4, 1, f); std::fwrite ("WAVEfmt ", 1, 8, f); std::fwrite (&fmtSize, 4, 1, f);
    std::fwrite (&pcm, 2, 1, f); std::fwrite (&channels, 2, 1, f); std::fwrite (&rate, 4, 1, f); std::fwrite (&byteRate, 4, 1, f); std::fwrite (&align, 2, 1, f); std::fwrite (&bits, 2, 1, f);
    std::fwrite ("data", 1, 4, f); std::fwrite (&bytes, 4, 1, f);
    for (size_t i = 0; i < s.l.size(); ++i) for (float v : { s.l[i], s.r[i] }) { const int16_t q = (int16_t) std::lrint (32767.0f * std::min (1.0f, std::max (-1.0f, v))); std::fwrite (&q, 2, 1, f); }
    std::fclose (f);
}

// ---- voices ---------------------------------------------------------------------------------------------------------
struct Voice { std::string name, origin, note; size_t memory; std::function<Stereo (const Buf&, const std::vector<EnvFrame>*)> render; };

/** The instrument's engine in one of its modes / LAB voices. WORLD: ROOM LEVEL -> FEEDBACK and ROOM HITS -> STRIKE. */
Stereo engineRender (const Buf& x, const std::vector<EnvFrame>* world, Mode mode, int voice)
{
    Engine e;
    e.prepare (fs);
    Controls c;
    for (int i = 0; i < kNumParams; ++i) c.p[i] = paramDefaults[i];
    c.p[kMix] = 1.0f; c.p[kLevel] = 0.7071f; c.p[kCouple] = 0.8f; c.p[kFeedback] = 0.62f;      // the voice alone, at unity level
    c.mode = mode; c.voice = voice;
    Graph g;
    g.connect (kRadioAudioOut, kResAudioIn);
    g.connect (kRoomLevelOut, kResFeedbackIn);
    g.connect (kRoomActivityOut, kResExciteIn);
    c.plan = RoutingPlan::fromGraph (g);
    Stereo out { Buf (N), Buf (N) };
    const Buf local (64, 0.0f);
    for (size_t at = 0; at + 64 <= N; at += 64)
    {
        c.room = world != nullptr ? (*world)[at / 64] : EnvFrame {};
        e.process (x.data() + at, x.data() + at, local.data(), out.l.data() + at, out.r.data() + at, 64, c);
    }
    return out;
}

#if WILDBAND_LAB_TAPESTRY
Stereo wyrdRender (const Buf& x, const std::vector<EnvFrame>* world)   // Wyrd's time/filter delay, as written, wet only; WORLD: level -> decay
{
    auto delay = std::make_unique<shortwav::wyrd::TimeFilterExperiment>();
    Stereo out { Buf (N), Buf (N) };
    for (size_t i = 0; i < N; ++i)
    {
        const float level = world != nullptr ? (*world)[i / 64].level : 0.0f;
        const auto r = delay->process (x[i] * 5.0f, 0.0f, 0.42f, 0.0f, std::min (1.0f, 0.45f + 0.45f * level), 0.55f, 0.3f, (float) fs);   // Rack levels: +/-5 V
        out.l[i] = out.r[i] = 0.2f * r.wet;                                   // wet only, like every other voice here (Rack +/-5 V -> +/-1)
    }
    return out;
}

/** Drift is a control source, not a voice: the resonator with its PITCH and TONE inputs driven by `source`. This is
    INTERNAL modulation; it travels on the engine's ROOM LEVEL wire here only because that is the lab's one free
    control wire, and it is never reported as WORLD. */
Stereo modulatedResonator (const Buf& x, int source)
{
    Engine e;
    e.prepare (fs);
    Controls c;
    for (int i = 0; i < kNumParams; ++i) c.p[i] = paramDefaults[i];
    c.p[kMix] = 1.0f; c.p[kLevel] = 0.7071f; c.p[kCouple] = 0.8f; c.p[kFeedback] = 0.62f; c.p[kDrift] = 0.0f;
    Graph g;
    g.connect (kRadioAudioOut, kResAudioIn);
    g.connect (kRoomLevelOut, kResPitchIn);
    g.connect (kRoomLevelOut, kResDampIn);
    c.plan = RoutingPlan::fromGraph (g);
    shortwav::drift::DriftCoreProcessor drift;
    shortwav::drift::Controls dc;
    Stereo out { Buf (N), Buf (N) };
    const Buf local (64, 0.0f);
    float value = 0.5f;
    for (size_t at = 0; at + 64 <= N; at += 64)
    {
        if (source == 1) for (int i = 0; i < 64; ++i) value = 0.5f + 0.1f * drift.process ({ 1.0f / (float) fs, 0.0f, false }, dc).outputVolts;   // +/-5 V -> 0..1
        if (source == 2) value = 0.5f + 0.5f * std::sin (2 * kPi * 0.21f * (float) at / (float) fs);
        c.room = { source == 0 ? 0.0f : value, 0.0f, 0.0f };
        e.process (x.data() + at, x.data() + at, local.data(), out.l.data() + at, out.r.data() + at, 64, c);
    }
    return out;
}
#endif

#if WILDBAND_LAB_RACK
struct KoruptStats { double lock = 0, tracking = 0, glitch = 0; };
KoruptStats lastKorupt;
// Korupt as written, driven the way its own module drives it (input at Rack full scale, Schmitt trigger at +/-0.18).
// WORLD: room level -> oscillator mix. (Room activity -> RATE was tried first and changed nothing in the output.)
Stereo koruptRender (const Buf& x, const std::vector<EnvFrame>* world)
{
    ShortwavDSP::KoruptInputStage stage;
    ShortwavDSP::KoruptDSP dsp;
    stage.setSampleRate ((float) fs); stage.reset();
    dsp.setSampleRate ((float) fs); dsp.reset();
    ShortwavDSP::KoruptParams p;
    Stereo out { Buf (N), Buf (N) };
    bool high = false;
    lastKorupt = {};
    for (size_t i = 0; i < N; ++i)
    {
        p.oscillatorMix = world != nullptr ? std::min (1.0f, 0.15f + (*world)[i / 64].level) : 0.7f;
        const auto conditioned = stage.process (x[i]);
        bool rising = false;                                                  // the module wrapper's Schmitt trigger, +/-0.18
        if (! high && conditioned.comparator >= 0.18f) { high = true; rising = true; }
        else if (high && conditioned.comparator <= -0.18f) high = false;
        const auto r = dsp.process (conditioned.audio, rising, high, p);
        out.l[i] = out.r[i] = r.mixed;
        lastKorupt.lock += r.lock; lastKorupt.tracking += r.tracking; lastKorupt.glitch += r.glitch;
    }
    lastKorupt.lock /= N; lastKorupt.tracking /= N; lastKorupt.glitch /= N;
    return out;
}
#endif
} // namespace

int main (int argc, char** argv)
{
    const std::string dir = argc > 1 ? argv[1] : "voice-lab";
    mkdir (dir.c_str(), 0755);
    const std::pair<const char*, Buf> fixtures[] = { { "speech-like", speechLike() }, { "percussion", percussion() }, { "music", music() }, { "static", staticNoise() }, { "silence", Buf (N, 0.0f) },
                                                    { "sine-220", [] { Buf x (N); for (size_t i = 0; i < N; ++i) x[i] = 0.5f * std::sin (2 * kPi * 220.0f * (float) i / (float) fs); return x; }() } };   // a diagnostic, not a musical fixture
    const auto world = scriptedWorld();

    std::vector<Voice> voices = {
        { "dry", "WILDBAND", "the fixture itself", 0, [] (const Buf& x, const std::vector<EnvFrame>* w) { return engineRender (x, w, Mode::Dry, 0); } },
        { "reference", "WILDBAND", "ordinary filter + delay", (size_t) (fs * 0.33 * 2 * 4), [] (const Buf& x, const std::vector<EnvFrame>* w) { return engineRender (x, w, Mode::Reference, 0); } },
        { "resonator", "WILDBAND", "baseline voice", 6 * 4096 * 4, [] (const Buf& x, const std::vector<EnvFrame>* w) { return engineRender (x, w, Mode::Engine, 0); } },
        { "dissolve", "WILDBAND (new, D2)", "rolling granular delay", [] { Dissolve d; d.prepare (fs); return d.memoryBytes(); }(), [] (const Buf& x, const std::vector<EnvFrame>* w) { return engineRender (x, w, Mode::Engine, 1); } },
        { "dissolve-into-resonator", "WILDBAND (new, D2)", "grains excite the resonators", 0, [] (const Buf& x, const std::vector<EnvFrame>* w) { return engineRender (x, w, Mode::Engine, 2); } },
       #if WILDBAND_HAS_RINGS
        { "rings-modal", "Rings (Emilie Gillet, MIT), upstream in place", "P1-R, external excitation", RingsVoice::memoryBytes(), [] (const Buf& x, const std::vector<EnvFrame>* w) { return engineRender (x, w, Mode::Engine, 3); } },
        { "rings-sympathetic", "Rings (Emilie Gillet, MIT), upstream in place", "P1-R, external excitation", RingsVoice::memoryBytes(), [] (const Buf& x, const std::vector<EnvFrame>* w) { return engineRender (x, w, Mode::Engine, 4); } },
        { "rings-string", "Rings (Emilie Gillet, MIT), upstream in place", "P1-R, external excitation", RingsVoice::memoryBytes(), [] (const Buf& x, const std::vector<EnvFrame>* w) { return engineRender (x, w, Mode::Engine, 5); } },
       #endif
       #if WILDBAND_LAB_RACK
        { "korupt", "vcv-tapestry, in place (D1)", "as written, Rack SDK types", sizeof (ShortwavDSP::KoruptDSP) + sizeof (ShortwavDSP::KoruptInputStage), koruptRender },
       #endif
       #if WILDBAND_LAB_TAPESTRY
        { "wyrd-timefilter", "vcv-tapestry, in place (D3)", "as written; wet only", (size_t) shortwav::wyrd::TimeFilterExperiment::kMaxDelaySamples * 4 + (32768 + 24576 + 16384) * 4, wyrdRender },
       #endif
    };

    std::string report = "# Voice lab render report\n\nFixtures: synthesized, " + std::to_string (seconds) + " s, 48 kHz. Every file is level-matched to -23 dBFS RMS; a render below -70 dBFS is left as it is and reported as silent. WORLD is a scripted stand-in.\n"
                         "For the WILDBAND voices, ns/sample is the whole engine with that voice selected, not the voice alone.\n"
                         "Cost is one thread, no audio device: a comparison between voices, not a CoreAudio measurement.\n\n"
                         "| voice | origin | fixture | ns/sample | memory kB | level before matching dBFS | crest dB | WORLD on vs off dB | finite | file stem |\n|---|---|---|---|---|---|---|---|---|---|\n";
    int bad = 0;
    std::string notes;
    for (auto& v : voices)
        for (auto& f : fixtures)
        {
            const auto t0 = std::chrono::steady_clock::now();
            Stereo off = v.render (f.second, nullptr);
            const double ns = std::chrono::duration<double, std::nano> (std::chrono::steady_clock::now() - t0).count() / (double) N;
            Stereo on = v.render (f.second, &world);
            Stereo diff { Buf (N), Buf (N) };
            for (size_t i = 0; i < N; ++i) { diff.l[i] = on.l[i] - off.l[i]; diff.r[i] = on.r[i] - off.r[i]; }
            const double level = rms (off), change = rms (diff);
            const bool ok = finite (off) && finite (on);
            bad += ! ok;
            const std::string stem = std::string (f.first) + "__" + v.name;
            char line[512];
            std::snprintf (line, sizeof (line), "| %s | %s | %s | %.0f | %.0f | %.1f | %.1f | %s | %s | `%s` |\n", v.name.c_str(), v.origin.c_str(), f.first, ns, (double) v.memory / 1024.0,
                           20 * std::log10 (level + 1e-9), level > kSilent ? 20 * std::log10 (peak (off) / level) : 0.0,
                           level > kSilent ? (change < 1e-9 ? std::string ("no change") : std::to_string ((int) std::lrint (20 * std::log10 (change / level)))).c_str() : (change > kSilent ? "sound only with WORLD" : "silent"), ok ? "yes" : "NO", stem.c_str());
            report += line;
            for (auto* s : { &off, &on })
            {
                const double r = rms (*s);
                if (r > kSilent) { const float gain = (float) (kTarget / r); for (size_t i = 0; i < N; ++i) { s->l[i] *= gain; s->r[i] *= gain; } }
                if (peak (*s) > 1.0f) { const float trim = 0.999f / peak (*s); for (size_t i = 0; i < N; ++i) { s->l[i] *= trim; s->r[i] *= trim; } notes += "- `" + stem + "` would have clipped at the matched level and was turned down by " + std::to_string (-20 * std::log10 (trim)).substr (0, 3) + " dB.\n"; }
            }
            const std::string where = v.name == "korupt" ? dir + "/korupt-unverified" : dir;      // see the diagnostic in the report
            mkdir (where.c_str(), 0755);
            writeWav (where + "/" + stem + "__world-off.wav", off);
            writeWav (where + "/" + stem + "__world-on.wav", on);
           #if WILDBAND_LAB_RACK
            if (v.name == "korupt") { char k[200]; std::snprintf (k, sizeof (k), "|  |  | %s: mean lock %.2f, tracking %.2f, glitch %.3f (WORLD on) |  |  |  |  |  |  |  |\n", f.first, lastKorupt.lock, lastKorupt.tracking, lastKorupt.glitch); report += k; }
           #endif
        }

    if (! notes.empty()) report += "\nNot exactly level-matched:\n" + notes;
   #if WILDBAND_LAB_RACK
    {
        // Diagnostic: Korupt's core driven exactly as its own unit tests drive it (a 400 Hz square straight into
        // KoruptDSP, no input stage), then silence. This separates "the lab drives it wrongly" from "that is what it does".
        ShortwavDSP::KoruptDSP dsp;
        dsp.setSampleRate ((float) fs); dsp.reset();
        ShortwavDSP::KoruptParams p;
        double lock = 0, tracking = 0, maxLock = 0;
        for (int i = 0; i < 48000 * 4; ++i) { const int phase = i % 120; const auto r = dsp.process (phase < 60 ? 1.0f : -1.0f, phase == 0, phase < 60, p); if (i >= 48000) { lock += r.lock; tracking += r.tracking; maxLock = std::max (maxLock, (double) r.lock); } }
        ShortwavDSP::KoruptResult silent;
        for (int i = 0; i < 48000 * 4; ++i) silent = dsp.process (0.0f, false, false, p);
        ShortwavDSP::KoruptInputStage stage;
        stage.setSampleRate ((float) fs); stage.reset();
        ShortwavDSP::KoruptConditionedInput rest;
        for (int i = 0; i < 48000 * 2; ++i) rest = stage.process (0.0f);
        char line[600];
        std::snprintf (line, sizeof (line), "\n## Korupt diagnostic\n\nCore driven as in its own tests (400 Hz square, default parameters, 3 s): mean lock %.3f, highest lock %.3f, mean tracking %.2f. "
                       "Then 4 s of zero input straight into the core: tracking %.3f. Its input stage fed 2 s of silence settles at audio = %.3f, comparator = %.3f, envelope = %.3f "
                       "(the core is given that 'audio' value, so with the input stage in front it does not see silence as zero).\n", lock / (48000.0 * 3), maxLock, tracking / (48000.0 * 3), (double) silent.tracking, (double) rest.audio, (double) rest.comparator, (double) rest.envelope);
        report += line;
    }
   #endif
   #if WILDBAND_LAB_TAPESTRY
    report += "\n## Drift as a control source (INTERNAL modulation, not WORLD)\n\nResonator with PITCH and TONE driven by: nothing, Drift (vcv-tapestry, in place), a plain 0.21 Hz sine.\n\n| fixture | Drift vs none dB | sine vs none dB | Drift ns/sample (core alone) |\n|---|---|---|---|\n";
    for (auto& f : fixtures)
    {
        Stereo none = modulatedResonator (f.second, 0), drift = modulatedResonator (f.second, 1), sine = modulatedResonator (f.second, 2);
        auto difference = [&] (const Stereo& a) { Stereo d { Buf (N), Buf (N) }; for (size_t i = 0; i < N; ++i) { d.l[i] = a.l[i] - none.l[i]; d.r[i] = a.r[i] - none.r[i]; } const double n = rms (none); return n > kSilent ? 20 * std::log10 (rms (d) / n + 1e-9) : 0.0; };
        shortwav::drift::DriftCoreProcessor core;
        shortwav::drift::Controls dc;
        const auto t0 = std::chrono::steady_clock::now();
        float sink = 0;
        for (size_t i = 0; i < N; ++i) sink += core.process ({ 1.0f / (float) fs, 0.0f, false }, dc).outputVolts;
        const double ns = std::chrono::duration<double, std::nano> (std::chrono::steady_clock::now() - t0).count() / (double) N;
        char line[300];
        std::snprintf (line, sizeof (line), "| %s | %.0f | %.0f | %.0f%s |\n", f.first, difference (drift), difference (sine), ns, sink == 12345.0f ? " " : "");
        report += line;
        const std::string stem = std::string (f.first) + "__resonator";
        for (auto item : { std::pair<const char*, Stereo*> { "__mod-none.wav", &none }, { "__mod-drift.wav", &drift }, { "__mod-sine.wav", &sine } })
        {
            const double r = rms (*item.second);
            if (r > kSilent) { const float gain = (float) (kTarget / r); for (size_t i = 0; i < N; ++i) { item.second->l[i] *= gain; item.second->r[i] *= gain; } }
            writeWav (dir + "/" + stem + item.first, *item.second);
        }
    }
   #endif

    if (FILE* f = std::fopen ((dir + "/REPORT.md").c_str(), "w")) { std::fputs (report.c_str(), f); std::fclose (f); }
    std::fputs (report.c_str(), stdout);
    return bad;
}
