// Receiver-level tuner acceptance tests (docs/p0/TUNER-ACCEPTANCE.md: CT-03..CT-08, CT-15), in real time.
// The real Radio and the real Engine, with this thread standing in for the audio callback, against
// tools/stream_test_server.py on 127.0.0.1. Run it with tools/run_tuner_check.sh. Exit code = number of failed checks.
#include "../src/Engine.h"
#include "../src/Radio.h"

#include <cmath>
#include <cstdio>

using namespace wildband;

namespace
{
int failures = 0, checks = 0;
#define CHECK(cond, ...) \
    do { ++checks; const bool ok = (cond); std::printf ("  %s  ", ok ? "pass" : "FAIL"); std::printf (__VA_ARGS__); std::printf ("\n"); if (! ok) ++failures; } while (0)

constexpr double fs = 48000.0;
constexpr int block = 480;                                   // 10 ms
constexpr float tones[3] = { 440.0f, 880.0f, 1320.0f };      // clips a, b, c

struct Frame { double t; Radio::State state; float gap, staticGain, outPeak, radioRms; int tone; };

struct Rig
{
    Radio radio;
    Engine engine;
    Controls c;
    std::vector<float> l = std::vector<float> (block), r = l, zeros = l, ol = l, orr = l;
    std::vector<Frame> log;
    double t0 = 0;
    juce::int64 blocks = 0;

