# WILDBAND P0 — Evidence log

Everything here was produced on one machine on 2026-11-27. "Not done" means not done.

| | |
|---|---|
| Machine | MacBook Pro `Mac17,6`, Apple Silicon (arm64) |
| OS / tools | macOS 26.6.2 (25G83), Xcode 27.2, CMake 4.5.1, JUCE 9.0.5 (`d60b7a78`), Blender 5.2.1 LTS |
| Audio | Output: Arturia MiniFuse 1 (USB, duplex). Default input: MacBook Pro Microphone (44.1 kHz) |
| Network | Not recorded |
| Listening | **None.** The author of this prototype cannot hear. Every run was muted (LEVEL forced to 0) |

## Owner first audition (2026-11-27, after muted development runs)

See [OWNER-FIRST-AUDITION.md](OWNER-FIRST-AUDITION.md) for the owner's six responses, verbatim notes and approved follow-up. Positive first impression; resonator expressiveness and per-cable modulation depth need work. Session protocol details remain unspecified. All earlier technical measurements and gate verdicts below are preserved as historical evidence, not a claim that nobody has since listened.

## Gate status

| Gate | Technical evidence | Owner-dependent part | Status |
|---|---|---|---|
| G0 Define the instrument | Stack chosen and justified; AFTERIMAGE and TALORA assessed (`ARCHITECTURE.md`, `TALORA-REUSE.md`) | Accept or change the one-sentence promise and non-goals; licensing | **Awaiting owner** |
| G1 Source | 6 real stations from 5 operators decode and switch; drops and stalls recover (`RADIO-COMPATIBILITY.md`) | Hearing it play reliably; `http` stations inside the app | **Technical part done; awaiting owner** |
| G2 Environment | Analyser maths tested; permission flow verified on hardware (prompt only on ROOM, denial, grant, values from the MiniFuse input, off) — see "Microphone prompt" below | Whether the room signal is *musically* usable; revocation | **Technical part done; awaiting owner listening** |
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

## Microphone prompt at launch: cause, fix, verification (2026-11-27, second pass)

**Owner's report:** with permission undetermined, macOS asked for the microphone before ROOM was enabled; after
"Don't Allow" there was no usable output; choosing an output in AUDIO raised another request.

### Cause (established)

`WILDBAND.app --probe=FILE` performs CoreAudio operations one at a time and logs the microphone status and elapsed
time around each, so a step that prompts is the last `BEGIN` without an `END`. Three runs, each after
`tccutil reset Microphone com.shortwavlabs.wildband`:

| Build | Result | Log |
|---|---|---|
| Original (app declares `NSMicrophoneUsageDescription`) | Blocked in the very first step with the system prompt on screen. Process sample: `AudioComponentInstanceNew` → `AudioDeviceCreateIOProcID` → `_TellServerAboutStreamUsage`. The step asked for the built-in speakers with input disabled; Apple's output unit attaches to the *default* output (MiniFuse, 4 in / 4 out) first | [`mic-trigger-probe-undetermined.txt`](evidence/mic-trigger-probe-undetermined.txt) |
| Same binary, usage text removed from Info.plist, re-signed | All five steps complete in 4.5 s: output unit and raw IO proc, on the speakers and on the MiniFuse, 93–94 output callbacks per second each. No prompt, status stays `undetermined` | [`mic-trigger-probe-no-usage-string.txt`](evidence/mic-trigger-probe-no-usage-string.txt) |
| Fixed build | Same as the row above | [`mic-trigger-probe-fixed-build.txt`](evidence/mic-trigger-probe-fixed-build.txt) |

So: **an app that declares microphone use is shown the microphone prompt the moment any IO proc is created on a
device that has input streams, whichever API creates it, and the call does not return until the prompt is answered.**
Answers to the specific questions asked:

- *Saved XML device state / restored input channel masks:* not the cause. It happens on a first launch with no
  saved state, and in a probe that never touches JUCE.
- *`closeInputs()` only after initialisation:* irrelevant for the same reason (and that code is gone).
- *Does CoreAudio open the input side of combined devices?* In effect yes: an IO proc is created on the whole
  device, and JUCE 9.0.5's CoreAudio backend (`AudioDeviceProcessor`, one `AudioDeviceCreateIOProcIDWithBlock`) has
  no means of marking input streams unused. Apple's own output unit with input disabled does the same.
- *Does selecting an output reopen the device with inputs?* It creates a new IO proc, so it asks again while the
  status is undetermined.
- *Does the app or JUCE call an authorization API unexpectedly?* No. JUCE's audio-device code contains no
  authorization call on macOS; the app's only request was behind the ROOM button.
- *Why some earlier launches did not block* (noted in the first pass as unexplained): not investigated further.
  An unanswered prompt from a process that has exited stays on screen for a minute or more and holds up later
  requests, which muddied those observations.

### Fix

