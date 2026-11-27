# WILDBAND — Tuner refinement: station changes versus internal restarts

**Branch:** `p0/tuner-restarts`, on `p0/strike-response`. **Status:** criteria committed before the change; results
at the bottom.

## The problem

The receiver restarts its stream for two different reasons, and the tuner could not tell them apart:

1. **The player asked for a station** (`tune`): TUNE came to rest, `<` / `>`, a station name, SET, a mapped button.
2. **The audio configuration changed** (`setOutputRate`): another output device, another sample rate, or a device
   coming back after sleep. The stream is restarted so that it can be decoded at the new rate.

Both bumped the same generation counter, so both opened the "tune gap", and STATIC sounded for a device switch as
if it were a retune. Changing an audio setting is housekeeping, not a musical gesture, and should not produce one.

## Contract

- Static, and the "request" the engine uses to restart static's time limit, follow **station requests only**.
- A restart for an audio-configuration change is silent, as a spontaneous dropout already is: fade out, re-buffer,
  fade in. Resonator tails are untouched, as before.
- If a configuration change lands **in the middle** of a station change, that station change is still in progress:
  static continues until the station plays.
- The off-centre TUNE sound and the buffer-driven static are otherwise unchanged. No artificial delays.

## Acceptance criteria (registered before implementation)

| ID | Test (real time, `wildband_tuner_check`) | Passes when |
|---|---|---|
| TR-01 | Rate change while a station is playing | The stream restarts and plays again; the tune gap stays 0 and no static sounds; the request number does not change |
| TR-02 | Rate change during a station change | Static continues until the station is LIVE |
| TR-03 | Nothing else moved | The 24 existing real-time checks and all engine checks (CT-01 … CT-16 included) still pass |
| TR-04 | In the app, on real CoreAudio | Switching the output device and back while a station plays: gap 0 and static 0 in every log line |

Not automatable (owner): the listening comparisons in `OWNER-AUDITION.md`.

---

## Results (added after implementation, 2026-11-27)

**Technical evidence only; all runs muted.**

The receiver now keeps a separate count of station requests and a "station wanted" flag that only `tune` sets and
that clears once the station has played. The tune gap (and so the static) opens only while that flag is set.

| ID | Result | Measured |
|---|---|---|
| TR-01 | **Pass** | Rate 48 → 44.1 → 48 kHz while playing: the stream restarted each time and was LIVE again within about 0.3 s (local server); gap stayed 0, no static, request number unchanged |
| TR-02 | **Pass** | A rate change one second into a 3 s station change: static for the whole 4.0 s until LIVE, none after |
| TR-03 | **Pass** | `wildband_tuner_check`: **27 checks, 0 failed** ([`evidence/tuner-restarts-check.txt`](evidence/tuner-restarts-check.txt)); `wildband_tests`: 788 checks, 0 failed |
| TR-04 | **Pass, but weaker than it looks** | In the app, MiniFuse → built-in speakers → MiniFuse while playing: gap 0 and static 0 throughout ([`evidence/tuner-restarts-device-switch.txt`](evidence/tuner-restarts-device-switch.txt)). Both devices were at 48 kHz, so the stream was **not** restarted in this run (connection count stayed at 1). The restart path itself is exercised by TR-01, not here |

My first version of TR-01 failed for a reason in the test, not the receiver: it looked at the state 0.3 s after the
rate change, by which time the local server had already brought the stream back.

Still true, by design: launching the app is a station request, so the first connection has static.

Not done: a device switch that really changes the sample rate, in the app, by hand; sleep/wake.
