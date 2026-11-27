// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Stephane P. Pericat
// WILDBAND P0 engine tests. Plain checks, no framework; exit code = number of failed checks.
// `wildband_tests --metrics` also prints level/brightness figures for a few control states (proxies, not listening).
#include "../src/Engine.h"
#include "baseline/Engine.h"
#include "../src/ReceiverGate.h"   // the auditioned engine, frozen: reference for depth +100 %

#include <cstdio>
#include <cstring>
#include <ctime>
#include <pthread.h>
#include <random>
#include <thread>

using namespace wildband;

namespace
{
int failures = 0, checks = 0;
const char* currentTest = "";
#define CHECK(cond) \
    do { ++checks; if (! (cond)) { ++failures; std::printf ("  FAIL [%s] %s:%d  %s\n", currentTest, __FILE__, __LINE__, #cond); } } while (0)

using Buf = std::vector<float>;

Controls defaults()
{
    Controls c;
    for (int i = 0; i < kNumParams; ++i) c.p[i] = paramDefaults[i];
    return c;
}

/** Runs mono `x` (to both inputs) through `e` in blocks of `block` (0 = varying sizes). Returns interleaved L,R. */
Buf run (Engine& e, const Buf& x, const Controls& c, int block)
{
    Buf l (x.size()), r (x.size()), out (x.size() * 2);
    std::mt19937 rng (7);
    for (size_t at = 0; at < x.size();)
    {
        const int n = (int) std::min (x.size() - at, (size_t) (block > 0 ? block : 1 + (int) (rng() % 700)));
        e.process (x.data() + at, x.data() + at, x.data() + at, l.data() + at, r.data() + at, n, c);
        at += (size_t) n;
    }
    for (size_t i = 0; i < x.size(); ++i) { out[2 * i] = l[i]; out[2 * i + 1] = r[i]; }
    return out;
}

/** A plan nobody would build: arbitrary sources and depths, including NaN, infinity and huge values. */
RoutingPlan garbagePlan (std::mt19937& rng)
{
    RoutingPlan p;
    static const float nasty[] = { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), 1.0e30f, -1.0e30f, -1.0f, 1.0f, 0.0f };
    for (int i = 0; i < kNumInputs; ++i)
    {
        p.source[i] = (int8_t) (rng() & 0xff);
        p.depth[i] = rng() % 3 == 0 ? nasty[rng() % 8] : (float) (rng() % 4001) / 1000.0f - 2.0f;
    }
    return p;
}

Buf noise (size_t n, float amp, unsigned seed = 1)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> u (-amp, amp);
    Buf x (n);
    for (auto& v : x) v = u (rng);
    return x;
}

Buf testSource (double fs, size_t n)
{
    TestSource t;
    t.prepare (fs);
    Buf x (n);
    for (auto& v : x) v = t.next();
    return x;
}

float rms (const Buf& x, size_t from = 0)
{
    double s = 0;
    for (size_t i = from; i < x.size(); ++i) s += (double) x[i] * x[i];
    return (float) std::sqrt (s / (double) std::max<size_t> (1, x.size() - from));
}
float peak (const Buf& x) { float p = 0; for (auto v : x) p = std::max (p, std::abs (v)); return p; }
bool allFinite (const Buf& x) { for (auto v : x) if (! std::isfinite (v)) return false; return true; }

// ---------------------------------------------------------------------------------------------------------------------
void testDial()
{
    currentTest = "dial";
    CHECK (dial (0.0f, 1).station == 0 && dial (0.9f, 1).offCentre == 0.0f);
    for (int n = 2; n <= 8; ++n)
        for (int s = 0; s < n; ++s)
        {
            const auto d = dial (dialCentre (s, n), n);
            CHECK (d.station == s && d.offCentre == 0.0f);                       // centre of an arc is clean
            CHECK (dial (dialCentre (s, n) + 0.19f / (float) n, n).offCentre == 0.0f);   // so is the middle 40 %
            CHECK (dial (((float) s + 0.98f) / (float) n, n).offCentre > 0.9f);  // the edge is fully detuned
        }
    CHECK (dial (1.0f, 6).station == 5 && dial (-3.0f, 6).station == 0 && dial (7.0f, 6).station == 5);
}

void testBounded()
{
    currentTest = "bounded";
    std::mt19937 rng (3);
    std::uniform_real_distribution<float> u (0.0f, 1.0f);
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        const size_t n = (size_t) fs * 2;
        Buf nasty = noise (n, 1.0f);
        for (size_t i = 0; i < n; ++i)
        {
            if (i > n / 4 && i < n / 4 + 2000) nasty[i] = 4.0f;                        // DC slab
            if (i % 9973 == 0) nasty[i] = std::numeric_limits<float>::quiet_NaN();
            if (i % 12345 == 1) nasty[i] = std::numeric_limits<float>::infinity();
            if (i % 5000 == 2) nasty[i] = -1.0e30f;
        }
        for (int trial = 0; trial < 24; ++trial)
        {
            Controls c;
            for (auto& p : c.p) p = trial == 0 ? 0.0f : trial == 1 ? 1.0f : trial < 12 ? (float) (rng() & 1) : u (rng);
            c.p[kLevel] = 1.0f;
            if (trial > 1) c.p[kFeedback] = 1.0f;                                    // feedback at the stop in most trials
            c.mode = (Mode) (trial % 3);
            c.detune = u (rng);
            c.gap = trial % 3 == 0 ? 0.0f : u (rng) * 2.0f;                          // CT-11: tuner static in the hostile runs
            c.strikeResponse = trial % 5 == 4 ? 99 : trial % 3;                      // SR-06: every response (and a nonsense value)
            c.voice = trial % 7 == 6 ? 42 : (trial / 2) % 3;                         // DV-03: every LAB voice (and a nonsense value)
            c.room = { u (rng) * 3.0f, u (rng) * 3.0f, u (rng) * 3.0f };             // out-of-range room values too
            if (trial % 4 == 3) c.plan = garbagePlan (rng);                          // garbage plans too
            else if (trial % 4 == 1)                                                // AT-08: every destination at -1 or +1
            {
                Graph g = Graph::defaultPatch();
                for (int in = kResPitchIn; in < kNumPorts; ++in) g.setDepth (g.cableInto (in), in, trial % 8 == 1 ? -1.0f : 1.0f);
                c.plan = RoutingPlan::fromGraph (g);
            }
            Engine e;
            e.prepare (fs);
            const auto y = run (e, nasty, c, trial % 2 ? 0 : 64);
            CHECK (allFinite (y));
            CHECK (peak (y) <= Engine::kCeiling);
            CHECK (e.faults.load() == 0);
        }
    }
}

void testDeterministicAndBlockInvariant()
{
    currentTest = "block-invariance";
    const auto x = testSource (48000.0, 48000 * 3);
    auto c = defaults();
    c.p[kDrift] = 0.8f; c.p[kCouple] = 0.9f; c.p[kFeedback] = 0.85f; c.room = { 0.5f, 0.3f, 0.6f }; c.detune = 0.4f;
    Graph depths = Graph::defaultPatch();                                           // AT-08: with mixed signed depths
    const float mixed[] = { -1.0f, 0.4f, -0.6f, 1.0f, -0.25f, 0.8f };
    for (int in = kResPitchIn; in < kNumPorts; ++in) depths.setDepth (depths.cableInto (in), in, mixed[in - kResPitchIn]);
    c.plan = RoutingPlan::fromGraph (depths);
    c.gap = 1.0f; c.p[kStatic] = 0.8f;                                              // CT-11: with tuner static playing
    Buf ref;
    for (int block : { 1, 64, 1000, 0 })
    {
        Engine e;
        e.prepare (48000.0);
        const auto y = run (e, x, c, block);
        if (ref.empty()) ref = y;
        CHECK (std::memcmp (ref.data(), y.data(), y.size() * sizeof (float)) == 0);
    }
}

/** With no drift or coupling, the harmonic SHAPE must ring at the PITCH knob's note. */
void testTuning()
{
    currentTest = "tuning";
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (float midi : { 48.0f, 60.0f, 72.0f })
        {
            auto c = defaults();
            c.p[kPitch] = (midi - 36.0f) / 48.0f; c.p[kShape] = 2.0f / 3.0f; c.p[kTone] = 1.0f;
            c.p[kCouple] = 0.0f; c.p[kDrift] = 0.0f; c.p[kFeedback] = 0.62f; c.p[kMix] = 1.0f;
            Engine e;
            e.prepare (fs);
            Buf x ((size_t) fs, 0.0f);
            x[10] = 1.0f;
            const auto y = run (e, x, c, 64);
            const double period = fs / (440.0 * std::exp2 ((midi - 69.0) / 12.0));
            const int span = 8;                                                    // measure over 8 periods for resolution
            const size_t from = (size_t) (fs * 0.1), len = (size_t) (fs * 0.5);
            auto corr = [&] (int lag) { double s = 0; for (size_t i = from; i < from + len; ++i) s += (double) y[2 * i] * y[2 * (i + (size_t) lag)]; return s; };
            int best = 0;
            double bestV = -1e30;
            for (int lag = (int) (span * period * 0.96); lag <= (int) (span * period * 1.04) + 1; ++lag)
                if (const double v = corr (lag); v > bestV) { bestV = v; best = lag; }
            const double a = corr (best - 1), b = bestV, d = corr (best + 1);
            const double lag = best + 0.5 * (a - d) / (a - 2.0 * b + d);
            const double cents = 1200.0 * std::log2 (span * period / lag);
            CHECK (std::abs (cents) < 10.0);
            if (std::abs (cents) >= 10.0) std::printf ("    fs %.0f midi %.0f: %.1f cents\n", fs, (double) midi, cents);
        }
}

void testDecayAndSelfOscillation()
{
    currentTest = "decay";
    const double fs = 48000.0;
    Buf x = noise ((size_t) fs * 12, 0.5f);
    std::fill (x.begin() + (long) fs, x.end(), 0.0f);            // 1 s of noise, then silence
    {
        auto c = defaults();                                       // FEEDBACK 0.55: below unity loop gain
        Engine e;
        e.prepare (fs);
        const auto y = run (e, x, c, 64);
        CHECK (rms (y, (size_t) (fs * 2 * 11)) < 1.0e-4f);         // silent 10 s after the input stops
    }
    {
        auto c = defaults();
        c.p[kFeedback] = 1.0f; c.p[kLevel] = 1.0f;                 // above unity: it must sustain, bounded
        Engine e;
        e.prepare (fs);
        const auto y = run (e, x, c, 64);
        CHECK (rms (y, (size_t) (fs * 2 * 11)) > 1.0e-3f);
        CHECK (peak (y) <= Engine::kCeiling);
        e.reset();                                                 // PANIC
        const auto z = run (e, Buf ((size_t) fs, 0.0f), c, 64);
        CHECK (peak (z) == 0.0f);                                  // and from silence it makes no sound of its own
    }
}

/** Abrupt control changes must not put a step in the output. */
void testNoClicks()
{
    currentTest = "no-clicks";
    const double fs = 48000.0;
    Buf x ((size_t) fs);
    for (size_t i = 0; i < x.size(); ++i) x[i] = 0.3f * std::sin (2.0f * kPi * 100.0f * (float) i / (float) fs);
    auto maxStep = [&] (Controls a, Controls b)
    {
        Engine e;
        e.prepare (fs);
        Buf first (x.begin(), x.begin() + (long) x.size() / 2), second (x.begin() + (long) x.size() / 2, x.end());
        auto y = run (e, first, a, 64);
        const auto y2 = run (e, second, b, 64);
        y.insert (y.end(), y2.begin(), y2.end());
        float m = 0;
        for (size_t i = 2; i < y.size(); ++i) m = std::max (m, std::abs (y[i] - y[i - 2]));
        return m;
    };
    auto base = defaults();
    base.p[kTone] = 0.2f; base.p[kPitch] = 0.1f; base.p[kDrift] = 0.0f; base.p[kCouple] = 0.0f;
    const float steady = maxStep (base, base);
    for (int p = 0; p < kNumParams; ++p)
        for (float to : { 0.0f, 1.0f })
        {
            auto b = base;
            b.p[p] = to;
            CHECK (maxStep (base, b) < steady + 0.03f);
        }
    for (Mode m : { Mode::Dry, Mode::Reference })
    {
        auto b = base;
        b.mode = m;
        CHECK (maxStep (base, b) < steady + 0.03f);
        CHECK (maxStep (b, base) < steady + 0.03f);
    }
    auto b = base;
    b.detune = 1.0f;
    CHECK (maxStep (base, b) < steady + 0.03f);
}

void testRoom()
{
    currentTest = "room";
    const double fs = 48000.0;
    auto sine = [&] (float hz, float amp, size_t n) { Buf x (n); for (size_t i = 0; i < n; ++i) x[i] = amp * std::sin (2.0f * kPi * hz * (float) i / (float) fs); return x; };
    auto feed = [&] (RoomAnalyser& a, const Buf& x) { EnvFrame f; for (size_t at = 0; at < x.size(); at += 64) f = a.process (x.data() + at, (int) std::min<size_t> (64, x.size() - at)); return f; };
    RoomAnalyser a;
    a.prepare (fs);
    auto f = feed (a, Buf (48000, 0.0f));
    CHECK (f.level == 0.0f && f.activity == 0.0f && f.brightness == 0.0f);          // silence is neutral
    f = feed (a, sine (200.0f, 0.1f, 4800));                                        // a sound starts: an onset
    CHECK (f.activity > 0.5f);
    f = feed (a, sine (200.0f, 0.1f, 48000 * 3));                                   // ...which then stays: no onset
    CHECK (f.activity < 0.05f);
    CHECK (std::abs (f.level - (20.0f * std::log10 (0.1f / std::sqrt (2.0f)) + 60.0f) / 50.0f) < 0.03f);
    CHECK (f.brightness < 0.3f);
    f = feed (a, sine (8000.0f, 0.1f, 48000));
    CHECK (f.brightness > 0.8f);
    Buf junk = noise (48000, 100.0f);
    junk[100] = std::numeric_limits<float>::quiet_NaN();
    f = feed (a, junk);
    CHECK (f.level >= 0 && f.level <= 1 && f.activity >= 0 && f.activity <= 1 && f.brightness >= 0 && f.brightness <= 1);

    EnvBus bus;
    CHECK (bus.read (1000).level == 0.0f);                                          // nothing published: neutral
    bus.publish (kEnvMicrophone, { 0.7f, 2.0f, 0.2f }, 1000);
    CHECK (bus.read (1100).level == 0.7f && bus.read (1100).activity == 1.0f);      // clamped
    CHECK (bus.read (1000 + EnvBus::staleMs).level == 0.0f);                        // stale: neutral again
    bus.publish (kEnvMicrophone, { 0.7f, 0.0f, 0.2f }, 5000);
    bus.withdraw (kEnvMicrophone);
    CHECK (! bus.fresh (kEnvMicrophone, 5001));
}

void testGraph()
{
    currentTest = "graph";
    Graph g;
    CHECK (g.check (kRadioAudioOut, kResAudioIn) == ConnectError::None);
    CHECK (g.check (kRadioAudioOut, kResDampIn) == ConnectError::TypeMismatch);       // audio never drives a control input
    CHECK (g.check (kRadioLevelOut, kResAudioIn) == ConnectError::TypeMismatch);
    CHECK (g.check (kResAudioIn, kRadioAudioOut) == ConnectError::Reversed);
    CHECK (g.check (kRadioLevelOut, kRoomLevelOut) == ConnectError::OutputToOutput);
    CHECK (g.check (kResDampIn, kResPitchIn) == ConnectError::InputToInput);
    CHECK (g.check (kRadioLevelOut, kRadioAudioOut) == ConnectError::OutputToOutput);
    CHECK (g.check (-1, kResAudioIn) == ConnectError::UnknownPort && g.check (kRadioAudioOut, kNumPorts) == ConnectError::UnknownPort);
    CHECK (g.connect (kRadioLevelOut, kResDampIn) == ConnectError::None);
    CHECK (g.connect (kRadioLevelOut, kResDampIn) == ConnectError::Duplicate);
    CHECK (g.connect (kRoomLevelOut, kResDampIn) == ConnectError::FanIn);             // one cable per input
    CHECK (g.connect (kRadioLevelOut, kResFeedbackIn) == ConnectError::None);         // one output, many inputs
    CHECK (g.connect (kRadioLevelOut, kResDriftIn) == ConnectError::None);
    CHECK (g.size() == 3 && g.cableInto (kResFeedbackIn) == kRadioLevelOut && g.cableInto (kResPitchIn) == -1);
    CHECK (g.disconnect (kRadioLevelOut, kResPitchIn) == ConnectError::NotConnected);
    CHECK (g.disconnect (kRadioLevelOut, kResDampIn) == ConnectError::None && g.size() == 2);
    CHECK (g.connect (kRoomLevelOut, kResDampIn) == ConnectError::None);              // the input is free again

    const auto plan = RoutingPlan::fromGraph (g);
    CHECK (plan.sourceOf (kResDampIn) == kRoomLevelOut && plan.sourceOf (kResAudioIn) == -1);
    for (int p = 0; p < kNumPorts; ++p) CHECK (findPort (ports[p].id) == p);
    CHECK (findPort ("resonator.nope") == -1);
    CHECK (Graph::defaultPatch().size() == 7);
}

/** A cable must change the sound, pulling it must undo that, and COUPLE at zero must silence every control cable. */
void testPatching()
{
    currentTest = "patching";
    auto x = testSource (48000.0, 48000 * 2);                        // pings and bursts, plus hiss so TONE has something to open
    const auto hiss = noise (x.size(), 0.05f);
    for (size_t i = 0; i < x.size(); ++i) x[i] += hiss[i];
    const auto silence = Buf (x.size(), 0.0f);
    auto render = [&] (const Graph& g, float couple, EnvFrame room, const Buf& radio, const Buf& local)
    {
        auto c = defaults();
        c.p[kCouple] = couple; c.p[kDrift] = 0.6f;
        c.room = room;
        c.plan = RoutingPlan::fromGraph (g);
        Engine e;
        e.prepare (48000.0);
        Buf l (x.size()), r (x.size());
        e.process (radio.data(), radio.data(), local.data(), l.data(), r.data(), (int) x.size(), c);
        return l;
    };
    auto differ = [] (const Buf& a, const Buf& b) { Buf d (a.size()); for (size_t i = 0; i < a.size(); ++i) d[i] = a[i] - b[i]; return rms (d) / (rms (a) + 1.0e-9f); };
    const EnvFrame quiet {}, loud { 0.9f, 0.0f, 0.0f }, clap { 0.3f, 1.0f, 0.0f }, bright { 0.3f, 0.0f, 1.0f };

    Graph audioOnly;
    audioOnly.connect (kRadioAudioOut, kResAudioIn);
    const auto base = render (audioOnly, 0.8f, quiet, x, silence);
    CHECK (rms (base) > 1.0e-3f);
    CHECK (differ (base, render (audioOnly, 0.8f, loud, x, silence)) == 0.0f);          // no cable: the room does nothing
    CHECK (rms (render (Graph(), 0.8f, quiet, x, x)) == 0.0f);                          // audio cable pulled: silence
    Graph localOnly;
    localOnly.connect (kLocalAudioOut, kResAudioIn);
    CHECK (rms (render (localOnly, 0.8f, quiet, silence, x)) > 1.0e-3f);                // local source patched instead
    CHECK (rms (render (localOnly, 0.8f, quiet, x, silence)) == 0.0f);

    struct Case { Port from, to; EnvFrame room; };
    for (const auto& k : { Case { kRoomLevelOut, kResFeedbackIn, loud }, Case { kRoomLevelOut, kResDampIn, loud }, Case { kRoomActivityOut, kResExciteIn, clap },
                           Case { kRoomActivityOut, kResPitchIn, clap }, Case { kRoomBrightnessOut, kResShapeIn, bright }, Case { kRoomLevelOut, kResDriftIn, loud },
                           Case { kRadioLevelOut, kResDampIn, quiet }, Case { kRadioActivityOut, kResExciteIn, quiet } })
    {
        auto g = audioOnly;
        CHECK (g.connect (k.from, k.to) == ConnectError::None);
        const float heard = differ (base, render (g, 0.8f, k.room, x, silence));
        CHECK (heard > 0.05f);                                                          // the cable is audible
        if (heard <= 0.05f) std::printf ("    %s -> %s: %.4f\n", ports[k.from].id, ports[k.to].id, (double) heard);
        CHECK (differ (render (audioOnly, 0.0f, quiet, x, silence), render (g, 0.0f, k.room, x, silence)) == 0.0f); // COUPLE 0: not
    }
    auto fan = audioOnly;                                                                // one source, three destinations
    fan.connect (kRoomLevelOut, kResFeedbackIn);
    const auto one = render (fan, 0.8f, loud, x, silence);
    fan.connect (kRoomLevelOut, kResDampIn);
    fan.connect (kRoomLevelOut, kResDriftIn);
    CHECK (differ (one, render (fan, 0.8f, loud, x, silence)) > 0.05f);

    // re-patching while audio runs: no step in the output
    Engine e;
    e.prepare (48000.0);
    auto c = defaults();
    c.p[kTone] = 0.2f; c.p[kPitch] = 0.1f; c.room = loud;
    Buf tone (48000);
    for (size_t i = 0; i < tone.size(); ++i) tone[i] = 0.3f * std::sin (2.0f * kPi * 100.0f * (float) i / 48000.0f);
    Buf y;
    std::mt19937 rng (5);
    for (int seg = 0; seg < 40; ++seg)
    {
        Graph g;
        for (int tries = 0; tries < 12; ++tries) g.connect ((int) (rng() % kFirstInput), kFirstInput + (int) (rng() % kNumInputs));
        if (seg % 2 == 0) g.connect (kRadioAudioOut, kResAudioIn);
        c.plan = RoutingPlan::fromGraph (g);
        Buf l (1200), r (1200);
        e.process (tone.data() + seg * 1200, tone.data() + seg * 1200, tone.data() + seg * 1200, l.data(), r.data(), 1200, c);
        y.insert (y.end(), l.begin(), l.end());
    }
    float step = 0;
    for (size_t i = 1; i < y.size(); ++i) step = std::max (step, std::abs (y[i] - y[i - 1]));
    CHECK (step < 0.03f);
}

// ---- allocation gate: Engine::process must not allocate (same instrument as AFTERIMAGE's tests)
using MallocLogger = void (uint32_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uint32_t);
extern "C" MallocLogger* malloc_logger;
std::atomic<int> allocs { 0 };
pthread_t watched;
void countAllocs (uint32_t type, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uint32_t) { if ((type & 2) != 0 && pthread_equal (pthread_self(), watched)) ++allocs; }

void testNoAllocation()
{
    currentTest = "no-allocation";
    Engine e;
    e.prepare (48000.0);
    RoomAnalyser room;
    room.prepare (48000.0);
    EnvBus bus;
    const auto x = noise (64, 0.5f);
    Buf l (64), r (64);
    auto c = defaults();
    PlanMailbox mail;
    std::mt19937 rng (9);
    watched = pthread_self();
    malloc_logger = countAllocs;
    { void* volatile p = std::malloc (64); std::free (p); }                         // positive control
    const int control = allocs.exchange (0);
    for (int b = 0; b < 4000; ++b)
    {
        c.p[b % kNumParams] = (float) (b % 17) / 16.0f;
        c.gap = (b / 400) % 2 ? 1.0f : 0.0f;                                         // CT-11: static starting and stopping
        c.strikeResponse = (b / 250) % 3;                                            // SR-06
        c.voice = (b / 700) % 3;                                                     // DV-03
        c.mode = (Mode) ((b / 500) % 3);
        bus.publish (kEnvMicrophone, room.process (x.data(), 64), b);
        c.room = bus.read (b);
        e.process (x.data(), x.data(), x.data(), l.data(), r.data(), 64, c);
        if (b % 300 == 0) mail.publish (garbagePlan (rng));                           // AT-10: plans published and read here too
        c.plan = mail.read();
        if (b == 2000) e.reset();
    }
    malloc_logger = nullptr;
    CHECK (control == 1);
    CHECK (allocs.load() == 0);
}


// =====================================================================================================================
// Per-connection depth (attenuverter) acceptance tests: docs/p0/ATTENUVERTER-ACCEPTANCE.md
namespace depth
{
using Cable = std::pair<int, int>;
template <typename G> G wire (std::initializer_list<Cable> cables) { G g; for (auto c : cables) g.connect (c.first, c.second); return g; }
const std::initializer_list<Cable> kAudio = { { kRadioAudioOut, kResAudioIn } };

struct Run { Buf out; std::vector<float> f0, f1; };
float strikeDefaultDifferenceDb = 0, strikeDefaultLevelDb = 0;   // ST-04, reported by --metrics

/** Radio = pings and bursts plus hiss; local = other noise. `track`: also record resonator frequencies per block. */
Run render (const RoutingPlan& plan, Controls c, double fs = 48000.0, double seconds = 2.0, double inputSeconds = 1.0e9)
{
    const size_t n = (size_t) (fs * seconds);
    auto radio = testSource (fs, n);
    const auto hiss = noise (n, 0.05f), local = noise (n, 0.2f, 5);
    for (size_t i = 0; i < n; ++i) radio[i] = (double) i < inputSeconds * fs ? radio[i] + hiss[i] : 0.0f;
    c.plan = plan;
    Engine e;
    e.prepare (fs);
    Run r;
    r.out.resize (n * 2);
    Buf l (64), rr (64);
    for (size_t at = 0; at + 64 <= n; at += 64)
    {
        e.process (radio.data() + at, radio.data() + at, local.data() + at, l.data(), rr.data(), 64, c);
        for (size_t i = 0; i < 64; ++i) { r.out[2 * (at + i)] = l[i]; r.out[2 * (at + i) + 1] = rr[i]; }
        r.f0.push_back (e.lineFrequency (0));
        r.f1.push_back (e.lineFrequency (1));
    }
    return r;
}
bool same (const Buf& a, const Buf& b) { return a.size() == b.size() && std::memcmp (a.data(), b.data(), a.size() * sizeof (float)) == 0; }
RoutingPlan plan (Graph g) { return RoutingPlan::fromGraph (g); }
Graph with (Graph g, int from, int to, float depth) { g.connect (from, to, depth); return g; }

Controls loud()
{
    auto c = defaults();
    c.p[kCouple] = 1.0f; c.p[kDrift] = 0.0f;
    c.room = { 1.0f, 1.0f, 1.0f };
    return c;
}

void legacyParity()   // AT-01
{
    currentTest = "AT-01 legacy parity";
    namespace base = wildband_baseline;
    // Since the STRIKE correction (docs/p0/STRIKE-NEUTRAL.md) a cable in STRIKE is intentionally NOT sample-identical
    // to P0, so exact parity is required for every other destination; STRIKE has its own tests in strikeNeutral().
    const std::initializer_list<Cable> patches[] = {
        { { kRadioAudioOut, kResAudioIn }, { kRadioLevelOut, kResDampIn }, { kRadioLevelOut, kResDriftIn },
          { kRoomLevelOut, kResFeedbackIn }, { kRoomActivityOut, kResPitchIn }, { kRoomBrightnessOut, kResShapeIn } },                       // the default patch minus STRIKE
        { { kLocalAudioOut, kResAudioIn }, { kRoomLevelOut, kResPitchIn }, { kRoomLevelOut, kResShapeIn }, { kRoomLevelOut, kResDampIn },
          { kRoomLevelOut, kResDriftIn }, { kRoomLevelOut, kResFeedbackIn } },                                                               // one source everywhere but STRIKE
        { { kRadioAudioOut, kResAudioIn } } };
    std::mt19937 rng (21);
    std::uniform_real_distribution<float> u (0.0f, 1.0f);
    int compared = 0;
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (const auto& cables : patches)
            for (int state = 0; state < 4; ++state)
            {
                Controls c = defaults();
                base::Controls b;
                if (state > 0) for (auto& p : c.p) p = state == 1 ? 1.0f : u (rng);
                c.mode = (Mode) (state % 3);
                c.detune = state * 0.3f;
                c.room = { u (rng), u (rng), u (rng) };
                for (int i = 0; i < base::kNumParams; ++i) b.p[i] = c.p[i];
                b.mode = (base::Mode) (int) c.mode; b.detune = c.detune; b.room = { c.room.level, c.room.activity, c.room.brightness };
                c.plan = RoutingPlan::fromGraph (wire<Graph> (cables));                                // every depth +1
                b.plan = base::RoutingPlan::fromGraph (wire<base::Graph> (cables));

                const size_t n = (size_t) fs;
                const auto radio = testSource (fs, n), local = noise (n, 0.2f, 5);
                Engine now;
                base::Engine then;
                now.prepare (fs);
                then.prepare (fs);
                Buf l1 (n), r1 (n), l2 (n), r2 (n);
                for (size_t at = 0; at + 128 <= n; at += 128)
                {
                    now.process (radio.data() + at, radio.data() + at, local.data() + at, l1.data() + at, r1.data() + at, 128, c);
                    then.process (radio.data() + at, radio.data() + at, local.data() + at, l2.data() + at, r2.data() + at, 128, b);
                }
                CHECK (same (l1, l2) && same (r1, r2));
                ++compared;
            }
    CHECK (compared == 36);
}

void persistence()   // AT-02, AT-12
{
    currentTest = "AT-02 migrated patch";
    const Graph old = loadPatch ("radio.audio_out|resonator.audio_in;radio.activity_out|resonator.excite_cv_in;radio.level_out|resonator.damp_cv_in;"
                                 "radio.level_out|resonator.drift_cv_in;room.level_out|resonator.feedback_cv_in;room.activity_out|resonator.pitch_cv_in;"
                                 "room.brightness_out|resonator.shape_cv_in");                    // exactly what P0 wrote for the default patch
    CHECK (old.size() == 7);
    const auto a = RoutingPlan::fromGraph (old), b = RoutingPlan::fromGraph (Graph::defaultPatch());
    for (int i = 0; i < kNumInputs; ++i) CHECK (a.source[i] == b.source[i] && a.depth[i] == 1.0f && b.depth[i] == 1.0f);

    Graph g = Graph::defaultPatch();
    g.setDepth (kRoomLevelOut, kResFeedbackIn, -0.5f);
    g.setDepth (kRadioLevelOut, kResDampIn, 0.25f);
    g.setDepth (kRadioLevelOut, kResDriftIn, 0.0f);
    const Graph back = loadPatch (savePatch (g), savePatch (g, true));                            // round trip keeps depths
    CHECK (loadPatch (savePatch (g)).depthInto (kResFeedbackIn) == 1.0f);                         // topology alone: +1
    CHECK (loadPatch (savePatch (g, true)).depthInto (kResFeedbackIn) == -0.5f);                  // the three-field form is also readable on its own
    {
        Graph other = Graph::defaultPatch();                                                      // depths for cables that are not there are ignored
        other.disconnect (kRoomLevelOut, kResFeedbackIn);
        const Graph partial = loadPatch (savePatch (other), savePatch (g, true));
        CHECK (partial.size() == 6 && partial.cableInto (kResFeedbackIn) == -1 && partial.depthInto (kResDampIn) == 0.25f);
    }
    {
        // The P0 build must still read what this build saves under "cables" (both builds share one settings file).
        // This is P0's own parsing rule, from its Main.cpp: split on ';', then on the FIRST '|', and look both ids up.
        namespace base = wildband_baseline;
        base::Graph old2;
        const std::string cables = savePatch (g);
        for (size_t at = 0; at <= cables.size();)
        {
            const size_t end = std::min (cables.find (';', at), cables.size());
            const std::string cable = cables.substr (at, end - at);
            at = end + 1;
            const size_t bar = cable.find ('|');
            if (bar == std::string::npos) continue;
            old2.connect (base::findPort (cable.substr (0, bar).c_str()), base::findPort (cable.substr (bar + 1).c_str()));
        }
        CHECK (old2.size() == g.size());
        for (int in = kFirstInput; in < kNumPorts; ++in) CHECK (old2.cableInto (in) == g.cableInto (in));
    }
    CHECK (back.size() == 7 && back.depthInto (kResFeedbackIn) == -0.5f && back.depthInto (kResDampIn) == 0.25f && back.depthInto (kResDriftIn) == 0.0f && back.depthInto (kResPitchIn) == 1.0f);

    currentTest = "AT-12 malformed state";
    const char* bad[] = {
        "", ";;;", "|", "||", "garbage", "radio.audio_out", "radio.audio_out|", "|resonator.audio_in", "nope|resonator.audio_in|0.5",
        "radio.level_out|resonator.damp_cv_in|nan", "radio.level_out|resonator.damp_cv_in|inf", "radio.level_out|resonator.damp_cv_in|-inf",
        "radio.level_out|resonator.damp_cv_in|1e30", "radio.level_out|resonator.damp_cv_in|-7", "radio.level_out|resonator.damp_cv_in|abc",
        "radio.level_out|resonator.damp_cv_in|", "radio.level_out|resonator.damp_cv_in|0.5|extra|stuff",
        "radio.audio_out|resonator.audio_in|-1",                                                    // a depth on an audio cable
        "radio.level_out|resonator.damp_cv_in|0.3;room.level_out|resonator.damp_cv_in|0.9",          // second cable into the same input
        "radio.level_out|resonator.damp_cv_in|0.3;radio.level_out|resonator.damp_cv_in|0.9",          // duplicate
        "radio.audio_out|resonator.damp_cv_in|0.5", "resonator.audio_in|radio.audio_out|0.5",        // type mismatch, reversed
        "radio.level_out|resonator.pitch_cv_in|0.1;radio.level_out|resonator.shape_cv_in|0.2;radio.level_out|resonator.damp_cv_in|0.3;radio.level_out|resonator.excite_cv_in|0.4;radio.level_out|resonator.drift_cv_in|0.5;radio.level_out|resonator.feedback_cv_in|0.6;radio.audio_out|resonator.audio_in;local.audio_out|resonator.audio_in" };
    for (const char* text : bad)
    {
        const Graph g2 = loadPatch (text);
        CHECK (g2.size() <= kNumInputs);
        for (int i = 0; i < g2.size(); ++i)
        {
            CHECK (Graph().check (g2[i].from, g2[i].to) == ConnectError::None);                     // each cable is one connect() would accept
            CHECK (std::isfinite (g2[i].depth) && g2[i].depth >= -1.0f && g2[i].depth <= 1.0f);
            if (ports[g2[i].to].type == PortType::Audio) CHECK (g2[i].depth == 1.0f);
            for (int j = 0; j < i; ++j) CHECK (g2[j].to != g2[i].to);                               // one cable per input
        }
    }
    CHECK (loadPatch ("radio.level_out|resonator.damp_cv_in|nan").depthInto (kResDampIn) == 1.0f);
    CHECK (loadPatch ("radio.level_out|resonator.damp_cv_in|1e30").depthInto (kResDampIn) == 1.0f);
    CHECK (loadPatch ("radio.level_out|resonator.damp_cv_in|-7").depthInto (kResDampIn) == -1.0f);
    CHECK (loadPatch ("radio.level_out|resonator.damp_cv_in|0.3;room.level_out|resonator.damp_cv_in|0.9").depthInto (kResDampIn) == 0.3f);
}

void graphRules()   // AT-13
{
    currentTest = "AT-13 graph rules";
    Graph g;
    CHECK (g.connect (kRadioAudioOut, kResAudioIn, -0.5f) == ConnectError::None && g.depthInto (kResAudioIn) == 1.0f);   // audio: always +1
    CHECK (g.connect (kRoomLevelOut, kResFeedbackIn) == ConnectError::None && g.depthInto (kResFeedbackIn) == 1.0f);     // default +1
    CHECK (g.connect (kRoomLevelOut, kResDampIn, -3.0f) == ConnectError::None && g.depthInto (kResDampIn) == -1.0f);     // clamped
    CHECK (g.setDepth (kRoomLevelOut, kResFeedbackIn, 0.4f) == ConnectError::None && g.depthInto (kResFeedbackIn) == 0.4f);
    CHECK (g.depthInto (kResDampIn) == -1.0f);                                                                           // fan-out: the other cable is untouched
    CHECK (g.setDepth (kRoomLevelOut, kResFeedbackIn, 9.0f) == ConnectError::None && g.depthInto (kResFeedbackIn) == 1.0f);
    CHECK (g.setDepth (kRoomLevelOut, kResFeedbackIn, std::numeric_limits<float>::quiet_NaN()) == ConnectError::NoDepth && g.depthInto (kResFeedbackIn) == 1.0f);
    CHECK (g.setDepth (kRadioAudioOut, kResAudioIn, 0.5f) == ConnectError::NoDepth && g.depthInto (kResAudioIn) == 1.0f);
    CHECK (g.setDepth (kRoomLevelOut, kResPitchIn, 0.5f) == ConnectError::NotConnected);
    CHECK (g.depthInto (kResPitchIn) == 1.0f);                                                                           // no cable: +1 by convention
    CHECK (g.connect (kRadioLevelOut, kResDampIn) == ConnectError::FanIn);                                               // existing rules unchanged
    const auto p = RoutingPlan::fromGraph (g);
    CHECK (p.sourceOf (kResDampIn) == kRoomLevelOut && p.depthOf (kResDampIn) == -1.0f && p.depthOf (kResAudioIn) == 1.0f && p.sourceOf (kResPitchIn) == -1);
}

void zeroCoupleAndAbsentSources()   // AT-03, AT-06, AT-07
{
    const Graph audio = wire<Graph> (kAudio);
    const auto none = render (plan (audio), loud()).out;
    currentTest = "AT-03 zero is neutral";
    for (int in = kResPitchIn; in < kNumPorts; ++in)
        for (int from : { kRoomLevelOut, kRoomActivityOut, kRadioLevelOut })
            CHECK (same (none, render (plan (with (audio, from, in, 0.0f)), loud()).out));

    currentTest = "AT-06 COUPLE is the master";
    auto off = loud();
    off.p[kCouple] = 0.0f;
    Graph busy = Graph::defaultPatch();
    const float d[] = { -1.0f, 0.5f, -0.3f, 1.0f, -0.8f, 0.2f };
    for (int in = kResPitchIn; in < kNumPorts; ++in) busy.setDepth (busy.cableInto (in), in, d[in - kResPitchIn]);
    CHECK (same (render (plan (audio), off).out, render (plan (busy), off).out));

    currentTest = "AT-07 unavailable source stays neutral";
    auto silentRoom = loud();
    silentRoom.room = {};                                                           // ROOM off, denied or stale reads as zero
    const auto quiet = render (plan (audio), silentRoom).out;
    for (float depthValue : { -1.0f, -0.4f, 0.0f, 0.4f, 1.0f })
    {
        Graph g = audio;
        g.connect (kRoomLevelOut, kResFeedbackIn, depthValue); g.connect (kRoomLevelOut, kResDampIn, depthValue); g.connect (kRoomActivityOut, kResExciteIn, depthValue);
        g.connect (kRoomActivityOut, kResPitchIn, depthValue); g.connect (kRoomBrightnessOut, kResShapeIn, depthValue); g.connect (kRoomLevelOut, kResDriftIn, depthValue);
        CHECK (same (quiet, render (plan (g), silentRoom).out));                    // all six destinations, STRIKE included
    }
}

/** STRIKE correction: docs/p0/STRIKE-NEUTRAL.md. The frozen P0 engine is the reference for what changed and what did not. */
void strikeNeutral()
{
    namespace base = wildband_baseline;
    const double fs = 48000.0;
    const Graph audio = wire<Graph> (kAudio);

    currentTest = "ST-01 a silent source in STRIKE has no influence";
    auto silentRoom = loud();
    silentRoom.room = {};
    const auto none = render (plan (audio), silentRoom).out;
    for (int from : { kRoomLevelOut, kRoomActivityOut, kRoomBrightnessOut })
        for (float d : { -1.0f, -0.5f, 0.0f, 0.3f, 1.0f })
            CHECK (same (none, render (plan (with (audio, from, kResExciteIn, d)), silentRoom).out));
    {
        // ...where P0 dimmed the sound: the defect this corrects, shown on the frozen engine
        const size_t n = (size_t) fs * 2;
        auto radio = testSource (fs, n);
        base::Controls b;
        for (int i = 0; i < base::kNumParams; ++i) b.p[i] = silentRoom.p[i];
        b.p[kMix] = 1.0f;                                                 // resonators only, so the dimming is not masked by the dry signal
        base::Graph cabled, bare;
        bare.connect (kRadioAudioOut, kResAudioIn);
        cabled.connect (kRadioAudioOut, kResAudioIn);
        cabled.connect (kRoomActivityOut, kResExciteIn);
        Buf l1 (n), r1 (n), l2 (n), r2 (n);
        base::Engine e1, e2;
        e1.prepare (fs); e2.prepare (fs);
        b.plan = base::RoutingPlan::fromGraph (bare);
        e1.process (radio.data(), radio.data(), radio.data(), l1.data(), r1.data(), (int) n, b);
        b.plan = base::RoutingPlan::fromGraph (cabled);
        e2.process (radio.data(), radio.data(), radio.data(), l2.data(), r2.data(), (int) n, b);
        CHECK (rms (l2) < 0.5f * rms (l1));
    }

    currentTest = "ST-02 an active source in STRIKE behaves as in P0";
    {
        // a source that is active from the start: sample-identical to the frozen engine at +100 %
        const size_t n = (size_t) fs * 2;
        const auto radio = testSource (fs, n), local = noise (n, 0.2f, 5);
        Controls c = loud();
        base::Controls b;
        for (int i = 0; i < base::kNumParams; ++i) b.p[i] = c.p[i];
        b.room = { 1.0f, 1.0f, 1.0f };
        c.plan = plan (with (audio, kRoomActivityOut, kResExciteIn, 1.0f));
        base::Graph g;
        g.connect (kRadioAudioOut, kResAudioIn);
        g.connect (kRoomActivityOut, kResExciteIn);
        b.plan = base::RoutingPlan::fromGraph (g);
        Engine now;
        base::Engine then;
        now.prepare (fs); then.prepare (fs);
        Buf l1 (n), r1 (n), l2 (n), r2 (n);
        now.process (radio.data(), radio.data(), local.data(), l1.data(), r1.data(), (int) n, c);
        then.process (radio.data(), radio.data(), local.data(), l2.data(), r2.data(), (int) n, b);
        CHECK (same (l1, l2) && same (r1, r2));
    }

    currentTest = "ST-03 the influence ends when the source stops, and returns when it resumes";
    {
        // room hits for 2 s, silence for 16 s, hits again for 2 s; compared with no cable over the same input
        Engine cabled, bare;
        cabled.prepare (fs); bare.prepare (fs);
        auto c = loud();
        c.p[kMix] = 1.0f;
        const auto radio = noise ((size_t) fs * 20, 0.2f);
        Buf l (4800), r (4800), l0 (4800), r0 (4800);
        std::array<float, 3> ratio {};                                    // cabled / bare RMS: during hits, late in the silence, after resuming
        float held = 0;                                                   // ...and 3 s into the silence
        for (int block = 0; block < 200; ++block)
        {
            const double t = block * 0.1;
            c.room = (t < 2.0 || t >= 18.0) ? EnvFrame { 0.0f, 1.0f, 0.0f } : EnvFrame {};
            c.plan = plan (with (audio, kRoomActivityOut, kResExciteIn, 1.0f));
            cabled.process (radio.data() + block * 4800, radio.data() + block * 4800, radio.data(), l.data(), r.data(), 4800, c);
            c.plan = plan (audio);
            bare.process (radio.data() + block * 4800, radio.data() + block * 4800, radio.data(), l0.data(), r0.data(), 4800, c);
            if (block == 15) ratio[0] = rms (l) / rms (l0);
            if (block == 50) held = rms (l) / rms (l0);
            if (block == 175) ratio[1] = rms (l) / rms (l0);
            if (block == 195) ratio[2] = rms (l) / rms (l0);
        }
        CHECK (ratio[0] > 2.0f);                                          // hits strike: much louder than the bare patch
        CHECK (held < 0.5f);                                              // 3 s after the last hit the back-off is still fully held (dimmer than bare)
        CHECK (std::abs (ratio[1] - 1.0f) < 0.01f);                       // 15.5 s into the silence: within 1 % of no cable
        CHECK (ratio[2] > 2.0f);                                          // and back
    }

    currentTest = "ST-04 the default patch differs from P0 only modestly";
    {
        const size_t n = (size_t) fs * 8;
        const auto radio = testSource (fs, n), local = noise (n, 0.2f, 5);
        Controls c = defaults();
        base::Controls b;
        for (int i = 0; i < base::kNumParams; ++i) b.p[i] = c.p[i];
        Engine now;
        base::Engine then;
        now.prepare (fs); then.prepare (fs);
        Buf l1 (n), r1 (n), l2 (n), r2 (n);
        now.process (radio.data(), radio.data(), local.data(), l1.data(), r1.data(), (int) n, c);
        then.process (radio.data(), radio.data(), local.data(), l2.data(), r2.data(), (int) n, b);
        Buf d (n);
        for (size_t i = 0; i < n; ++i) d[i] = l1[i] - l2[i];
        strikeDefaultDifferenceDb = 20.0f * std::log10 (rms (d) / rms (l2) + 1.0e-9f);
        strikeDefaultLevelDb = 20.0f * std::log10 (rms (l1) / rms (l2));
        CHECK (! same (l1, l2));                                          // the intended deviation exists...
        CHECK (std::abs (strikeDefaultLevelDb) < 3.0f);                   // ...and the overall level moves by less than 3 dB on this source
    }
}

void signAndDirection()   // AT-04
{
    currentTest = "AT-04 sign and direction";
    const Graph audio = wire<Graph> (kAudio);
    auto three = [&] (int from, int to, Controls c, double seconds = 2.0, double inputSeconds = 1.0e9)
    { return std::array<Run, 3> { render (plan (with (audio, from, to, -1.0f)), c, 48000.0, seconds, inputSeconds), render (plan (with (audio, from, to, 0.0f)), c, 48000.0, seconds, inputSeconds),
                                  render (plan (with (audio, from, to, 1.0f)), c, 48000.0, seconds, inputSeconds) }; };
    auto ordered = [] (float minus, float zero, float plus) { CHECK (minus < zero); CHECK (zero < plus); };
    auto distinct = [] (const std::array<Run, 3>& r) { CHECK (! same (r[0].out, r[1].out)); CHECK (! same (r[2].out, r[1].out)); CHECK (! same (r[0].out, r[2].out)); };
    auto hf = [] (const Buf& y) { double s = 0; for (size_t i = 4; i < y.size(); i += 2) s += (double) (y[i] - y[i - 2]) * (y[i] - y[i - 2]); return (float) std::sqrt (s / (double) y.size()); };
    auto travel = [] (const std::vector<float>& f) { double s = 0; for (size_t i = 1; i < f.size(); ++i) s += std::abs (f[i] - f[i - 1]); return (float) s; };

    {   // PITCH: the resonator frequency ends above / below neutral by about a semitone
        const auto r = three (kRoomLevelOut, kResPitchIn, loud());
        distinct (r);
        ordered (r[0].f0.back(), r[1].f0.back(), r[2].f0.back());
        CHECK (std::abs (1200.0f * std::log2 (r[2].f0.back() / r[1].f0.back()) - 100.0f) < 2.0f);
        CHECK (std::abs (1200.0f * std::log2 (r[0].f0.back() / r[1].f0.back()) + 100.0f) < 2.0f);
    }
    {   // SHAPE: the second resonator's ratio moves up / down
        auto c = loud();
        c.p[kShape] = 0.5f;
        const auto r = three (kRoomBrightnessOut, kResShapeIn, c);
        distinct (r);
        ordered (r[0].f1.back(), r[1].f1.back(), r[2].f1.back());
    }
    {   // TONE: brighter / darker ringing
        auto c = loud();
        c.p[kTone] = 0.5f; c.p[kMix] = 1.0f;
        const auto r = three (kRoomLevelOut, kResDampIn, c);
        distinct (r);
        ordered (hf (r[0].out), hf (r[1].out), hf (r[2].out));
    }
    {   // STRIKE: a constant "hit" signal drives the excitation up (+) or ducks it to nothing (-)
        auto c = loud();
        c.p[kMix] = 1.0f; c.p[kFeedback] = 0.3f;
        const auto r = three (kRoomActivityOut, kResExciteIn, c);
        distinct (r);
        ordered (rms (r[0].out, r[0].out.size() / 2), rms (r[1].out, r[1].out.size() / 2), rms (r[2].out, r[2].out.size() / 2));
        CHECK (rms (r[0].out, r[0].out.size() / 2) < 0.05f * rms (r[1].out, r[1].out.size() / 2));   // ducked almost to silence, never inverted
    }
    {   // DRIFT: the pitch wanders further per second (+) or less (-)
        auto c = loud();
        c.p[kDrift] = 0.5f;
        const auto r = three (kRoomLevelOut, kResDriftIn, c, 6.0);
        distinct (r);
        ordered (travel (r[0].f0), travel (r[1].f0), travel (r[2].f0));
    }
    {   // FEEDBACK: the tail after the input stops is longer (+) or shorter (-)
        auto c = loud();
        c.p[kFeedback] = 0.6f; c.p[kMix] = 1.0f;
        const auto r = three (kRoomLevelOut, kResFeedbackIn, c, 3.0, 1.0);
        distinct (r);
        const size_t from = (size_t) (48000 * 2 * 2);                                 // the last second
        ordered (rms (r[0].out, from), rms (r[1].out, from), rms (r[2].out, from));
    }
}

void fanOut()   // AT-05
{
    currentTest = "AT-05 independent fan-out";
    const Graph audio = wire<Graph> (kAudio);
    const auto one = render (plan (with (audio, kRoomLevelOut, kResFeedbackIn, 0.6f)), loud());
    CHECK (same (one.out, render (plan (with (with (audio, kRoomLevelOut, kResFeedbackIn, 0.6f), kRoomLevelOut, kResDampIn, 0.0f)), loud()).out));
    // same source into PITCH (fixed depth) and TONE (varied): the pitch does not care what the TONE cable's depth is
    const auto a = render (plan (with (with (audio, kRoomLevelOut, kResPitchIn, 0.7f), kRoomLevelOut, kResDampIn, 1.0f)), loud());
    const auto b = render (plan (with (with (audio, kRoomLevelOut, kResPitchIn, 0.7f), kRoomLevelOut, kResDampIn, -1.0f)), loud());
    CHECK (a.f0 == b.f0);
    CHECK (! same (a.out, b.out));
    const auto c = render (plan (with (with (audio, kRoomLevelOut, kResPitchIn, -0.7f), kRoomLevelOut, kResDampIn, 1.0f)), loud());
    CHECK (a.f0 != c.f0);
}

void clickFree()   // AT-09
{
    currentTest = "AT-09 click-free depth changes";
    Engine e;
    e.prepare (48000.0);
    auto c = loud();
    c.p[kTone] = 0.2f; c.p[kPitch] = 0.1f; c.p[kCouple] = 0.8f; c.room = { 0.9f, 0.0f, 0.5f };
    Buf tone (48000 * 2);
    for (size_t i = 0; i < tone.size(); ++i) tone[i] = 0.3f * std::sin (2.0f * kPi * 100.0f * (float) i / 48000.0f);
    Buf y;
    for (int seg = 0; seg < 80; ++seg)
    {
        Graph g = Graph::defaultPatch();
        for (int in = kResPitchIn; in < kNumPorts; ++in) g.setDepth (g.cableInto (in), in, (seg + in) % 2 ? 1.0f : -1.0f);   // every depth flips end to end
        c.plan = RoutingPlan::fromGraph (g);
        Buf l (1200), r (1200);
        e.process (tone.data() + seg * 1200, tone.data() + seg * 1200, tone.data() + seg * 1200, l.data(), r.data(), 1200, c);
        y.insert (y.end(), l.begin(), l.end());
    }
    float step = 0;
    for (size_t i = 1; i < y.size(); ++i) step = std::max (step, std::abs (y[i] - y[i - 1]));
    CHECK (step < 0.03f);
    CHECK (rms (y) > 1.0e-3f);
}

void coherentHandOff()   // AT-11
{
    currentTest = "AT-11 coherent hand-off";
    PlanMailbox mail;
    // every field of plan k is derived from the one value k % 200, and the source/depth pairing differs per input, so
    // a plan assembled from two different publications cannot satisfy the check below
    auto make = [] (int k)
    {
        RoutingPlan p;
        const int v = k % 200;
        for (int i = 0; i < kNumInputs; ++i) { p.source[i] = (int8_t) ((v + i) % 100); p.depth[i] = (float) (v + 3 * i) / 250.0f - 0.5f; }
        return p;
    };
    auto whole = [] (const RoutingPlan& p)
    {
        const int low = p.source[0];                                         // v % 100: v is low or low + 100
        for (int v : { low, low + 100 })
        {
            bool ok = true;
            for (int i = 0; i < kNumInputs; ++i) ok = ok && p.source[i] == (int8_t) ((v + i) % 100) && p.depth[i] == (float) (v + 3 * i) / 250.0f - 0.5f;
            if (ok) return true;
        }
        return false;
    };
    const int total = 400000;
    std::atomic<bool> done { false };
    std::atomic<int> torn { 0 };
    std::atomic<long long> reads { 0 };
    mail.publish (make (0));
    std::thread reader ([&]
    {
        while (! done.load())
        {
            const RoutingPlan p = mail.read();
            if (! whole (p)) torn.fetch_add (1);                              // sources of one plan with depths of another, or a half-written slot
            reads.fetch_add (1);
        }
    });
    for (int k = 1; k <= total; ++k) mail.publish (make (k));
    done = true;
    reader.join();
    CHECK (torn.load() == 0);
    CHECK (reads.load() > 1000);
    CHECK (! whole ([&] { auto p = make (5); p.depth[3] = make (6).depth[3]; return p; }()));   // the check itself rejects a mixture
    const RoutingPlan last = mail.read(), want = make (total);                // the reader always ends on the newest plan
    CHECK (std::memcmp (last.source, want.source, sizeof (want.source)) == 0 && std::memcmp (last.depth, want.depth, sizeof (want.depth)) == 0);
}

void panic()   // AT-15
{
    currentTest = "AT-15 PANIC";
    auto c = loud();
    for (auto& p : c.p) p = 1.0f;
    Graph g = Graph::defaultPatch();
    for (int in = kResPitchIn; in < kNumPorts; ++in) g.setDepth (g.cableInto (in), in, -1.0f);
    c.plan = RoutingPlan::fromGraph (g);
    Engine e;
    e.prepare (48000.0);
    const auto loudIn = noise (48000, 1.0f);
    Buf l (48000), r (48000);
    e.process (loudIn.data(), loudIn.data(), loudIn.data(), l.data(), r.data(), 48000, c);
    CHECK (peak (l) > 0.0f && peak (l) <= Engine::kCeiling);
    e.reset();
    const Buf silence (48000, 0.0f);
    e.process (silence.data(), silence.data(), silence.data(), l.data(), r.data(), 48000, c);
    CHECK (peak (l) == 0.0f && peak (r) == 0.0f);
}

void all()
{
    legacyParity();
    persistence();
    graphRules();
    zeroCoupleAndAbsentSources();
    strikeNeutral();
    signAndDirection();
    fanOut();
    clickFree();
    coherentHandOff();
    panic();
}
} // namespace depth


// =====================================================================================================================
// Playable tuner acceptance tests: docs/p0/TUNER-ACCEPTANCE.md
namespace tuner
{
float staticDb = 0;   // CT-12, reported by --metrics

/** CT-01: the off-centre TUNE sound the owner singled out must not change. Reference: the frozen P0 engine. */
void offCentreRegression()
{
    currentTest = "CT-01 off-centre regression";
    namespace base = wildband_baseline;
    int compared = 0;
    for (double fs : { 44100.0, 48000.0 })
        for (float offCentre : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
            for (int mode = 0; mode < 3; ++mode)
            {
                Controls c = defaults();
                base::Controls b;
                for (int i = 0; i < base::kNumParams; ++i) b.p[i] = c.p[i];
                c.detune = b.detune = offCentre;
                c.mode = (Mode) mode;
                b.mode = (base::Mode) mode;
                Graph g;                                                  // no cable in STRIKE: that destination has its own tests
                base::Graph bg;
                for (auto cable : { std::pair<int, int> { kRadioAudioOut, kResAudioIn }, { kRadioLevelOut, kResDampIn }, { kRadioLevelOut, kResDriftIn } }) { g.connect (cable.first, cable.second); bg.connect (cable.first, cable.second); }
                c.plan = RoutingPlan::fromGraph (g);
                b.plan = base::RoutingPlan::fromGraph (bg);
                const size_t n = (size_t) fs * 2;
                const auto radio = testSource (fs, n), hiss = noise (n, 0.05f), local = noise (n, 0.2f, 5);
                Buf in (n);
                for (size_t i = 0; i < n; ++i) in[i] = radio[i] + hiss[i];
                Engine now;
                base::Engine then;
                now.prepare (fs);
                then.prepare (fs);
                Buf l1 (n), r1 (n), l2 (n), r2 (n);
                for (size_t at = 0; at + 256 <= n; at += 256)
                {
                    now.process (in.data() + at, in.data() + at, local.data() + at, l1.data() + at, r1.data() + at, 256, c);
                    then.process (in.data() + at, in.data() + at, local.data() + at, l2.data() + at, r2.data() + at, 256, b);
                }
                CHECK (depth::same (l1, l2) && depth::same (r1, r2));
                if (offCentre > 0.0f && mode == 1) CHECK (rms (l1) > 1.0e-3f);   // and it is doing something
                ++compared;
            }
    CHECK (compared == 30);
}

Buf mono (Engine& e, const Buf& radio, const Controls& c)
{
    Buf l (radio.size()), r (radio.size());
    const Buf local (radio.size(), 0.0f);
    for (size_t at = 0; at + 64 <= radio.size(); at += 64) e.process (radio.data() + at, radio.data() + at, local.data() + at, l.data() + at, r.data() + at, 64, c);
    return l;
}
Buf part (const Buf& x, double fromSeconds, double toSeconds, double fs = 48000.0) { return Buf (x.begin() + (long) (fromSeconds * fs), x.begin() + (long) (toSeconds * fs)); }

void staticOffIsTheOldBehaviour()   // CT-02
{
    currentTest = "CT-02 STATIC 0 is the old behaviour";
    const auto radio = testSource (48000.0, 48000 * 2);
    auto c = defaults();
    c.p[kStatic] = 0.0f;
    Engine a, b;
    a.prepare (48000.0); b.prepare (48000.0);
    const auto none = mono (a, radio, c);
    c.gap = 1.0f;
    CHECK (depth::same (none, mono (b, radio, c)));
    CHECK (b.meterStatic.load() == 0.0f);
}

void levelAndShape()   // CT-12
{
    currentTest = "CT-12 level and shape";
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        StaticSource s;
        s.prepare (fs);
        Buf x ((size_t) fs * 10);
        for (auto& v : x) v = s.next();
        double mean = 0;
        for (auto v : x) mean += v;
        const float db = 20.0f * std::log10 (paramDefaults[kStatic] * rms (x));
        CHECK (db > -30.0f && db < -14.0f);
        CHECK (peak (x) <= StaticSource::kPeak);
        CHECK (std::abs (mean / (double) x.size()) < 0.001);
        if (fs == 48000.0) staticDb = db;
    }
    StaticSource a, b;                                                             // deterministic
    a.prepare (48000.0); b.prepare (48000.0);
    bool identical = true;
    for (int i = 0; i < 10000; ++i) identical = identical && a.next() == b.next();
    CHECK (identical);
}

void timeBound()   // CT-13
{
    currentTest = "CT-13 time bound";
    auto c = defaults();
    c.p[kStatic] = 1.0f; c.mode = Mode::Dry;                                        // DRY shows the engine's input, static included
    Engine e;
    e.prepare (48000.0);
    const Buf silence (48000 * 12, 0.0f);
    c.gap = 1.0f;
    const auto y = mono (e, silence, c);
    const float early = rms (part (y, 1.0, 3.5)), late = rms (part (y, 3.5, 3.9)), fading = rms (part (y, 5.5, 6.5));
    CHECK (early > 1.0e-3f);
    CHECK (std::abs (late / early - 1.0f) < 0.25f);                                 // still at full strength just before 4 s
    CHECK (fading < 0.8f * early && fading > 0.0f);                                 // fading between 4 and 8 s
    CHECK (peak (part (y, 8.2, 12.0)) == 0.0f);                                     // silent from 8 s
    const auto more = mono (e, Buf (48000 * 2, 0.0f), c);                           // the gap is still open: it does not start again
    CHECK (peak (more) == 0.0f);
    c.gap = 0.0f;
    mono (e, Buf (4800, 0.0f), c);                                                  // the gap closes...
    c.gap = 1.0f;
    CHECK (rms (part (mono (e, Buf (48000, 0.0f), c), 0.2, 1.0)) > 1.0e-3f);        // ...and the next retune has static again
}

void hoppingOnward()   // CT-16
{
    currentTest = "CT-16 a new request gets fresh static";
    auto c = defaults();
    c.p[kStatic] = 1.0f; c.mode = Mode::Dry; c.gap = 1.0f; c.request = 7;
    Engine e;
    e.prepare (48000.0);
    const auto first = mono (e, Buf (48000 * 9, 0.0f), c);                          // a station that never arrives: 9 s with the gap open
    const float full = rms (part (first, 1.0, 3.0));
    CHECK (full > 1.0e-3f && peak (part (first, 8.2, 9.0)) == 0.0f);                // its static has run out
    c.request = 8;                                                                  // NEXT: the gap never closed in between
    const auto next = mono (e, Buf (48000 * 3, 0.0f), c);
    CHECK (std::abs (rms (part (next, 1.0, 3.0)) / full - 1.0f) < 0.25f);           // full static for the new request
    e.panic();
    CHECK (peak (mono (e, Buf (48000, 0.0f), c)) == 0.0f);                          // PANIC still silences this request...
    c.request = 9;
    CHECK (rms (part (mono (e, Buf (48000, 0.0f), c), 0.2, 1.0)) > 1.0e-3f);        // ...and the next one sounds
}

void throughTheEngine()   // CT-14
{
    currentTest = "CT-14 static goes through the engine";
    auto c = defaults();
    c.p[kStatic] = 1.0f; c.p[kMix] = 1.0f; c.p[kFeedback] = 0.7f; c.gap = 1.0f;
    const Buf silence (48000 * 2, 0.0f);
    Engine e;
    e.prepare (48000.0);
    const auto rung = mono (e, silence, c);
    CHECK (rms (part (rung, 0.5, 2.0)) > 1.0e-3f);                                  // MIX fully right: this is resonator output only
    CHECK (e.meterRadioLevel.load() > 0.2f);                                        // the radio LEVEL output responds to it
    Engine bare;
    bare.prepare (48000.0);
    auto pulled = c;
    pulled.plan = RoutingPlan::fromGraph (Graph());                                 // no audio cable: static cannot reach the resonators
    CHECK (peak (mono (bare, silence, pulled)) == 0.0f);
    Engine detuned;                                                                 // and the off-centre ring modulator acts on it
    detuned.prepare (48000.0);
    auto off = c;
    off.detune = 1.0f;
    CHECK (! depth::same (rung, mono (detuned, silence, off)));
}

void tails()   // CT-09
{
    currentTest = "CT-09 tails survive a retune";
    const double fs = 48000.0;
    Buf radio = noise ((size_t) fs * 4, 0.3f);
    std::fill (radio.begin() + (long) fs, radio.begin() + (long) (fs * 2.5), 0.0f);  // the station is gone from 1.0 s to 2.5 s
    auto c = defaults();
    c.p[kFeedback] = 0.68f; c.p[kMix] = 1.0f; c.p[kStatic] = 0.0f;
    Engine plain, retuned;
    plain.prepare (fs); retuned.prepare (fs);
    const auto reference = mono (plain, radio, c);                                   // the same samples with no retune signalled
    Buf out;
    for (int block = 0; block < 40; ++block)                                         // 0.1 s blocks; the gap is open while the station is gone
    {
        c.gap = block >= 10 && block < 25 ? 1.0f : 0.0f;
        const auto y = mono (retuned, Buf (radio.begin() + block * 4800, radio.begin() + (block + 1) * 4800), c);
        out.insert (out.end(), y.begin(), y.end());
    }
    CHECK (depth::same (reference, out));                                            // STATIC 0: a retune touches nothing
    CHECK (rms (part (out, 1.05, 1.5)) > 1.0e-4f);                                   // and the tail is audibly there after the station has gone

    c.p[kStatic] = 0.6f;                                                             // with static: the tail carries on underneath, no step
    Engine withStatic;
    withStatic.prepare (fs);
    Buf out2;
    for (int block = 0; block < 40; ++block)
    {
        c.gap = block >= 10 && block < 25 ? 1.0f : 0.0f;
        const auto y = mono (withStatic, Buf (radio.begin() + block * 4800, radio.begin() + (block + 1) * 4800), c);
        out2.insert (out2.end(), y.begin(), y.end());
    }
    CHECK (depth::same (part (reference, 0.0, 1.0), part (out2, 0.0, 1.0)));         // identical until the gap opens
    float step = 0;
    for (size_t i = 48000 - 200; i < 48000 + 2400; ++i) step = std::max (step, std::abs (out2[i] - out2[i - 1]));
    float before = 0;
    for (size_t i = 48000 - 2400; i < 48000 - 200; ++i) before = std::max (before, std::abs (reference[i] - reference[i - 1]));
    CHECK (step < before + 0.05f);                                                   // no click where static comes in
    CHECK (rms (part (out2, 1.5, 2.4)) > rms (part (reference, 1.5, 2.4)));          // static keeps the resonators excited through the wait
}

void panicDuringStatic()   // CT-10
{
    currentTest = "CT-10 PANIC during static";
    auto c = defaults();
    c.p[kStatic] = 1.0f; c.p[kFeedback] = 1.0f; c.p[kLevel] = 1.0f; c.gap = 1.0f;
    Engine e;
    e.prepare (48000.0);
    const Buf silence (48000, 0.0f);
    CHECK (peak (mono (e, silence, c)) > 0.0f);
    e.panic();
    CHECK (peak (mono (e, silence, c)) == 0.0f);                                    // exactly silent for the rest of this retune
    CHECK (peak (mono (e, silence, c)) == 0.0f);
    c.gap = 0.0f;
    const auto live = noise (48000, 0.3f);
    CHECK (peak (mono (e, live, c)) > 0.0f);                                        // the station arrives and plays
    c.gap = 1.0f;
    CHECK (peak (mono (e, silence, c)) > 0.0f);                                     // and the next retune has static again
}

void all()
{
    offCentreRegression();
    staticOffIsTheOldBehaviour();
    levelAndShape();
    timeBound();
    hoppingOnward();
    throughTheEngine();
    tails();
    panicDuringStatic();
}
} // namespace tuner


// =====================================================================================================================
// STRIKE response candidates: docs/p0/STRIKE-RESPONSE.md
namespace strike
{
const char* const names[3] = { "A Held (existing)", "B Fast", "C Organic" };
struct Curve { float swell90 = -1, t90 = -1, t50 = -1, t10 = -1, t1 = -1; };
Curve curves[3];
float rhythmMin[3] = {}, rhythmMax[3] = {};

/** Back-off (0..1) over time for a source that is `on (t)`, sampled every 10 ms. COUPLE 1, depth +1. */
template <typename On> std::vector<float> backoff (int response, double seconds, On on)
{
    Engine e;
    e.prepare (48000.0);
    auto c = depth::loud();
    c.strikeResponse = response;
    c.plan = depth::plan (depth::with (depth::wire<Graph> (depth::kAudio), kRoomActivityOut, kResExciteIn, 1.0f));
    const Buf in (480, 0.0f);
    Buf l (480), r (480);
    std::vector<float> out;
    for (int block = 0; block < (int) (seconds * 100); ++block)
    {
        c.room = { 0.0f, on (block * 0.01) ? 1.0f : 0.0f, 0.0f };
        e.process (in.data(), in.data(), in.data(), l.data(), r.data(), 480, c);
        out.push_back (e.strikeBackoff());
    }
    return out;
}

void all()
{
    currentTest = "SR-02 every response is neutral for a silent source";
    {
        auto silentRoom = depth::loud();
        silentRoom.room = {};
        const Graph audio = depth::wire<Graph> (depth::kAudio);
        const auto none = depth::render (depth::plan (audio), silentRoom).out;
        for (int response = 0; response < 3; ++response)
            for (float d : { -1.0f, 0.0f, 0.4f, 1.0f })
            {
                auto c = silentRoom;
                c.strikeResponse = response;
                CHECK (depth::same (none, depth::render (depth::plan (depth::with (audio, kRoomActivityOut, kResExciteIn, d)), c).out));
            }
    }

    // one second of activity, then silence (a second, so that the slow-rising Organic response gets all the way in)
    for (int response = 0; response < 3; ++response)
    {
        const auto b = backoff (response, 17.0, [] (double t) { return t < 1.0; });
        auto& k = curves[response];
        for (size_t i = 0; i < b.size(); ++i)
        {
            const float t = (float) i * 0.01f;
            if (k.swell90 < 0 && b[i] >= 0.9f) k.swell90 = t;
            if (t > 1.0f)
            {
                if (k.t90 < 0 && b[i] < 0.9f) k.t90 = t - 1.0f;
                if (k.t50 < 0 && b[i] < 0.5f) k.t50 = t - 1.0f;
                if (k.t10 < 0 && b[i] < 0.1f) k.t10 = t - 1.0f;
                if (k.t1 < 0 && b[i] < 0.01f) k.t1 = t - 1.0f;
            }
        }
    }
    currentTest = "SR-03 Fast is fast";
    CHECK (curves[1].t10 > 0 && curves[1].t10 < 1.5f);
    CHECK (curves[1].t1 > 0 && curves[1].t1 < 2.5f);
    currentTest = "SR-04 Organic is gradual";
    {
        const auto swell = backoff (2, 3.0, [] (double) { return true; });            // a source that stays on: how fast does it come in?
        float t90 = -1;
        for (size_t i = 0; i < swell.size(); ++i) if (t90 < 0 && swell[i] >= 0.9f) t90 = (float) i * 0.01f;
        CHECK (t90 > 0.3f);
        const auto b = backoff (2, 16.0, [] (double t) { return t < 1.0; });          // after a second of activity
        CHECK (b[300] > 0.5f);                                                        // still more than half 2 s later
        CHECK (b[1300] < 0.1f);                                                       // below 10 % by 12 s
    }
    currentTest = "SR baseline figures";                                              // the reported 4.6 s / 13.8 s, from the code
    CHECK (std::abs (curves[0].t90 - 4.6f) < 0.3f);
    CHECK (curves[0].t1 > 12.5f && curves[0].t1 < 14.5f);

    currentTest = "SR-05 rhythm";
    for (int response = 0; response < 3; ++response)
    {
        const auto b = backoff (response, 12.0, [] (double t) { return std::fmod (t, 0.5) < 0.05; });   // a 50 ms hit every 0.5 s
        rhythmMin[response] = 1.0f; rhythmMax[response] = 0.0f;
        for (size_t i = 800; i < b.size(); ++i) { rhythmMin[response] = std::min (rhythmMin[response], b[i]); rhythmMax[response] = std::max (rhythmMax[response], b[i]); }
    }
    CHECK (rhythmMin[0] > 0.95f);                                                     // Held: no breathing
    CHECK (rhythmMin[1] < 0.70f);                                                     // Fast: breathes between hits
    CHECK (rhythmMin[2] > rhythmMin[1] && rhythmMax[2] - rhythmMin[2] < 0.15f);       // Organic: steadier, small ripple

    currentTest = "SR-06 switching response is click-free";
    {
        Engine e;
        e.prepare (48000.0);
        auto c = depth::loud();
        c.p[kTone] = 0.2f; c.p[kPitch] = 0.1f; c.p[kCouple] = 0.8f; c.room = { 0.9f, 0.6f, 0.5f };
        Buf tone (48000 * 2);
        for (size_t i = 0; i < tone.size(); ++i) tone[i] = 0.3f * std::sin (2.0f * kPi * 100.0f * (float) i / 48000.0f);
        Buf y;
        for (int seg = 0; seg < 80; ++seg)
        {
            c.strikeResponse = seg % 3;
            c.room.activity = seg % 7 < 3 ? 0.8f : 0.0f;
            Buf l (1200), r (1200);
            e.process (tone.data() + seg * 1200, tone.data() + seg * 1200, tone.data() + seg * 1200, l.data(), r.data(), 1200, c);
            y.insert (y.end(), l.begin(), l.end());
        }
        float step = 0;
        for (size_t i = 1; i < y.size(); ++i) step = std::max (step, std::abs (y[i] - y[i - 1]));
        CHECK (step < 0.03f);
    }
}

void report()
{
    std::printf ("\nSTRIKE back-off after one second of activity (COUPLE 1, depth +100 %%), seconds after it ends\n%-20s %10s %8s %8s %8s %8s   %s\n",
                 "response", "rise to 90%", "<90%", "<50%", "<10%", "<1%", "hit every 0.5 s: back-off range");
    for (int i = 0; i < 3; ++i)
        std::printf ("%-20s %10.2f %8.2f %8.2f %8.2f %8.2f   %.0f %% .. %.0f %%\n", names[i], (double) curves[i].swell90, (double) curves[i].t90, (double) curves[i].t50, (double) curves[i].t10,
                     (double) curves[i].t1, (double) rhythmMin[i] * 100.0, (double) rhythmMax[i] * 100.0);
}
} // namespace strike


// =====================================================================================================================
// Voice experiment D2, Dissolve: docs/p0/VOICE-RESEARCH.md
namespace voice
{
void all()
{
    const double fs = 48000.0;
    currentTest = "DV-01 silence in, silence out; deterministic";
    {
        Dissolve a, b;
        a.prepare (fs); b.prepare (fs);
        Dissolve::Params p;
        p.dissolve = 1.0f; p.spray = 1.0f; p.regen = 0.95f;
        float l, r, peakOut = 0;
        for (int i = 0; i < 48000; ++i) { a.process (0.0f, 0.0f, l, r, p); peakOut = std::max (peakOut, std::max (std::abs (l), std::abs (r))); }
        CHECK (peakOut == 0.0f);
        const auto x = noise (48000 * 2, 0.4f);
        bool identical = true;
        for (auto v : x) { float l2, r2; a.process (v, -v, l, r, p); b.process (0.0f, 0.0f, l2, r2, p); (void) l2; (void) r2; }
        a.reset(); b.reset();
        for (auto v : x) { float l2, r2; a.process (v, -v, l, r, p); b.process (v, -v, l2, r2, p); identical = identical && l == l2 && r == r2; }
        CHECK (identical);
    }
    currentTest = "DV-02 with nothing dissolved it is a plain delay";
    {
        Dissolve d;
        d.prepare (fs);
        Dissolve::Params p;
        p.time = 0.25f; p.dissolve = 0.0f; p.spray = 0.0f; p.regen = 0.0f;
        int at = -1, count = 0;
        float l, r;
        for (int i = 0; i < 48000; ++i)
        {
            d.process (i == 100 ? 1.0f : 0.0f, 0.0f, l, r, p);
            if (std::abs (l) > 0.5f) { ++count; if (at < 0) at = i; }
        }
        CHECK (count >= 1 && count <= 2);                                             // one echo (an interpolated tap may straddle two samples)
        CHECK (std::abs (at - (100 + (int) (0.25 * fs))) <= 1);
    }
    currentTest = "DV-03 bounded at maximum regeneration; no allocation";
    {
        Dissolve d;
        d.prepare (fs);
        Dissolve::Params p;
        p.dissolve = 0.6f; p.spray = 1.0f; p.regen = 5.0f; p.damp = 1.0f; p.drive = 3.0f;   // regeneration asked far beyond its limit
        const auto x = noise (48000 * 6, 1.0f);
        float l, r, peakOut = 0;
        bool finite = true;
        watched = pthread_self();
        allocs = 0;
        malloc_logger = countAllocs;
        for (auto v : x) { d.process (v, v, l, r, p); peakOut = std::max (peakOut, std::max (std::abs (l), std::abs (r))); finite = finite && std::isfinite (l) && std::isfinite (r); }
        malloc_logger = nullptr;
        CHECK (finite && peakOut < 16.0f);                                            // the engine's own clip follows; this only has to stay sane
        CHECK (allocs.load() == 0);
        // DV-04 / DV-06: reset forgets everything
        currentTest = "DV-04 PANIC clears its memory";
        d.reset();
        peakOut = 0;
        for (int i = 0; i < 48000 * 3; ++i) { d.process (0.0f, 0.0f, l, r, p); peakOut = std::max (peakOut, std::max (std::abs (l), std::abs (r))); }
        CHECK (peakOut == 0.0f);
        CHECK (d.memoryBytes() < 2 * 1024 * 1024);                                    // DV-06: a fixed, small history (2 s stereo)
    }
    currentTest = "DV-05 switching voices is click-free and the engine stays bounded";
    {
        Engine e;
        e.prepare (fs);
        auto c = defaults();
        c.p[kTone] = 0.2f; c.p[kPitch] = 0.1f;
        Buf tone (48000 * 3);
        for (size_t i = 0; i < tone.size(); ++i) tone[i] = 0.3f * std::sin (2.0f * kPi * 100.0f * (float) i / 48000.0f);
        Buf y;
        for (int seg = 0; seg < 60; ++seg)
        {
            c.voice = seg % 3;
            Buf l (2400), r (2400);
            e.process (tone.data() + seg * 2400, tone.data() + seg * 2400, tone.data() + seg * 2400, l.data(), r.data(), 2400, c);
            y.insert (y.end(), l.begin(), l.end());
        }
        float step = 0;
        for (size_t i = 1; i < y.size(); ++i) step = std::max (step, std::abs (y[i] - y[i - 1]));
        CHECK (step < 0.03f);
        CHECK (peak (y) <= Engine::kCeiling && allFinite (y));
        // knob jumps with a LAB voice selected: no step (Dissolve's parameters are smoothed like the resonator's)
        Buf x (48000);
        for (size_t i = 0; i < x.size(); ++i) x[i] = 0.3f * std::sin (2.0f * kPi * 100.0f * (float) i / 48000.0f);
        for (int v = 1; v < 3; ++v)
        {
            auto maxStep = [&] (Controls a, Controls b)
            {
                Engine e3;
                e3.prepare (fs);
                Buf firstHalf (x.begin(), x.begin() + 24000), secondHalf (x.begin() + 24000, x.end());
                auto y1 = run (e3, firstHalf, a, 64);
                const auto y2 = run (e3, secondHalf, b, 64);
                y1.insert (y1.end(), y2.begin(), y2.end());
                float m = 0;
                for (size_t i = 2; i < y1.size(); ++i) m = std::max (m, std::abs (y1[i] - y1[i - 2]));
                return m;
            };
            auto base = defaults();
            base.voice = v; base.p[kTone] = 0.2f; base.p[kPitch] = 0.1f; base.p[kDrift] = 0.0f; base.p[kCouple] = 0.0f;
            const float steady = maxStep (base, base);
            for (int p : { (int) kShape, (int) kTone, (int) kFeedback, (int) kMix, (int) kLevel, (int) kDrift, (int) kCouple })
                for (float to : { 0.0f, 1.0f })
                {
                    auto b = base;
                    b.p[p] = to;
                    const float step2 = maxStep (base, b);
                    CHECK (step2 < steady + 0.03f);
                    if (step2 >= steady + 0.03f) std::printf ("    voice %d, %s -> %.0f: step %.3f (steady %.3f)\n", v, paramNames[p], (double) to, (double) step2, (double) steady);
                }
        }

        currentTest = "DV-07 a deselected Dissolve keeps nothing";
        {
            // voice 1 hears noise for a second; voice 0 for three; then voice 1 again. From the moment it is re-selected the
            // output must be what it would be if Dissolve had never run before - nothing from the first second comes back.
            const auto loud2 = noise (48000 * 5, 0.4f);
            auto c2 = defaults();
            c2.p[kMix] = 1.0f; c2.p[kPitch] = 0.0f;                                   // the longest TIME: 1.6 s
            Engine used, fresh;
            used.prepare (fs); fresh.prepare (fs);
            Buf lu (loud2.size()), ru (loud2.size()), lf (loud2.size()), rf (loud2.size());
            const Buf zeros (loud2.size(), 0.0f);
            auto go = [&] (Engine& e4, Buf& l, Buf& r, size_t from, size_t to, int voiceNow, const Buf& in)
            { auto c4 = c2; c4.voice = voiceNow; for (size_t at = from; at + 64 <= to; at += 64) e4.process (in.data() + at, in.data() + at, zeros.data() + at, l.data() + at, r.data() + at, 64, c4); };
            go (used, lu, ru, 0, 48000, 1, loud2);       go (fresh, lf, rf, 0, 48000, 0, loud2);
            go (used, lu, ru, 48000, 192000, 0, loud2);  go (fresh, lf, rf, 48000, 192000, 0, loud2);
            go (used, lu, ru, 192000, 240000, 1, zeros); go (fresh, lf, rf, 192000, 240000, 1, zeros);
            CHECK (depth::same (Buf (lu.begin() + 192000, lu.begin() + 239936), Buf (lf.begin() + 192000, lf.begin() + 239936)));
        }

        for (int v = 1; v < 3; ++v)                                                   // each LAB voice makes sound, and PANIC silences it
        {
            Engine e2;
            e2.prepare (fs);
            auto c2 = defaults();
            c2.voice = v; c2.p[kMix] = 1.0f; c2.p[kFeedback] = 0.9f;
            const auto x = testSource (fs, 48000 * 2);
            Buf l (x.size()), r (x.size());
            e2.process (x.data(), x.data(), x.data(), l.data(), r.data(), (int) x.size(), c2);
            CHECK (rms (l) > 1.0e-3f);
            e2.panic();
            const Buf silence (48000 * 3, 0.0f);
            Buf l2 (silence.size()), r2 (silence.size());
            e2.process (silence.data(), silence.data(), silence.data(), l2.data(), r2.data(), (int) silence.size(), c2);
            CHECK (peak (l2) == 0.0f);
        }
    }
}
} // namespace voice

#if WILDBAND_HAS_RINGS
// =====================================================================================================================
// Rings resonator experiment: docs/p1/RINGS-EXPERIMENT.md. Built only when upstream Rings is configured.
namespace ringsTests
{
float costNs[3] = {};

void all()
{
    const double fs = 48000.0;
    auto rendered = [&] (Controls c, const Buf& x, int block = 64) { Engine e; e.prepare (fs); return run (e, x, c, block); };
    auto ringsControls = [] (int model) { auto c = defaults(); c.voice = 3 + model; c.p[kMix] = 1.0f; c.p[kCouple] = 0.0f; return c; };

    currentTest = "RG-02 silence and reset";
    for (int model = 0; model < 3; ++model)
    {
        auto c = ringsControls (model);
        Engine e;
        e.prepare (fs);
        Buf x = noise (48000 * 14, 0.4f);
        std::fill (x.begin() + 48000, x.end(), 0.0f);                              // one second of noise, then silence
        const auto y = run (e, x, c, 64);
        CHECK (rms (Buf (y.begin(), y.begin() + 96000)) > 1.0e-3f);                // it sounds
        CHECK (rms (y, 48000 * 2 * 13) < 3.2e-5f);                                 // below -90 dBFS 12 s after the input stops
        Engine e2;
        e2.prepare (fs);
        run (e2, noise (48000, 0.4f), c, 64);
        e2.panic();
        CHECK (peak (run (e2, Buf (48000 * 2, 0.0f), c, 64)) == 0.0f);             // PANIC: exact silence from silence
    }

    currentTest = "RG-03 tuning (modal, 48 kHz)";
    for (float midi : { 48.0f, 60.0f, 72.0f })
    {
        auto c = ringsControls (0);
        c.p[kPitch] = (midi - 36.0f) / 48.0f; c.p[kShape] = 0.25f; c.p[kTone] = 0.7f; c.p[kFeedback] = 0.8f; c.p[kDrift] = 0.3f;
        Buf x ((size_t) fs, 0.0f);
        x[10] = 1.0f;
        const auto y = rendered (c, x);
        const double period = fs / (440.0 * std::exp2 ((midi - 69.0) / 12.0));
        const int span = 8;
        const size_t from = (size_t) (fs * 0.05), len = (size_t) (fs * 0.4);
        auto corr = [&] (int lag) { double s = 0; for (size_t i = from; i < from + len; ++i) s += (double) y[2 * i] * y[2 * (i + (size_t) lag)]; return s; };
        int best = 0;
        double bestV = -1e30;
        for (int lag = (int) (span * period * 0.96); lag <= (int) (span * period * 1.04) + 1; ++lag)
            if (const double v = corr (lag); v > bestV) { bestV = v; best = lag; }
        const double a = corr (best - 1), b = bestV, d = corr (best + 1);
        const double cents = 1200.0 * std::log2 (span * period / (best + 0.5 * (a - d) / (a - 2.0 * b + d)));
        CHECK (std::abs (cents) < 10.0);
        if (std::abs (cents) >= 10.0) std::printf ("    midi %.0f: %.1f cents\n", (double) midi, cents);
    }

    currentTest = "RG-04 safety, determinism, no allocation";
    {
        std::mt19937 rng (31);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);
        Buf nasty = noise (48000 * 2, 1.0f);
        for (size_t i = 0; i < nasty.size(); ++i) { if (i % 9973 == 0) nasty[i] = std::numeric_limits<float>::quiet_NaN(); if (i % 5000 == 2) nasty[i] = 1.0e30f; }
        for (int trial = 0; trial < 18; ++trial)
        {
            Controls c;
            for (auto& p : c.p) p = trial == 0 ? 0.0f : trial == 1 ? 1.0f : trial < 9 ? (float) (rng() & 1) : u (rng);
            c.p[kLevel] = 1.0f; c.voice = 3 + trial % 3; c.room = { u (rng), u (rng), u (rng) }; c.gap = u (rng);
            Engine e;
            e.prepare (trial % 2 ? 48000.0 : 44100.0);
            const auto y = run (e, nasty, c, trial % 2 ? 0 : 64);
            CHECK (allFinite (y) && peak (y) <= Engine::kCeiling && e.faults.load() == 0);
        }
        const auto x = testSource (fs, 48000);
        auto c = ringsControls (2);
        CHECK (depth::same (rendered (c, x), rendered (c, x)));                    // deterministic
        Engine e;
        e.prepare (fs);
        Buf l (64), r (64);
        const auto in = noise (64, 0.5f);
        watched = pthread_self();
        allocs = 0;
        malloc_logger = countAllocs;
        for (int b = 0; b < 3000; ++b) { c.voice = 3 + (b / 500) % 3; c.p[b % kNumParams] = (float) (b % 17) / 16.0f; e.process (in.data(), in.data(), in.data(), l.data(), r.data(), 64, c); if (b == 1500) e.panic(); }
        malloc_logger = nullptr;
        CHECK (allocs.load() == 0);
    }