    Rig()
    {
        for (int i = 0; i < kNumParams; ++i) c.p[i] = paramDefaults[i];
        c.p[kStatic] = 1.0f; c.p[kLevel] = 1.0f;
        radio.setOutputRate (fs);
        engine.prepare (fs);
        t0 = juce::Time::getMillisecondCounterHiRes();
    }
    double now() const { return (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0; }

    /** One 10 ms "audio callback", then sleep until the next is due. */
    void step()
    {
        radio.read (l.data(), r.data(), block);
        c.gap = radio.tuneGap.load();                        // as in the app: the value read() left
        c.request = radio.request();
        engine.process (l.data(), r.data(), zeros.data(), ol.data(), orr.data(), block, c);
        Frame f { now(), radio.state(), radio.tuneGap.load(), engine.meterStatic.load(), 0.0f, 0.0f, -1 };
        double sum = 0, power[3] = {};
        for (int k = 0; k < 3; ++k)                          // which station is this? (Goertzel at the three clip tones)
        {
            const double w = 2.0 * 3.14159265358979 * tones[k] / fs, coeff = 2.0 * std::cos (w);
            double s1 = 0, s2 = 0;
            for (int i = 0; i < block; ++i) { const double s0 = l[(size_t) i] + coeff * s1 - s2; s2 = s1; s1 = s0; }
            power[k] = s1 * s1 + s2 * s2 - coeff * s1 * s2;
        }
        for (int i = 0; i < block; ++i) { sum += (double) l[(size_t) i] * l[(size_t) i]; f.outPeak = std::max (f.outPeak, std::abs (ol[(size_t) i])); }
        f.radioRms = (float) std::sqrt (sum / block);
        if (f.radioRms > 0.003f) f.tone = power[0] > power[1] && power[0] > power[2] ? 0 : power[1] > power[2] ? 1 : 2;
        log.push_back (f);
        ++blocks;
        if (const double ahead = t0 + 1000.0 * (double) (blocks * block) / fs - juce::Time::getMillisecondCounterHiRes(); ahead > 0) juce::Thread::sleep ((int) ahead);
    }
    void run (double seconds) { const double until = now() + seconds; while (now() < until) step(); }
    /** Runs until the station is playing (or `limit` seconds pass); returns the time taken, or -1. */
    double untilLive (double limit)
    {
        const double start = now();
        while (now() - start < limit) { step(); if (radio.state() == Radio::State::Playing) return now() - start; }
        return -1.0;
    }
    double staticSeconds (double from, double to) const { double s = 0; for (auto& f : log) if (f.t >= from && f.t < to && f.staticGain > 0.02f) s += block / fs; return s; }
    float maxStatic (double from, double to) const { float m = 0; for (auto& f : log) if (f.t >= from && f.t < to) m = std::max (m, f.staticGain); return m; }
    float maxGap (double from, double to) const { float m = 0; for (auto& f : log) if (f.t >= from && f.t < to) m = std::max (m, f.gap); return m; }
    float maxOut (double from, double to) const { float m = 0; for (auto& f : log) if (f.t >= from && f.t < to) m = std::max (m, f.outPeak); return m; }
};
} // namespace

int main (int argc, char** argv)
{
    const juce::String base = argc > 1 ? argv[1] : "http://127.0.0.1:8765";
    const juce::String deadPort = argc > 2 ? argv[2] : "http://127.0.0.1:8799/live";

    std::printf ("CT-03 static follows the real wait\n");
    for (double delay : { 0.0, 1.0, 3.0 })
    {
        Rig rig;
        rig.run (0.2);
        const double start = rig.now();
        rig.radio.tune (base + "/wait/" + juce::String (delay, 1));
        const double live = rig.untilLive (10.0);
        rig.run (1.5);
        const double heard = rig.staticSeconds (start, start + live + 0.3);
        CHECK (live > 0 && std::abs (heard - live) < 0.15, "server delay %.0f s: LIVE after %.2f s, static audible for %.2f s", delay, live, heard);
        CHECK (rig.maxStatic (start + live + 0.3, rig.now()) < 0.01f, "  and no static once LIVE (max %.4f)", (double) rig.maxStatic (start + live + 0.3, rig.now()));
    }

    std::printf ("CT-04 no added delay\n");
    {
        double live[2] = {};
        for (int k = 0; k < 2; ++k)
        {
            Rig rig;
            rig.c.p[kStatic] = k == 0 ? 0.0f : 1.0f;
            rig.run (0.2);
            rig.radio.tune (base + "/wait/1.0");
            live[k] = rig.untilLive (10.0);
            if (k == 0) CHECK (rig.maxStatic (0, rig.now()) == 0.0f, "STATIC 0: no static at all");
        }
        CHECK (live[0] > 0 && live[1] > 0 && std::abs (live[0] - live[1]) < 0.1, "time to LIVE: %.2f s with STATIC 0, %.2f s with STATIC full", live[0], live[1]);
    }

    std::printf ("CT-05 failure\n");
    for (const char* path : { "/missing", "/ogg" })
    {
        Rig rig;
        rig.run (0.2);
        const double start = rig.now();
        rig.radio.tune (base + path);
        double failedAt = -1;
        while (rig.now() - start < 5.0) { rig.step(); if (failedAt < 0 && rig.radio.state() == Radio::State::Failed) failedAt = rig.now(); }
        CHECK (failedAt > 0 && rig.maxGap (failedAt + 0.02, rig.now()) == 0.0f && rig.maxStatic (failedAt + 0.5, rig.now()) < 0.01f,
               "%s: FAILED after %.2f s (%s); gap closed, static gone within 0.5 s", path, failedAt - start, rig.radio.detail().toRawUTF8());
    }
    {
        Rig rig;
        rig.run (0.2);
        const double start = rig.now();
        rig.radio.tune (deadPort);
        rig.run (11.0);
        CHECK (rig.maxStatic (start + 0.5, start + 3.5) > 0.5f && rig.maxStatic (start + 8.5, rig.now()) < 0.001f,
               "refused port: static while trying (max %.2f), ended by 8.5 s (max after: %.5f); state %s, %d attempts", (double) rig.maxStatic (start + 0.5, start + 3.5),
               (double) rig.maxStatic (start + 8.5, rig.now()), Radio::name (rig.radio.state()), rig.radio.connects.load());
    }

    std::printf ("CT-06 stall\n");
    {
        Rig rig;
        rig.run (0.2);
        const double start = rig.now();
        rig.radio.tune (base + "/silent");
        rig.run (19.0);
        CHECK (rig.maxStatic (start + 8.5, rig.now()) < 0.001f, "server never answers: static ended by 8.5 s and stayed silent for 10 s of retries (max after: %.5f); state %s, %d attempts",
               (double) rig.maxStatic (start + 8.5, rig.now()), Radio::name (rig.radio.state()), rig.radio.connects.load());
    }

    std::printf ("CT-17 getting away from a bad station\n");
    {
        const std::pair<const char*, juce::String> bad[] = { { "a server that never answers", base + "/silent" }, { "a refused port", deadPort }, { "a 404", base + "/missing" } };
        for (const auto& b : bad)
        {
            Rig rig;
            rig.run (0.2);
            rig.radio.tune (b.second);
            rig.run (1.0);                                                    // one second stuck on it, then move on
            const double start = rig.now();
            rig.radio.tune (base + "/a");
            const double live = rig.untilLive (10.0);
            rig.run (0.3);
            int right = 0;
            for (auto& f : rig.log) if (f.t > start && f.tone == 0) ++right;
            CHECK (live > 0 && live < 1.5 && right > 0, "from %s: LIVE on the next station after %.2f s", b.first, live);
            CHECK (rig.maxStatic (start + 0.05, start + juce::jmax (0.06, live)) > 0.5f || live < 0.1, "  with static during that wait (max %.2f)", (double) rig.maxStatic (start + 0.05, start + juce::jmax (0.06, live)));
        }
        Rig rig;                                                              // asking again for a station that FAILED tries again
        rig.radio.tune (base + "/missing");
        rig.run (1.0);
        const int before = rig.radio.connects.load();
        const bool failed = rig.radio.state() == Radio::State::Failed;
        rig.radio.tune (base + "/missing");
        rig.run (1.0);
        CHECK (failed && rig.radio.connects.load() == before + 1 && rig.radio.state() == Radio::State::Failed, "re-selecting a FAILED station makes one new attempt (%d -> %d), and reports FAILED again", before, rig.radio.connects.load());
    }

    std::printf ("CT-07 / CT-08 rapid switching and stale audio\n");
    {
        Rig rig;
        rig.radio.tune (base + "/a");
        rig.untilLive (10.0);
        rig.run (1.0);
        const int before = rig.radio.connects.load();
        const char* paths[3] = { "/a", "/b", "/c" };
        for (int k = 0; k < 50; ++k) { rig.radio.tune (base + paths[(k + 1) % 3]); rig.run (0.04); }   // 50 requests in 2 s; the last is /b
        rig.radio.tune (base + "/b");
        const double last = rig.now();
        rig.run (6.0);
        int wrong = 0, right = 0;
        for (auto& f : rig.log) if (f.t > last + 0.05 && f.tone >= 0) (f.tone == 1 ? right : wrong)++;
        const int attempts = rig.radio.connects.load() - before;
        CHECK (rig.radio.state() == Radio::State::Playing && right > 100, "ends LIVE on the last station (%d blocks of its tone)", right);
        CHECK (attempts <= 20, "connection attempts for 51 requests: %d", attempts);
        CHECK (wrong == 0, "blocks carrying another station's tone after the last request: %d", wrong);
    }

    std::printf ("CT-10 PANIC during a real wait\n");
    {
        Rig rig;
        rig.c.p[kFeedback] = 1.0f;
        rig.run (0.2);
        const double start = rig.now();
        rig.radio.tune (base + "/wait/3.0");
        rig.run (1.0);
        const float beforePanic = rig.maxOut (start + 0.3, rig.now());
        rig.engine.panic();
        const double panicAt = rig.now();
        const double live = rig.untilLive (10.0);
        const double liveAt = rig.now();
        rig.run (1.0);
        CHECK (beforePanic > 0.0f && live > 0 && rig.maxOut (panicAt + 0.011, liveAt - 0.02) == 0.0f && rig.maxOut (liveAt + 0.1, rig.now()) > 0.0f,
               "static rings the engine (peak %.2f), PANIC gives exact silence until the station arrives %.2f s later, then it plays", (double) beforePanic, liveAt - panicAt);
    }

    std::printf ("CT-15 spontaneous dropout\n");
    {
        Rig rig;
        rig.radio.tune (base + "/drop");                      // the server closes the connection every 3 s
        const double live = rig.untilLive (10.0);
        rig.run (0.5);
        const double from = rig.now();
        rig.run (9.0);
        CHECK (live > 0 && rig.radio.reconnects.load() >= 2 && rig.maxGap (from, rig.now()) == 0.0f && rig.maxStatic (from, rig.now()) < 0.001f,
               "%d reconnects of a playing station: gap stayed closed, no static (underruns %d)", rig.radio.reconnects.load(), rig.radio.underruns.load());
    }

    std::printf ("%d checks, %d failed\n", checks, failures);
    return failures;
}
