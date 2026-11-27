# Third-party notices

WILDBAND's own code is licensed under **AGPL-3.0-only** (see [LICENSE](LICENSE)); copyright © 2026 Stephane P. Pericat.

WILDBAND is built with one third-party framework, **JUCE 9.0.5** (commit `d60b7a78030661574a615b586d9325ba7a7fa7dd`),
fetched at configure time and **not stored in this repository**. JUCE's modules are dual-licensed (AGPLv3 or the
commercial JUCE licence). **WILDBAND uses JUCE under the AGPLv3 option** — confirmed by the owner on 2026-11-27 —
not the commercial licence. JUCE's own licence statement is reproduced in [`licenses/JUCE-LICENSE.md`](licenses/JUCE-LICENSE.md).

## How this list was derived

From JUCE's SPDX bill of materials (`JUCE.spdx.json` at the pinned commit) and from the compiler dependency files of
the WILDBAND app target, which show which vendored sources are actually compiled. **Rebuild this list whenever the
JUCE pin, the linked modules or the build options change**, and again from the release build before distribution.

JUCE modules linked by the app: `juce_audio_utils`, `juce_audio_processors`, `juce_audio_processors_headless`,
`juce_audio_formats`, `juce_audio_devices`, `juce_audio_basics`, `juce_gui_extra`, `juce_gui_basics`,
`juce_graphics`, `juce_data_structures`, `juce_events`, `juce_core`. The radio probe and tuner check link
`juce_core` only. The WILDBAND Room helper, the engine tests and the voice lab link no JUCE.

## Compiled into the WILDBAND app

| Component | Version | Licence | Vendored at (in JUCE `modules/`) | Licence text |
|---|---|---|---|---|
| JUCE modules listed above | 9.0.5 | AGPL-3.0-only (chosen) OR JUCE commercial | — | [`licenses/JUCE-LICENSE.md`](licenses/JUCE-LICENSE.md), [LICENSE](LICENSE) |
| zlib | 1.3.2 | Zlib | `juce_core/zip/zlib` | [`licenses/zlib.txt`](licenses/zlib.txt) |
| HarfBuzz | 15.0.0 | MIT-Modern-Variant ("Old MIT") | `juce_graphics/fonts/harfbuzz` | [`licenses/HarfBuzz.txt`](licenses/HarfBuzz.txt) |
| SheenBidi | 2.9.0 | Apache-2.0 | `juce_graphics/unicode/sheenbidi` | [`licenses/SheenBidi-Apache-2.0.txt`](licenses/SheenBidi-Apache-2.0.txt) (upstream ships no NOTICE file) |
| LunaSVG | 3.5.0 | MIT | `juce_graphics/drawables/lunasvg` | [`licenses/LunaSVG.txt`](licenses/LunaSVG.txt) |
| PlutoVG | 1.3.2 | MIT | `juce_graphics/drawables/lunasvg/plutovg` | [`licenses/PlutoVG.txt`](licenses/PlutoVG.txt) |
| libpng | 1.6.58 | libpng-2.0 | `juce_graphics/image_formats/pnglib` | [`licenses/libpng.txt`](licenses/libpng.txt) |
| jpeglib (Independent JPEG Group) | 10.0 | IJG | `juce_graphics/image_formats/jpglib` | [`licenses/jpeglib-IJG-README.txt`](licenses/jpeglib-IJG-README.txt) |

"This software is based in part on the work of the Independent JPEG Group."

The licence texts above are copied unmodified from the JUCE checkout at the pinned commit, and are placed in the
app bundle (`WILDBAND.app/Contents/Resources/licenses/`) by the build.

## Vendored in a linked JUCE module but not compiled in this configuration

| Component | Licence | Why it is not compiled |
|---|---|---|
| FLAC, libogg, libvorbis, Opus, opusfile, libopusenc | BSD-3-Clause | `JUCE_USE_FLAC=0`, `JUCE_USE_OGGVORBIS=0`, `JUCE_USE_OPUS=0`; streams are decoded by macOS (AudioToolbox) |
| libwebp | BSD-3-Clause | `JUCE_USE_WEBP=0` |
| JUCE's MP3 reader | part of JUCE | `JUCE_USE_MP3AUDIOFORMAT=0` |
| VST3 SDK 3.8.0 | MIT | no plugin hosting and, in this build, no plugin target |
| LV2, lilv, serd, sord, sratom | ISC | no plugin hosting |
| PreSonus plug-in extensions | public domain | no plugin target |
| Oboe | Apache-2.0 | Android only |
| ASIO SDK | Steinberg ASIO OR GPL-3.0-only | Windows only; `JUCE_ASIO` off |
| `@juce-framework/webview` | AGPL-3.0-only OR JUCE commercial | `JUCE_WEB_BROWSER=0` |

**This table will change when the AU/VST3 plugin targets are added** (the VST3 SDK and AudioUnit SDK then become
compiled-in components, as in AFTERIMAGE). The list must be regenerated at that point.

## System components (not distributed)

Apple frameworks — AudioToolbox, CoreAudio, AVFoundation, CoreFoundation, Foundation, Cocoa, CoreMIDI, CoreAudioKit,
Accelerate, IOKit, QuartzCore, Security, WebKit, DiscRecording — are part of macOS. MP3 and AAC decoding is done by
macOS; WILDBAND distributes no codec.

## Build-time tools (not distributed)

Blender renders `assets/knob/knob.png` from `assets/blender/knob.py`; ffmpeg tiles it and makes test tones;
Python runs the test server; `vultc` was used only to verify another repository's generated code. None is linked
into or shipped with WILDBAND.

## Not in WILDBAND

No source code from TALORA (`shortwavlabs/modulo`), `vcv-tapestry` (Tapestry, Korupt, Wyrd, Drift), AFTERIMAGE,
Mutable Instruments, the VCV Rack SDK or the Vult runtime is contained in this repository at this commit. What has
been *derived* from other Shortwav Labs work, and what is planned, is recorded in
[`docs/LICENSING.md`](docs/LICENSING.md) and [`docs/p0/LICENSING-AND-PROVENANCE.md`](docs/p0/LICENSING-AND-PROVENANCE.md).

No broadcast audio, recording or sample is contained in or distributed with WILDBAND. The default station
addresses in `src/Main.cpp` point at third parties' public streams; their names belong to their owners.
