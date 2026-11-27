# WILDBAND — PRD (P0 Research Draft)

**Product:** live, macOS-first, environment-responsive electronic performance instrument. **Status:** proposed requirements pending evidence.

## Primary scenario

Open the standalone app; hear a safe default. Tune a live Icecast/Shoutcast-compatible radio stream. Play expressive resonance/modulation/feedback controls while live radio activity shapes the sound. Optionally let room microphone activity influence the behavior. Perform without a timeline, capture command or recording button.

## P0 capabilities to validate

| ID | Candidate capability | Minimum demonstration |
|---|---|---|
| P0-01 | Real live HTTP audio stream | Two compatible external stream URLs each stream; error states and station switching work |
| P0-02 | Nonblocking audio | Network operations off real-time thread; output continues through stream disruptions |
| P0-03 | Playable DSP | One distinct resonant or nonlinear processor, with controllable response and limited feedback |
| P0-04 | Coupling | Extract a signal-following modulation value from live broadcast energy or transients and route to DSP |
| P0-05 | Room connection | Permission-based microphone activity signal; demonstrably alters response without forcing raw mic monitoring |
| P0-06 | Human performance | Three to five continuous controls accessible by mouse and mapped MIDI hardware |
| P0-07 | Graceful fallback | No network/permission -> app still produces optional local test source, clear source status |
| P0-08 | Safety and privacy | Bounded dynamics, no automatic mic permission, no sensor IDs persisted |
| P0-09 | Play test | Owner spends 15+ minutes, can intentionally achieve three audibly different states and wants to return |
| P0-10 | Design workflow proof | One Blender-rendered control integrated with live JUCE hit testing, automation and resize behavior |

**P1 options, only after P0:** second independent stream with cross-modulation, weather data as optional slow control signal, Bluetooth RSSI experiment, Wi-Fi API feasibility, deeper modulation patching, native AU/VST3.

**Field Kit-inspired follow-ups (proposed, not added to P0):** prioritize (1) auxiliary mixer and bounded feedback routing and (2) adjustable envelope follower + gate as first musical experiments after owner P0 audition. Evaluate (3) sensor/CV calibration and (4) a minimal LFO later; optional electromagnetic/contact pickup inputs and external actuators are future hardware investigations. Use TALORA's typed patching architecture where qualified. See [`FIELD-KIT-INSPIRED-UTILITIES.md`](FIELD-KIT-INSPIRED-UTILITIES.md) for rationale, safety limits and patch-recipe ideas.

**Post-P0 amendment:** [LOOP / Tapestry research](TAPESTRY-LOOPER-RESEARCH.md) is an owner-approved high-priority investigation of very short, explicitly captured volatile radio fragments. No change to P0 gates. No persisted audio or sample library.

**Out of scope:** full-performance recording/export, sample harvesting, cloud user accounts, livestream relay service, Windows parity, raw RF sensing without SDR, full Eurorack simulation.

## Experience acceptance

The user can hear the input source, understand what a gesture changes, create an intentional progression from quiet to unstable and back, and hear different meaningful results when either radio input or environment changes. A dry-radio + generic-filter baseline should be used for comparison.

## Technical and operational acceptance

Do not block, allocate unexpectedly or perform I/O on the DSP callback. Prove sample-rate/buffer-size behaviors in real CoreAudio; test session sleep/wake, output-device changes, interrupted streams, permission denial, sensor absence and extreme feedback. Measure latency and xruns using validated instrumentation rather than assuming a host statistic is reliable. Set exact performance thresholds with evidence in the RFC before qualification.

## Product integrity

The app must distinguish *live broadcast* from optional local demonstration audio. No stream content is bundled without permission. Public accessibility of a stream does not itself grant reuse/rebroadcast rights; investigate usage terms for demos and distribution. No recording is required to use the app; external user-directed audio routing remains outside the product.
