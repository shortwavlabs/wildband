# WILDBAND — What to audition now (one build)

Everything is in **one build**. Monitors down first; nobody has heard any of this.

```bash
open /Users/shortwavlabs/Workspace/shortwavlabs/wildband-integration/build/WILDBAND_artefacts/Release/WILDBAND.app
```

It keeps its own settings (`WILDBAND-integration.settings`, started as a copy of your existing setup) and never
writes the file the four older builds share. With **LAB** left alone it is the integration candidate: your P0
instrument plus per-cable DEPTH, the STRIKE correction and the tuner. The four older builds are still where they
were. ROOM will ask for the microphone once more (the helper was rebuilt).

For a comparison you do not want to keep, use a throwaway profile:

```bash
open /Users/shortwavlabs/Workspace/shortwavlabs/wildband-integration/build/WILDBAND_artefacts/Release/WILDBAND.app --args --profile=scratch
```

## 1. The consolidated instrument (15 minutes)

| Try | Listen for |
|---|---|
| Your usual patch and stations | Does anything feel different from the build you auditioned, other than what was added on purpose? |
| Click a round cable, turn DEPTH through 0 to −100 | Zero = that cable does nothing; negative = it works backwards |
| `<` `>`, click station names, STATIC at 0, half, full | Static only while the station is really arriving; the tail rings through |
| TUNE off-centre | The degradation you liked, unchanged |
| AUDIO… → another output and back | Silence and back, **no static** (changing a device is no longer treated as retuning) |

## 2. STRIKE response — LAB → STRIKE response

Patch ROOM HITS → STRIKE (headphones, ROOM on), COUPLE high. For each of **A Held**, **B Fast**, **C Organic**:

| Do | Listen for |
|---|---|
| One clap, then wait | How long the instrument stays "ducked" before the steady sound returns: A about 5 s then a long tail, B under a second, C a slow swell out over a few seconds |
| Clap a steady pulse | A and C stay ducked between claps; **B breathes back** between them |
| Talk, then stop | Which one follows you without lagging or flapping? |

Tell me which suits rhythmic playing, which suits ambient playing, and whether one of them could simply be *the*
behaviour. None of them has a knob, on purpose.

## 3. Tuner listening comparisons

| Comparison | How |
|---|---|
| Off-centre degradation | TUNE slowly from a station's centre to its edge and back, STATIC at 0 |
| Static at different intensities | STATIC 0 / 0.25 / 0.5 / 1, same `>` press each time |
| Rapid hopping | Hammer `>`; then map it to a Launch Control XL button (MIDI LEARN → click `>` → press the button) |
| Transitions with strong feedback | FEEDBACK about 0.8, MIX right, then `>` |
| Fast, slow and failed connections | Real stations are fast. For repeatable slow and failed ones, run the test server and SET these addresses into spare slots |

```bash
cd /Users/shortwavlabs/Workspace/shortwavlabs/wildband-integration && ffmpeg -v error -y -f lavfi -i "sine=frequency=330:duration=20" -ac 2 -ar 44100 -b:a 128k build/tone.mp3 && python3 -I tools/stream_test_server.py build/tone.mp3
```

`http://127.0.0.1:8765/wait/3` (three seconds to arrive), `http://127.0.0.1:8765/wait/7` (longer than the static
lasts at full strength), `http://127.0.0.1:8765/missing` (fails at once), `http://127.0.0.1:8765/silent` (never
answers). Stop the server with Ctrl-C.

## 4. Voice experiment — LAB → Voice experiment (live)

**Dissolve** and **Dissolve → Resonator** use the same knobs and cables: PITCH = time, SHAPE = clear echo … cloud,
DRIFT = spray, FEEDBACK = regeneration, TONE = damping. Try a speech station, then change station with a long TIME
and SHAPE near the top. Then pull and re-plug ROOM cables, with DEPTH. PANIC clears it.

Questions: is it a voice or an effect? Does it belong with the resonators, instead of them, or not at all?

## 5. Candidate renders (offline, 5 minutes per fixture)

Level-matched files for every candidate, including the ones that are not in the instrument (Korupt, Wyrd, Drift):

```bash
open /Users/shortwavlabs/Workspace/shortwavlabs/wildband-integration/build/voice-lab
```

Sort by name: `music__dry`, `music__reference`, `music__resonator`, `music__dissolve`, `music__korupt`,
`music__wyrd-timefilter`, each with `world-off` and `world-on`. The fixtures are synthetic (a formant voice,
drums, a chord sequence, tuner static, silence, a sine); the "world" in these files is a scripted stand-in, not a
room. **Treat the Korupt files with suspicion**: the harness may be driving it wrongly (see `VOICE-RESEARCH.md`).

## What I need back

1. STRIKE response: A, B or C — or "none, because…".
2. Static: keep, change its sound, or drop? Default level?
3. Dissolve: worth pursuing? alone or into the resonators?
4. From the renders: which candidates deserve a proper port?
5. "Apply it" for AGPL-3.0-only, and the copyright holder's name for the headers.

## Not yet done by anyone

Listening; operating DEPTH, LAB, `<` `>` by hand; MIDI hardware; ROOM on this build; CoreAudio timing with a LAB
voice selected; sleep/wake, network loss, long sessions.
