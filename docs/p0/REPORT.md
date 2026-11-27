# WILDBAND P0 — Feasibility and musical-experiment report

**Date:** 2026-11-27. **Status:** prototype handed over for the owner's listening tests. No gate that needs the
owner has been marked passed. Supporting documents: `ARCHITECTURE.md`, `TALORA-REUSE.md`,
`RADIO-COMPATIBILITY.md`, `EVIDENCE-LOG.md`, `PLAYTEST-GUIDE.md`.

## The question

> Can we build a live electronic instrument that feels alive, responds to its performer and surroundings, and makes
> someone want to play it even when they have no intention of recording anything?

**Answer today: technically feasible; musically unknown.** A working instrument exists and can be launched and
played. Whether it is any good to play has not been tested by anyone, and this report cannot answer it.

## What exists

A macOS standalone app (JUCE 9.0.5, C++20, CMake) in which:

- a live MP3 or AAC internet radio stream is received, decoded by macOS and fed to the engine, with reconnection,
  station switching and status display;
- the broadcast excites six tuned, coupled, saturating resonators (`ARCHITECTURE.md`);
- the broadcast's own level and transients, and — after explicit opt-in, through a separate helper app that alone
  holds the microphone permission — the room's level, onsets and brightness, are control signals that can be patched into six resonator inputs with on-screen cables, while audio runs;
- nine knobs, MIDI controllers (with learn) and MIDI notes play it;
- DRY and REFERENCE (filter + delay) modes sit beside the engine for comparison;
- nothing is recorded, stored or exported, and there is no control that would do so.

## What the evidence supports

| Claim | Support |
|---|---|
| Live radio can be received and switched on macOS with no codec dependency | Six stations from five operators, MP3 and AAC, `http` and `https`, 0.2–1.7 s to first audio on a switch |
| Interrupted streams recover | Closed and stalled connections reconnect (local test server) |
| The engine is safe at the extremes | 442 automated checks: bounded output, no NaN, no allocation, block-size independent, click-free control and patch changes |
| It fits comfortably in a CoreAudio callback | Worst case 4.5–5.5 % of the buffer period down to 32 samples at 96 kHz on one USB interface; a positive control shows the late-callback counter works. Caveat: the sweep did not log whether the radio was playing, so the engine's state in each window is unknown; a synthetic check puts the difference at about 30 ns per sample |
| The room and the broadcast change the sound only through cables, scaled by COUPLE | Tested numerically for every cable |
| TALORA's patching rules carry over | Adapter agrees with TALORA's real `Graph` on 120 000 random edits |
| A Blender-rendered control can be driven by JUCE | One knob, checked in a snapshot |

## What the evidence does not support

- **That it sounds good, or distinct.** Nobody has heard it. The only sound-related figures are level and
  zero-crossing proxies on a synthetic test source, and they are unremarkable: across its control states the engine
  stays within about 5 dB of the dry source, with crest factors of 12–16 dB against 11 dB dry. Those numbers say the
  engine is neither silent nor exploding. They say nothing about character.
- **That it differs enough from "radio through a filter and delay."** This is the brief's stated failure case and
  it is a live risk. Structurally the engine is a bank of feedback comb filters, and a comb filter *is* a short
  delay with feedback. What might set it apart is tuning by a pitch control, six coupled voices, saturation that
  lets it sustain past unity gain, and control inputs driven by the broadcast and the room. Whether a listener
  hears an instrument or an effect is exactly what the REFERENCE mode is there to settle. **If the two modes sound
  alike in the listening test, the concept as built has failed and should be reported as such**, not decorated.
- **That the room signal is musically useful.** It now works on hardware (values from the MiniFuse input respond
  to sound; the permission flow is verified), but nobody has played with it.
- **That mouse or MIDI interaction feels right.** Neither has been used by a person.
- **CoreAudio reliability in general.** One device, minutes not hours, no sleep/wake, no device switching.

## Defects and limitations found

1. **Fixed: macOS asked for the microphone at launch, and denying it left no output.** Cause established: an app
   that declares microphone use is prompted as soon as it opens a device that has inputs, even for playback. The
   instrument now declares none; ROOM runs in a separate helper app with its own permission. Verified on fresh,
   denied and granted permissions (`EVIDENCE-LOG.md`). Residue: two apps side by side, and macOS asks again about
   the helper after each unsigned rebuild.
2. The radio is several seconds behind the broadcast (up to 6 s of buffer).
3. Station and Mac clocks are not synchronised; a slow station will eventually cause a 0.75 s re-buffer.
4. Ogg/Opus/FLAC and HLS streams are refused. Some stations only offer those.
5. Control signals are unipolar: a cable can only push a parameter one way.
6. An input takes one cable; two sources cannot be mixed into one destination.
7. No MIDI controller is mapped by default; MIDI learn is the mechanism. No hardware has been tried.
8. Speakers plus microphone form a loop through the control path. It is bounded (ROOM can add at most 0.2 of loop
   gain, the loops saturate, the output is clipped at −1 dBFS) but it will be audible as the instrument "pumping
   itself". Headphones remove it.
9. Bluetooth headsets switch to a low-quality mode when their microphone opens; ROOM uses the default input.
10. The app is ad-hoc signed: other Macs will refuse it without a Developer ID build, and macOS may treat each
    rebuild as a new program when it remembers the microphone decision.

## Go / revise / stop

Not decided here; it depends on the listening sessions. The prototype is built so that each outcome is cheap:

- **Go** if the engine is clearly preferred to REFERENCE and the owner wants to keep playing. Next would be the
  microphone-prompt fix, bipolar and mixable control signals, and the first real look.
- **Revise** if the interaction is good but the sound is not: the engine is one 300-line header behind a fixed
  interface, so a different resonator or excitation model can be swapped in without touching the radio, patching
  or UI.
- **Stop** if it is a radio with effects.

## Open questions

Technical:

1. Should the helper stay a separate app, or become a signed XPC service once there is a Developer ID?
2. What happens when microphone access is revoked while ROOM is on?
3. How should clock drift be handled over a long session: adaptive resampling, or let the buffer breathe?
4. Is a 6 s buffer the right trade between resilience and "live"? Would 1–2 s survive real networks?
5. What happens on sleep/wake, on losing Wi-Fi, on unplugging the interface?
6. Should Ogg/Opus be added, and with which library and licence?
7. Should TALORA's routing layer be extracted into a shared library, and under which licence?
8. Is the per-sample cost variation between buffer sizes real or a measurement artefact?
9. Which licence does WILDBAND ship under, given JUCE's AGPL/commercial terms?

Creative:

1. Does it feel like an instrument or an effect? (REFERENCE A/B.)
2. Is retuning a gesture, with its fade, ring-out and 0.2–2 s wait, or just a wait?
3. Is the TUNE edge "detune" (ring modulation) worth keeping, or a gimmick?
4. Does the room matter? Can a clap or a voice be *played*, or is it only a wobble?
5. Is patching a performance gesture here, or configuration? Would fewer, fatter sockets be better?
6. Are six resonators and four SHAPE families the right palette? Should MIDI chords set the ratios?
7. Should DRIFT wander by itself at all, or only when something drives it?
8. Is the speech / music / noise difference between stations interesting enough to make station choice musical?
9. What is the right behaviour in silence: decay to nothing (as now), or keep a breath of self-noise?
