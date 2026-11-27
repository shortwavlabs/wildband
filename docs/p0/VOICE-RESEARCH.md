# WILDBAND — Comparative voice research (Task D)

**Branch:** `p0/voice-lab`, on `p0/tuner-restarts`. **Status:** the method, the per-candidate briefs and the
acceptance criteria in this file were committed before any harness or experiment code; results are appended at the
bottom. Background: `PLAN-D-COMPARATIVE-DSP.md`, the manifesto, and the reuse studies in `docs/`.

**No musical conclusion is drawn anywhere in this document. Only the owner's listening can do that.**

## Why

The owner's verdict on the current voice: it works and is fun, but "the resonator isn't particularly expressive".
The aim is one cohesive WILDBAND voice — possibly a few complementary primitives — not a rack of processors.
Every candidate has to answer the manifesto's question: what gesture, character or world-to-sound relationship does
it add? "It already exists" is not an answer.

## Method

1. **The current resonator stays as the baseline**, with DRY and REFERENCE beside it.
2. **One experimental interface for every candidate**: stereo excitation in, stereo out, the instrument's existing
   controls (PITCH, SHAPE, TONE, DRIFT, FEEDBACK) and its six control inputs, so the same TUNE, static, patch bay,
   per-cable depths, COUPLE, MIX and PANIC apply to all. A candidate gets **no new panel control** for the comparison.
3. **Two harnesses:**
   - *Offline*, `wildband_voice_lab`: renders every candidate over the same fixtures, with a WORLD cable connected
     and pulled, **level-matched** to −20 dBFS RMS, into WAV files for quick A/B, plus a table of cost, memory,
     peak and how much the WORLD cable changed the sound. Candidates that live in other repositories are compiled
     **from their own repository, in place** (`-DTAPESTRY_DIR=…`); nothing of theirs is copied here.
   - *Live*, the LAB menu: candidates that are WILDBAND code can be switched while playing a real station.
4. **Fixtures are synthesized by the harness** — speech-like (a formant voice with pauses), percussion, music (a
   chord sequence), silence, and the tuner's own static. No recordings: in particular none of AFTERIMAGE's Freesound
   files, and no broadcast audio. The render files are written to the build directory and are not committed.
5. **WORLD**: each candidate is rendered with one meaningful environmental route (a room-activity track driving one
   of its inputs) connected and disconnected, same patch otherwise. In the offline harness that track is a
   **scripted stand-in** (bursts and a slow swell) and is labelled as such; real room influence is a live, owner test.
   Internally generated modulation (Drift, the built-in drift) is always reported separately from WORLD.
6. **Reported separately, never blended into a score:** cost · safety · responsiveness to controls and WORLD ·
   licensing · owner preference (blank until the owner listens).

## Candidate briefs

### D0 — Resonator (baseline, in the instrument)

| | |
|---|---|
| Purpose, character | Tuned sympathetic strings struck by the broadcast; from comb-like colouring to self-sustaining drone |
| WORLD destinations | All six inputs exist; default patch uses FEEDBACK, PITCH, SHAPE from ROOM, STRIKE/TONE/DRIFT from RADIO |
| Controls | PITCH, SHAPE, TONE, DRIFT, FEEDBACK |
| Cost, memory | About 75 ns/sample synthetic; six delay lines, about 0.2 MB at 48 kHz |
| Safety | Bounded by construction; tested |
| Licensing | WILDBAND's own |
| Uncertainty | The owner's: not expressive enough. Is it the timbre, or how it is driven? |

### D1 — Korupt (CMOS/PLL-style tracking, subharmonics, instability)

| | |
|---|---|
| Purpose, character | A phase-locked oscillator that tries to follow the broadcast's pitch and fails in interesting ways: squares, divided-down subharmonics, lock and loss of lock. The radio would *steer* a synth voice rather than be filtered |
| WORLD destinations (proposed) | Loop rate (how fast it chases), subharmonic mix, the comparator's sensitivity; LOCK / TRACKING as control *outputs* back into the patch |
| Minimal controls | RATE → DRIFT knob, oscillator/sub program → SHAPE, mixes → TONE; root via PITCH |
| Cost, memory | To be measured by the harness. No buffers; a few dozen floats |
| Safety | Output is built from squares and one-poles, so bounded; a naive (non-band-limited) square means aliasing that changes with sample rate — part of the sound, but it must be a decision |
| Licensing | GPL-3.0-or-later, authorship confirmed by the owner. **Uses VCV Rack SDK types** (`RCFilter`, slew and exponential filters, `clamp`, and `SchmittTrigger` in the module wrapper): these must be replaced by WILDBAND's own one-poles, not copied. **Integration blocked** until that rewrite and until WILDBAND's licence is applied. Study in place is allowed |
| Existing tests | `src/tests/test_korupt.cpp` in its repository (need the Rack SDK) |
| Uncertainty | Does it track anything on dense music or only on speech and single lines? On silence and on static? Is "unstable" musical here or merely broken? |

