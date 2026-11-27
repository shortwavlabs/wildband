# WILDBAND — Licensing

**Approved by the owner on 2026-11-27.** Engineering documentation, not legal advice.

## Licence

- **WILDBAND's original code: GNU Affero General Public License, version 3.0 only** — SPDX `AGPL-3.0-only`.
  Full text: [`LICENSE`](../LICENSE).
- **Copyright © 2026 Stephane P. Pericat** (confirmed by the owner as the legal copyright holder).
- **JUCE is used under its AGPLv3 option** (owner's confirmation, 2026-11-27); no commercial JUCE licence covers
  WILDBAND. A distributed WILDBAND binary is therefore AGPL-3.0 as a whole.
- "Only", not "or later": JUCE's own declaration is `AGPL-3.0-only`, and AFTERIMAGE uses the same.
- No licence to the **Shortwav Labs** or **WILDBAND** names, logos or visual identity is implied by the code licence.

## What carries which notice

| Files | Notice |
|---|---|
| Every WILDBAND source file (`src/`, `tests/`, `tools/`, `research/`, `CMakeLists.txt`, `cmake/`, `assets/blender/`) | `SPDX-License-Identifier: AGPL-3.0-only` and the copyright line, at the top |
| `tests/baseline/*` | The same header. They are frozen copies of WILDBAND's own files; the header is the only thing added to them |
| Documentation (`README.md`, `docs/`) and the rendered image `assets/knob/knob.png` | No per-file header; covered by this statement, same licence and holder |
| Third-party licence texts in `licenses/` | **Unmodified.** Each keeps its own terms; they are not relicensed |
| Third-party code brought in later (see below) | Keeps its own licence header and notice, in its own directory, never re-headed as AGPL |

A file with no licence header is **not** free to reuse: its repository's licence and its authorship decide.

## Source distribution obligations (AGPL-3.0, sections 4–6 and 13)

Whenever a WILDBAND binary is given to anyone — a download, the Plugin Jam submission, a build sent to a tester:

1. **Complete corresponding source** for exactly that binary must be offered: this repository at the tagged commit
   the binary was built from, plus the build instructions (the README) and the pinned JUCE commit (fetched by CMake;
   JUCE's source is public at that commit).
2. The binary must be accompanied by the **licence and the notices**: `LICENSE`, `THIRD-PARTY-NOTICES.md` and
   `licenses/` are copied into the app bundle's `Resources` by the build; an installer must also show or include them.
3. **State where the source is**: the About/credits text and the download page must give the repository URL and the
   exact tag. Tag every released build; never ship from an untagged or dirty tree.
4. **Modified versions** distributed by anyone carry the same obligations and must say they were modified.
5. **Section 13 (network use)**: WILDBAND is a local instrument, not a network service, so the remote-interaction
   clause has no practical effect today. It would if any part were ever run as a service for others.
6. Do not add terms that restrict these rights (no extra EULA on the open-source build).
7. Signing and notarization do not change any of this: a notarized binary still needs its source offer.

### Corresponding source: every dependency of the distributed configuration

"Complete corresponding source" means everything needed to rebuild **the binary that was actually shipped**, in the
configuration it was built with — not just this repository. Record, for each release, the exact configure command
and every external source it pulled in:

| Dependency | When it is compiled in | What the source offer must identify |
|---|---|---|
| WILDBAND (this repository) | always | the release tag |
| JUCE | always | commit `d60b7a78030661574a615b586d9325ba7a7fa7dd` (pinned in `CMakeLists.txt`) |
| Third-party code inside JUCE (zlib, HarfBuzz, SheenBidi, LunaSVG/PlutoVG, libpng, IJG jpeglib) | always, via JUCE | covered by the JUCE commit; licence texts in `licenses/` |
| VST3 SDK, AudioUnitSDK | plugin builds | covered by the JUCE commit; licence texts in `licenses/` |
| Any optional upstream enabled at configure time (for example Rings and `stmlib` with `-DEURORACK_DIR` / `-DWILDBAND_FETCH_RINGS=ON`) | **only if enabled** | the upstream repositories and exact commits, and their licence texts in the bundle. A build that enables an optional upstream ships that upstream's source obligations too |

A release built from a configuration not listed here is not ready to ship.

### Release checklist

- [ ] Tag the commit; build from a clean checkout of the tag.
- [ ] Record the exact configure command; list every optional upstream it enables, with repository and commit.
- [ ] Regenerate `THIRD-PARTY-NOTICES.md` from `JUCE.spdx.json` and the release build's dependency files
      (the AU/VST3 targets add the VST3 SDK and AudioUnit SDK).
- [ ] `LICENSE`, `THIRD-PARTY-NOTICES.md`, `licenses/` present inside every bundle and in the installer.
- [ ] Credits text names the licence, the repository URL and the tag.
- [ ] Every third-party source directory still carries its own licence file and headers.
- [ ] No recording, sample or broadcast audio in the repository, the bundle or the demo material without rights.
- [ ] The reuse disclosure below is up to date.

## Plugin Jam: reuse disclosure

The contest requires a public repository under an OSI-approved licence (AGPL-3.0 is one) and disclosure of reused
code and third-party material. **As of this commit:**

| What | Origin | Licence | How it is used |
|---|---|---|---|
| JUCE 9.0.5 | JUCE / Raw Material Software | AGPL-3.0-only (chosen option) | Application framework, fetched at build time; not in this repository |
| Third-party code inside JUCE (zlib, HarfBuzz, SheenBidi, LunaSVG/PlutoVG, libpng, IJG jpeglib) | their authors | Zlib, MIT variants, Apache-2.0, libpng-2.0, IJG | Compiled in through JUCE; texts in `licenses/` |
| Patch graph validation and plan hand-off (`src/Patch.h`: `Graph::check`, port/connection types, `PlanMailbox`) | **TALORA** (`shortwavlabs/modulo`, private, by the same author) | Released here under AGPL-3.0-only with the owner's authorization | Re-typed from TALORA's routing layer and hand-off; no TALORA file is included. Authorization covers TALORA's architectural concepts, not its codebase, its Mutable Instruments integration or its assets |
| Test-harness and measurement patterns (plain `CHECK` macro, allocation gate, host-time late-callback counting, pinned-JUCE CMake layout) | **AFTERIMAGE** (`shortwavlabs/after-image`, AGPL-3.0-only, same author) | AGPL-3.0-only | Patterns re-typed; no AFTERIMAGE file is included. Its Freesound test recordings are **not** used |
| "Dissolve" voice experiment (`src/Dissolve.h`) | New for WILDBAND, after the TRACE → CLOUD idea described for AFTERIMAGE | AGPL-3.0-only | Shares no code with AFTERIMAGE |
| Research harness candidates (Korupt, Wyrd, Drift) | `shortwavlabs/vcv-tapestry` (GPL-3.0-or-later, same author); Korupt also uses VCV Rack SDK headers (GPL-3.0-or-later with a plugin exception); Drift and Wyrd use the Vult runtime (MIT, © 2015 Leonardo Laguna Ruiz) | as stated | **Compiled from their own repository when its path is given to CMake; none of their code is in this repository** |
| Rings resonator DSP and `stmlib` (**optional LAB experiment**, `docs/p1/RINGS-EXPERIMENT.md`) | Émilie Gillet, Mutable Instruments — upstream `pichenettes/eurorack` at `f4cfa76`, `stmlib` at `e3bd7c9` | MIT | Compiled **unmodified from an upstream checkout** when the build is configured for it; not in this repository. `src/RingsVoice.h` is WILDBAND's own adapter around upstream's public interface. Notice in `licenses/Rings-stmlib-MIT.txt` |
| AI assistance | Claude (Anthropic) | — | Much of this repository's code and documentation was written with AI assistance under the owner's direction; commits carry `Co-Authored-By` trailers. The owner is the copyright holder |
| Knob image | Rendered with Blender from `assets/blender/knob.py` | AGPL-3.0-only | Placeholder asset |
| Radio stations | Third-party broadcasters | — | Only public stream addresses are in the source; no audio is stored or distributed. Use of broadcast audio in the contest film needs separate rights |

**Planned, not yet in the repository** (each will be added to the table when it lands, with its notices):

| Planned | Origin | Licence | Constraint |
|---|---|---|---|
| Rings, if chosen as the voice | as in the table above | MIT | Decide then whether to keep fetching upstream at the pinned commit or to vendor the used files (headers intact). Never the hardware design files (CC BY-SA), AVR code (GPL-3.0), the "Mutable Instruments" name as branding, or TALORA's private adapters beyond concepts |
| LOOP, from Tapestry | `shortwavlabs/vcv-tapestry` (same author) | GPL-3.0-or-later | Combined with AGPL-3.0 code under GPLv3 §13; the Tapestry-derived files keep a GPL-3.0-or-later header and notice. No Rack SDK types; no file import/export |
| Korupt | `vcv-tapestry` | GPL-3.0-or-later | As above, and the Rack SDK types are replaced, not copied |
| Wyrd, Drift | `vcv-tapestry`, generated from the owner's Vult sources (reproduced byte for byte with `vultc` 0.4.15) | GPL-3.0-or-later; Vult runtime MIT | Keep the Vult runtime's MIT notice; keep the `.vult` sources and the exact generator command with the generated files |
| AU / VST3 wrappers | JUCE; VST3 SDK (MIT); AudioUnit SDK (Apache-2.0) | as stated | Regenerate the notices when the plugin targets exist |

## Combining GPL-3.0 and AGPL-3.0 code

GPLv3 §13 and AGPLv3 §13 each allow a work under one to be combined with a work under the other; each part stays
under its own licence, and the AGPL's network clause applies to the combination. So GPL-3.0-or-later files from
`vcv-tapestry` can live in WILDBAND **as GPL-3.0-or-later files**. As their sole author the owner could also
relicense them; that is not assumed. (An engineer's reading; see the open questions.)

## Still open (owner or lawyer)

- Whether AI-assisted authorship affects ownership or the ability to license, and how it should be credited.
- Exact notice wording an installer must show for the Apache-2.0 and IJG components.
- Whether default third-party station addresses and names may ship, and rights for broadcast audio in the film.
- Whether reimplementing Korupt without the Rack types leaves any Rack-licence question.
- Status of Vult compiler output (assumed: the owner's work, plus the MIT runtime notice).
