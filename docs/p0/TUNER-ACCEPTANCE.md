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
