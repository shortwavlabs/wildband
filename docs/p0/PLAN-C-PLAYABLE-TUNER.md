# WILDBAND — Plan C: playable radio tuner (next experiment, not started)

**Status:** this was the plan. **It has since been carried out on branch `p0/tuner`**; what was built, the tests and
their results are in `TUNER-ACCEPTANCE.md` on that branch. The text below is kept as written. Originally: plan only. Source: `OWNER-FIRST-AUDITION.md` §C and
`RADIO-TUNER-PERFORMANCE.md` (docs branch). Branch when started: `p0/tuner`, from the attenuverter branch once the
owner has auditioned it (the patch hand-off changes there), otherwise from `p0/prototype`.

## The one question

Does filling the real connect-and-buffer wait with synthesized static, which excites the patch, turn station
changes into a gesture the owner wants to play?

## What must not change

The off-centre TUNE sound the owner singled out. Today it is one expression in `Engine::process`: a ring modulator
at `20 + 380·d²` Hz with gain `(1 − d + d·cos φ)(1 − 0.4·d)`, where `d` is `dial().offCentre` (clean inside the
middle 40 % of a station's arc). **First commit of the branch, before any tuner code:** a regression test that
renders the detune path at d = 0, 0.25, 0.5, 0.75, 1 against the frozen engine in `tests/baseline/` and requires
sample-identical output with static at zero. Static is a new, separate layer; it is never folded into the detune.

## Scope of the experiment

In:

1. **PREV / NEXT** buttons (wrap round) and direct preset selection, each MIDI-learnable as a button. TUNE still
   sweeps. All three end in the same single `tune(slot)` request.
2. **STATIC**: a synthesized texture that plays *only* while the receiver reports CONNECTING, BUFFERING or
   RECONNECTING for a deliberate retune, at a level set by a STATIC control (0 = exactly the current behaviour).
3. **Injection point: the radio input of the engine**, before the detune ring modulator and before
   `resonator.audio_in`. So static is detuned, patched, struck and resonated like a broadcast, and
   `radio.level_out` / `radio.activity_out` respond to it. It is not mixed after the engine.
4. Truthful status: TUNING → CONNECTING → BUFFERING → LIVE, or FAILED with the reason.

Out (deferred, per the audition notes): automatic or environmental hopping, HOP clock/trigger input, a second
receiver for pre-buffering, a STATIC character control, a separate STATIC patch output.

## Design

| Concern | Approach | Why |
|---|---|---|
| When static plays | `Radio::read` already knows, sample-accurately, when its gain is below 1 (fading out, waiting, fading in). Expose that as a per-block "gap" value 0..1 = `1 − radioGain`; static gain = `gap × STATIC × transitionEnvelope` | The texture then spans the *actual* wait: a 0.2 s connect gets 0.2 s, and nothing is delayed to make room for an effect |
| Fast stations | No minimum duration. The only fixed times are the existing 20 ms fades | "Fast readiness must not be delayed for a fake transition" |
| Failure and stalls | `transitionEnvelope` decays to zero over a bounded time (proposal: full for 4 s, gone by 8 s) and is forced to zero on FAILED. Then silence, or LOCAL if the owner has patched it | No endless noise during an outage; FAILED must not sound like "nearly there" |
| Underrun vs retune | Static only for a retune the player asked for (generation changed). A spontaneous underrun/reconnect keeps today's fade to silence, unless the owner asks otherwise after listening | A dropout should not masquerade as a gesture |
| Stale audio | Already handled and tested by design: `gen` / `sessionGen` / `ackGen` in `Radio`; PCM from a superseded session is discarded before the new one may write | Needs an explicit test (below), not new machinery |
| Rapid switching | TUNE already waits 250 ms at rest. Buttons: act on press, but coalesce — only the last request within 150 ms reaches `Radio::tune` | Bounded connection attempts; no retry storm |
| Tails | Nothing in the engine is reset on a retune (true today). Add a test | "Preserve existing DSP tails" |
| PANIC | Clears the engine and also zeroes the static envelope for the current transition | PANIC stays authoritative |
| Sound | Start with the cheapest thing: seeded noise → band-pass that wanders slowly → sparse crackle impulses; deterministic, no allocation, bounded peak (≤ −12 dBFS before STATIC), DC-free. Compare three settings by ear: off, plain noise, shaped | Decide by listening before investing in character |
| Honesty | The UI says STATIC is synthesized. `radio.level_out` during static is labelled as tuner activity in the docs; it is not broadcast analysis | Manifesto: simulated static must be honest |
| Feedback loops | Static cannot trigger anything in this iteration (no hopping input exists) | Removes the self-triggering risk by construction |

## Acceptance tests to register before coding

| ID | Test |
|---|---|
| CT-01 | Detune regression: with STATIC 0, output is sample-identical to the frozen baseline at five off-centre positions |
| CT-02 | STATIC 0 reproduces today's transition exactly (fade, silence, fade) |
| CT-03 | Static level follows the gap: zero while LIVE, non-zero only between fade-out and fade-in; duration equals the measured wait (local test server with 0.1 s, 1 s and 5 s start delays) |
| CT-04 | No added delay: time from `tune()` to full-level live audio is unchanged (±1 block) with STATIC at maximum |
| CT-05 | Failure: against `/missing`, `/ogg` and a refused port, static ends within the bound and the state reads FAILED |
| CT-06 | Stall: against `/stall`, static does not restart on each silent reconnect attempt beyond the bound |
| CT-07 | Rapid switching: 50 requests in 2 s end on the last station, with at most N connection attempts and no audio from any other station (each test stream carries a distinct tone) |
| CT-08 | Stale events: a session that delivers PCM after being superseded is never heard |
| CT-09 | Tails: resonator state is bit-identical across a retune compared with the same engine fed the same samples without one |
| CT-10 | PANIC during static: output is exactly zero from the next block until the station is live |
| CT-11 | Safety: static + FEEDBACK at the stop + every destination patched stays under the ceiling; no allocation; block-size independent; deterministic |
| CT-12 | Level: static RMS at STATIC = default sits within 6 dB of the test stations' measured RMS (−11 to −26 dBFS) — to be tuned by ear afterwards |

Owner audition (not automatable): the same six-station sequence with STATIC off, plain and shaped; preset jumps,
NEXT/PREV from a MIDI button, fast sweeps; high FEEDBACK during a retune; whether detune and static feel like two
related gestures or one muddle.

## Risks

- The wait is often only 0.2–0.5 s on good connections: the static may be too brief to play. If so, the honest
  options are to accept that or to make TUNE's between-station arc audible *while turning* (a gesture, not a wait).
- Static through six resonators at high FEEDBACK is the loudest thing the engine can be fed. CT-11 and a
  conservative default matter.
- Clicking through presets hits third-party servers repeatedly. Coalescing bounds it; broadcaster terms are still
  unexamined.
