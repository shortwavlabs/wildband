# WILDBAND — LOOP / Microsound: Tapestry reuse

**Date:** 2026-11-27. **Status:** owner-approved direction; technical proposal pending implementation and audition. High priority in P1/P2, without changing historical P0 acceptance.

## Intent

Catch a fleeting radio fragment, loop, chop, reverse, scan and mangle it while the world continues playing. Retune and play the new station against the fragment. This is the narrow exception to the original no-capture principle: temporary musical memory, not a recording workflow. No file import/export, reel library, sample management, full-performance recording, background capture or persistent audio. Save the instrument, not the performance.

## Source evidence and reuse boundaries

Tapestry 2.2.0 in [vcv-tapestry](https://github.com/shortwavlabs/vcv-tapestry) declares GPL-3.0-or-later in plugin.json. Pin the exact source revision and audit notices/contributions/dependencies before porting. The following code was inspected, not executed:

| Source | Observed capability | Adaptation |
| --- | --- | --- |
| [tapestry-dsp.h](https://github.com/shortwavlabs/vcv-tapestry/blob/main/src/dsp/tapestry-dsp.h) | Stereo recording, replace/overdub, splice navigation, variable speed, CV parameters, envelope and end event | Extract short-loop processing; omit reel import/export and existing Rack voltage conventions |
| [tapestry-buffer.h](https://github.com/shortwavlabs/vcv-tapestry/blob/main/src/dsp/tapestry-buffer.h) | Preallocated interleaved stereo buffer, cubic reads, bounded splice reads; clear fills allocation | Smaller prepare-time storage and logical invalidation; never bulk clear on audio callback |
| [tapestry-core.h](https://github.com/shortwavlabs/vcv-tapestry/blob/main/src/dsp/tapestry-core.h) | 8,352,000 frames (~174 s at 48 kHz), 300 splices, four grain voices | Replace capacity constants and rate-dependent mappings; no full-reel semantics |
| [tapestry-splice.h](https://github.com/shortwavlabs/vcv-tapestry/blob/main/src/dsp/tapestry-splice.h) | Marker management using std::vector | Bounded fixed-capacity marker array; audit inserts, deletes and reads |
| [tapestry-grain.h](https://github.com/shortwavlabs/vcv-tapestry/blob/main/src/dsp/tapestry-grain.h) | Four-voice granular engine, bounded interpolation, clock sync | Qualify seams, speed, overlap, clock and sample-rate behavior |
| [Tapestry.hpp](https://github.com/shortwavlabs/vcv-tapestry/blob/main/src/Tapestry.hpp) | Rack wrapper, triggers, WAV/reel file state, UI | Do not import wrapper, file I/O, 32 reel slots, combo-button UX or expander as-is |

The original stereo allocation is 66,816,000 bytes (~63.7 MiB). Preallocation and noexcept alone do not establish callback safety; full-buffer clear and vector operations require redesign. No performance or listening claim is made. Review the actual source license alongside proposed AGPL-3.0-only WILDBAND and JUCE route; preserve attribution, notices and respective obligations. Reuse DSP, not Morphagene-inspired panel/trade dress.

## Proposed initial scope (duration to audition)

- One stereo RAM-only loop, **8 seconds hard cap initially**, with 1–4 s typical captures; audition 4/8/12 s alternatives only if necessary. No multi-minute reels. These are engineering proposals, not owner-approved exact values.
- At 96 kHz, 8 seconds × 2 channels × 4 bytes = 6,144,000 bytes (~5.86 MiB) per instance, excluding optional staging. Budget maximum supported rate and simultaneous instances; no hidden unbounded storage. Prepare off audio thread; if rate changes, explicitly clear/fade with status or resample off callback if later justified. Do not reinterpret samples at wrong rate.
- Explicit CAPTURE start/stop, automatic stop at cap, PLAY/STOP, CHOP (marker or divide), NEXT/SELECT, CLEAR. One replaceable fragment; overdub and clock sync optional after basic audition.
- Candidate continuous controls SPEED (forward/reverse), SIZE (grain/window), SCAN (position), MORPH (overlap), MIX or auxiliary mixer level. Curate rather than expose all controls. Default clean forward loop with short seam crossfade. Provisional 4/8/16 equal slices and manual marker edits; choose simplest after audition.
- Patchable stereo audio input/output; normalled radio capture path, live radio remains independent, loop output can excite Rings/resonance, Korupt, Wyrd or DISSOLVE where selected. Avoid implicit recursive recording; any feedback path requires explicit bounded delayed routing. RADIO live/LOOP playback have distinct status; synthesized static recording only if intentionally patched/selected, not silently treated as broadcast.
- Patchable continuous modulation: SPEED/SIZE/SCAN/MORPH and mix if meaningful, with WILDBAND per-connection bipolar depth and neutral stale sources. Candidate typed event inputs CHOP/NEXT/RETRIGGER; initial manual/MIDI operation first. No unattended capture trigger by default. Optional external capture trigger requires explicit arming, visible state and rate limit, not initial scope. Output envelope and end-of-slice event if useful.
- MIDI learn CAPTURE, PLAY, CHOP and selected parameters; visible recording indicator, elapsed/remaining, small waveform/markers/playhead. No audio data in UI snapshots beyond ephemeral visualization; no file or DAW-state audio serialization. Keep main display and controls coherent with enclosure.

## Lifecycle, privacy and realtime contract

State EMPTY → CAPTURING → READY/PLAYING → STOPPED; CLEAR invalidates readable frames/markers and stops capture/playback, fade safely. New capture replaces old content with explicit status. Retune preserves completed loop and existing downstream tails; if retune during recording, define actual incoming capture behavior visibly (prefer stop capture at station change in first prototype). Failure/underrun never reads stale/uninitialized samples. No implicit capture on startup/restore, no raw microphone recording through LOOP in initial scope.

Audio callback owns DSP state; UI/MIDI publish bounded commands/snapshots. Fixed-size buffers and markers, bounded work per block, no file/network I/O, allocation, blocking, full-capacity clear or waveform generation on callback. Logical clear immediately hides old material; off-thread safe wiping/reuse if required without races. Validate all indices, NaN/Inf, zero/one-frame buffers, high/reverse speed, marker collisions and overlap levels. Anti-alias and seam behavior audition/test. PANIC silences loop and feedback immediately, stops capture; CLEAR destroys logical content. On close/unload, discard loop. DAW recall restores settings only and EMPTY state; never serialize audio or auto-resume capture. Offline bounce may process an existing in-memory loop if host lifecycle permits; no promise of deterministic recall or live network behavior. No persistence or telemetry of radio content. OS memory/swap/crash-dump behavior is not a guarantee of forensic erasure.

## Relationship to AFTERIMAGE

LOOP = explicitly caught short fragment, precise repetition, splices, speed and grain manipulation. DISSOLVE = continuously overwritten live history, coherent delay to diffuse cloud, no capture UI. Compare both before committing two granular engines. Share qualified interpolation/windows/scheduling primitives only where useful and licensed; do not bring AFTERIMAGE capture, FFT or freeze workflow back by default. Tapestry may satisfy part of the granular requirement; use listening to decide.

## Acceptance gates

1. **L0 source/provenance:** pinned sources, license review, reference fixtures and baseline; define exact max duration/rates/memory, register tests before port.
2. **L1 capture:** sample-accurate start/stop, capacity auto-stop, short/empty loop, replacement, clear, independent live tuner, no hidden recording, no uninitialized/stale reads. Stereo channels and duration verified across supported rates.
3. **L2 playback/chop:** forward/reverse/speed, seam fades, scan/morph/grain overlap, marker bounds, slice navigation, deterministic fixture tests and deliberate audible comparison. Test no-click and finite bounded output.
4. **L3 interaction:** UI/MIDI hardware capture/chop/clear, independent signed modulation and neutral unavailable WORLD, retune against retained loop, panic and recovery. Gate events distinct from continuous CV; no feedback-trigger storm.
5. **L4 realtime:** combined live radio + selected voice + LOOP at worst-case modulation/feedback and max grain count, 44.1/48/96 kHz and 32–512 sample buffers, multiple instances, callback allocation detector and timing/late-callback positive control. Qualify host suspend/restore, offline behavior, sample-rate change, unload, clear and long-session stability. Test saved state contains no audio. AUv2 and any AUv3 target independently qualified.
6. **L5 owner audition:** short speech/percussion/music from lawful fixtures; catch, loop, chop and mangle in seconds, change station, patch ROOM HITS→NEXT, phone tilt→SCAN or Drift→SPEED, feed loop to selected voice. Compare against live-only and DISSOLVE. Does it encourage playful performance, and does WORLD still matter? No musical pass from numerical checks.

Use only licensed/cleared or original test and demo content; live stream availability does not grant capture, reuse or publication rights. Verify station terms; no bundled broadcasts or distribution of captured material. For demonstration use an owned temporary live stream if needed.

## Scope control

Stage research after consolidated baseline; evaluate with P1 sonic candidates and integrate curated routing in P2 before enclosure finalization. High priority, not permission to widen P0 or delay contest-eligible AU. Keep draft PRs and existing audition builds; owner approves duration and final controls after listening. No code copied or tests run by this document.