    currentTest = "RG-05 latency is 24 samples at any block size";
    {
        const auto x = testSource (fs, 48000);
        auto c = ringsControls (0);
        const auto a = rendered (c, x, 1), b = rendered (c, x, 64), d = rendered (c, x, 1000);
        CHECK (depth::same (a, b) && depth::same (a, d));
        Buf impulse (4800, 0.0f);
        impulse[0] = 1.0f;
        const auto y = rendered (c, impulse);
        size_t first = 0;
        while (first < impulse.size() && y[2 * first] == 0.0f && y[2 * first + 1] == 0.0f) ++first;
        CHECK (first >= (size_t) RingsVoice::kBlock && first <= (size_t) RingsVoice::kBlock + 2);
    }

    currentTest = "RG-06 controls and WORLD are heard";
    {
        const auto x = testSource (fs, 48000 * 2);
        auto differ = [] (const Buf& a, const Buf& b) { Buf d (a.size()); for (size_t i = 0; i < a.size(); ++i) d[i] = a[i] - b[i]; return rms (d) / (rms (a) + 1.0e-9f); };
        for (int model = 0; model < 3; ++model)
        {
            auto base = ringsControls (model);
            const auto y0 = rendered (base, x);
            for (int p : { (int) kShape, (int) kTone, (int) kFeedback, (int) kDrift })
            {
                auto c = base;
                c.p[p] = base.p[p] > 0.5f ? 0.1f : 0.9f;
                CHECK (differ (y0, rendered (c, x)) > 0.05f);
            }
            Graph g;
            g.connect (kRadioAudioOut, kResAudioIn);
            g.connect (kRoomLevelOut, kResDampIn);                                 // ROOM LEVEL -> TONE (Rings BRIGHTNESS)
            auto c = base;
            c.plan = RoutingPlan::fromGraph (g);
            c.room = { 0.9f, 0.0f, 0.0f };
            const auto uncoupled = rendered (c, x);
            auto quiet = c;
            quiet.room = {};
            CHECK (depth::same (uncoupled, rendered (quiet, x)));                  // COUPLE 0: the room does nothing
            c.p[kCouple] = quiet.p[kCouple] = 0.8f;
            CHECK (differ (rendered (quiet, x), rendered (c, x)) > 0.05f);         // COUPLE up: it does
        }
    }

