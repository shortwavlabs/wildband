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
