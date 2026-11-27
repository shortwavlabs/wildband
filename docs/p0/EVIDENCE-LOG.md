# WILDBAND P0 — Evidence log

Everything here was produced on one machine on 2026-11-27. "Not done" means not done.

| | |
|---|---|
| Machine | MacBook Pro `Mac17,6`, Apple Silicon (arm64) |
| OS / tools | macOS 26.6.2 (25G83), Xcode 27.2, CMake 4.5.1, JUCE 9.0.5 (`d60b7a78`), Blender 5.2.1 LTS |
| Audio | Output: Arturia MiniFuse 1 (USB, duplex). Default input: MacBook Pro Microphone (44.1 kHz) |
| Network | Home broadband, no VPN details recorded |
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

`WILDBAND.app --measure=docs/p0/evidence/coreaudio-minifuse.md`: the real app on the real device, radio live,
engine in its heaviest ordinary state (FEEDBACK 0.85, DRIFT 0.7, COUPLE 0.8), output muted, 20 s per setting after
5 s of settling. Time is measured around the whole callback (ring read, test source, engine). Full table:
[`evidence/coreaudio-minifuse.md`](evidence/coreaudio-minifuse.md).

| | Result |
|---|---|
| Settings | 44.1 / 48 / 96 kHz × 32 / 64 / 128 / 512 samples, all granted by the device |
| Worst callback, as a share of its buffer period | **4.5 %** (96 kHz / 64: 30 µs of 667 µs). A first run, kept below, had 5.5 % at 96 kHz / 32 |
| Late callbacks (host-time gap above 1.5 periods) | 0 in all twelve settings |
| Positive control | One callback deliberately held for three periods: counted as late (1), host-time gap 21.5 ms. The counter does respond |
| Radio underruns / reconnects / engine faults | 0 / 0 / 0 |

Limits of this evidence: one device, one machine, 20 s per setting, editor window open, no other load, ROOM off
(no input stream), and nothing was listened to. Per-sample cost is not constant across settings (for example about
0.30 µs/sample at 48 kHz / 512 against 0.15 at 96 kHz / 512); this was not investigated and is probably CPU
frequency or core scheduling. JUCE's xrun counter was not used: AFTERIMAGE's P1-B work found it does not move on
this macOS.

First run, for the record (no positive control, so its zeros alone proved nothing): worst 5.5 % at 96 kHz / 32
(18 µs of 333 µs); 0 late callbacks, underruns, reconnects and faults.

## First-launch block: macOS asks for the microphone although no input is requested

- **Seen:** on its first launch, and again after a later rebuild, the app froze inside
  `AudioDeviceManager::initialise` → `AudioDeviceCreateIOProcIDWithBlock` (process sample). The second time, the
  system dialog *"WILDBAND" would like to access the Microphone* was on screen.
- **Cause, as far as established:** the app asks JUCE for 0 inputs and 2 outputs, but CoreAudio raises the consent
  prompt when a device that has input streams is opened, and holds the call until it is answered. JUCE gives no way
  to mark the input streams unused. Whether this also happens with an output-only device (built-in speakers) could
  not be separated, because the unanswered prompt blocked every later launch.
- **Why it matters:** PRD P0-08 says no automatic microphone permission. The app never *reads* the microphone until
  ROOM is on, but the *prompt* appears unasked on duplex interfaces. That is a defect against the requirement.
- **Mitigation in the prototype:** the window now opens first and says why macOS may be asking; the audio device is
  opened a quarter of a second later. Either answer should leave the output working — **not verified**.
- **Because the app is ad-hoc signed, macOS may ask again after each rebuild.**
- **The prompt was left unanswered.** Answering it is the owner's decision.
- **Fix candidates (not done):** open the output with a small CoreAudio HAL unit that disables input streams, or
  patch JUCE's CoreAudio device to set the IOProc stream usage.

## Blender → JUCE spike (PRD P0-10)

`assets/blender/knob.py` renders one knob with Cycles (64 frames, 192 px, transparent, about 7 s) and ffmpeg tiles
them into `assets/knob/knob.png`. JUCE draws the frame for the TUNE slider's position; dragging, hit-testing, MIDI
and resizing stay in JUCE. Checked from an offscreen snapshot of the real UI
([`evidence/ui-snapshot.png`](evidence/ui-snapshot.png)): the correct frame is drawn for the knob's position.
**Not checked:** dragging it by hand, 100–200 % window sizes, Retina sharpness when enlarged (the frames are
192 px). The knob is a placeholder object, not a design proposal. No further modelling was done.

## Not done

Listening of any kind; a real microphone; mouse interaction with the patch bay or knobs; any MIDI hardware (one
MIDI input was detected, nothing was received); the default controller numbers (an unverified guess); sleep/wake;
switching output devices while playing; internet loss; revoking microphone access; sessions longer than a few
minutes; any second Mac.
