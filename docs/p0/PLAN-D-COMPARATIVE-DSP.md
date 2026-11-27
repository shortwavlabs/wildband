# WILDBAND — Plan D: comparative voice research (plan only, nothing integrated)

**Status:** research plan. No candidate code has been copied, ported or built. Sources: `OWNER-FIRST-AUDITION.md`
§D, the manifesto, the reuse studies on the docs branch, issue #2, and a survey of the candidates' actual source on
2026-11-27 (paths below). **How much of this is checked:** the survey was done by a delegated read-only pass. I then
re-checked these myself against the files: both repositories are public; `vcv-tapestry`'s licence text (GPL v3) and
`plugin.json` ("GPL-3.0-or-later"); the Rack includes and `rack::math` calls in `korupt-dsp.h`; the file lengths;
Wyrd's feedback-gain line; AFTERIMAGE's SPDX header and the failed-gate text and figures in its P1-B qualification.
Everything else in the table (primitive boundaries, state sizes, per-sample operation counts, Drift's details) is
from the survey and **not independently verified**. No candidate's tests were run.

## Why

The owner's verdict on the current voice: fun, feels like an instrument, but "the resonator isn't particularly
expressive". The aim is **one** voice with an identity, not a rack. The manifesto's test for every function applies:
what gesture, character or world-to-sound relationship does it add? "It already exists" is not an answer.

## Candidates, as found

| | Korupt | AFTERIMAGE | Wyrd | Drift |
|---|---|---|---|---|
| Role considered | Nonlinear CMOS/PLL-style voice or colour | Rolling granular delay (TRACE → CLOUD) | Feedback delay / space | **Modulation source only**, not a voice |
| Where | `vcv-tapestry` (public), `src/dsp/korupt-dsp.h`, 472 lines | `after-image` (public), unmerged draft branches; `plugin/Source/dsp/Engine.{h,cpp}`, 346 + 883 lines | `vcv-tapestry`, `src/dsp/wyrd-dsp.h`, 964 lines | `vcv-tapestry`, `src/dsp/drif-dsp.h` + generated Vult core, about 430 lines |
| Licence of the source | GPL-3.0 (repo `LICENSE.md`; no per-file header) | AGPL-3.0-only (per-file SPDX) | GPL-3.0 | GPL-3.0; Vult runtime files are MIT (third party) |
| Framework ties | **VCV Rack SDK**: `rack::dsp::RCFilter`, slew limiters, `SchmittTrigger` in the module wrapper | JUCE (`juce::dsp::FFT`, `AbstractFifo`); grain code itself plain C++ | Plain C++ | Plain C++ + Vult runtime |
| Smallest primitive with the character | `KoruptInputStage::process` + `KoruptDSP::process`; about 15 one-pole states, no buffers | `generateGrains` / `admitGrains` / `renderVoice`, private members of one `Engine`, reading a **frozen captured slot**, not a rolling history | `TimeFilterExperiment`: delay with allpasses and saturation; about 4.2 MB of delay memory allocated in its constructor | `DriftCoreProcessor::process`; about 20 floats |
| Known cost | None measured. About 5 `tanh` + 1 `pow` per sample by inspection | Measured: about 90–100 µs per 32-sample callback at 96 kHz | None measured. 5 cubic reads, 3 `tanh`, 5 `exp` per sample by inspection | None measured; runs per sample with several `exp`/`sin` |
| Open or failed gates | None documented | **Real-time gate NOT MET** in its own qualification: 96 kHz / 32 at p99.9 26.7–27.5 % against a 25 % limit, identical code failing 2 of 3 runs, cause unexplained; xrun instrument failed its control; hosts and listening not assessed | None documented; its guards "do not qualify it for arbitrary live network-driven feedback graphs" (reuse study) | None documented |
| Notable in the code | Naive (non-band-limited) square VCO behind fixed low-passes: aliasing is part of the sound and will change with sample rate | Built around capture; the docs propose a *new* rolling variant, i.e. mostly new code | A loop-gain term reaches 1.13 (`0.05 + 1.08·smoothstep`, line 680) before a further `persistence` factor; the survey reports a `tanh` at ±1.2 on the write | Name clash: WILDBAND already has a DRIFT knob and input |

Not established: authorship and contributor rights for any of these repositories; whether their tests pass today;
any CPU cost for Korupt, Wyrd or Drift.

## Licensing gate (before a single line is ported)

WILDBAND has no licence. The candidates are GPL-3.0 and AGPL-3.0-only; JUCE is AGPLv3 or commercial; Korupt also
leans on Rack SDK types (GPLv3-or-later with a non-commercial plugin exception). Shortwav Labs can relicense what
it wholly owns, but only the owner can confirm that it does, and Rack SDK and Vult code are third-party. **Owner
decision needed:** WILDBAND's licence, and confirmation of sole authorship of the three DSP headers. Until then the
experiment may *study* the sources and write clean WILDBAND code against the published behaviour; it must not paste.

## Method

1. **A voice slot, not a rewrite.** Extract the interface the current resonator already has — stereo excitation in,
   stereo out, six control inputs, `prepare` / `reset` / `process`, the safety clip outside it — so candidates
   plug into the same TUNE, patch bay, depths, COUPLE, MIX and PANIC. The current resonator stays as voice A and
   REFERENCE and DRY stay as they are. This is the only engine change before listening.
2. **Smallest primitive first**, one candidate at a time, each on the experiment branch `p0/voices`:
   - *Korupt*: a Rack-free rewrite of the input stage + PLL core (replace five Rack filter/slew types with one-poles).
     Question: does radio-driven lock/unlock give a playable instability? Expose LOCK or TRACKING as a control
     output only if it earns a socket.
   - *Wyrd*: `TimeFilterExperiment`'s topology with bounded delay memory (seconds, not 2.75 s × 384 kHz) and
     per-block coefficient updates. Question: is a feedback space after (or instead of) the resonators the missing
     expressiveness?
   - *AFTERIMAGE*: a new rolling grain reader over a short live history (no capture, no freeze, nothing kept),
     raw-history path only; the FFT path stays out unless listening demands it. It starts with a known performance
     blocker, so it goes **last** and only if the first two leave a gap.
   - *Drift*: evaluated separately as a control source feeding existing inputs, against a plain slow LFO and against
     the current built-in drift. Renamed in the UI to avoid the DRIFT clash. Never described as a world signal.
3. **Same conditions for every candidate:** the six default stations plus the local test source; the same three
   fixtures (speech, music, noisy) generated or licensed for local use only and never committed if they contain
   broadcast audio; levels matched to within 1 dB RMS at the output before listening; ROOM cable connected versus
   pulled on an unchanged patch; the same PITCH/SHAPE/TONE/FEEDBACK gestures.
4. **Four separate reports per candidate**, never blended into one score:
   - *Cost*: synthetic ns/sample, then the CoreAudio sweep (44.1/48/96 kHz, 32–512 samples) with the radio
     confirmed live and the positive control. A candidate that cannot hold 96 kHz / 32 is recorded as such, not tuned
     until it passes.
   - *Safety*: the existing bounded/finite/no-allocation/click-free/PANIC tests, run unchanged against the slot.
   - *Expressiveness*: can each of the six control inputs and both signs of depth be heard? (Measured as the same
     observables the attenuverter tests use; this says "responds", not "sounds good".)
   - *Owner preference*: blind where practical (voice order shuffled), same patch, 10 minutes each.
5. **Decision rule.** Pick one direction: keep A, replace A, or A plus one stage (e.g. resonator → space). Adding
   two or more candidates needs a stated reason that survives the manifesto's question. A candidate nobody prefers
   is deleted from the branch, with its evidence kept.

## Order and stop points

Voice slot + parity tests → Korupt core → Wyrd delay → (owner listening checkpoint) → AFTERIMAGE rolling grains only
if warranted → Drift as CV, any time after the attenuverter work is accepted. Stop and report at the checkpoint;
do not build all four and then ask.

## What would make this plan wrong

- If the owner's "not particularly expressive" is really about *control* (depths, tuner) rather than timbre, the
  attenuverter and tuner auditions will show it and this plan should shrink to "tune the resonator".
- If WILDBAND cannot be GPL/AGPL-compatible, every candidate here becomes a clean-room reimplementation or is out.
