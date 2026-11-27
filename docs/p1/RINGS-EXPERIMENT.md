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

---

## Results (added after implementation, 2026-11-27)

**Technical evidence only; every run was muted. Nobody has listened to Rings in WILDBAND.**

`wildband_tests`: **884 checks, 0 failed** with Rings configured (831 without, unchanged).

| ID | Result | Notes |
|---|---|---|
| RG-01 | **Pass** | Default build: no Rings, 831 checks. With `-DEURORACK_DIR`: three Rings voices in the voice lab and in LAB |
| RG-02 | **Pass** | All three models: below −90 dBFS twelve seconds after the input stops; exact silence after PANIC |
| RG-03 | **Pass** | Modal model within 10 cents of the PITCH note at C3, C4 and C5 (48 kHz) |
| RG-04 | **Pass** | 18 hostile runs across the three models at 44.1 and 48 kHz; deterministic; 0 allocations while processing, including model switches and PANIC |
| RG-05 | **Pass** | Identical output for block sizes 1, 64 and 1000; an impulse first appears 24–26 samples later |
| RG-06 | **Pass** | STRUCTURE, BRIGHTNESS, DAMPING and POSITION each change the output in every model; a ROOM LEVEL → TONE cable changes it with COUPLE up and not at all with COUPLE at 0 |
| RG-07 | **Pass** | Switching among the resonator, Dissolve and the three Rings models every 50 ms: no step |
| RG-08 | **Pass** | `git status` clean in the upstream checkout and in its `stmlib` submodule: compiled unmodified |

The adapter (`src/RingsVoice.h`, 110 lines, WILDBAND's own) worked against upstream's public interface at the first
attempt; eight upstream source files are compiled.

### Measured

| | Current resonator | Rings modal | Rings sympathetic | Rings string |
|---|---|---|---|---|
| Whole engine, synthetic, 48 kHz, ns per sample (voice lab, per fixture) | 49–65 | 103–117 | 114–129 | 80–96 |
| State | about 0.1 MB | 172 kB (shared by the three models) | | |
| Added latency | 0 | 24 samples (0.5 ms at 48 kHz) | | |
| CoreAudio, MiniFuse 1, radio live in every window | worst 4.0 % of the buffer period | — | **worst 6.4 %** (96 kHz / 64), 0 late callbacks ([`evidence/coreaudio-rings-sympathetic.md`](evidence/coreaudio-rings-sympathetic.md)) | — |

Only the costliest model (sympathetic strings) was swept on CoreAudio. Roughly: Rings costs about twice the
current resonator and is still far inside the budget.

### Mandatory production fixes if Rings is selected

Rings is **not musically accepted**; it is an experiment awaiting the owner's audition. If it is chosen, these two
are release blockers, not polish:

1. **Sample-rate adaptation.** Upstream runs as if at 48 kHz. At any other device rate the adapter only corrects the
   pitch; filter coefficients, decay times and the reverb all scale with the rate. A production voice must run
   Rings at its native 48 kHz behind a qualified, bounded-latency resampler (or upstream must be made
   rate-independent and requalified against the pinned reference), with tests at 44.1, 48, 88.2 and 96 kHz.
2. **Shared random-number state — a data race.** `stmlib::Random` keeps a single *static* state per process
   (`Random::rng_state_`), read and written without synchronisation by Rings' string model (and its plucker). Two
   WILDBAND instances in one host, or any two threads running that code, access it concurrently: **that is a data
   race and undefined behaviour in C++**, not merely correlated noise between instances. A production build must
   give each voice its own generator (for example by compiling a WILDBAND replacement for `random.cc`/`random.h`
   with per-instance state, recorded as a modification of upstream with its MIT notice), and prove it with a
   thread-sanitiser run of two instances.

### Honest limits

- **Fixed 48 kHz inside.** At 44.1 or 96 kHz the pitch is corrected but timbre and decay times scale with the
  device rate (at 96 kHz everything rings half as long and an octave "brighter" in filter terms). Do not judge Rings
  at 96 kHz from this build. A resampler is the proper fix if Rings is chosen.
- External excitation only. The strummed / internally excited modes, polyphony above 1, the chord table and the
  "easter egg" string synth were not connected.
- Level: Rings' outputs are scaled by a fixed 0.5 here; before matching, its levels sit within a few dB of the
  current resonator's on most fixtures, 10 dB apart on some. Listen to the level-matched renders, not to raw LAB
  loudness, when comparing.
- Upstream's own reverb and limiter are left as they are; they are part of what is heard.
- Rings is fetched from outside the repository; a build without it has no Rings voices.
- `stmlib::Random` keeps one static state per process, used without synchronisation by the string model: with two
  instances this is a data race (see "Mandatory production fixes" above).

### How to audition

Live: **LAB → Voice experiment → Rings: modal / sympathetic strings / inharmonic string**, at 48 kHz. Same knobs:
PITCH = note, SHAPE = structure, TONE = brightness, FEEDBACK = damping, DRIFT = position.
Offline: `build/voice-lab/*__rings-*.wav` beside `*__resonator*.wav`, level-matched, WORLD on and off.