    currentTest = "RG-07b knob jumps with a Rings voice are click-free";
    {
        Buf x (48000);
        for (size_t i = 0; i < x.size(); ++i) x[i] = 0.3f * std::sin (2.0f * kPi * 100.0f * (float) i / 48000.0f);
        for (int model = 0; model < 3; ++model)
        {
            auto maxStep = [&] (Controls a, Controls b)
            {
                Engine e3;
                e3.prepare (fs);
                Buf firstHalf (x.begin(), x.begin() + 24000), secondHalf (x.begin() + 24000, x.end());
                auto y1 = run (e3, firstHalf, a, 64);
                const auto y2 = run (e3, secondHalf, b, 64);
                y1.insert (y1.end(), y2.begin(), y2.end());
                float m = 0;
                for (size_t i = 2; i < y1.size(); ++i) m = std::max (m, std::abs (y1[i] - y1[i - 2]));
                return m;
            };
            auto base = ringsControls (model);
            base.p[kTone] = 0.2f; base.p[kPitch] = 0.1f;
            const float steady = maxStep (base, base);
            for (int p : { (int) kPitch, (int) kShape, (int) kTone, (int) kFeedback, (int) kDrift, (int) kMix, (int) kLevel })
                for (float to : { 0.0f, 1.0f })
                {
                    auto b = base;
                    b.p[p] = to;
                    const float step2 = maxStep (base, b);
                    CHECK (step2 < steady + 0.03f);
                    if (step2 >= steady + 0.03f) std::printf ("    model %d, %s -> %.0f: step %.3f (steady %.3f)\n", model, paramNames[p], (double) to, (double) step2, (double) steady);
                }
        }
    }

