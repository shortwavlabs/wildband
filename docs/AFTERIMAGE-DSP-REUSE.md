# WILDBAND — AFTERIMAGE Spectral / Granular Delay Reuse

**Status:** post-P0 research proposal. No port, realtime qualification, or new P0 requirement. Preserve the current microphone/output fix and owner audition. The intent is to reuse existing Shortwav Labs DSP and evidence, not to rebuild AFTERIMAGE inside WILDBAND.

## Verified prior work

- [AFTERIMAGE research consolidation](https://github.com/shortwavlabs/after-image/blob/main/docs/RESEARCH-CONSOLIDATION.md): offline P0-A/B/C evidence, real-audio audition, provisional decisions and blockers.
- [P1-A realtime core (draft PR #6)](https://github.com/shortwavlabs/after-image/pull/6): C++ streaming STFT and history, FULL/FILTERED reconstruction, bounded capture scheduling, CLOUD (32-grain admission cap, SplitMix64, Catmull-Rom reads), TRACE → CLOUD DISSOLVE and fades. Reports 148/148 reference comparisons within limits and 4,304 checks passing, but the worst-case callback gate was not met; C++ listening not assessed.
- [P1-B performance capture (draft PR #7)](https://github.com/shortwavlabs/after-image/pull/7): MANUAL/RANDOM/AUTO/HOLD functional evidence, activity gating, flux/level, cooldown/hysteresis. Reports 4,584 checks and zero audio-thread allocations in its tested configurations; 96 kHz/32-sample device callback timing gate failed (99.9th percentile 26.7% vs 25% criterion), xrun component not assessed. Do not equate this with a qualified reusable realtime engine.
- [Licensing policy](https://github.com/shortwavlabs/after-image/blob/main/docs/LICENSING.md): original project AGPL-3.0-only; verify provenance, dependencies and rights/compatibility with WILDBAND's undecided distribution license before copying code.

The name "spectral granular delay" is a useful musical shorthand, not an assertion that AFTERIMAGE is a conventional live spectral delay. The research moved from IFFT spectral ghost synthesis toward granular synthesis over reconstructed time-domain material, with STFT used for analysis, selection and optional filtering. The WILDBAND rolling live-delay adaptation is **new research**.

## Why reuse it

Owner audition preferred the granular cloud over earlier spectral loops in the P0-B engine sets; CLOUD was the leading all-round candidate, FOCUS useful in selected material, and the parallel TRACE-to-CLOUD morph preferred in several comparisons. The owner found the delay-to-granular tail appealing and short sounds (drums, vocals) generally more interesting than sustained notes. That makes speech, percussion and transient-rich live radio particularly useful candidates for a new continuous processor. The owner liked some GLITCH/S3 results, but not consistently. Do not claim universal superiority or that a spectral mask is inherently better than conventional filtering: prior comparisons did not establish that advantage.

## Reuse priorities

| Candidate | WILDBAND adaptation | Priority |
| --- | --- | --- |
| CLOUD grain scheduler, windows, interpolation, stereo balance and bounded voice pool | Continuously launch grains from a recent live delay history; fixed admission budget, safe source-age bounds | First DSP audition |
| TRACE → CLOUD parallel DISSOLVE | Move from coherent echoes to diffuse granular textures; compare a conventional delay TRACE with adapted loop-based TRACE | First interaction audition |
| FULL vs FILTERED source conditioning | Broadband or band-limited recent material; compare a cheap conventional filter with spectral selection only where it adds clear value | High, filter-first |
| Onset/activity analysis and cooldown logic | Let radio syllables, room transients and MIDI gestures influence grain activity or density; no mandatory capture action | High, adapt detector and discard capture workflow |
| FOCUS harmonic-position guidance | Evaluate musically meaningful spectral guidance for grain selection on live broadcasts | Later |
| GLITCH / S3 frequency-dependent grain behavior | Short upper-band grains, reverse, spray and stereo dispersal, possibly retaining coherent lower-band delays | Experimental; conventional crossovers first |
| STFT, WOLA, spectral history | Reuse only what a chosen spectral mode actually needs; no full capture UI/history requirement | Conditional on benefit and cost |

## Proposed WILDBAND processor: live spectral / granular delay (working concept)

```
RADIO / TUNER / other AUDIO -> bounded rolling delay memory
                             |-> coherent TRACE / delay ---------+
                             |-> CLOUD grain scheduler ----------+-> DISSOLVE -> AUDIO OUT
                             +-> optional spectral analysis/filter
WORLD CV / MIDI / Drift / TOUCH -> delay age, grain density, spray, dissolve, filter, feedback
```

For the continuous DISSOLVE processor: no selection, IMPRINT button, frozen GHOST, audio export, sampling workflow, or saved audio. The later owner-approved [Tapestry LOOP](TAPESTRY-LOOPER-RESEARCH.md) is a distinct short volatile-memory source, not a revival of AFTERIMAGE's capture workflow. Compare complementary use and shared qualified primitives before committing to two granular engines. Temporary delay history is ordinary DSP state: preallocated, overwritten continuously, discarded on clear/shutdown, not serialized. Grains read only valid past samples and must expire; no persistent frozen capture or replay mode. Past audio must not become a user-retrievable recording. A continuous source can still produce longer tails using qualified bounded feedback. Keep existing tails alive on a station retune (including the proposed static transition) without resurrecting an old station as the current live source.

Start small: TIME (delay age), DENSITY, SPRAY, DISSOLVE, MIX and bounded FEEDBACK as candidate controls, not a committed final panel. Grain length/window, pitch, reverse, stereo and band selection can be added only if they produce useful audible differences. Keep the main source path responsive; report wet-path latency honestly. A stereo input/output adapter must preserve channel semantics.

## World-first musical experiments (unvalidated)

1. **Disappearing broadcast:** live speech -> TRACE/CLOUD; RADIO envelope or syllable onsets influence density, a MIDI fader performs DISSOLVE. Words disintegrate into a cloud rather than being saved as loops.
2. **Tuning ghosts:** actual receiver-state-driven synthesized static and the new broadcast feed the rolling processor; coherent/granular tails from one station overlap the next. Preserve tail state safely, distinguish simulated static from real broadcast observations.
3. **Weathered echoes:** optional weather or room brightness -> SENSE calibration -> delay age, spray or damping; performer can temporarily increase/decrease influence via Wyrd-inspired TOUCH.
4. **Magnetic cloud:** optional phone sensor -> calibrated field change or intentional tilt -> density/DISSOLVE, with actual live radio excitation. Phone remains optional.
5. **Frequency dissolution:** compare broadband CLOUD vs conventional crossover splitting vs spectral band guidance on speech and percussion. Keep only audible and playable improvements. Do not assume the expensive spectral route wins.

These are proposed patches, not working WILDBAND features. No new sensor, Wyrd, Korupt or phone dependency is necessary for the first experiment.

## Qualification and time-saving strategy

1. Pin and audit the actual AFTERIMAGE candidate source revision and dependencies; inspect engine boundaries and tests before choosing extraction versus adaptation. Existing P1-A/B branches are draft evidence, not shipping code. Reuse the smallest qualified scheduler/interpolation/window primitives, not the JUCE editor or plugin capture/host state.
2. Build a narrow JUCE-independent rolling live-delay experiment with synthetic fixtures and lawful local test inputs. Establish history/read-head/epoch semantics, grain scheduling, resets, stereo behavior, smoothing and level contract. Compare unchanged reusable primitives with original reference vectors where meaningful; use separate tests for new rolling behavior.
3. Audition against WILDBAND's existing resonator, Wyrd time filter if available, and a normal modulated delay, on equivalent live radio inputs with matched listening levels. Evaluate static off/on and WORLD routes disconnected/connected. The question is distinct playable musical behavior, not feature count.
4. Budget and measure worst-case callback cost under combined live radio, patch graph, maximum grains, feedback and rapid CV/MIDI at 44.1/48/96 kHz including 32-sample buffers on the target Mac. Reproduce AFTERIMAGE's timing failures and verify xrun/host continuity instrumentation with positive controls. No allocation, lock, network access or unbounded analysis/capture work in the callback; decimate or schedule FFT work only if required.
5. Bound overlapping-grain gain, feedback and final output. Prior AFTERIMAGE research found coherent accumulation and mix peaks, aliasing on upward transposition, stereo/mono issues and expensive reconstruction bursts. Test peak/RMS, impulse, long feedback sessions, finite/hostile CV, aliasing if pitch enabled, no stale reads, station disconnect/reconnect, sample-rate changes and PANIC. No hidden assumption that prior AGPL plugin gain policy is WILDBAND's safety policy.
6. Promote only after owner audition and cost/safety gates. If continuous granular processing is too expensive, start with the cheaper CLOUD-only and conventional filtered input path. Do not revive the entire spectral capture pipeline merely because it exists.

**Scope discipline:** this could save development time by reusing a tested grain engine, reference renders, controls and measurement harness. It still requires new continuous-stream semantics, license review and qualification. Do not interrupt P0 or adopt new dependencies without evidence. The outside world supplies the sound and variation; the musician shapes how it dissolves.
