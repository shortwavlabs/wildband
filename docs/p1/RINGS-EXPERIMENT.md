# WILDBAND P1-R — Rings resonator experiment

**Branch:** `p1/rings`, on `licensing/agpl-3.0-only`. **Status:** provenance, method and acceptance criteria were
committed before any Rings code was connected; results are appended at the bottom. Roadmap: `docs/ROADMAP.md`, P1-R.

**No musical conclusion is drawn here. Whether Rings is more expressive than the current resonator is the owner's
call, after listening.**

## Why

The owner found the current resonator fun but "not particularly expressive". Rings (Mutable Instruments, Émilie
Gillet) is a mature resonator with three models — modal, sympathetic strings, inharmonic/nonlinear string — and
takes external audio as its exciter, which is exactly WILDBAND's situation: a broadcast striking a resonator.

## Provenance and licence (gate before any code)

| | |
|---|---|
| Source | Upstream `https://github.com/pichenettes/eurorack`, commit `f4cfa7697095a28d292bff758beb51deb49b5a86`; its `stmlib` submodule at `e3bd7c9cc00e4364166f9905c0509b6ffd0535ec` (`https://github.com/pichenettes/stmlib`) |
| Licence | **MIT** for STM32 projects (repository README) and in the header of every file used: "Copyright 2015 Emilie Gillet" plus the MIT permission notice |
| What is used | `rings/dsp/**`, `rings/resources.{h,cc}`, and the `stmlib` DSP and utility files those include. Nothing else |
| What is not | Firmware (`drivers/`, `bootloader/`, `cv_scaler`, `ui`), hardware design files (CC BY-SA), AVR projects (GPL-3.0), any other module |
| How it enters the build | **Not copied into this repository.** CMake compiles it from a checkout of upstream at the pinned commit (`-DEURORACK_DIR=…`), or fetches that commit itself (`-DWILDBAND_FETCH_RINGS=ON`). Upstream files are compiled unmodified |
| Notices | The MIT text goes in `licenses/` and `THIRD-PARTY-NOTICES.md` gains a row **in the commit that first links Rings into a distributed target**. Upstream headers stay intact |
| Combination | MIT code inside an AGPL-3.0 program: permitted, with the notice preserved |
| TALORA | TALORA integrated Rings before. Its adapter and its patched upstream files are private and are **not opened or copied**; this adapter is written from upstream's public interface (`rings::Part`) |
| Naming | Credits say "Rings resonator DSP by Émilie Gillet (Mutable Instruments), MIT licence". The UI does not use the Mutable Instruments name or panel; in the LAB menu it is listed as what it is, for comparison |

The checkout used for development is the public fork `shortwavlabs/eurorack` at the same commit hash as upstream
(`f4cfa76`), so it is byte-for-byte upstream.

## What upstream assumes (audit before integration)

| Assumption in upstream | Consequence for WILDBAND | Decision for this experiment |
|---|---|---|
| Sample rate fixed at 48 kHz (`kSampleRate`, coefficient tables, `a3`) | At another device rate everything scales | Pitch is compensated by `12·log2(48000/fs)`; timbre and decay still scale with the rate. A resampler is future work if Rings is chosen. **Results are stated for 48 kHz** |
| Blocks of at most 24 samples (`kMaxBlockSize`); coefficients are recomputed per block | Cannot be called per sample at reasonable cost | Adapter collects 24 samples, processes, and plays them out: **24 samples (0.5 ms) of latency**, independent of the host block size |
| Mono input, two outputs ("odd" and "even" partials) | Stereo broadcast must be summed | Input = L + R mix; outputs → left and right |
| Internal exciter and strummer (firmware decides when to strum) | WILDBAND's exciter is the broadcast | External excitation, no internal strum, polyphony 1. Internally triggered excitation is a separate, later comparison |
| A reverb needing a 32 768-sample buffer; an output limiter | Extra memory, extra character | Allocated by the adapter; behaviour left as upstream |
| Parameters arrive already smoothed by firmware CV code | Jumps could click | The adapter gets WILDBAND's already-smoothed controls; upstream interpolates within each block |
| `stmlib` expects an ARM target | Does not build as is | Compile with `-DTEST`, upstream's own switch for host builds |

## Control mapping (no new panel control)

PITCH → note (C2–C6, as now) · SHAPE → STRUCTURE · TONE → BRIGHTNESS · FEEDBACK → DAMPING (decay) · DRIFT →
POSITION. The six control inputs keep their names and act on the same five parameters plus input gain (STRIKE), so
the patch bay, per-cable depth, COUPLE, MIX, static and PANIC all apply unchanged.

## Acceptance criteria (registered before implementation)

| ID | Test | Passes when |
|---|---|---|
| RG-01 | Optional build | Without Rings configured, everything builds and all existing checks pass unchanged. With it, the three Rings voices appear in the voice lab and the LAB menu |
| RG-02 | Silence and reset | Silence in gives silence out (below −90 dBFS) once tails have decayed; after PANIC, silence in gives silence out |
| RG-03 | Tuning | An impulse rings at the PITCH note within 10 cents, modal model, at 48 kHz, for C3, C4 and C5 |
| RG-04 | Safety | Finite and under the engine's ceiling with hostile input and every control at its extremes; no allocation while processing; deterministic |
| RG-05 | Latency | Exactly 24 samples, for any host block size; output is identical for block sizes 1, 64 and 1000 |
| RG-06 | Responds | Each of STRUCTURE, BRIGHTNESS, DAMPING, POSITION changes the output; a ROOM cable changes it, and at COUPLE 0 does not |
| RG-07 | Click-free switching | Switching between the resonator and each Rings model while playing adds no step above 0.03 |
| RG-08 | Upstream unmodified | The upstream files compiled are byte-identical to the pinned commit (checked with `git status` in the checkout) |

Measured, not pass/fail: cost per sample for each model against the current resonator; memory; level; WORLD
on/off difference; CoreAudio callback timing with a Rings voice selected.

Renders for listening: `wildband_voice_lab`, level-matched, three models × six fixtures × WORLD on/off.

Not automatable (owner): which model, if any, is more expressive than the current resonator on speech, music,
transients and static; whether it should replace or complement it; which controls matter.