    currentTest = "RG-07 switching to and from Rings is click-free";
    {
        Engine e;
        e.prepare (fs);
        auto c = defaults();
        c.p[kTone] = 0.2f; c.p[kPitch] = 0.1f;
        Buf tone (48000 * 3);
        for (size_t i = 0; i < tone.size(); ++i) tone[i] = 0.3f * std::sin (2.0f * kPi * 100.0f * (float) i / 48000.0f);
        Buf y;
        static const int order[] = { 0, 3, 0, 4, 1, 5, 2, 3, 5, 0 };
        for (int seg = 0; seg < 60; ++seg)
        {
            c.voice = order[seg % 10];
            Buf l (2400), r (2400);
            e.process (tone.data() + seg * 2400, tone.data() + seg * 2400, tone.data() + seg * 2400, l.data(), r.data(), 2400, c);
            y.insert (y.end(), l.begin(), l.end());
        }
        float step = 0;
        for (size_t i = 1; i < y.size(); ++i) step = std::max (step, std::abs (y[i] - y[i - 1]));
        CHECK (step < 0.03f);
        if (step >= 0.03f) std::printf ("    step %.3f\n", (double) step);
    }

    for (int model = 0; model < 3; ++model)                                        // cost, for --metrics
    {
        const auto in = noise (64, 0.3f);
        auto c = ringsControls (model);
        Engine e;
        e.prepare (fs);
        Buf l (64), r (64);
        const int blocks = (int) (fs * 10 / 64);
        timespec t0, t1;
        clock_gettime (CLOCK_MONOTONIC, &t0);
        for (int b = 0; b < blocks; ++b) e.process (in.data(), in.data(), in.data(), l.data(), r.data(), 64, c);
        clock_gettime (CLOCK_MONOTONIC, &t1);
        costNs[model] = (float) (((double) (t1.tv_sec - t0.tv_sec) * 1e9 + (double) (t1.tv_nsec - t0.tv_nsec)) / (blocks * 64.0));
    }
}
} // namespace ringsTests
#endif


