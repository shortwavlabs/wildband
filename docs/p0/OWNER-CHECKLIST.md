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

## Builds to audition

All four share **one settings file** (knob positions, stations, patch, MIDI map), so a change made in one shows up
in the others. Run one at a time; quit one before opening the next. Each needs its `WILDBAND Room.app` beside it,
and macOS may ask about WILDBAND Room again the first time ROOM is used in each.

| Build | Branch | Path |
|---|---|---|
| Baseline (what you auditioned) | `p0/prototype` | [`auditions/p0-baseline/WILDBAND.app`](../../auditions/p0-baseline/) |
| + per-cable DEPTH | `p0/attenuverter` | [`auditions/p0-depth/WILDBAND.app`](../../auditions/p0-depth/) |
| + DEPTH + STRIKE correction | `p0/strike-neutral` | [`auditions/p0-strike-neutral/WILDBAND.app`](../../auditions/p0-strike-neutral/) |
| + DEPTH + tuner (static, NEXT/PREVIOUS) | `p0/tuner` | [`auditions/p0-tuner/WILDBAND.app`](../../auditions/p0-tuner/) |

Paths are relative to the repository root. `auditions/` is a local folder kept out of git (excluded in
`.git/info/exclude`); each build sits beside its own `WILDBAND Room.app`, and `auditions/README.txt` records the
branch and commit each was built from. The tuner build does not include the STRIKE correction.

## C. Attenuverter audition (build from branch `p0/attenuverter`)

Click a round cable (or its lower socket): a DEPTH strip appears next to MIDI LEARN.

| # | Do this | Listen for | Notes |
|---|---|---|---|
| 1 | Launch with your saved patch | It sounds exactly as before (every cable loads at +100 %) | |
| 2 | Headphones, ROOM on, ROOM LEVEL → FEEDBACK. DEPTH at 0, about +25, +100, −100 | 0: the room does nothing. +: louder room = longer ring. −: louder room = shorter ring | |
| 3 | RADIO HITS → STRIKE at +100, 0, −100 | +: hits strike. 0: steady. −: hits punch holes (provisional behaviour) | |
| 4 | RADIO LEVEL → TONE and → DRIFT with different depths on each; DRIFT negative | Each cable keeps its own amount; negative DRIFT slows the wander (provisional) | |
| 5 | Drag a cable with a non-default depth to another input | It keeps its depth (badge moves with it) | |
| 6 | Is selecting a cable and turning DEPTH quick enough to do while playing? | | |

## D. STRIKE correction audition (build from `p0/strike-neutral`)

| # | Do this | Listen for | Notes |
|---|---|---|---|
| 1 | ROOM off, drag ROOM HITS → STRIKE | Nothing changes (in the other builds the sound dims) | |
| 2 | Default patch on a music station, then on BBC World Service | Same as the baseline on busy material? On speech, the steady excitation comes back a few seconds into a pause, where the baseline stayed dimmed | |
| 3 | Is the hold right? (fully held about 4.6 s after a strong hit, gone by about 14 s) | Shorter breathes more; longer keeps the punchy contrast | |

## E. Tuner audition (build from `p0/tuner`)

| # | Do this | Listen for | Notes |
|---|---|---|---|
| 1 | STATIC at 0, change stations | Exactly the old behaviour: fade, silence, fade | |
| 2 | STATIC at half, press `>` and `<`, click station names | Static for as long as the station takes, ringing the resonators; none once LIVE. On fast stations it is only a blip — is that worth having? | |
| 3 | TUNE off-centre | The degradation you liked, unchanged | |
| 4 | High FEEDBACK, then `>` | The tail rings through the change; static excites it | |
| 5 | SET a bad address (e.g. a web page), then `>` | FAILED with a reason, no hiss left hanging; the next station arrives normally | |
| 6 | Hammer `>` ten times | It lands on the right station, once | |
| 7 | PANIC during static | Silence until the station arrives | |
| 8 | MIDI LEARN → click `>` → press a Launch Control XL button; same for `<` and a station name | The button navigates; a pad mapped this way no longer changes PITCH | |
| 9 | Does static plus off-centre detune feel like one instrument or a muddle? Is the static's sound right (it was designed without listening)? | | |

## F. Decisions waiting on you

1. **Authorize, or not, `PlanMailbox`** (the TALORA-style plan hand-off now on three public branches). Nothing more
   will be derived from TALORA, and no history rewritten, until you say.
2. WILDBAND's licence: AGPL-3.0-only is proposed; `LICENSING-AND-PROVENANCE.md` lists what approving it involves.
3. Sole authorship of Korupt, Wyrd and Drift in `vcv-tapestry` (before any port; Task D stays at planning).
4. After auditioning: keep the STRIKE correction? keep STRIKE ducking and DRIFT slowing at negative depth?