### D2 — Dissolve (rolling granular delay, after AFTERIMAGE's TRACE → CLOUD idea)

| | |
|---|---|
| Purpose, character | The broadcast leaves a trail: at one end a clear echo (TRACE), at the other a cloud of overlapping fragments (CLOUD). Station changes smear into each other |
| WORLD destinations | Dissolve amount, spray, regeneration, time |
| Minimal controls | TIME ← PITCH, DISSOLVE ← SHAPE, SPRAY ← DRIFT, regeneration ← FEEDBACK, damping ← TONE |
| Cost, memory | To be measured. A short stereo history (2 s), continuously overwritten; up to 24 grains |
| Safety | Regeneration through a saturator and a low-pass, gain below 1; cleared by PANIC |
| Licensing | **Written new for WILDBAND.** It follows the idea described in the AFTERIMAGE reuse studies; it does not use AFTERIMAGE's capture, selection, IMPRINT or GHOST code, nor its FFT path, nor its recordings. So it is not blocked |
| Reference evidence | AFTERIMAGE's own real-time gate was not met (96 kHz / 32 samples, p99.9 26.7–27.5 % against 25 %). That engine is FFT-based; this is not, but the cost here must be measured, not assumed |
| Uncertainty | Is a granular trail a *voice* or an effect after one? Does it need pitch to belong to this instrument? |

### D3 — Wyrd (feedback-aware time/filter, reverb, strength, TOUCH)

| | |
|---|---|
| Purpose, character | A delay whose feedback path is filtered, diffused and saturated so that it ages; an atmosphere behind the voice |
| Which primitives | *Time/filter delay*: the main candidate. *Reverb*: overlaps with it and with Dissolve's cloud — only one "space" should survive. *Strength* (nonlinear level processing): overlaps with the engine's saturators. *TOUCH*: a control-routing idea, already covered by the patch bay and depths |
| WORLD destinations (proposed) | Decay, absorb, time modulation |
| Minimal controls | TIME ← PITCH, decay ← FEEDBACK, filter ← TONE, absorb ← SHAPE |
| Cost, memory | To be measured. As written it allocates 2.75 s × 384 kHz of delay (about 4.2 MB) in its constructor: a WILDBAND version would size for the actual rate |
| Safety | A loop-gain term above 1 held by a saturator; "does not qualify it for arbitrary live network-driven feedback graphs" per its own reuse study |
| Licensing | GPL-3.0-or-later, authorship confirmed, plain C++. Portable once WILDBAND's licence is applied. Study in place now |
| Uncertainty | Does it complement the resonator (space after strings) or duplicate Dissolve? |

### D4 — Drift (organic control source, not a voice)

| | |
|---|---|
| Purpose | Slow, lifelike movement to patch into any input — to *complement* real WORLD signals when the room is quiet, never to pose as one |
| Destinations | Any control input, with its own depth |
| Controls | SPEED and RANGE at most; possibly none (fixed character, depth on the cable) |
| Cost, memory | To be measured; about 20 floats |
| Licensing | GPL-3.0-or-later Vult source by the owner; generated C++ reproduced byte for byte with `vultc` 0.4.15; Vult runtime MIT. Portable once WILDBAND's licence is applied |
| Naming | WILDBAND already has a DRIFT knob and input; the source would need another name in the UI |
| Uncertainty | Is it audibly better than the built-in drift or a plain slow LFO? It must be labelled as internal modulation |

## Acceptance criteria (registered before implementation)

Harness:

| ID | Passes when |
|---|---|
| VL-01 | `wildband_voice_lab` builds with no external path and renders the in-repo voices (dry, reference, resonator, dissolve) over all five fixtures, WORLD on and off |
| VL-02 | Every render is finite, peaks below full scale after matching, and is within 0.5 dB of −20 dBFS RMS (silence-in renders excepted and reported as silent) |
| VL-03 | The report lists, per voice: ns/sample, memory, pre-match peak, crest factor, and the WORLD on/off difference in dB |
| VL-04 | With `-DTAPESTRY_DIR` (and `-DRACK_SDK_DIR` for Korupt) the external candidates are compiled from their own repository and rendered the same way; without them the harness still builds |
| VL-05 | Nothing from another repository and no audio file is added to this repository |

