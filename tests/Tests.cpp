// WILDBAND P0 engine tests. Plain checks, no framework; exit code = number of failed checks.
// `wildband_tests --metrics` also prints level/brightness figures for a few control states (proxies, not listening).
#include "../src/Engine.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <pthread.h>
#include <random>

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
            c.room = { u (rng) * 3.0f, u (rng) * 3.0f, u (rng) * 3.0f };             // out-of-range room values too
            if (trial % 4 == 3) c.plan = RoutingPlan::unpack (((uint64_t) rng() << 32) | rng());   // garbage plan words too
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
    CHECK (std::memcmp (RoutingPlan::unpack (plan.pack()).source, plan.source, sizeof (plan.source)) == 0);
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
    watched = pthread_self();
    malloc_logger = countAllocs;
    { void* volatile p = std::malloc (64); std::free (p); }                         // positive control
    const int control = allocs.exchange (0);
    for (int b = 0; b < 4000; ++b)
    {
        c.p[b % kNumParams] = (float) (b % 17) / 16.0f;
        c.mode = (Mode) ((b / 500) % 3);
        bus.publish (kEnvMicrophone, room.process (x.data(), 64), b);
        c.room = bus.read (b);
        e.process (x.data(), x.data(), x.data(), l.data(), r.data(), 64, c);
        if (b % 300 == 0) c.plan = RoutingPlan::unpack ((uint64_t) b * 0x9E3779B97F4A7C15ull); // any word at all is a safe plan
        if (b == 2000) e.reset();
    }
    malloc_logger = nullptr;
    CHECK (control == 1);
    CHECK (allocs.load() == 0);
}

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
    if (argc > 1 && std::strcmp (argv[1], "--metrics") == 0) metrics();
    std::printf ("%d checks, %d failed\n", checks, failures);
    return failures;
}
