# WILDBAND P0 — Play-testing guide

A prototype for listening sessions. It is ugly on purpose. Nothing it does is recorded or saved except knob
positions, the six station addresses, the patch and the MIDI map.

## Before you start

- **Turn your monitors down first.** LEVEL starts at or below half, but nobody has heard this program yet.
- Build it (see the repository README), then:

```bash
open build/WILDBAND_artefacts/Release/WILDBAND.app
```

- The app must stay next to **WILDBAND Room.app** (the build puts both in `build/WILDBAND_artefacts/Release/`).
- **WILDBAND itself never asks for the microphone.** If macOS ever asks on behalf of "WILDBAND" (not "WILDBAND
  Room"), that is a regression: please report it.

## How do I start a live radio stream?

It starts by itself on the station under the TUNE needle. The top line shows `CONNECTING`, `BUFFERING`, then
`LIVE` with the codec and buffer length. To change station, turn **TUNE** (the big knob) into another station's
section of the strip and let it rest a quarter of a second. To use your own station, turn to the slot you want to
replace, paste a direct MP3 or AAC stream address (`http://` or `https://`; `.m3u` and `.pls` also work) into the
box and press **SET**. `FAILED` with a reason means that address is not supported.

No internet? Drag a cable from **LOCAL AUDIO** to **RESONATOR IN**. That is a built-in test pattern, labelled as
such; it is not radio.

## How do I enable or disable environmental sensing?

Tick **ROOM (microphone)**. A small helper app, WILDBAND Room, starts, and macOS asks whether *it* may use the
microphone (the first time, and again after every rebuild of this unsigned prototype). The radio keeps playing
whatever you answer. If you allow it, the line beside the buttons shows what ROOM hears: `level`, `hits`, `bright`
and the raw input level in dBFS. Untick ROOM to stop: the helper quits and the three ROOM sockets read zero. If you
deny it, ROOM unticks itself and says how to change your mind (System Settings > Privacy & Security > Microphone >
WILDBAND Room).

**Which input:** **AUDIO…** → *ROOM input*. By default it is the interface you are playing through if it has inputs
(the MiniFuse), otherwise the system default input. All of that device's input channels are summed. Changing it
while ROOM is on restarts the helper only.

The input is analysed for loudness, sudden sounds and brightness. It is never played, stored or sent off this Mac.

**Use headphones for the first ROOM session**, or keep the microphone away from the speakers. Otherwise the
instrument hears itself and can push its own feedback up. It is limited, but meet that on purpose.

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
Turn Wi-Fi off for 20 seconds. Revoke WILDBAND Room's microphone access in System Settings while ROOM is on. Unplug the
input device while ROOM is on.
Leave it running for an hour and note any re-buffering.

## Known limitations

- macOS asks about WILDBAND Room again after every rebuild (unsigned prototype).
- Room values arrive about 35 ms late; all input channels are summed, with no channel picker.
- MP3 and AAC only: no Ogg, Opus, FLAC or HLS stations.
- The radio is a few seconds behind the broadcast; a retune takes up to about two seconds.
- Control cables only push one way; one cable per input.
- No presets, no undo for cables, no keyboard control, no accessibility work.
- Keep WILDBAND.app and WILDBAND Room.app in the same folder.
- Ad-hoc signed: it runs on this Mac only.
- Tested for minutes on one audio interface. Not tested across sleep, device changes or network loss.