Dissolve (first eligible experiment):

| ID | Passes when |
|---|---|
| DV-01 | Silence in gives silence out; output is deterministic |
| DV-02 | With DISSOLVE, SPRAY and regeneration at zero it is a plain delay: an impulse comes back once, at the set time (±1 sample) |
| DV-03 | Bounded and finite at maximum regeneration with hostile input; no allocation while processing |
| DV-04 | PANIC clears its memory: silence in then gives exactly silence out |
| DV-05 | With the resonator selected, the instrument is unchanged (all earlier checks pass); switching voices while playing adds no step above 0.03 |
| DV-06 | Its memory is temporary: a fixed-size history that is overwritten continuously and never read back after `reset`, never written anywhere else |

Live: the LAB menu offers Resonator (default) / Dissolve / Dissolve → Resonator.

Out of scope for this branch: porting Korupt, Wyrd or Drift; any panel change; choosing a voice.

---

## Results (added after implementation, 2026-11-27)

**Numbers and files only. Nobody has listened to any of these renders or to the live LAB voices. No candidate is
preferred.**

### Harness

| ID | Result | Notes |
|---|---|---|
| VL-01 | **Pass** | Compiled and run with no external path (61 files: five in-repo voices × six fixtures × WORLD on/off, plus the report). First marked passed without having been run that way; corrected after review |
| VL-02 | **Pass with one exception** | All renders finite. Matched to −23 dBFS RMS (the −20 dBFS first planned left too little headroom for crest factors near 20 dB). One file, `percussion__dissolve`, would still have clipped and was turned down 1.5 dB; the report lists it. A render below −70 dBFS is left alone and reported as silent (the first version amplified Wyrd's −93 dBFS idle noise up to the matched level) |
| VL-03 | **Pass** | Full table: [`evidence/voice-lab-report.md`](evidence/voice-lab-report.md) |
| VL-04 | **Pass** | With `-DTAPESTRY_DIR` and `-DRACK_SDK_DIR`, Wyrd's time/filter delay, Drift and Korupt are compiled from `vcv-tapestry` in place, Korupt against the Rack SDK headers |
| VL-05 | **Pass** | No code from another repository and no audio file is in this repository. The renders (about 100 WAV files, about 190 MB) are in the build tree; Korupt's are in a `korupt-unverified` subfolder |

To make the listening files:

```bash
cmake -S . -B build -DTAPESTRY_DIR=$HOME/Workspace/shortwavlabs/vcv-tapestry -DRACK_SDK_DIR=$HOME/Workspace/rack-plugin-toolchain/Rack-SDK-mac-arm64
```

```bash
cmake --build build --target wildband_voice_lab && build/wildband_voice_lab build/voice-lab
```

Files are named `fixture__voice__world-on.wav` / `…world-off.wav`, so sorting by name puts the voices for one
fixture side by side.

### Dissolve (D2, new WILDBAND code)

`wildband_tests`: **831 checks, 0 failed** (788 before, plus DV-01 … DV-07).

| ID | Result |
|---|---|
| DV-01 … DV-04 | **Pass** |
| DV-05 | **Pass.** With the resonator selected nothing changed (all earlier checks, including sample-identical parity with the frozen engine); switching between the three LAB voices every 50 ms adds no step. **Extended after review:** jumping SHAPE, TONE, FEEDBACK, MIX, LEVEL, DRIFT or COUPLE with a LAB voice selected adds no step either — Dissolve's parameters were first passed in unsmoothed and are now smoothed like the resonator's |
| DV-06 | **Failed as first built, fixed after review.** While deselected, Dissolve's history was neither written nor cleared, so re-selecting it could replay up to 1.6 s of whatever had been on air earlier — contrary to "temporary, continuously overwritten". Now, once the voice is deselected and has faded out, the history is erased (in pieces, so no single audio callback pays for it) |
| DV-07 (added) | **Pass.** Dissolve hears noise for a second, is deselected for three, and is re-selected with silent input: the output is sample-identical to an engine in which Dissolve had never run |

Measured: about 14 ns per sample on top of the engine (one thread, synthetic); 1.0 MB of history at 48 kHz.
CoreAudio callback timing with Dissolve selected: see `OWNER-AUDITION.md`, "What was measured on this build".

### What the numbers show (and what they cannot)

Per-sample cost, one thread, 48 kHz: whole engine with the resonator about 50–64 ns; with Dissolve about 63–72 ns;
Korupt as written 38–49 ns; Wyrd's time/filter delay as written 65–85 ns; Drift's core 31 ns (it runs at audio
rate; a control source would run at control rate, about 16 times cheaper). Memory: Wyrd as written 4.4 MB, Dissolve
1.0 MB, resonator 0.1 MB, Korupt and Drift negligible.

"WORLD on vs off" (how much the scripted room track changed the output, relative to the output), from the current
report: every candidate responds, by between about −9 and +9 dB depending on voice and fixture. These figures say
that a difference exists, not that it is musical; see the table in the evidence file for each one.

Findings worth acting on:

- **Korupt, as written, does two things that need the owner's knowledge of it before anything is concluded.**
  Driven the way its own module drives it, it reported *lock ≈ 0 on every fixture, including a pure 220 Hz sine*,
  "tracking" 1.0 even on silence, and an output near full scale (−0.5 to −2.8 dBFS RMS, crest 3–5 dB) **including
  on silence**. A diagnostic then drove its core exactly as its own unit tests do (a 400 Hz square straight into
  `KoruptDSP`, default parameters):
  - *Lock stayed at 0.000 there too* (highest value in 3 s: 0.000, with tracking at 1.00). Its tests only assert that
    lock is finite and within 0..1, never that it rises. So "never locks" is not the lab's wiring; whether it is the
    default parameters, a property of the loop, or a defect is not established.
  - *Zero straight into the core gives tracking 0.000* (as its test expects). But its **input stage fed silence
    settles at audio = −0.277 and comparator = +0.843**, and the module hands that "audio" to the core — so behind
    its input stage, silence is not zero, which is why it "tracks" and sounds on silence.
  For WILDBAND, where silence between stations is real and valid, that second point matters: a Korupt voice would
  need gating or a different input conditioning. The Korupt renders are kept in `korupt-unverified/` for that reason.
  Also: the first WORLD route tried (room activity → RATE) changed nothing in its output.
- **Wyrd's delay is not silent on silence** (−105 dBFS idle); harmless, but a WILDBAND version should be exactly
  silent after PANIC. As written it allocates 4.4 MB whatever the sample rate. Its renders are wet-only, like every
  other voice here (the first version mixed in half the dry signal, which was not like-for-like); wet-only it sits
  about 8–20 dB below the fixture before matching.
- **Dissolve's cloud has a high crest factor** (up to 19–21 dB on percussion). Loud grains will meet the engine's
  safety clip; the grain normalisation needs listening and probably revising.
- **Drift against a plain sine** as a control source: within 0–2 dB of each other in how much they change the
  resonator's output on these fixtures. Whether Drift *sounds* more alive is exactly what a difference figure cannot
  say.

### Live

**LAB → Voice experiment**: Resonator (default) / Dissolve / Dissolve → Resonator, switchable while a station
plays, on the same knobs and cables (PITCH = time, SHAPE = echo…cloud, DRIFT = spray, FEEDBACK = regeneration,
TONE = damping). The status line says when a LAB voice is on. The selection is remembered per settings profile.

### Status of each candidate

| | Studied | Renders for the owner | Live in the LAB menu | Integration |
|---|---|---|---|---|
| D0 Resonator | — | yes | yes (default) | in the instrument |
| D1 Korupt | compiled in place; **behaviour not established** | yes, but see above | no | blocked: Rack SDK types to replace; licence not yet applied |
| D2 Dissolve | new code | yes | yes | experimental; no blocker |
| D3 Wyrd time/filter | compiled in place | yes | no | allowed once the licence is applied; needs resizing and exact silence |
| D3 Wyrd reverb, strength, TOUCH | read only | no | no | reverb overlaps Dissolve and the delay; strength overlaps the engine's saturators; TOUCH is already the patch bay |
| D4 Drift | compiled in place; reproduced from source | yes (as modulation) | no | allowed once the licence is applied; needs a control-rate version and another name |

### Remaining uncertainties

Musical: everything. Technical: Korupt's behaviour; CoreAudio cost of the LAB voices; whether Dissolve needs pitch
to belong to this instrument; whether one "space" (Dissolve's cloud, Wyrd's delay, Wyrd's reverb) is enough.
