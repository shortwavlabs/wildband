# WILDBAND — Licensing and provenance inventory (for owner approval)

> **Applied, 2026-11-27 (third pass).** The owner approved AGPL-3.0-only and named the copyright holder
> (Stephane P. Pericat). `LICENSE`, SPDX and copyright headers on every WILDBAND source file,
> `THIRD-PARTY-NOTICES.md`, the third-party licence texts in `licenses/` and [`docs/LICENSING.md`](../LICENSING.md)
> (source-distribution obligations and the Plugin Jam reuse disclosure) now exist. This file remains the inventory
> and its history; **`docs/LICENSING.md` is the current statement.** Mutable Instruments / Rings and Tapestry LOOP
> provenance are in section 0a below.
>
> **Update, 2026-11-27 (second pass) — see section 0.** The owner has confirmed the JUCE route (AGPLv3 option) and
> sole authorship of Korupt, Wyrd and Drift, and has authorized reuse of TALORA's architectural concepts. The
> delegated inventory has been re-verified first-hand, and the Vult-generated code has been reproduced byte for
> byte. **The licence itself is still not applied**: no `LICENSE` file or headers exist yet.

## 0a. Third pass: Mutable Instruments (Rings) and Tapestry LOOP

Both are on the approved roadmap and neither is in the repository yet.

| Source | What was checked first-hand | Finding |
|---|---|---|
| **Rings**, upstream `pichenettes/eurorack` (read from the public fork `shortwavlabs/eurorack`, checked out as TALORA's submodule) | Header of `rings/dsp/part.h`; the repository README's licence section | Every DSP file carries "Copyright 2015 Emilie Gillet" and the **MIT** permission notice. README: "Code (AVR projects): GPL3.0. Code (STM32F projects): MIT license. Hardware: cc-by-sa-3.0". Rings is an STM32F project → MIT. `stmlib`, which it depends on, is a separate MIT repository |
| What to take | — | Only `rings/dsp/**`, `rings/resources.*` and the `stmlib` DSP/utility headers they include, from **upstream at a pinned commit**, with headers intact, plus the MIT text in `licenses/`. Not the firmware (`drivers/`, `bootloader/`, `cv_scaler`, `ui`), not hardware files, not AVR code |
| TALORA's Rings adapter (`modulo/src/modules/RingsAdapter.*`, `third_party/overrides`) | Not opened for this pass | Private. Covered by the concept-level authorization only (how blocks, rates and strumming were handled may inform WILDBAND's own adapter); its code and its patched upstream files are **not** copied |
| Name | — | "Mutable Instruments" and "Rings" identify the origin in credits. WILDBAND does not present itself as a Mutable Instruments product, and the voice gets its own name in the UI |
| **Tapestry** (`vcv-tapestry`, files `src/dsp/tapestry-*.h`) | `git log --format=%an` per file (below) | GPL-3.0-or-later per `plugin.json`. See the authorship note |

**Tapestry authorship, checked for this pass** (`vcv-tapestry` at `48517c8`, `git log --format=%an -- <file>`):

| File | Committers | Co-author trailers | Note |
|---|---|---|---|
| `tapestry-dsp.h`, `tapestry-buffer.h`, `tapestry-core.h`, `tapestry-splice.h`, `tapestry-grain.h` | the owner only | none | The files the LOOP research names. Plain C++; they include only each other |
| `tapestry-effects.h` | the owner | **one "Co-authored-by: Copilot" trailer** | Not needed for LOOP. If it is ever wanted, that AI-assisted contribution is covered by the open question on AI authorship |
| `Tapestry.cpp`, `TapestryExpander.*` (Rack wrapper) | the owner and `copilot-swe-agent[bot]` | several | Not to be ported in any case (Rack wrapper, file I/O) |

The owner's confirmation of sole authorship (second pass) was given for Korupt, Wyrd and Drift. **It has not been
asked for Tapestry's LOOP files**; git supports it, and the port (roadmap gate L0) should record the owner's
confirmation before code is copied.

## 0. Second pass: decisions recorded and verification completed

### Owner decisions (recorded verbatim in substance)