// =====================================================================================================================
// Plugin receiver gate: docs/p1/PLUGIN-RECEIVER-LIFECYCLE.md
namespace gateTests
{
using G = ReceiverGate;
/** Feeds the gate a scenario: `audioPerWall` seconds of audio per wall second (0 = no callbacks), sampled at 30 Hz. */
G::State drive (G& g, double& now, double& rendered, double& lastCb, double seconds, double audioPerWall, bool offline = false)
{
    G::State s = g.current();
    for (double t = 0; t < seconds; t += 1.0 / 30.0)
    {
        now += 1.0 / 30.0;
        if (audioPerWall > 0) { rendered += audioPerWall / 30.0; lastCb = now; }
        s = g.update (now, rendered, lastCb, offline);
    }
    return s;
}

void all()
{
    currentTest = "RL-01 construction, prepare and restore never activate";
    {
        G g;
        double now = 100, rendered = 0, last = -1;
        CHECK (g.current() == G::State::Dormant);
        CHECK (drive (g, now, rendered, last, 10, 0) == G::State::Dormant);
        CHECK (drive (g, now, rendered, last, 10, 1.0) == G::State::Dormant);      // even rendering does nothing before prepare
        g.prepared (false);
        CHECK (g.current() == G::State::Armed);
        last = -1;
        CHECK (drive (g, now, rendered, last, 10, 0) == G::State::Armed);           // prepared, but nothing rendered
    }
    currentTest = "RL-02 faster than real time never activates";
    for (double speed : { 2.5, 10.0, 1000.0 })
    {
        G g;
        g.prepared (false);
        double now = 100, rendered = 0, last = -1;
        CHECK (drive (g, now, rendered, last, 20, speed) == G::State::Armed);
    }
    currentTest = "RL-03 real time: 1.4 s is not enough, 1.6 s is";
    {
        G g;
        g.prepared (false);
        double now = 100, rendered = 0, last = -1;
        CHECK (drive (g, now, rendered, last, 1.4, 1.0) == G::State::Armed);
        CHECK (drive (g, now, rendered, last, 0.25, 1.0) == G::State::Active);
        G slow;                                                                    // a busy host running a little late still counts
        slow.prepared (false);
        double n2 = 100, r2 = 0, l2 = -1;
        CHECK (drive (slow, n2, r2, l2, 1.7, 0.7) == G::State::Active);
    }
    currentTest = "RL-04 a gap restarts the window";
    {
        G g;
        g.prepared (false);
        double now = 100, rendered = 0, last = -1;
        drive (g, now, rendered, last, 1.0, 1.0);
        drive (g, now, rendered, last, 0.6, 0.0);                                    // 0.6 s without callbacks
        CHECK (drive (g, now, rendered, last, 1.0, 1.0) == G::State::Armed);        // only 1 s since the gap
        CHECK (drive (g, now, rendered, last, 0.6, 1.0) == G::State::Active);
    }
    currentTest = "RL-05 offline rendering never activates, and suspends";
    {
        G g;
        g.prepared (true);
        double now = 100, rendered = 0, last = -1;
        CHECK (drive (g, now, rendered, last, 10, 1.0, true) == G::State::Dormant);
        G h;
        h.prepared (false);
        double n2 = 100, r2 = 0, l2 = -1;
        CHECK (drive (h, n2, r2, l2, 2, 1.0) == G::State::Active);
        CHECK (drive (h, n2, r2, l2, 0.1, 1.0, true) == G::State::Suspended);       // host switches to offline
        CHECK (drive (h, n2, r2, l2, 10, 1.0, true) == G::State::Suspended);
    }
    currentTest = "RL-06 silence from the host suspends; only real-time audio resumes";
    {
        G g;
        g.prepared (false);
        double now = 100, rendered = 0, last = -1;
        drive (g, now, rendered, last, 2, 1.0);
        CHECK (drive (g, now, rendered, last, 4.9, 0.0) == G::State::Active);
        CHECK (drive (g, now, rendered, last, 0.2, 0.0) == G::State::Suspended);
        CHECK (drive (g, now, rendered, last, 5, 50.0) == G::State::Suspended);    // fast rendering does not resume it
        // a window that began during the fast phase is discarded first, so resuming can take up to two windows
        CHECK (drive (g, now, rendered, last, 1.4, 1.0) == G::State::Suspended);
        CHECK (drive (g, now, rendered, last, 1.8, 1.0) == G::State::Active);
    }
    currentTest = "RL-07 release goes dormant at once";
    {
        G g;
        g.prepared (false);
        double now = 100, rendered = 0, last = -1;
        drive (g, now, rendered, last, 2, 1.0);
        g.released();
        CHECK (g.current() == G::State::Dormant);
        CHECK (drive (g, now, rendered, last, 5, 1.0) == G::State::Dormant);
    }
}
} // namespace gateTests

