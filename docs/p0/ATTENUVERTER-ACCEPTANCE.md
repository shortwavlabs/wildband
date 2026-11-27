# WILDBAND — Per-connection modulation depth: contract and acceptance tests

**Branch:** `p0/attenuverter`, from `p0/prototype` (the build the owner auditioned, plus measurement-only changes).
**Status:** this file was committed **before** the implementation. Results are filled in at the bottom afterwards.
Source of the contract: `docs/p0/OWNER-FIRST-AUDITION.md`, section B.

## Contract

1. Every control (CV) cable has its own signed **depth** in [−1, +1], shown as −100 % … +100 %.
2. **+1 reproduces the auditioned behaviour exactly.** A patch saved before this change loads with every depth +1.
3. **0 is no influence**: the destination behaves exactly as if the cable were not there.
4. **−1 reverses the cable's influence** relative to the unmodulated parameter, inside each destination's valid range.
5. Depth multiplies the source's 0..1 value before the destination's own mapping. Sources are not re-biased, so a
   source that reads 0 (ROOM off, denied or stale) has no effect at any depth.
6. Global **COUPLE** stays the master depth of all control cables; at 0 nothing modulates.
7. One output fanned out to several inputs has an independent depth on each cable.
8. Audio cables have no depth. Typed ports, one cable per input and all port ids are unchanged.
9. The audio thread always sees a coherent patch (a source never paired with another cable's depth), without
   locks or allocation. Depth and route changes are smoothed.
10. Only configuration is persisted. Depths read from disk are validated (range, finiteness).

### Negative depth, destination by destination

| Destination | At +1 (unchanged) | At −1 (new) | Guard |
|---|---|---|---|
| PITCH | up to +1 semitone | up to −1 semitone | resonator frequency clamp (existing) |
| SHAPE | pushes SHAPE up by 0.35 | pushes it down by 0.35 | clamped to the knob's range (existing) |
| TONE | opens the damping filter up to +3 octaves | closes it up to −3 octaves | cutoff never below 60 Hz |
| STRIKE | steady excitation backs off, the signal strikes | steady excitation stays, the signal **ducks** it | excitation gain never negative |
| DRIFT | drift runs up to 4× faster | drift runs down to 4× slower | rate factor always positive |
| FEEDBACK | up to +0.2 loop gain | up to −0.2 loop gain | loop gain never negative, upper limit unchanged |

## Acceptance tests (automated, deterministic unless stated)

| ID | Test | Passes when |
|---|---|---|
| AT-01 | Legacy parity | With every depth +1, output is sample-identical to the frozen baseline engine (`tests/baseline/`, commit `9c0a409`) for the default patch and other patches, across control states, room values and 44.1/48/96 kHz |
| AT-02 | Migrated patch | A saved patch without depths (`from\|to;…`) loads with every depth +1 and yields the same plan as before |
| AT-03 | Zero is neutral | For each of the six destinations, a cable at depth 0 gives output sample-identical to no cable |
| AT-04 | Sign and direction | For each destination, −1 differs from 0 and from +1, and moves the destination's observable the opposite way to +1 (frequency, ratio, brightness, excitation, drift movement, decay) |
| AT-05 | Independent fan-out | One source into two inputs with depths (d, 0) equals the first cable alone; changing one depth leaves the other destination's observable unchanged |
| AT-06 | COUPLE is the master | With COUPLE at 0, any depths give output identical to no control cables |
| AT-07 | Unavailable source stays neutral | With ROOM reading 0, ROOM cables at −1, 0 and +1 give identical output |
| AT-08 | Numerical safety | Finite, never above the 0.89 ceiling, zero engine faults: every destination at −1 and at +1 with knobs at their extremes, hostile input, and plans carrying NaN, infinity and out-of-range depths; block-size independent |
| AT-09 | Click-free | Depth jumps between −1 and +1, and re-patching, add no step above 0.03 to a steady tone |
| AT-10 | No allocation | No allocation on the processing thread while plans are published and read |
| AT-11 | Coherent hand-off | A writer thread publishing plans and a reader thread reading them concurrently: every plan read is one the writer published, whole (never a mixture), and the reader ends on the last one |
| AT-12 | Malformed state | Garbage, unknown ports, NaN / huge / missing depths, a depth on an audio cable, duplicates: loading yields a valid graph with every depth in [−1, +1] and audio depth +1 |
| AT-13 | Graph rules | New cables default to +1; `setDepth` clamps, rejects non-finite values, missing cables and audio cables; existing validation unchanged |
| AT-14 | TALORA topology parity | `wildband_talora_parity` still reports 0 disagreements (optional target, needs the private repository) |
| AT-15 | PANIC | After reset, silence in gives exactly silence out with every destination at −1 and FEEDBACK at the stop |

Measured, not pass/fail: synthetic cost of `Engine::process` before and after (`wildband_tests --metrics`), and the
CoreAudio callback sweep on this branch's build with the radio confirmed live.

## Not covered by automated tests (owner)

- Whether a depth control makes ROOM (or the radio) more *playable*: audition ROOM → FEEDBACK and ROOM → STRIKE at
  0, about +25 %, +100 % and −100 %.
- Whether selecting a cable and turning DEPTH is quick enough to be a performance gesture.
- Whether the negative-depth behaviours above are the musically right ones (STRIKE ducking and DRIFT slowing are
  design choices, not derived from the old behaviour).

None of the automated tests is evidence of sound quality.

## Deliberately out of scope for this iteration

Mixing several sources into one input; trigger/gate port types; source offset or re-biasing; MIDI control of a
cable's depth (the selection-follows-MIDI semantics need to be designed first, so a controller can never retarget
by accident); undo.