| Decision | Recorded |
|---|---|
| **JUCE licensing route** | "AGPLv3 option" (owner's answer, 2026-11-27). No commercial JUCE licence covers WILDBAND, so a distributed WILDBAND must be AGPL-3.0 as a whole |
| **Authorship of Korupt, Wyrd, Drift and their `.vult` sources** | "Yes, solely mine" (owner's answer, 2026-11-27), reusable in WILDBAND under GPL-3.0-or-later terms |
| **TALORA reuse** | Authorized: reuse of TALORA's **architectural concepts** — typed patching, routing validation, graph management and real-time-safe communication patterns — and the existing `PlanMailbox` may remain public in PRs #4, #5 and #6 with no replacement or history rewrite. **Not authorized:** publishing TALORA's private codebase as such, its Mutable Instruments integrations, third-party assets or dependencies |
| Proposed licence | AGPL-3.0-only remains the direction; **not yet applied** |

### What was re-verified first-hand in this pass

| Item | How | Result |
|---|---|---|
| Authors of the candidate DSP files | `git log --format=%an -- <path>` in `vcv-tapestry` for `korupt-dsp.h`, `wyrd-dsp.h`, `drif-dsp.h`, `src/dsp/generated`, `src/dsp/vult` | Only the owner; no co-author trailers on those paths |
| Third-party code actually compiled into the app | Compiler dependency files of the WILDBAND target | HarfBuzz, SheenBidi, IJG jpeglib, LunaSVG (with PlutoVG), libpng, zlib. From `juce_audio_formats/codecs` only JUCE's own wrapper headers — no FLAC, Vorbis, Opus, VST3, LV2, ASIO or Oboe source |
| JUCE's declared licence | `JUCE.spdx.json` | 28 packages `AGPL-3.0-only OR LicenseRef-JUCE-Commercial` |
| GPLv3 ↔ AGPLv3 combination | Text of GPLv3 §13 in `vcv-tapestry/LICENSE.md` | "you have permission to link or combine any covered work with a work licensed under version 3 of the GNU Affero General Public License into a single combined work… The terms of this License will continue to apply to the part which is the covered work" |
| Rack SDK terms | `Rack-SDK/LICENSE.html` | GPLv3-or-later; the plugin exception covers *using the API from a plugin distributed free of charge*; "plugins that copy a significant portion of non-API source code from Rack must be licensed under GPLv3" |
| Vult runtime licence | Header of `vultin.h` / `vultin.cpp` | MIT, © 2015 Leonardo Laguna Ruiz |
| **Vult reproducibility** | Ran the installed compiler (`vultc` v0.4.15, npm package `vult`, MIT) on the committed `.vult` sources: `vultc -ccode drift_core.vult -o drift_core`, likewise `wyrd_core` | **All six generated files (`.h`, `.cpp`, `.tables.h` for both cores) are byte-identical to the committed ones.** The generated DSP is reproducible from the owner's sources with a known compiler version. (The `make generate-vult` target the sources mention still does not exist; the command above is what it should run) |

Still from the delegated survey and not re-checked: the per-branch commit counts, the exact module list from build
flags, and the description of `modulo`'s vendored Mutable Instruments tree.

### What this unblocks, and what it does not

| Area | Status |
|---|---|
| WILDBAND's own code, the TALORA-concept code in `src/Patch.h` | **Clear** to be licensed AGPL-3.0-only once the owner says "apply it" |
| Wyrd and Drift (plain C++ and Vult output) | Provenance clear. May be brought in as GPL-3.0-or-later files with their notices and the MIT notice for the Vult runtime, **after** WILDBAND's licence is applied. Until then they are studied in place, from their own repository |
| Korupt | Provenance clear, but it uses VCV Rack SDK types. **Do not copy the Rack types**: replace `RCFilter`, the slew limiter and exponential filter with WILDBAND's own one-poles (they are a few lines of textbook DSP each) and `rack::math::clamp` with `std::clamp`. Blocked until that rewrite and the licence application |
| AFTERIMAGE code | Same licence as proposed (AGPL-3.0-only). Its Freesound test recordings are **not** to be reused: WILDBAND's research fixtures are synthesized (see `VOICE-RESEARCH.md`) |
| Unrelated WILDBAND work (integration, STRIKE, tuner, research harness, WILDBAND's own new DSP) | **Not blocked** |

Rule kept throughout: a file with no licence header is not free to reuse; its repository's licence and its authorship decide.

---

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
| `src/Patch.h` — `PlanMailbox` (branch `p0/attenuverter`) | The standard three-slot "latest value" exchange, written with TALORA's `setRouting`/`adoptRouting` in view and matching it closely | **Published in PR #4 before authorization, and carried unchanged by the two branches stacked on it (`p0/strike-neutral`, `p0/tuner`). Recommendation: retain, subject to the owner's explicit authorization of this specific code.** No history has been rewritten and no further TALORA-derived code has been written or published |
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
  `AGPL-3.0-only OR LicenseRef-JUCE-Commercial` — "only", not "or later". **WILDBAND has been built on the assumption that it uses the AGPLv3
  option.** Whether Shortwav Labs holds a commercial JUCE licence was not checked and is for the owner to confirm; if
  none is held, AGPL-3.0-only is the natural choice for the whole program.
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
| Vult **generated** code (`*_core.{h,cpp}`, tables) | Header is only "Code automatically generated by Vult"; no licence text, no generator version. The compiler project (`modlfo/vult`) is MIT *(survey)* | Generated from `.vult` sources the owner wrote and committed | Includes `vultin.h`; uses one inline helper from it *(survey, by grep)* | Presumably the MIT runtime notice plus whatever applies to compiler output (**a question for a lawyer, below**). The `make generate-vult` target the sources mention does not exist *(survey)*, so the generated files cannot currently be regenerated and verified — worth fixing before reuse |
| `after-image` — AFTERIMAGE | **AGPL-3.0-only**, per-file SPDX; JUCE under its AGPLv3 option | Owner, with Claude co-author trailers *(survey)* | JUCE | Same licence as proposed for WILDBAND: no conflict. Its test recordings (Freesound, licences unknown, its blocker B11) must not be redistributed or reused as WILDBAND fixtures |
| `modulo` — TALORA | **No licence file**, private | Owner, with Claude co-author trailers *(survey)* | Mutable Instruments sources as a submodule, mixed licences | Owner authorization per derivation (section 1) |

Compatibility in one line: AGPL-3.0-only (WILDBAND, JUCE, AFTERIMAGE) + GPL-3.0-or-later (tapestry) + MIT (Vult
runtime) can be distributed together as an AGPL-3.0 combined work, provided each notice is preserved and the
whole is offered with complete corresponding source. That reading of GPLv3 §13 is an engineer's, and is on the
lawyer's list below.

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

1. ~~Say "apply it" for AGPL-3.0-only, and name the copyright holder.~~ **Done** (third pass).
2. ~~Authorize, or not, `PlanMailbox`.~~ **Authorized** (section 0).
3. Confirm Shortwav Labs holds all rights in TALORA's routing layer and is content for the derived `Patch.h` to be
   AGPL-licensed in a public repository.
4. ~~Confirm sole authorship of Korupt, Wyrd, Drift and their `.vult` sources.~~ **Confirmed** (section 0).
5. Decide how AI-assisted contributions are recorded (the co-author trailers here, Copilot commits in tapestry).

## 6. Questions for a lawyer, not an engineer

- Whether AI-assisted or AI-authored commits affect copyright ownership or the ability to license.
- Whether re-implementing Korupt without the Rack types leaves any Rack GPL obligation.
- Exact notice obligations for the compiled permissive components in a distributed macOS app.
- Whether shipping default third-party station addresses and names, or performing with broadcast audio in a public
  demo, needs the broadcasters' permission.
- "AGPL-3.0-only" versus "or later" for WILDBAND's own files (JUCE's declaration is "only").
- Whether code generated by the Vult compiler from the owner's `.vult` sources is unencumbered, and whether the MIT
  notice for the Vult runtime is the only obligation.
- That GPL-3.0(-or-later) code from `vcv-tapestry` may be combined into an AGPL-3.0-only program under GPLv3 §13,
  and what each part's licence then is.