// ---- informational: what the control states do to level and brightness. Proxies only; nobody has listened to these.
void metrics()
{
    const double fs = 48000.0;
    const auto x = testSource (fs, (size_t) fs * 8);
    struct State { const char* name; Mode mode; float shape, tone, couple, drift, feedback; EnvFrame room; };
    const State states[] = {
        { "dry source", Mode::Dry, 0.4f, 0.6f, 0, 0, 0, {} },
        { "reference filter+delay", Mode::Reference, 0.4f, 0.6f, 0, 0.2f, 0.55f, {} },
        { "engine: default", Mode::Engine, 0.4f, 0.6f, 0.4f, 0.2f, 0.55f, {} },
        { "engine: quiet texture", Mode::Engine, 0.1f, 0.3f, 0.2f, 0.1f, 0.3f, {} },
        { "engine: evolving resonance", Mode::Engine, 0.66f, 0.7f, 0.3f, 0.6f, 0.68f, {} },
        { "engine: COUPLE full, loud room", Mode::Engine, 0.4f, 0.6f, 1.0f, 0.2f, 0.5f, { 0.9f, 0.0f, 0.3f } },
        { "engine: unstable", Mode::Engine, 0.95f, 0.8f, 0.7f, 1.0f, 1.0f, { 0.8f, 0.2f, 0.8f } } };
    std::printf ("\n%-34s %8s %8s %8s %10s\n", "state (local test source, 8 s)", "rms dB", "peak dB", "crest dB", "zero-x Hz");
    for (const auto& s : states)
    {
        auto c = defaults();
        c.mode = s.mode; c.p[kShape] = s.shape; c.p[kTone] = s.tone; c.p[kCouple] = s.couple; c.p[kDrift] = s.drift; c.p[kFeedback] = s.feedback;
        c.room = s.room;
        Engine e;
        e.prepare (fs);
        const auto y = run (e, x, c, 64);
        int zc = 0;
        for (size_t i = 2; i < y.size(); i += 2) zc += (y[i] >= 0) != (y[i - 2] >= 0);
        const float r = rms (y), p = peak (y);
        std::printf ("%-34s %8.1f %8.1f %8.1f %10.0f\n", s.name, 20 * std::log10 (r + 1e-9f), 20 * std::log10 (p + 1e-9f), 20 * std::log10 (p / (r + 1e-9f)), 0.5 * zc / 8.0);
    }

    strike::report();
   #if WILDBAND_HAS_RINGS
    std::printf ("\nWhole engine with a Rings voice selected, synthetic, 48 kHz, noise input: modal %.0f, sympathetic %.0f, string %.0f ns per sample (Rings state: %zu kB)\n",
                 (double) ringsTests::costNs[0], (double) ringsTests::costNs[1], (double) ringsTests::costNs[2], RingsVoice::memoryBytes() / 1024);
   #endif
    std::printf ("\nTuner static at the default STATIC setting: %.1f dBFS RMS at the engine's radio input (48 kHz)\n", (double) tuner::staticDb);
    std::printf ("\nSTRIKE correction, default patch vs the frozen P0 engine (local test source, 8 s): difference %.1f dB relative to P0, level change %+.2f dB\n",
                 (double) depth::strikeDefaultDifferenceDb, (double) depth::strikeDefaultLevelDb);
    // synthetic cost of Engine::process alone, single thread, no audio device: NOT a CoreAudio measurement
    std::printf ("\nEngine::process cost, synthetic (48 kHz, block 64, 20 s of audio per row)\n%-28s %14s\n", "input", "ns per sample");
    for (int kind = 0; kind < 2; ++kind)
    {
        const auto in = kind == 0 ? Buf (64, 0.0f) : noise (64, 0.3f);
        auto c = defaults();
        c.p[kFeedback] = 0.85f; c.p[kDrift] = 0.7f; c.p[kCouple] = 0.8f;
        Engine e;
        e.prepare (fs);
        Buf l (64), r (64);
        const int blocks = (int) (fs * 20 / 64);
        timespec t0, t1;
        clock_gettime (CLOCK_MONOTONIC, &t0);
        for (int b = 0; b < blocks; ++b) e.process (in.data(), in.data(), in.data(), l.data(), r.data(), 64, c);
        clock_gettime (CLOCK_MONOTONIC, &t1);
        std::printf ("%-28s %14.1f\n", kind == 0 ? "silence (engine idle)" : "noise (engine ringing)", ((double) (t1.tv_sec - t0.tv_sec) * 1e9 + (double) (t1.tv_nsec - t0.tv_nsec)) / (blocks * 64.0));
    }
}
} // namespace

int main (int argc, char** argv)
{
    testDial();
    testBounded();
    testDeterministicAndBlockInvariant();
    testTuning();
    testDecayAndSelfOscillation();
    testNoClicks();
    testRoom();
    testGraph();
    testPatching();
    testNoAllocation();
    depth::all();
    tuner::all();
    strike::all();
    voice::all();
    gateTests::all();
   #if WILDBAND_HAS_RINGS
    ringsTests::all();
   #endif
    if (argc > 1 && std::strcmp (argv[1], "--metrics") == 0) metrics();
    std::printf ("%d checks, %d failed\n", checks, failures);
    return failures;
}
