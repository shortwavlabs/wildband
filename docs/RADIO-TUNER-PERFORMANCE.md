# WILDBAND — Playable Radio Tuner and Station Hopping

**Status:** post-P0 research proposal; no implementation or qualification implied. Prioritize after the current output/permission blocker and P0 owner audition. The goal is to turn the real latency of internet radio into an expressive musical transition, not conceal it.

## Creative thesis: play the radio itself

Remember the physical buttons of a car stereo and the space between stations on an analog radio dial. WILDBAND should let performers jump directly to a saved station or sweep the dial, hearing an imperfect, crackling transition as the next live broadcast arrives. The wait for network connection, decoding and buffering becomes a sound-producing event in its own right.

**The static is synthesized; the timing is driven by the real stream state.** Do not describe it as received FM, RF measurements, or electromagnetic interference from the room. WILDBAND remains a live internet-radio instrument.

## Existing P0 starting point

The [P0 prototype PR #3](https://github.com/shortwavlabs/wildband/pull/3) reports six saved station slots, a TUNE dial, off-centre detuning, a stream receiver with buffering/reconnection, and measured 0.2–1.7 s to first audio in the receiver probe. These are useful starting points, not proof of the new interaction or guaranteed in-app switch times. Consult the actual implementation and current evidence before making changes.

## Performance controls

- **PRESET:** directly select a saved station using a mouse or MIDI-mapped button; reuse the existing six slots.
- **NEXT / PREVIOUS:** cycle through the configured stations, wrapping at the ends. Map to physical MIDI controls.
- **SWEEP:** turn TUNE through station positions as an analog-inspired gesture. Between stations, move toward simulated static; settle on a station to request it. Reuse or refine existing off-centre detuning rather than layering redundant effects.
- **HOP:** jump to another selected station on demand, with optional sequential or random selection. Evaluate a clock or patchable gate/trigger mode later (radio transients, ROOM hits, Drift or phone gestures). Edge-trigger, debounce, apply hysteresis/cooldown and coalesce rapid changes; never initiate a network operation at audio rate. Manual selection takes precedence over automatic hopping.
- **STATIC:** adjustable intensity including completely off; conservative default and smoothed changes. An optional character control may be explored only if it improves playing.

The first implementation should be manual station selection and actual-buffer-driven static. Automatic hopping and extra controls are follow-up experiments, not prerequisites.

## Transition state model

```
LIVE A -> REQUEST B -> CONNECTING / BUFFERING B -> READY B -> LIVE B
              |              |                      |
       fade A out       simulated static       fade static out
                        excites patch          fade B in
                             |
                        FAILED / TIMEOUT -> bounded fade to silence,
                                            visible failure, retry/retune
```

- When a new selection is accepted, begin a short ramp from the current source to the tuning texture. The texture fills the actual waiting interval until decoded PCM reaches a qualified readiness threshold. Do not use an arbitrary fixed-duration effect to pretend the station is ready.
- On successful readiness, crossfade into the new live stream, with a satisfying lock-in gesture. Fast connections should not be artificially delayed; use only the ramp needed for a safe handoff.
- Distinguish a deliberate retune from a spontaneous underrun/reconnect. The same texture may optionally accompany reconnection, but must not disguise a failed connection as live audio. Display TUNING, CONNECTING, BUFFERING, LIVE and FAILED truthfully. Static on failure is time/level bounded and fades to silence or the explicitly chosen local fallback. No endless loud noise during outages.
- Rapid requests cancel or supersede obsolete work; generation-tag readiness and PCM so a stale decoder cannot make an old station audible. Preserve cancellation, retry backoff and bounded resource use.
- Preserve DSP state across station selection. Existing resonator energy, delay/reverb tails and PLL behavior (if later integrated) should interact with the new excitation; do not reset the entire instrument as a side effect of retuning. PANIC must override the transition and silence output according to the instrument's safety contract.

## Static sound design

Start with lightweight seeded pseudo-random band-limited noise, gentle level variation and filtered crackle. Compare against plain noise and silence. Later audition soft whistles/heterodyne-inspired tones and dial-position-dependent color. These are synthesized sound-design choices, not a circuit-accurate FM tuner model or measured RF.

Feed the mixed tuner output into the existing RADIO AUDIO port upstream of the patch graph: static is a playable excitation for resonators and any subsequently qualified Korupt/Wyrd processing, not an overlay after the DSP. Consider a separate typed TUNING CV/gate or STATIC AUDIO port later if it offers genuine patching value. Keep actual broadcast-derived WORLD analysis distinct from synthetic static-derived features; explicitly label any optional combined tuner envelope. Noise must not create self-triggered hopping loops by accident.

Bound noise peak/RMS, avoid DC, ramp transitions, and verify worst-case feedback and listening levels. Generate noise with preallocated real-time-safe DSP; no network, logging, allocation, locking or permission operations in the audio callback. Pass a bounded state/readiness snapshot from the receiver to DSP. The design must work with one active receiver; multi-stream prebuffering is optional future research subject to network cost and broadcaster terms, not required.

## Musical experiments and acceptance

1. Compare the same station sequence with static off, simple noise and shaped tuning noise. Does the real buffer wait become an intentional performance gesture?
2. Try manual preset jumps, MIDI NEXT/PREVIOUS, rapid dial sweeps and deliberately slow/stalled streams. Confirm responsiveness, correct destination and honest status.
3. Test fast and slow connections, failures, superseded requests, network outages, rebuffering, repeated hops, sample-rate/device changes, STATIC zero and PANIC. Confirm no clicks, dangerous peaks, stale audio, retry storms or unbounded threads/buffers.
4. At high feedback, verify that retuning preserves tails safely and that static can excite the patch in an audibly useful way; compare levels against live broadcasts.
5. Later audition one external trigger path (e.g. room hit or MIDI gate) with rate limiting; ensure actual external events cause predictable hops without accidental loops.
6. Owner audition: does the tuner itself feel like a playable instrument? Can the performer intentionally create musical transitions and want to keep playing? Technical correctness alone is insufficient.

## Persistence and boundaries

Save station URLs, preset selection/configuration, MIDI mappings, tuner settings and patch relationships only. No stored broadcast excerpts, recordings, static captures or performance history. Keep internet radio playback independent of microphone access, phone pairing or other optional sensors. Investigate broadcaster terms and rights before publishing any contest performance containing broadcast audio.

**Decision rule:** keep the inexpensive one-receiver, actual-state-driven transition if it makes latency musically useful. Add complexity only when owner listening demonstrates a clear benefit.
