# WILDBAND — AFTERIMAGE-derived spectral/granular live delay

**Status:** post-P0 research proposal, not implementation approval or realtime qualification. Do not interrupt the current P0 permission/output repair or listening gates.

## Intent

Reuse Shortwav Labs' AFTERIMAGE DSP to transform live broadcasts and other patchable audio into a continuously evolving delay/cloud. The sound should remain connected to the present: radio, room, environmental signals and physical gestures influence the texture and its decay. WILDBAND is not a sampler, capture editor or recorder. Temporary bounded delay/history buffers are normal DSP state, not saved performances.

## Source-grounded reuse audit

- [AFTERIMAGE research consolidation](https://github.com/shortwavlabs/after-image/blob/main/docs/RESEARCH-CONSOLIDATION.md): offline numerical evidence and owner blind-listening notes.
- [P1-A realtime implementation, draft PR #6](https://github.com/shortwavlabs/after-image/pull/6), head `b0c437f6c6ef2f877d6c97b902400326af78fbcd`; inspected [Engine.h](https://github.com/shortwavlabs/after-image/blob/feature/p1a-realtime-core/plugin/Source/dsp/Engine.h).
- [P1-B performance capture, draft PR #7](https://github.com/shortwavlabs/after-image/pull/7), head `c38b037ebfd475fb5445592652b6d1d4ddc9a134`; PR evidence, not a full code audit.

Terminology matters: the research evolved from spectral resynthesis to **granular synthesis over time-domain audio**, with STFT history used for analysis and optional band-selective reconstruction. The current C++ CLOUD is not a fully implemented continuous spectral delay or per-bin feedback network. A WILDBAND rolling delay is an adaptation, not an already-shipping AFTERIMAGE feature.

| Component | Existing evidence | WILDBAND candidate |
| --- | --- | --- |
| CLOUD | Bounded grain scheduler, SplitMix64 random generator, Catmull-Rom reads, windowing, reverse/spread/spray; max 32 active grains per voice and two voices in P1-A | Rolling granular delay/cloud over live history; begin with bounded grain admission and stereo output |
| TRACE → CLOUD | Parallel DISSOLVE crossfade, owner liked progression from delay-like trace to granular tail | A single expressive DISSOLVE control, moving from a recognizable delayed radio trace to fragmented cloud |
| FULL / FILTERED | Raw history or STFT masked WOLA reconstruction in P1-A | Optional frequency-selective grain source or lighter conventional filtering first; compare audible benefit and latency |
| FOCUS | Offline harmonic-position CDF-guided grain selection; liked on several sources | Later research: let spectral content of radio guide grain selection, if cost and musical value justify it |
| GLITCH / SPLIT S3 | Offline band-dependent grain length, spray, spread, reverse; mixed owner preferences | Optional frequency-dependent fragmentation; prefer conventional crossovers if equivalent, not costly STFT split by default |
| History, scheduling, fades | Six-second raw/complex history, preallocated resources, epoch-tagged capture jobs, block-independent scheduling, raised-cosine replacement/release in P1-A | Reuse scheduling, bounded memory and safe fades; redesign history ownership/read-head safety for continuous live operation |
| AUTO / RANDOM / HOLD | P1-B functional trigger strategies; AUTO flux + level is experimental | Borrow event gating/cooldown and deterministic tests, not a user-facing capture mode; adapt for transient-driven grain density or bursts |

The consolidation records that the owner generally found drums/vocals more interesting than sustained tones, preferred CLOUD most often across the auditioned engine sets, and liked parallel DISSOLVE's delay-to-granular tail. This motivates trying speech and rhythm radio plus tuner static; it does not establish that WILDBAND will sound good. SPLIT was generally not distinguishable from conventional multiband approaches in the offline comparisons.

## Proposed instrument module (working label: DISSOLVE)

```
RADIO / TUNER STATIC / OTHER PATCH AUDIO
  -> bounded rolling history
  -> delayed trace + grain scheduler (optional band shaping)
  -> smoothed TRACE-to-CLOUD mix
  -> decay / bounded feedback / stereo wet output
```

Start with a small control set: TIME (history read offset), DISSOLVE, DENSITY, SPRAY, DECAY, MIX; evaluate whether separate SIZE, WIDTH, REVERSE or spectral-band controls are needed only after audition. These are candidate controls, not a final UI. Expose typed audio in/out and patchable CV for the few most expressive parameters. Prefer reuse of the existing graph and MIDI learn; do not add a second capture editor or duplicate Wyrd's conventional feedback-delay/reverb functionality.

Continuous rolling delay is the primary contract: grains read safe available history behind the write head, expire, and are replenished from incoming signal. Decide on interpolation margins, read-head/epoch safety, grain admission, memory length and silence/decay behavior. If a short-lived internally refreshed snapshot is required by the existing algorithm, it must be implementation-only, bounded, automatically expiring and neither manually selected nor persisted. No IMPRINT, record, sample export, permanent HOLD/freeze or stored audio. Save only patch and parameters; do not include private observations or radio material.

## World-driven musical experiments (unvalidated)

- **Broadcast dissolution:** RADIO AUDIO -> DISSOLVE -> resonator or Wyrd reverb. Radio envelope/flux -> density or band emphasis; MIDI fader steers TRACE-to-CLOUD mix.
- **Tune through the ghosts:** actual buffering-driven simulated tuner static excites the same live delay. Preserve existing tails during retune; the next broadcast naturally replaces old material.
- **Room fragments:** ROOM HITS -> bounded grain burst/density modulation; room level or brightness -> spray/filter. Room raw mic audio is not implicitly routed.
- **Magnetic cloud:** optional iPhone calibrated magnetic change/tilt -> spray or DISSOLVE depth; keep a live broadcast as excitation and MIDI as intentional intervention.
- **Drift + cloud:** Drift CV shapes delay time or grain variation, while observed live signals still materially control the patch. Seeded randomness is not a measurement of the world.

## Port and qualification

- Inspect `plugin/Source/dsp/Engine.cpp`, `Engine.h`, reference harness/tests and actual P1-B changes before extracting components. Keep source provenance and pin exact commits; AFTERIMAGE's code is AGPL-3.0-only and WILDBAND licensing is unresolved. Verify contributor rights, JUCE and other dependencies before copying; no licence change is authorized here.
- Separate useful grain primitives from the plugin shell, spectrogram display, manual time-frequency selection, capture-command UX, host automation/state, and full history if not needed. Avoid importing the entire Engine unchanged.
- AFTERIMAGE P1-A reported 148/148 Python/C++ parity cases, zero audio-thread allocations in its tests, but failed the worst-case callback gate in both runs (23/24 gated rows). P1-B real CoreAudio E3 failed at 96 kHz/32 samples (99.9th percentile 26.7% vs 25%; 27.0–27.5% repeats); E4 not run, xrun component not assessed. Treat this as a reuse candidate with a known performance blocker, not a production-qualified library.
- Test the simplest raw-history granular path first; optional FFT/masked reconstruction is gated on musical value. Do not port bursty capture scheduling blindly. Preallocate buffers, bound active/pending grains and work per block, prevent out-of-range history access and stale epochs, no network/locks/allocation on audio thread. Qualify rapid modulation, sample-rate/block-size changes, reset, channel mapping, wrap, input loss and retune continuity.
- AFTERIMAGE P1-A uses no normalisation or limiter and reports overs; offline evidence identified coherent overlap gain and aliasing risks. WILDBAND requires explicit wet/feedback gain/overlap safety, smoothing, measured peaks and anti-alias qualification for transposition (omit pitch shifts initially). Test mono compatibility and high-feedback combinations with existing processors. PANIC clears/mutes per instrument contract.
- Benchmark the combined WILDBAND graph under actual active radio and grain activity at 44.1/48/96 kHz, including small buffers and UI/MIDI activity, with trustworthy deadline/xrun positive controls. Preserve failed evidence. Reuse existing tests as reference, add independent tests for continuous operation; offline parity is not live musical approval.

## Decision order

1. Finish P0 and compare its ENGINE/REFERENCE modes. Do not use this proposal to bypass that decision.
2. A/B current engine and simple conventional delay vs a minimal continuously refreshed AFTERIMAGE-derived CLOUD/TRACE path on speech, music, noisy radio and simulated tuner static. Match level and preserve source context; listen first.
3. Test a real external modulation route enabled/disabled with comparable radio input and gestures. The player should hear a controllable difference and want to keep performing.
4. Only if useful, add filtered/spectral or FOCUS/GLITCH behavior; compare CPU and audible distinction. Do not require spectral processing just for the name.

**Decision rule:** reuse the most compelling and inexpensive components that make WILDBAND's present-world interactions musically richer. Do not turn WILDBAND into AFTERIMAGE with a radio input.
