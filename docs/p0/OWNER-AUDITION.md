# WILDBAND — What to audition now (one build)

Everything is in **one build**. Monitors down first; nobody has heard any of this.

```bash
open auditions/p1-lab/WILDBAND.app        # from the repository root
```

This binary is built from the top of the stack: branch `p1/plugin-proof` (#15), which contains the integration
candidate (#7), the STRIKE responses (#8), the tuner restart fix (#9), the voice lab (#10), the licence (#11), the
Rings experiment (#13) and the shared instrument core. The standalone behaves as before; the bundle carries its
licence and third-party notices in `Contents/Resources`.

It keeps its own settings (`WILDBAND-integration.settings`, started as a copy of your existing setup) and never
writes the file the four older builds share. **LAB choices are remembered between launches**; while either is not at
its default the status line says so ("LAB voice: …", "LAB strike: …"). With **LAB** left alone it is the integration candidate: your P0
instrument plus per-cable DEPTH, the STRIKE correction and the tuner. The four older builds are in `auditions/p0-*`
(see `OWNER-CHECKLIST.md`). ROOM will ask for the microphone once more (the helper was rebuilt).

For a comparison you do not want to keep, use a throwaway profile:

```bash
open auditions/p1-lab/WILDBAND.app --args --profile=scratch
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
| Fast, slow and failed connections | Real stations are fast. For repeatable slow and failed ones, run the test server and SET the addresses below. **SET replaces the station in the current slot and there are no spare slots, so do this in the `scratch` profile** (command above), which leaves your stations alone |

```bash
ffmpeg -v error -y -f lavfi -i "sine=frequency=330:duration=20" -ac 2 -ar 44100 -b:a 128k build/tone.mp3 && python3 -I tools/stream_test_server.py build/tone.mp3   # from the repository root
```

`http://127.0.0.1:8765/wait/3` (three seconds to arrive), `http://127.0.0.1:8765/wait/7` (longer than the static
lasts at full strength), `http://127.0.0.1:8765/missing` (fails at once), `http://127.0.0.1:8765/silent` (never
answers). Stop the server with Ctrl-C.

## 4. Voice experiment — LAB → Voice experiment (live)

**Dissolve** and **Dissolve → Resonator** use the same knobs and cables: PITCH = time, SHAPE = clear echo … cloud,
DRIFT = spray, FEEDBACK = regeneration, TONE = damping. Try a speech station, then change station with a long TIME
and SHAPE near the top. Then pull and re-plug ROOM cables, with DEPTH. PANIC clears it.

Questions: is it a voice or an effect? Does it belong with the resonators, instead of them, or not at all?

## 4b. Rings — LAB → Voice experiment → Rings (live, at 48 kHz)

The roadmap's priority resonator experiment: Émilie Gillet's Rings DSP (MIT), driven by the broadcast. Three models:
**modal**, **sympathetic strings**, **inharmonic string**. Same knobs: PITCH = note, SHAPE = structure, TONE =
brightness, FEEDBACK = damping (decay), DRIFT = position. Keep the interface at **48 kHz** (it is tuned for that).

Compare with **Resonator** on the same station and knob positions: speech, music, something percussive, and static
during a station change. Then patch ROOM LEVEL → TONE and ROOM HITS → STRIKE and use DEPTH.

Question: is any of the three more expressive than the current resonator — enough to replace it, or to sit beside it?

## 5. Candidate renders (offline, 5 minutes per fixture)

Level-matched files for every candidate, including the ones that are not in the instrument (Korupt, Wyrd, Drift):

```bash
open auditions/p1-lab/voice-lab
```

Sort by name: `music__dry`, `music__reference`, `music__resonator`, `music__dissolve`, `music__korupt`,
`music__wyrd-timefilter`, each with `world-off` and `world-on`. The fixtures are synthetic (a formant voice,
drums, a chord sequence, tuner static, silence, a sine); the "world" in these files is a scripted stand-in, not a
room. The Korupt files are in the `korupt-unverified/` subfolder. **Treat them with suspicion**: the harness may be driving it wrongly (see `VOICE-RESEARCH.md`).

## 6. The plugin, in Logic and Live (first time in a real host)

This build's AU and VST3 are kept in `auditions/p1-lab/plugin/` (ad-hoc signed; the AU passes `auval`). **The AU
now installed for your user is a later build** (#19, explicit RECEIVE: it connects to a station only after RECEIVE
is pressed in its temporary panel). To hear this build in a host, install its AU in place of that one, and copy the
VST3 if you want to try it in Live:

```bash
rm -rf ~/Library/Audio/Plug-Ins/Components/WILDBAND.component && cp -R auditions/p1-lab/plugin/WILDBAND.component ~/Library/Audio/Plug-Ins/Components/
```

```bash
cp -R auditions/p1-lab/plugin/WILDBAND.vst3 ~/Library/Audio/Plug-Ins/VST3/
```

It is an instrument (Logic: *Software Instrument → AU Instruments → Shortwav Labs → WILDBAND*). There is no WILDBAND
panel yet — the host shows ten sliders. Try: it makes sound within a few seconds (static, then a station);
automate TUNE across stations; play notes (PITCH); save, close and reopen the project; two instances; bypass; a
bounce. Note anything odd — especially whether **plain `http://` stations** (WFMU, BBC) play inside the host.
To remove it: delete `WILDBAND.component` from that folder.

## What I need back

1. STRIKE response: A, B or C — or "none, because…".
2. Static: keep, change its sound, or drop? Default level?
3. Dissolve: worth pursuing? alone or into the resonators?
4. From the renders: which candidates deserve a proper port?
5. What happened in Logic / Live (section 6).
6. Confirmation that the five Tapestry LOOP files (`tapestry-core/buffer/splice/grain/dsp.h`) are solely yours, so the LOOP port can start.

## What was measured on this build (muted; technical only)

- `wildband_tests` 831 checks, `wildband_tuner_check` 27 real-time checks, TALORA graph parity: all pass.
- CoreAudio sweep at defaults ([`evidence/audition-coreaudio-default.md`](evidence/audition-coreaudio-default.md)):
  radio live in every window, worst callback 4.0 % of the buffer period, no late callbacks.
- The same sweep with **Dissolve selected** ([`evidence/audition-coreaudio-dissolve.md`](evidence/audition-coreaudio-dissolve.md)):
  radio live in every window, worst callback 5.5 % (96 kHz / 32), no late callbacks; mean cost about 30 % above
  the resonator alone. Dissolve → Resonator was not swept.
- Both sweeps' positive control (a deliberately stalled callback) was detected.

## Not yet done by anyone

Listening; operating DEPTH, LAB, `<` `>` by hand; MIDI hardware; ROOM on this build; CoreAudio timing with a LAB
voice selected; sleep/wake, network loss, long sessions.
