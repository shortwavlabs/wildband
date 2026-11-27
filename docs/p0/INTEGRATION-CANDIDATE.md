# WILDBAND — Integration candidate

**Branch:** `p0/integration`. **Purpose:** one coherent instrument for the owner to audition, in place of four builds.
**Status:** draft; nothing merged into `main`; the four source branches and their PRs are untouched.

## Ancestry, as found before integrating

```
docs/p0-product-research (PR #1)  96a6b46
   \_ p0/prototype (PR #3)         49d671d   15 ahead / 19 behind the docs branch
        \_ p0/attenuverter (PR #4)  421e10b   3 ahead / 2 behind p0/prototype (two docs commits)
             |\_ p0/strike-neutral (PR #5)  685d750   2 ahead of p0/attenuverter
             \__ p0/tuner (PR #6)           133ac01   3 ahead of p0/attenuverter
```

PRs #5 and #6 were siblings: each lacked the other's change. Nothing had been rebased since they were opened.

## How it was built

`p0/integration` starts at `p0/tuner` and merges, with merge commits so every source commit keeps its identity:

1. `p0/strike-neutral` — **conflicts in two files, both "keep both sides"**: `src/Engine.h` (each branch added
   members and `prepare()` lines next to the same declarations) and `tests/Tests.cpp` (each added a line to the
   metrics printout).
2. `p0/prototype` — two documentation commits; no conflict.
3. `docs/p0-product-research` — the manifesto and reuse studies; no conflict.

**One semantic conflict that neither branch had alone:** the STRIKE tests copied `kNumParams` parameters into the
frozen P0 engine's array. That was correct on their branch (nine parameters); once the tuner branch's tenth
parameter (STATIC) arrived it wrote past the end of the frozen engine's nine-element array, and three STRIKE tests
failed with nonsense values (a 12 dB level change). They now copy the frozen engine's own count. This is exactly
the kind of fault the consolidated branch exists to catch.

## Settings isolation

The four earlier builds all read and write `~/Library/Application Support/WILDBAND.settings`, so auditioning one
silently changed what the next one started with. From this branch on:

- The build keeps its settings in **`WILDBAND-integration.settings`**. On first run it starts as a copy of the
  shared file, so your stations, patch and MIDI map carry over; **the shared file is never written by this build**
  (checked: its hash was unchanged after a snapshot run and a 40 s live run).
- `--profile=NAME` uses `WILDBAND-NAME.settings` instead, for a comparison that should not disturb your main setup:

```bash
open /Users/shortwavlabs/Workspace/shortwavlabs/wildband-integration/build/WILDBAND_artefacts/Release/WILDBAND.app --args --profile=compare-a
```

- The profile in use is shown in the tooltip of the WILDBAND title.
- To start a profile afresh, delete its file. To back up or restore, copy the file; it is plain XML:

```bash
cp "$HOME/Library/Application Support/WILDBAND-integration.settings" "$HOME/Library/Application Support/WILDBAND-integration.backup.settings"
```

The four older builds still share `WILDBAND.settings`; that cannot be fixed without changing them, and they are
kept as they are.

## Does each feature still behave as intended?

**Technical evidence only (all runs muted).**

| Feature (source PR) | Check on the integrated build | Result |
|---|---|---|
| P0 engine, radio, patching (#3) | The original 442-check suite is part of the run; frozen-engine parity for every patch with no cable in STRIKE | Pass |
| Per-cable depth (#4) | AT-01…AT-15 | Pass |
| STRIKE neutrality (#5) | ST-01…ST-04; AT-07 for all six destinations | Pass |
| Tuner (#6) | CT-01 (off-centre sound sample-identical to the frozen engine), CT-02…CT-16 engine-level | Pass |
| | `wildband_tuner_check`, real time against the local server: 24 checks ([`evidence/integration-tuner-check.txt`](evidence/integration-tuner-check.txt)) | Pass |
| | Live stations, muted: NEXT → KEXP, five quick NEXTs → 2 connections, PREVIOUS wraps ([`evidence/integration-live-navigation.txt`](evidence/integration-live-navigation.txt)) | Pass |
| All together | `wildband_tests`: **765 checks, 0 failed**, also under `-O3 -ffp-contract=off`; TALORA graph parity 120 000 edits, 0 disagreements | Pass |
| Real-time cost | CoreAudio sweep, 44.1/48/96 kHz × 32–512 samples ([`evidence/integration-coreaudio-minifuse.md`](evidence/integration-coreaudio-minifuse.md)) | Radio live in 100 % of all twelve windows; worst callback 3.9 % of the buffer period (96 kHz / 64); 0 late callbacks, underruns, faults; positive control responds |
| Interface | Rendered snapshot with a cable selected ([`evidence/integration-ui.png`](evidence/integration-ui.png)) | Renders; not operated by hand |

Not done on this build: any listening; any hand operation; MIDI hardware; ROOM (the helper will ask for the
microphone again the first time); long sessions.
