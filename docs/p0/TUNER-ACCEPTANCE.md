# WILDBAND — Playable tuner experiment: contract and acceptance tests

**Branch:** `p0/tuner`, from `p0/attenuverter`. **Status:** this file and the off-centre regression test (CT-01) were
committed **before** any tuner code. Results are added at the bottom afterwards. Plan and rationale:
`PLAN-C-PLAYABLE-TUNER.md`; source: `RADIO-TUNER-PERFORMANCE.md` on the docs branch.

## Contract

1. **The off-centre TUNE sound does not change.** Static is a separate layer and is never folded into it.
2. NEXT, PREVIOUS and direct selection of a station slot; each can be mapped to a MIDI controller or note.
3. **STATIC** is synthesized. It sounds only while the receiver is actually connecting or buffering for a retune the
   player asked for. Its length is the real wait: nothing is delayed to make room for it, and there is no minimum.
4. STATIC is a control from 0 to full. **At 0 the instrument behaves exactly as before.**
5. Static enters at the engine's radio input: it is detuned, patched and resonated like a broadcast, and the radio
   control outputs respond to it. It is not mixed in after the engine.
6. Nothing in the engine is reset by a retune; tails ring on.
7. A failed, refused or stalled connection never produces static for more than 8 s (full for 4 s, then fading), and
   FAILED silences it at once. A spontaneous dropout of a playing station gets no static.
8. Rapid requests are coalesced; only the last station is ever heard, and connection attempts are bounded.
9. PANIC silences static for the rest of the current transition as well as the resonators.
10. No automatic hopping, no trigger input, no second receiver, no recording.

## Acceptance tests

Engine-level (deterministic, `wildband_tests`):

| ID | Test | Passes when |
|---|---|---|
| CT-01 | Off-centre regression | With STATIC at 0, output is sample-identical to the frozen P0 engine at off-centre positions 0, 0.25, 0.5, 0.75 and 1, in ENGINE, DRY and REFERENCE modes |
| CT-02 | STATIC 0 is the old behaviour | With STATIC at 0, any receiver gap value gives output sample-identical to no gap |
| CT-09 | Tails | A retune does not touch resonator state: with STATIC 0, output across a gap equals the same input without one; with STATIC up, the tail present before the gap continues through it without a step |
| CT-10 | PANIC | After PANIC during a gap, output is exactly zero until the gap ends, however high STATIC is; static works again on the next retune |
| CT-11 | Safety | Static at full, FEEDBACK at the stop, every destination patched, hostile input: finite, never above the ceiling, no engine fault; no allocation; block-size independent; deterministic |
| CT-12 | Level and shape | At the default STATIC, static RMS at the engine input is between −30 and −14 dBFS; peak never above 0.9; mean (DC) below 0.001 |
| CT-13 | Time bound | With the gap held open, static is at full strength until 4 s, silent from 8 s, and does not restart until the gap has closed and reopened |
| CT-14 | Through the engine | With static playing and the radio silent: the resonators ring (MIX fully right gives output), the radio LEVEL meter rises, and pulling the audio cable silences it |

Receiver-level (real time, `wildband_tuner_check` against `tools/stream_test_server.py` on 127.0.0.1):

| ID | Test | Passes when |
|---|---|---|
| CT-03 | Static follows the real wait | For server start delays of 0, 1 and 3 s, the time static is audible equals the measured time to LIVE within 0.15 s, and it is zero while LIVE |
| CT-04 | No added delay | Time from request to LIVE is the same with STATIC at 0 and at full (within 0.1 s) |
| CT-05 | Failure | HTTP 404 and an unsupported stream type: state FAILED and the gap closed within 0.5 s. A refused port: static ended by 8.5 s |
| CT-06 | Stall | A server that accepts and never sends: static ended by 8.5 s and silent for the following 10 s of retries |
| CT-07 | Rapid switching | 50 requests in 2 s across three stations carrying different tones: only the last station's tone is heard afterwards; connection attempts ≤ 20 |
| CT-08 | Stale audio | In CT-07, no analysis window after the last request (plus the 20 ms fade) contains another station's tone |
| CT-15 | Spontaneous dropout | A playing station whose server closes: the gap stays closed (no static) |

Not automatable (owner): whether the wait has become a gesture; whether static and off-centre detune feel like two
related things or a muddle; the right default level; preset jumps and NEXT/PREVIOUS from hardware.

---

## Results (added after implementation, 2026-11-27)

**Technical evidence only. All runs were muted; nobody has heard the static or pressed a button by hand.**

Engine-level, `ctest` → `wildband_tests`: **743 checks, 0 failed** (670 from the attenuverter branch plus these).

