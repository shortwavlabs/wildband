# WILDBAND P0 — Evidence log

Everything here was produced on one machine on 2026-11-27. "Not done" means not done.

| | |
|---|---|
| Machine | MacBook Pro `Mac17,6`, Apple Silicon (arm64) |
| OS / tools | macOS 26.6.2 (25G83), Xcode 27.2, CMake 4.5.1, JUCE 9.0.5 (`d60b7a78`), Blender 5.2.1 LTS |
| Audio | Output: Arturia MiniFuse 1 (USB, duplex). Default input: MacBook Pro Microphone (44.1 kHz) |
| Network | Not recorded |
| Listening | **None.** The author of this prototype cannot hear. Every run was muted (LEVEL forced to 0) |

## Gate status

| Gate | Technical evidence | Owner-dependent part | Status |
|---|---|---|---|
| G0 Define the instrument | Stack chosen and justified; AFTERIMAGE and TALORA assessed (`ARCHITECTURE.md`, `TALORA-REUSE.md`) | Accept or change the one-sentence promise and non-goals; licensing | **Awaiting owner** |
| G1 Source | 6 real stations from 5 operators decode and switch; drops and stalls recover (`RADIO-COMPATIBILITY.md`) | Hearing it play reliably; `http` stations inside the app | **Technical part done; awaiting owner** |
| G2 Environment | Analyser maths tested (smooth, bounded, neutral on silence); permission flow written | The macOS prompt, real microphone, denial, revocation | **Not verified on hardware; awaiting owner.** One defect found (below) |
| G3 Musical engine | Engine, DRY and REFERENCE modes built; safety and patch tests pass | All listening, across speech, music and noise | **Awaiting owner** |
| G4 Performance interface | Knobs, patch bay, MIDI learn, Blender knob built; UI drawn and checked from a snapshot | Mouse and MIDI hardware use; two 15-minute sessions | **Awaiting owner** |
| G5 Reliability | Callback timing on real CoreAudio, extreme-feedback tests, stream failure recovery | Sleep/wake, output switching, internet loss, mic revocation | **Partly done; awaiting owner** |

No owner listening test has been performed, so none is marked passed.

## Automated tests

`ctest --test-dir build` → `wildband_tests`: **442 checks, 0 failed**. No JUCE, no audio device, deterministic.

| Test | What it proves |
|---|---|
| `dial` | TUNE maps to station slots; the centre 40 % of a slot is clean; edges are fully detuned; out-of-range values are safe |
| `bounded` | 72 runs (3 sample rates × 24 control states, most with FEEDBACK at the stop): full-scale noise, a DC slab at 4.0, NaN, infinity and 1e30 in the input, room values out of range, random plan words → output always finite, never above 0.89, zero engine faults |
| `block-invariance` | Bit-identical output for block sizes 1, 64, 1000 and random |
| `tuning` | The harmonic SHAPE rings within 10 cents of the PITCH note at C3, C4, C5 and 44.1/48/96 kHz |
| `decay` | Below unity gain: silent 10 s after the input stops. FEEDBACK at the stop: sustains, bounded. After PANIC and from silence: exactly zero |
| `no-clicks` | Jumping any knob 0↔1, the mode, or the detune amount adds no step above 0.03 to a steady tone |
| `room` | Silence → neutral; a sound starting → HITS above 0.5, then below 0.05 once it is steady; LEVEL matches the dB mapping; 8 kHz is "bright", 200 Hz is not; garbage input stays in range. Stale or withdrawn sources read as neutral |
| `graph` | Type mismatch, reversed, output-to-output, input-to-input, duplicate, one cable per input, fan-out, disconnect, plan packing, port ids |
| `patching` | Each of 8 control cables changes the output (relative difference above 5 %); with COUPLE at 0 none does (difference exactly 0); with no cable the room does nothing; pulling the audio cable gives silence; the local source can be patched instead; a 3-way fan-out; 40 random re-patches of a steady tone with no step above 0.03 |
| `no-allocation` | 4 000 blocks with moving controls, plans and a reset: 0 allocations on the processing thread (positive control: 1) |

`wildband_talora_parity` (optional): 120 000 random edits, 0 disagreements with TALORA's `Graph`.

Not covered by automated tests: `Radio` (exercised by the probe instead), the app's callback glue, MIDI, the UI.

## CoreAudio callback timing

`WILDBAND.app --measure=docs/p0/evidence/coreaudio-minifuse.md`: the real app on the real device, knobs at
FEEDBACK 0.85, DRIFT 0.7, COUPLE 0.8, output muted, 20 s per setting after 5 s of settling. Time is measured around
the whole callback (ring read, test source, engine). Full table:
[`evidence/coreaudio-minifuse.md`](evidence/coreaudio-minifuse.md).

