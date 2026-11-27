# WILDBAND

**A live instrument for playing the world.**

Wildband is an exploratory Shortwav Labs project: a macOS-first electronic performance instrument driven by live internet radio, the performer's surroundings, and hands-on interaction.

This repository is in **research / feasibility**. No implementation, platform qualification, or release commitment has been made.

## Design position

- **An instrument, not a recording utility.** Music is experienced in the moment; recording, sampling, and export are not required product workflows.
- **The performer participates.** Gesture, listening, improvisation, and learned technique matter more than autonomous randomization.
- **The world participates.** Live broadcasts and optional environmental sensors influence sound and modulation; live conditions can change from one performance to the next.
- **Musical agency first.** Environmental inputs can be shaped, attenuated, and bypassed; uncertainty is not an excuse for uncontrollable sound.
- **Mac first.** macOS standalone is the initial research target. AU/VST3 and Windows remain contingent decisions.

See the upcoming research documents for hypotheses, constraints, and explicit evidence gates. No code or DSP architecture is considered validated yet.

## P0 prototype (research build)

A playable proof of concept lives on the `p0/prototype` branch. It answers a feasibility question; it is not a
product, and none of its design is approved. Start with [`docs/p0/REPORT.md`](docs/p0/REPORT.md) and
[`docs/p0/PLAYTEST-GUIDE.md`](docs/p0/PLAYTEST-GUIDE.md).

Requirements: macOS 12 or later on Apple Silicon, Xcode command-line tools, CMake 3.22 or later. JUCE 9.0.5 is
fetched by CMake at a pinned commit (add `-DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE` to use a local checkout of
that commit). No other dependency: streams are decoded by macOS.

```bash
cmake -S . -B build
```

```bash
cmake --build build -j
```

```bash
ctest --test-dir build
```

```bash
open build/WILDBAND_artefacts/Release/WILDBAND.app
```

The build puts `WILDBAND.app` and `WILDBAND Room.app` side by side; keep them together. WILDBAND Room is the small
helper that holds the microphone permission for the optional ROOM feature. The instrument itself never asks for the
microphone.

Other tools: `build/wildband_radio_probe_artefacts/Release/wildband_radio_probe <url>...` tests stream addresses
without an audio device; `WILDBAND.app/Contents/MacOS/WILDBAND --measure=FILE` runs the muted CoreAudio timing
sweep (it changes the output device's sample rate and puts it back); `--mute` runs silently without touching saved
settings; `--probe=FILE` logs which CoreAudio operation, if any, makes macOS ask for the microphone. The Blender knob is rebuilt with the two commands at the top of `assets/blender/knob.py`.

## Licence

WILDBAND is free software under the **GNU Affero General Public License, version 3.0 only** (`AGPL-3.0-only`):
see [`LICENSE`](LICENSE). Copyright © 2026 Stephane P. Pericat. It is built with JUCE under JUCE's AGPLv3 option.
Third-party components and their licences are listed in [`THIRD-PARTY-NOTICES.md`](THIRD-PARTY-NOTICES.md); what
has been reused from other projects, and what distributing a build obliges you to do, is in
[`docs/LICENSING.md`](docs/LICENSING.md). Anyone who distributes a WILDBAND binary must offer its complete
corresponding source under the same licence. The licence covers the code; it grants no rights to the Shortwav Labs or
WILDBAND names, logos or visual identity.
