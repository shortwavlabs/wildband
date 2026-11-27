# WILDBAND P0 — Play-testing guide

A prototype for listening sessions. It is ugly on purpose. Nothing it does is recorded or saved except knob
positions, the six station addresses, the patch and the MIDI map.

## Before you start

- **Turn your monitors down first.** LEVEL starts at or below half, but nobody has heard this program yet.
- Build it (see the repository README), then:

```bash
open build/WILDBAND_artefacts/Release/WILDBAND.app
```

- **A macOS microphone prompt for WILDBAND is probably already on your screen**, left unanswered from testing, and
  macOS may ask again when the app starts. This is a known defect: the app has not asked for the microphone and does
  not listen until ROOM is on, yet it produces no sound until the prompt is answered. The cause is not established.
- **Please check both answers, "Don't Allow" first.** Whether the instrument still plays after "Don't Allow" has
  never been tested; if it stays silent, that is a second defect worth knowing before anything else. To get the
  question again afterwards:

```bash
tccutil reset Microphone com.shortwavlabs.wildband
```

## How do I start a live radio stream?

It starts by itself on the station under the TUNE needle. The top line shows `CONNECTING`, `BUFFERING`, then
`LIVE` with the codec and buffer length. To change station, turn **TUNE** (the big knob) into another station's
section of the strip and let it rest a quarter of a second. To use your own station, turn to the slot you want to
replace, paste a direct MP3 or AAC stream address (`http://` or `https://`; `.m3u` and `.pls` also work) into the
box and press **SET**. `FAILED` with a reason means that address is not supported.

No internet? Drag a cable from **LOCAL AUDIO** to **RESONATOR IN**. That is a built-in test pattern, labelled as
such; it is not radio.

## How do I enable or disable environmental sensing?

Tick **ROOM (microphone)**. macOS asks for permission the first time. The microphone is analysed for loudness,
sudden sounds and brightness only; it is never played, stored or sent anywhere. Untick to stop; the three ROOM
sockets then read zero. If permission is denied, ROOM stays off and the line beside the buttons says why.

**Use headphones for the first ROOM session.** With speakers, the instrument hears itself and can push its own
feedback up. It is limited, but you should meet that on purpose, not by surprise.

## How do I control the instrument?

| Control | Turn it and listen for |
|---|---|
| **TUNE** | Station choice. Near the edge of a station's section the radio goes "off-centre": a growl, then a warble |
| **PITCH** | The note the resonators ring at (C2–C6). MIDI notes set it too |
| **SHAPE** | The chord of the six resonators: near-unison → fifths and octaves → harmonic → bell-like |
| **TONE** | Dark to bright ringing |
| **COUPLE** | How strongly all the control cables act. At zero the radio and room stop modulating anything |
| **DRIFT** | Still and pure → wandering, with the resonators bleeding into each other |
| **FEEDBACK** | How long it rings. Past about two-thirds it sustains without the radio |
| **MIX** | Radio alone → resonators alone |
| **LEVEL** | Output volume |
| **PANIC** | Silences the resonators instantly |

Double-click a knob to reset it. **ENGINE / DRY / REFERENCE** switch between the instrument, the untouched source,
and an ordinary filter-and-delay on the same knobs.

**Patch bay** (bottom): outputs on top, resonator inputs below. Drag from an output to an input to patch; inputs
that will accept the cable light up. Square sockets are audio, round ones are control signals; the two do not mix.
Drag a cable's lower end to another input to move it, or drop it in empty space to pull it out. One output can
feed several inputs; each input takes one cable.

## How do I connect a MIDI controller?

Plug it in; every MIDI input is opened automatically (also when plugged in later). Nothing is mapped at first.
Tick **MIDI LEARN**, touch a knob on screen, move a control on the hardware; the knob's label then shows its
controller number and the mapping is remembered. Repeat per knob. Keys or pads set PITCH. No MIDI hardware has been
tried with this build.

## Which controls should I try first?

1. Let a music station play. Raise **FEEDBACK** slowly from the bottom to about two-thirds.
2. Sweep **PITCH**, then **SHAPE**, at that setting.
3. Take **MIX** fully right, then pull the **RADIO AUDIO → IN** cable out and listen to the tail. Plug it back.
4. Switch **ENGINE ↔ REFERENCE** with the same knob positions. This is the most important comparison.
5. Push **FEEDBACK** to the top, then play **TONE** and **DRIFT**. Use **PANIC** if you want out.
6. Turn **COUPLE** up, and move the **RADIO HITS** cable between STRIKE, PITCH and FEEDBACK.
7. Try a speech station (BBC World Service) and the noisy one (SF 10-33) with the same settings.
8. On headphones, enable **ROOM** and clap, talk, tap the desk; move the ROOM cables.
9. Ride **TUNE** across station boundaries as a gesture.

## What should I listen for?

- Does ENGINE do something REFERENCE cannot? If not, say so plainly.
- Can you get, on purpose: a quiet texture, a slowly evolving resonance, something rhythmic, something unstable —
  and get back?
- Does each knob do what you expect the second time you reach for it?
- Do the room and the broadcast change what you play, or only decorate it?
- Does retuning feel like part of the playing?
- Clicks, dropouts, sudden level jumps, anything harsh at FEEDBACK maximum.
- After fifteen minutes: do you want to keep going?

## Listening-test protocol (G3 / G4)

Two unrecorded sessions of at least 15 minutes, on different days if possible; one with headphones and ROOM on.
In each: at least one music, one speech and one noisy station; the ENGINE / DRY / REFERENCE comparison at three
different knob settings; ten minutes of free play. Write the notes below straight after.

```
Session:            date / duration / headphones or speakers / stations / MIDI controller / ROOM on?
Responsiveness      1–5   notes:
Learnability        1–5   notes:
Surprise (good)     1–5   notes:
Surprise (bad)      1–5   notes:
ENGINE vs REFERENCE       clearly different / somewhat / the same — what is the difference?
Three states reached on purpose?   quiet texture [ ]  evolving resonance [ ]  rhythmic [ ]  unstable [ ]
Did the room matter?      yes / a little / no
Did patching feel like playing?    yes / a little / no
Glitches heard (what, when):
Want to play again?       yes / maybe / no
One thing to change first:
```

## Reliability checks only the owner can do (G5)

Close and open the lid mid-stream. Switch the output device in **AUDIO…** and in System Settings while playing.
Turn Wi-Fi off for 20 seconds. Deny the microphone, then allow it. Revoke it in System Settings while ROOM is on.
Leave it running for an hour and note any re-buffering.

## Known limitations

- The microphone prompt at launch, described above; denying it is untested.
- MP3 and AAC only: no Ogg, Opus, FLAC or HLS stations.
- The radio is a few seconds behind the broadcast; a retune takes up to about two seconds.
- Control cables only push one way; one cable per input.
- No presets, no undo for cables, no keyboard control, no accessibility work.
- Only the output device can be chosen; ROOM uses the system's default input.
- Ad-hoc signed: it runs on this Mac. macOS may ask about the microphone again after a rebuild.
- Tested for minutes on one audio interface. Not tested across sleep, device changes or network loss.