**What these runs did not record:** whether the radio was actually playing during each window. The stream restarts
at every rate change, and the sweep logged neither the radio state nor its level, so in some windows the engine may
have been processing silence, which is cheaper (see the synthetic figures below). The tool now logs radio state,
buffered seconds and radio level per row; the sweep could not be re-run because of the unanswered microphone
prompt. **Read the table as callback timing with the engine in an unknown state of excitation.**

| | Result |
|---|---|
| Settings | 44.1 / 48 / 96 kHz × 32 / 64 / 128 / 512 samples, all granted by the device |
| Worst callback, as a share of its buffer period | **4.5 %** (96 kHz / 64: 30 µs of 667 µs). A first run, summarised below, had 5.5 % at 96 kHz / 32 |
| Late callbacks (host-time gap above 1.5 periods) | 0 in all twelve settings |
| Positive control | One callback deliberately held for three periods: counted as late (1), host-time gap 21.5 ms. The counter does respond |
| Radio underruns / reconnects / engine faults | 0 / 0 / 0 |

Limits of this evidence: one device, one machine, 20 s per setting, editor window open, no other load, ROOM off
(no input stream), and nothing was listened to. Per-sample cost is not constant across settings (about 0.10
µs/sample at 96 kHz / 32, 0.30 at 48 kHz / 512). That is unexplained: an idle engine in some windows and CPU
frequency or core scheduling are both possible, and neither was tested. JUCE's xrun counter was not used:
AFTERIMAGE's P1-B work found it does not move on this macOS.

Synthetic cross-check (`wildband_tests --metrics`, one thread, no audio device — **not** a CoreAudio measurement):
`Engine::process` alone costs about **44 ns per sample on silence and 74 ns on noise** with the same knob settings
(four runs, within 1 ns). So a ringing engine costs roughly 30 ns per sample more than an idle one. If every sweep
window had been idle, the worst row (30 µs for 64 samples at 96 kHz) would rise by about 2 µs, from 4.5 % to about
4.8 % of the period. The headroom conclusion does not depend on what the radio was doing; the exact figures do.

First run, for the record (no positive control, so its zeros alone proved nothing): worst 5.5 % at 96 kHz / 32
(18 µs of 333 µs); 0 late callbacks, underruns, reconnects and faults.

## Launch block: macOS asks for the microphone although no input is requested

- **Seen:** on its first launch the app froze for a minute or two inside `AudioDeviceManager::initialise` →
  `AudioDeviceCreateIOProcIDWithBlock` (process sample), then carried on by itself. Several rebuilt copies then
  launched without blocking, including both timing sweeps. After a later rebuild it froze in the same place, and
  this time the system dialog *"WILDBAND" would like to access the Microphone* was on screen (window capture).
- **What is established:** the app asks JUCE for 0 inputs and 2 outputs; the microphone prompt appeared anyway; the
  CoreAudio call does not return while the prompt is pending.
- **What is not established:** the cause. The suspect is that the output device (MiniFuse 1) also has inputs, but
  two launches forced onto the built-in speakers also never opened an audio device — while the earlier prompt was
  still on screen, so that test separates nothing. Why some rebuilt copies prompted and others did not is also
  unexplained (the app is ad-hoc signed, so macOS may treat a rebuild as a new program). How the first block
  cleared is unknown.
- **Why it matters:** PRD P0-08 says no automatic microphone permission. The app does not *read* the microphone
  until ROOM is on, but a *prompt* that appears unasked is a defect against that requirement.
- **Never run:** the "Don't Allow" path. If denying leaves the app with no audio output, that also fails PRD P0-07.
  **This is the first thing for the owner to check.**
- **Mitigation in the prototype:** the window now opens first with a note; the audio device is opened a quarter of
  a second later.
- **The prompt was left unanswered** and was still on screen at hand-over. Answering it is the owner's decision.
- **Fix candidates (not done, and unproven until the cause is known):** open the output with a small CoreAudio HAL
  unit that disables input streams, or patch JUCE's CoreAudio device to set the IOProc stream usage.

## Blender → JUCE spike (PRD P0-10)

`assets/blender/knob.py` renders one knob with Cycles (64 frames, 192 px, transparent, about 7 s) and ffmpeg tiles
them into `assets/knob/knob.png`. JUCE draws the frame for the TUNE slider's position; dragging, hit-testing, MIDI
and resizing stay in JUCE. Checked from an offscreen snapshot of the real UI
([`evidence/ui-snapshot.png`](evidence/ui-snapshot.png)): the correct frame is drawn for the knob's position.
**Not checked:** dragging it by hand, 100–200 % window sizes, Retina sharpness when enlarged (the frames are
192 px). The knob is a placeholder object, not a design proposal. No further modelling was done.

## Not done

Listening of any kind; a real microphone; mouse interaction with the patch bay or knobs; any MIDI hardware (one
MIDI input was detected, nothing was received; no controller is mapped until MIDI LEARN is used); sleep/wake;
switching output devices while playing; internet loss; revoking microphone access; sessions longer than a few
minutes; any second Mac.
