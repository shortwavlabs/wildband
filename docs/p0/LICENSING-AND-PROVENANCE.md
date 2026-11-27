# WILDBAND — Licensing and provenance inventory (for owner approval)

**Date:** 2026-11-27. **Proposed licence for WILDBAND's own code: `AGPL-3.0-only`**, matching AFTERIMAGE and JUCE's
open-source option. **Nothing has been applied:** no `LICENSE` file, no SPDX headers, no notices have been added.
This document is the inventory and the list of decisions; applying it waits for the owner's approval.

This is an engineering inventory, not legal advice. Questions that need the owner or a lawyer are listed at the end.

**How this was checked.** A delegated read-only pass gathered the facts from the files and git history. I re-checked
these against the sources myself: JUCE's SPDX licence declarations, the Vult runtime's MIT header and the generated
files' header, the Rack SDK licence wording, and that WILDBAND has no licence file or licence headers. Other items
are from that pass and are marked *(survey)* where they carry weight.

## 1. WILDBAND's own tree

| Item | Finding |
|---|---|
| Licence file, NOTICE, third-party notices | None on any branch. GitHub reports no licence. README says "Licensing is undecided" |
| Per-file licence or copyright headers | None in any source file |
| Authors | One human author on every branch (the owner, under two addresses). Commits made in this work carry a `Co-Authored-By: Claude Fable 5.2` trailer *(survey: 10 on `p0/prototype`, 13 on `p0/attenuverter` at the time)* |
| Third-party code vendored in the repository | **None.** JUCE is fetched by CMake at a pinned commit and is not stored here |
| Bundled assets | `assets/knob/knob.png`, rendered by `assets/blender/knob.py` (own script; `math`, `os`, `bpy` only). No fonts. Three UI screenshots under `docs/p0/evidence/` |
| Radio stations | Six default stream addresses in `src/Main.cpp`. No broadcast audio is stored or distributed. Station names are third parties' |

### Files with a provenance beyond "written for WILDBAND"

| File | Origin | Status |
|---|---|---|
| `src/Patch.h` — `Graph::check` and the port/connection types | Re-typed from TALORA (`shortwavlabs/modulo`, private, **no licence file**), rule for rule | Published in PR #3 with the owner's go-ahead for that push |
| `src/Patch.h` — `PlanMailbox` (branch `p0/attenuverter`) | The standard three-slot "latest value" exchange, written with TALORA's `setRouting`/`adoptRouting` in view and matching it closely | **Published in PR #4 before authorization. Recommendation: retain, subject to the owner's explicit authorization of this specific code.** No history has been rewritten and no further TALORA-derived code has been published |
| `tests/TaloraParity.cpp` | WILDBAND's own file; compiles TALORA's `Routing.cpp` from *outside* this repository when `-DTALORA_DIR` is given | No TALORA file is in the repository. It does encode TALORA's `PortSpec` field layout |
| `docs/p0/TALORA-REUSE.md` | Describes the private repository's structure | Published with the same go-ahead as `Patch.h` |
| `tests/baseline/*` (branches) | Frozen copies of WILDBAND's own `Engine.h` / `Patch.h` at `9c0a409` | Own code; the `Patch.h` copy carries the same TALORA derivation |

**Standing rule recorded here:** authorization to publish one piece of TALORA-derived code is not permission to
publish more. Each new derivation is flagged and waits.

To license `Patch.h` under AGPL-3.0-only, Shortwav Labs must hold the rights to the TALORA code it derives from and
choose to release that derivation. TALORA's own tree vendors Mutable Instruments sources (mixed GPLv3 / MIT /
CC BY-SA per its own notes) *(survey)*; the routing layer WILDBAND derives from is not part of that vendored code.

## 2. What a WILDBAND binary contains

### JUCE 9.0.5 (commit `d60b7a78`)

- Licence: dual, "AGPLv3" or commercial. JUCE's SPDX file declares every module
  `AGPL-3.0-only OR LicenseRef-JUCE-Commercial` — "only", not "or later". **WILDBAND uses the AGPLv3 option; no
  commercial JUCE licence is held.** That is what makes AGPL-3.0-only the natural choice for the whole program.