| ID | Result | Notes |
|---|---|---|
| CT-01 | **Pass** | 30 renders sample-identical to the frozen engine. Committed and passing before any tuner code |
| CT-02 | **Pass** | |
| CT-09 | **Pass** | With STATIC 0 a retune is sample-identical to no retune; with STATIC up the tail continues with no step and static keeps the resonators excited |
| CT-10 | **Pass** | Engine-level and in real time (below) |
| CT-11 | **Pass** | Static is part of the hostile-input, block-size and allocation tests |
| CT-12 | **Pass** | −23.1 dBFS RMS at the default setting (48 kHz); peak ≤ 0.9; zero mean; the same loudness at 44.1 and 96 kHz; deterministic |
| CT-13 | **Pass** | |
| CT-14 | **Pass** | Static rings the resonators, moves RADIO LEVEL, is cut by pulling the audio cable, and is changed by off-centre detune |

Receiver-level, `tools/run_tuner_check.sh` → `wildband_tuner_check`: **17 checks, 0 failed**. Full output:
[`evidence/tuner-check.txt`](evidence/tuner-check.txt).

| ID | Result | Measured |
|---|---|---|
| CT-03 | **Pass** | Server delay 0 / 1 / 3 s → LIVE after 0.05 / 1.03 / 3.03 s, static audible for 0.10 / 1.08 / 3.08 s; none once LIVE |
| CT-04 | **Pass** | Time to LIVE 1.03 s with STATIC 0, 1.02 s with STATIC full |
| CT-05 | **Pass** | 404 and an Ogg stream: FAILED in 0.01 s, gap closed, static gone. Refused port: static while trying, zero by 8.5 s; state RECONNECTING (5 attempts in 11 s) |
| CT-06 | **Pass** | Server that never answers: zero by 8.5 s and silent through 10 more seconds of retries |
| CT-07 | **Pass** | 51 requests in 2 s: **2** connection attempts; ends LIVE on the last station |
| CT-08 | **Pass** | 0 blocks carrying another station's tone after the last request |
| CT-10 | **Pass** | During a real 3 s wait: static rings the engine (peak 0.66), PANIC gives exact silence until the station arrives, then it plays |
| CT-15 | **Pass** | A playing station whose server closes every 3 s: 2 reconnects, gap stayed closed, no static |

One failure on the way, fixed: the first coalescing attempt let 34 of 51 requests connect (a wake-up left over from
the previous request). The receiver now waits until requests have been quiet for 150 ms; a single request does not
wait at all.

In the app, on real stations (muted; [`evidence/tuner-live-navigation.txt`](evidence/tuner-live-navigation.txt)):
NEXT from SomaFM Groove Salad reached KEXP with static for the 1.4 s it took to connect and none afterwards; five
NEXT presses in a third of a second ended on the right station with 2 connection attempts; PREVIOUS wrapped round to
the last slot. The navigation was driven through the same counter a mapped MIDI button increments, not by hardware.
CoreAudio sweep on this build ([`evidence/coreaudio-minifuse-tuner-branch.md`](evidence/coreaudio-minifuse-tuner-branch.md)):
worst callback 4.1 % of the buffer period (96 kHz / 64), 0 late callbacks, positive control responds, no engine
faults. The radio was LIVE for 100 % of eleven windows and **84 % of one** (48 kHz / 512): the stream took longer than
the 5 s settling time to come back after that rate change, so part of that window measured the engine during the
wait. That row is not evidence about a fully excited engine.

## As built

- **STATIC** knob (tenth parameter, default 0.5, MIDI-learnable like any knob). At 0 nothing changes anywhere.
- **`<` and `>`** beside the station strip: previous / next, wrapping. **Click a station's name** to jump to it.
- **MIDI:** tick MIDI LEARN, click `<`, `>` or a station name, then press a button or pad. A controller fires on
  rising through half way; a note fires on note-on, and a note mapped this way no longer sets PITCH.
- The status line reads TUNING while the knob is travelling to a new station, then CONNECTING, BUFFERING, LIVE or
  FAILED, and says "static (synthesized)" while it is sounding.
- Static is injected at the engine's radio input, before the off-centre ring modulator and the patch.
- PANIC silences static for the rest of the current retune.

## Honest limits

- On a fast connection the wait is a fraction of a second, so the static is a blip (0.10 s against a server that
  answers at once). That is the real wait; nothing pads it. Whether a blip is musically worth having is for the
  owner. Launching the app is also a "retune", so the first connection has static too.
- A change of sample rate or output device restarts the stream and is treated as a retune, so it also gets static.
- A refused or silent server shows RECONNECTING, not FAILED, because the receiver keeps retrying; the static still
  stops at 8 s.
- A quick run of requests now costs up to 150 ms before the last one connects.
- Spontaneous dropouts of a playing station were exercised only where the buffer covered the reconnect.
- Station navigation actions and their MIDI mapping are untested with hardware. Presets are the six existing slots;
  there is no separate preset bank.
- The static's sound (band-passed noise with a wandering centre, hiss, sparse crackle) was chosen without listening.