The instrument no longer declares microphone use at all, so macOS has nothing to ask on its behalf. ROOM moved to a
separate helper app, **WILDBAND Room** (`src/RoomHelper.mm`, bundle id `com.shortwavlabs.wildband.room`), which has
its own usage text and its own permission. The ROOM button starts it; nothing else does. It asks for permission,
analyses the chosen input, and sends three numbers 50 times a second to the instrument over loopback UDP. Switching
ROOM off terminates it. The instrument's audio output is not touched by any of this. The input ROOM listens to is
chosen in the AUDIO… dialog; by default it is the interface being played through if that has inputs.

Two things found on the way, both now in the code:

- The helper must sit **beside** `WILDBAND.app`, not inside it. Nested, macOS attributed its request to WILDBAND
  and killed it ("attempted to access privacy-sensitive data without a usage description"; two crash reports).
- It must not be launched in the background (`open -g`): macOS then denied silently, with no prompt.
- The first capture implementation (Apple's HAL unit, input side) delivered digital silence (−200 dBFS) with
  permission granted. Replaced by a plain device IO proc; cause of the silence not pursued.

### Verification

All runs muted, on this Mac, MiniFuse 1 as default output unless stated. "Seen" means a capture of the app window or
of the system alert; the owner clicked the prompts.

| # | Check | Result |
|---|---|---|
| 1 | Fresh permissions → launch without a prompt | **Pass.** No alert; `LIVE WFMU Freeform … 48.0 kHz / 512` on the MiniFuse. Same on the built-in speakers (`--output`), `LIVE BBC World Service` |
| 2 | Denied → radio plays | **Pass.** After the owner's "Don't Allow": relaunch, no alert, `LIVE` on the MiniFuse |
| 3 | Denied → other output device works | **Pass at launch** (`--output="MacBook Pro Speakers"`: `LIVE KEXP`, no alert). **Not performed:** switching by hand in the AUDIO… dialog while playing |
| 4 | ROOM on → permission requested explicitly | **Pass.** Alert appears only after ROOM is switched on, and reads *"WILDBAND Room" would like to access the Microphone*. Radio `LIVE` with a 6.0 s buffer while it is pending |
| 5 | ROOM denied → radio keeps playing | **Pass.** Owner clicked "Don't Allow": ROOM unticked itself with an explanation, helper exited, status line still `LIVE … buffer 6.0 s` |
| 6 | ROOM authorised → microphone features operate | **Pass on the MiniFuse input.** Six readouts over 8 s: level 0.48–0.61, hits 0.14–0.34, bright 0.06, raw input −29 to −51 dBFS. On the MacBook microphone the *first* capture implementation gave silence; the replacement has not been re-run on it |
| 7 | ROOM off → capture stops | **Pass.** Helper process gone; main app running and `LIVE`. The output device is not reopened (the code path does not touch it) |
| 8 | Relaunch with saved settings → no premature request | **Pass** with permission granted and with it denied: no alert at launch, helper not running until ROOM is switched on |

### Limitations

- The builds are ad-hoc signed, so **every rebuild of the helper makes macOS ask again** the next time ROOM is
  switched on (seen three times). A Developer ID signature would end that. The instrument itself never asks.
- Two apps must be kept side by side (`WILDBAND.app`, `WILDBAND Room.app`).
- While the prompt is up, macOS brings the helper to the front; the instrument takes focus back when it is answered.
- Room values travel by UDP on 127.0.0.1 and are read on the UI timer, so they reach the engine up to about 35 ms
  late. Any local process could send values to that port; they are clamped to 0..1 and can only move control inputs.
- All input channels of the chosen device are summed; there is no channel picker.
- Not performed: revoking permission while ROOM is on; unplugging the input while ROOM is on; ROOM across
  sleep/wake; choosing the input by hand in the dialog (the layout was checked from a snapshot,
  [`evidence/ui-audio-settings.png`](evidence/ui-audio-settings.png)).
- The timing sweep was not re-run on this build.

## Blender → JUCE spike (PRD P0-10)

`assets/blender/knob.py` renders one knob with Cycles (64 frames, 192 px, transparent, about 7 s) and ffmpeg tiles
them into `assets/knob/knob.png`. JUCE draws the frame for the TUNE slider's position; dragging, hit-testing, MIDI
and resizing stay in JUCE. Checked from an offscreen snapshot of the real UI
([`evidence/ui-snapshot.png`](evidence/ui-snapshot.png)): the correct frame is drawn for the knob's position.
**Not checked:** dragging it by hand, 100–200 % window sizes, Retina sharpness when enlarged (the frames are
192 px). The knob is a placeholder object, not a design proposal. No further modelling was done.

## Not done

Listening of any kind; mouse interaction with the patch bay or knobs; any MIDI hardware (one
MIDI input was detected, nothing was received; no controller is mapped until MIDI LEARN is used); sleep/wake;
switching output devices while playing; internet loss; revoking microphone access; sessions longer than a few
minutes; any second Mac.
