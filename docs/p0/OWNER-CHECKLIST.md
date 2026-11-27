# WILDBAND P0 — Owner checklist (things only you can do)

Short on purpose. Tick, add a few words, and paste it back. Baseline build: `p0/prototype`.
Nothing here is recorded by the app; write your notes somewhere else.

## A. Ten minutes of physical checks (no listening judgement needed)

| # | Do this | Expect | Result / notes |
|---|---|---|---|
| 1 | AUDIO… → change Output to another device and back while the radio plays | A short gap, then LIVE again; no microphone prompt | |
| 2 | AUDIO… → ROOM input: pick the MacBook microphone, tick ROOM, talk | Readout `level` moves, dBFS well above −100. (My first capture code gave silence on this mic; the replacement is unverified on it) | |
| 3 | With ROOM on, unplug the ROOM input device (or the MiniFuse) | ROOM switches itself off with a message; radio keeps playing or reconnects to another output | |
| 4 | With ROOM on: System Settings → Privacy & Security → Microphone → turn WILDBAND Room off | ROOM goes quiet or off; radio unaffected | |
| 5 | Close the lid for 30 s mid-stream, reopen | RECONNECTING → LIVE within about 10 s, no stuck silence | |
| 6 | Turn Wi-Fi off for 20 s, then on | Plays out the buffer, RECONNECTING, then LIVE; no noise burst | |
| 7 | MIDI LEARN with the Launch Control XL: map TUNE, FEEDBACK and one more; restart the app | Knobs follow the hardware; the mapping survives the restart | |
| 8 | Play a key or pad | PITCH jumps to the note | |
| 9 | FEEDBACK to maximum, then PANIC | Instant silence, then the radio excites it again | |

## B. Two listening sessions, 15 minutes each (different days if you can)

Session 1 on speakers, ROOM off. Session 2 on headphones, ROOM on.

In each, in this order:

1. **Three stations** with the same knob positions: one music, BBC World Service (speech), SF 10-33 (noisy).
2. **Matched comparison**, three times with different PITCH/SHAPE/FEEDBACK: ENGINE → DRY → REFERENCE → ENGINE.
   The question is only: does ENGINE do something REFERENCE cannot?
3. Ten minutes of free play, including pulling and moving cables and riding TUNE off-centre.

```
Session:          date / minutes / speakers or headphones / stations / controller / ROOM input
ENGINE vs REFERENCE:   clearly different / somewhat / the same — in what way?
States reached on purpose:   quiet texture [ ]  evolving resonance [ ]  rhythmic [ ]  unstable [ ]
Did the room matter (session 2)?   yes / a little / no — which cable?
Clicks, dropouts, level jumps (what, when):
Lost track of time?   yes / no        Open it tomorrow just to play?   yes / maybe / no
One thing to change first:
```

## C. Attenuverter audition (build from branch `p0/attenuverter`)

Click a round cable (or its lower socket): a DEPTH strip appears next to MIDI LEARN.

| # | Do this | Listen for | Notes |
|---|---|---|---|
| 1 | Launch with your saved patch | It sounds exactly as before (every cable loads at +100 %) | |
| 2 | Headphones, ROOM on, ROOM LEVEL → FEEDBACK. DEPTH at 0, about +25, +100, −100 | 0: the room does nothing. +: louder room = longer ring. −: louder room = shorter ring | |
| 3 | RADIO HITS → STRIKE at +100, 0, −100 | +: hits strike. 0: steady. −: hits punch holes | |
| 4 | RADIO LEVEL → TONE and → DRIFT with different depths on each | Each cable keeps its own amount | |
| 5 | Drag a cable with a non-default depth to another input | It keeps its depth (badge moves with it) | |
| 6 | Is selecting a cable and turning DEPTH quick enough to do while playing? | | |
| 7 | One decision: should a cable from a **silent** source into STRIKE dim the sound (as now, and as you auditioned), or do nothing? | See "Open conflict" in `ATTENUVERTER-ACCEPTANCE.md` | |

## D. Decisions waiting on you

1. WILDBAND's licence (JUCE is AGPLv3 or commercial; the reuse candidates are GPL-3.0 and AGPL-3.0-only).
2. Sole authorship of Korupt, Wyrd and Drift in `vcv-tapestry` (needed before any port).
3. The STRIKE question (C7).
4. Go-ahead for the tuner experiment (`PLAN-C-PLAYABLE-TUNER.md`) and for the voice comparison
   (`PLAN-D-COMPARATIVE-DSP.md`), in that order or another.
