# WILDBAND — Wyrd DSP Reuse & World-Coupling Research

**Status:** inspected source; integration proposal only. **P0 impact:** none until owner audition and explicit prioritization. WILDBAND's principal differentiator remains **physical-world influence** (live broadcasts, room, optional weather/sensors and hands-on MIDI). Wyrd is valuable because it lets those influences interact musically, not because it adds generic effects.

## Source inspection

- [Wyrd engine and helper DSP](https://github.com/shortwavlabs/vcv-tapestry/blob/main/src/dsp/wyrd-dsp.h)
- [Generated Vult core header](https://github.com/shortwavlabs/vcv-tapestry/blob/main/src/dsp/generated/wyrd_core.h) and accompanying generated source (dependency to inventory)
- [VCV wrapper](https://github.com/shortwavlabs/vcv-tapestry/blob/main/src/Wyrd.cpp) and [module declarations](https://github.com/shortwavlabs/vcv-tapestry/blob/main/src/Wyrd.hpp)
- [Wyrd unit tests](https://github.com/shortwavlabs/vcv-tapestry/blob/main/src/tests/test_wyrd.cpp)
- [VCV Tapestry license](https://github.com/shortwavlabs/vcv-tapestry/blob/main/LICENSE.md) (GPLv3 text; verify authorship, Vult-generated source and dependencies before use in a separately licensed product)

The verified header has real implementations of `WyrdAmbientReverb`, `TimeFilterExperiment`, `StrengthProcessor`, `AgitationGenerator`, `WyrdCoreProcessor` and `WyrdEngine`. The generated core is **not** represented completely by the header alone. Wyrd's code already includes stability guards, but these do **not** qualify it for arbitrary live network-driven feedback graphs without further testing.

## Reuse candidates, ranked for WILDBAND

| Candidate | What exists in Wyrd | WILDBAND use | Recommendation |
| --- | --- | --- | --- |
| **Touchable modulation matrix** | `TouchSource`: MANUAL, CV1, CV2 (feedback), AGITATION, SUB_HARMONICS, STRENGTH, NOISE; destination amounts for activation, time, filter, absorb, tonic, decay | Switch between or combine *actual world signals* and performable physical interventions. Recreate as named typed CV routes rather than hard-coded menu alone | **Highest interaction priority** |
| **TimeFilterExperiment** | ~5 ms–2.45 s primary delay map, up to 2.75 s internal cap; five taps, fractional interpolation, three diffusion allpasses, wow, filtered/aged saturated feedback and CV2 feedback output | Live radio becomes a time-smeared, partially self-sustaining texture; room/onset/weather/MIDI CV can steer delay time, decay, filter and absorb | **Highest DSP audition priority** |
| **WyrdAmbientReverb** | Stereo modulated diffuse tank, multi-stage input diffusion, cross-coupled damped feedback, size/decay/diffusion/tone/modulation | Distinct atmospheric space for radio/Korupt. Patch **weather/room → size, damping, tail** and perform with a MIDI fader | **High priority**, standalone optional audio module |
| **StrengthProcessor + follower** | Input conditioning with selectable STREGA/LINE_GENTLE/MODULAR_HOT drive curves, DC blocking, shaped gain, envelope CV1, a simulated cable pickup component | Bring real external audio or live radio into a responsive system; expose envelope/strength as patchable CV. Distinguish synthetic simulated cable noise from actual electromagnetic data | **High priority**, avoid duplicate follower code |
| **AgitationGenerator** | Asymmetric rise/fall modulation from SPEED/ANGLE with gate-dependent one-shot behavior when begin/end connected; free-running otherwise | Radio speech/transient gates, room hits, or MIDI buttons can trigger evolving gestures without a timeline/sequencer | **Medium–high priority**, world-triggered |
| **WyrdCoreProcessor** | Vult-generated activated tone/sub-harmonic feedback-driven synth engine with TONIC, TONES, FILTER, ABSORB, DECAY, activation and interference | Potential second sound engine or resonator alternative; compare against WILDBAND's current processor rather than silently replacing it | **Research-only** |
| **Raw random noise/touch** | Seeded xorshift noise and jitter/pickup simulation | Modulation accent; compare with dedicated Drift CV generator | **Lower priority**: prefer Drift for organic random CV |

### Insight: Wyrd's TOUCH matrix is not the same as WILDBAND's patchboard

Wyrd combines a single selected `TouchSource` with individual **touch amount** controls for activation/time/filter/absorb/tonic/decay. WILDBAND can adapt this as a **performer intervention layer** above TALORA-style patching:

- Patch RADIO ENVELOPE → TOUCH source, or ROOM HITS → agitation gate, or Wyrd DELAY FEEDBACK CV → TOUCH amount.
- Assign a Launch Control XL fader/button to enable, scale, invert or momentarily apply the intervention. The player can *grab* the live world rather than replace it.
- Be explicit about what is observed vs simulated: NOISE in Wyrd is pseudo-random, not a live environmental sensor.
- A multi-source mixer/attenuverter can be a future enhancement; the original Wyrd selector itself is one selected source, so don't pretend arbitrary multi-route fusion is already implemented there.

## Suggested world-first experiments (ideas; unvalidated)

1. **Broadcast Memories** — RADIO AUDIO → Wyrd time-filter/delay → Wyrd reverb; RADIO ENVELOPE → delay time/decay; ROOM HITS → agitation gate; MIDI fader → wet feedback. Listen for ephemeral echoes following speech/music; no capture or recording.
2. **Weathering** — RADIO or KORUPT → Wyrd reverb; verified weather wind/precipitation → SENSE scale → damping/size/modulation. Weather needs timestamps, fresh/stale indication and safe neutral fallback; no invented live sensors.
3. **Touch the Room** — ROOM ENVELOPE → TOUCH SOURCE, midi pad holds a temporary TOUCH DEPTH to strongly influence decay/absorb/filter; release restores the environment-led behavior.
4. **Wyrd/Drift conversation** — Drift CV ↔ Wyrd time/filter parameters, but keep a real world signal routed to activation or excitation so randomness does not displace WILDBAND's point.
5. **Self-exciting radio cloud** — RADIO → Wyrd delay → resonator, with Wyrd feedback-derived CV to adjust resonance. Use explicit delayed bounded feedback loops and panic control.

All examples are proposals: no claim that these wiring schemes already function in WILDBAND.

## Technical review and qualification

- **Module boundaries:** start with separable Wyrd time-filter/reverb/strength/agitation processors, not an unconditional transplant of the entire engine or UI. Evaluate modularity against TALORA's typed audio/CV patching and existing WILDBAND DSP.
- **Dependencies and rights:** Wyrd `WyrdCoreProcessor` invokes Vult-generated `wyrd_core`, and generated artifacts may depend on Vult support headers/tables. Verify actual source/provenance and license compatibility with WILDBAND before copying any code.
- **Allocation/latency:** `TimeFilterExperiment` preallocates a buffer sized for 2.75 seconds at **384 kHz** (about 1.056 million floats, ~4.2 MB per instance); `WyrdAmbientReverb` holds several fixed-size vector delay/allpass buffers. Allocate and reset outside the hot audio callback; review memory-per-instance, reset behavior and graph instantiation.
- **Sample-rate contract:** delay core allows up to 384 kHz; `WyrdAmbientReverb::process` internally clamps sampleRate at **192 kHz**. Qualify response at 44.1/48/96/192 kHz and handle higher rates explicitly.
- **Safety:** examine rapid CV moves, variable delay interpolation, audio-level conversion from Rack's ±5/±10 V semantics, stereo/mono expectations, DC, denormals, clipping and feedback stability. Wyrd's internal `feedbackGain` can exceed 1 in `TimeFilterExperiment`, though nonlinear saturation and other factors bound internal signal; test when placed in additional external feedback networks. Add routing-cycle safety, ramped controls, fail-safe mute/panic.
- **Determinism/randomness:** noise uses fixed seeds in components and reset tests assert repeatability. Wyrd/Drift are internally pseudo-random; the non-repeatable performance derives from *live* broadcasts, physical-world state and performer gestures.
- **Tests already present:** Wyrd's source tests cover ranges/calibration, agitation, time-filter impulse and tails, touch-source behavior, modulation routes, reverb decay, reset parity, finite extreme values and input coupling. Keep as reference test vectors; augment with real broadcast excerpts and measured CPU/xruns. Test vectors do not establish live-musical quality.
- **Interaction:** parameter changes and world input routing must be audible/expressive while physical MIDI performance stays possible without looking at the display; provide clear world-source freshness/offline status.

## Proposed order of work

1. **Do not interrupt P0.** Audition current radio/room/MIDI/patching build first.
2. **Narrow experiment A:** stand-alone Wyrd delay/time-filter module on real RADIO with envelope and MIDI modulation; compare against current effects.
3. **Narrow experiment B:** Wyrd stereo reverb with ROOM activity or weather shaping size/decay/damping; report whether the outside world is *clearly* affecting the result.
4. **Interaction experiment:** TOUCH-source and gesture routing concept, perhaps using existing TALORA-style patching rather than duplicating a patch matrix.
5. Only after A/B/owner listening, decide between using Wyrd components, adapting selected algorithms, or retaining existing WILDBAND engines. Every accepted module needs distinct musical utility and measurable or audible world coupling.

**Decision rule:** A Wyrd-derived module qualifies because it strengthens the live world ↔ musician ↔ sound relationship, not simply because Wyrd has more effects.