- Modules linked by the app *(survey, from the build's own flags)*: `juce_audio_utils`, `juce_audio_processors`,
  `juce_audio_processors_headless`, `juce_audio_formats`, `juce_audio_devices`, `juce_audio_basics`,
  `juce_gui_extra`, `juce_gui_basics`, `juce_graphics`, `juce_data_structures`, `juce_events`, `juce_core`.
  The radio probe and tuner check link `juce_core` only. The WILDBAND Room helper and the engine tests use no JUCE.

### Third-party code inside those JUCE modules

From `JUCE.spdx.json`; "compiled" is read from an existing build's dependency files *(survey)* and should be
regenerated from the release build before distribution.

| Component | Version | Licence | In the WILDBAND app |
|---|---|---|---|
| zlib | 1.3.2 | Zlib | Compiled |
| HarfBuzz | 15.0.0 | MIT-Modern-Variant | Compiled |
| SheenBidi | 2.9.0 | Apache-2.0 | Compiled |
| LunaSVG, PlutoVG | 3.5.0, 1.3.2 | MIT | Compiled |
| libpng | 1.6.58 | libpng-2.0 | Compiled |
| jpeglib (IJG) | 10.0 | IJG | Compiled |
| libwebp | 1.6.0 | BSD-3-Clause | Not compiled (`JUCE_USE_WEBP=0`) |
| FLAC, libogg, libvorbis, Opus, opusfile, libopusenc | — | BSD-3-Clause | Not compiled (codecs switched off; decoding is done by macOS) |
| VST3 SDK, LV2 stack, pslextensions | — | MIT / ISC / public domain | Not compiled (no plugin hosting or plugin targets) |
| Oboe, ASIO SDK, AAX SDK | — | Apache-2.0 / Steinberg-or-GPL-3.0-only / Avid-or-GPL-3.0-only | Not compiled (other platforms / formats) |
| `@juce-framework/webview` | 1.0.0 | AGPL-3.0-only or JUCE commercial | Not compiled (`JUCE_WEB_BROWSER=0`) |

All compiled components carry permissive licences with notice requirements (Apache-2.0 also has a NOTICE and
patent clause). None is copyleft beyond JUCE itself.

### System components

Apple frameworks linked: AudioToolbox, CoreAudio, AVFoundation, CoreFoundation, Foundation, Cocoa, CoreMIDI,
CoreAudioKit, Accelerate, IOKit, QuartzCore, Security, WebKit, DiscRecording (several only because JUCE's module
declarations ask for them). MP3 and AAC decoding is Apple's, inside macOS; no codec is distributed.

### Build-time only (not distributed)

Blender (GPL) renders the knob image; ffmpeg tiles it and makes test clips; Python runs the test server. Their
licences do not attach to the outputs used here, but the knob is a placeholder and will be replaced.

## 3. Reuse candidates — not in WILDBAND; what they would bring

No candidate code has been copied. This is what Task D would have to honour.

| Source | Licence found | Authorship found | Third-party content | Obligation if code is taken |
|---|---|---|---|---|
| `vcv-tapestry` — Korupt (`korupt-dsp.h`) | Repository `LICENSE.md` is GPL v3 text; `plugin.json`, README and contributing notes say **GPL-3.0-or-later**. No per-file header | The three DSP headers and the Vult sources were committed only by the owner. Elsewhere in the repository there are 6 commits by `copilot-swe-agent[bot]` and "Co-authored-by: Copilot" trailers; none touch these files *(survey)* | **VCV Rack SDK**: includes `<dsp/filter.hpp>` and `<math.hpp>`, uses `rack::dsp::RCFilter`, `ExponentialSlewLimiter`, `ExponentialFilter`, `rack::math::clamp` | Keep the GPL notice. GPL-3.0-or-later code can be combined into an AGPL-3.0 work (GPLv3 §13), the GPL part staying GPL. The Rack types must be **replaced, not copied**: Rack is GPLv3-or-later, and its plugin exception covers using the API from a free plugin, not lifting SDK source into another program |
| `vcv-tapestry` — Wyrd (`wyrd-dsp.h`, `wyrd_core.*`) | As above | Owner only *(survey)* | Plain C++, plus Vult (next row) | As above, minus the Rack question |
| `vcv-tapestry` — Drift (`drif-dsp.h`, `drift_core.*`) | As above | Owner only *(survey)* | Vult | As above |
| Vult **runtime** (`vultin.h`, `vultin.cpp`) | **MIT**, "Copyright (c) 2015 Leonardo Laguna Ruiz" | Third party | — | Reproduce the MIT notice wherever these files or substantial parts go |
| Vult **generated** code (`*_core.{h,cpp}`, tables) | Header is only "Code automatically generated by Vult"; no licence text, no generator version. The compiler project (`modlfo/vult`) is MIT *(survey)* | Generated from `.vult` sources the owner wrote and committed | Includes `vultin.h`; uses one inline helper from it *(survey, by grep)* | Treated as the owner's work derived from his `.vult` sources, plus the MIT runtime notice. The `make generate-vult` target the sources mention does not exist *(survey)*, so the generated files cannot currently be regenerated and verified — worth fixing before reuse |
| `after-image` — AFTERIMAGE | **AGPL-3.0-only**, per-file SPDX; JUCE under its AGPLv3 option | Owner, with Claude co-author trailers *(survey)* | JUCE | Same licence as proposed for WILDBAND: no conflict. Its test recordings (Freesound, licences unknown, its blocker B11) must not be redistributed or reused as WILDBAND fixtures |
| `modulo` — TALORA | **No licence file**, private | Owner, with Claude co-author trailers *(survey)* | Mutable Instruments sources as a submodule, mixed licences | Owner authorization per derivation (section 1) |

Compatibility in one line: AGPL-3.0-only (WILDBAND, JUCE, AFTERIMAGE) + GPL-3.0-or-later (tapestry) + MIT (Vult
runtime) can be distributed together as an AGPL-3.0 combined work, provided each notice is preserved and the
whole is offered with complete corresponding source. A GPL-3.0-**only** component could not be upgraded, but none
was found among the candidates.

## 4. What "applying AGPL-3.0-only" would consist of (not done)

Modelled on AFTERIMAGE, for consistency:

1. `LICENSE`: the unmodified AGPL v3 text.
2. `// SPDX-License-Identifier: AGPL-3.0-only` and a copyright line at the top of every WILDBAND source file
   (holder to be named by the owner).
3. `THIRD-PARTY-NOTICES.md`: JUCE and its licence choice; a "compiled into the app" table and a "vendored but not
   compiled" table regenerated from `JUCE.spdx.json` and the release build; the licence texts or notices for zlib,
   HarfBuzz, SheenBidi (with its NOTICE), LunaSVG/PlutoVG, libpng and IJG.
4. `docs/LICENSING.md`: the licence statement, that no trademark licence for the Shortwav Labs name is implied, a
   release checklist (corresponding source offered with every binary; notices shipped inside the app bundle; the
   list rebuilt whenever the JUCE pin, modules or build options change).
5. A provenance header on `src/Patch.h` reflecting whatever the owner authorizes for the TALORA derivation.
6. README: replace "Licensing is undecided".
7. For AGPL §13: WILDBAND is a local desktop app, not a network service, so the remote-interaction clause has no
   practical effect today; it would if any part ever ran as a service.

## 5. Decisions for the owner

1. **Approve AGPL-3.0-only** for WILDBAND, and name the copyright holder for the headers.
2. **Authorize, or not, `PlanMailbox`** specifically. If not: it is replaced and the PR #4 branch rewritten.
3. Confirm Shortwav Labs holds all rights in TALORA's routing layer and is content for the derived `Patch.h` to be
   AGPL-licensed in a public repository.
4. Confirm sole authorship of Korupt, Wyrd, Drift and their `.vult` sources before Task D ports anything.
5. Decide how AI-assisted contributions are recorded (the co-author trailers here, Copilot commits in tapestry).

## 6. Questions for a lawyer, not an engineer

- Whether AI-assisted or AI-authored commits affect copyright ownership or the ability to license.
- Whether re-implementing Korupt without the Rack types leaves any Rack GPL obligation.
- Exact notice obligations for the compiled permissive components in a distributed macOS app.
- Whether shipping default third-party station addresses and names, or performing with broadcast audio in a public
  demo, needs the broadcasters' permission.
- "AGPL-3.0-only" versus "or later" for WILDBAND's own files (JUCE's declaration is "only").
