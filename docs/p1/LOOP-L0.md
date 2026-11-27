# WILDBAND P1-L — LOOP from Tapestry: gate L0 (source, provenance, budget, tests)

**Branch:** `p1/loop-l0`. **Status:** research gate only. **No Tapestry code has been copied, ported or run.** This
document does what `docs/TAPESTRY-LOOPER-RESEARCH.md` asks of gate L0: pin the source, review the licence, fix the
budget, and register tests before a port. Statements about the source were read from the files at the pinned
revision.

## Pinned source

| | |
|---|---|
| Repository | `https://github.com/shortwavlabs/vcv-tapestry` (public) |
| Revision | `48517c8` (`main`) |
| Licence | `LICENSE.md` is the GPL v3 text; `plugin.json`, README and the contributing notes say **GPL-3.0-or-later**. No per-file headers |
| Files in scope | `src/dsp/tapestry-core.h` (440 lines), `tapestry-buffer.h` (285), `tapestry-splice.h` (468), `tapestry-grain.h` (442), `tapestry-dsp.h` (815). They include only each other and the standard library: **no Rack SDK types** |
| Authorship | Committed only by the owner, no co-author trailers (checked per file). **The owner's explicit confirmation for these five files has not been recorded yet**; it was given for Korupt, Wyrd and Drift |
| Out of scope | `Tapestry.cpp/.hpp`, `TapestryExpander.*` (Rack wrapper, WAV and reel file I/O, 32 reel slots; some commits by an AI agent), `tapestry-effects.h` (one AI co-author trailer; not needed) |
| How it would enter WILDBAND | As GPL-3.0-or-later files with a header saying so and naming their origin, in their own directory (`src/loop/`), combined with the AGPL-3.0 program under GPLv3 §13. A row in the reuse disclosure and the notices in the same commit |

## What the source does that WILDBAND cannot keep

| In Tapestry (file) | Why it cannot be used as is | Planned change |
|---|---|---|
| `kMaxReelFrames = 8 352 000` (about 174 s at 48 kHz); the buffer constructor does `data_.resize(kMaxFrames * kChannels)` — 66.8 MB of floats (`tapestry-core.h`, `tapestry-buffer.h`) | A multi-minute reel is a recorder. WILDBAND's LOOP is a few seconds | Capacity fixed at prepare time: **8 s × sample rate × 2 channels** |
| `clear()` fills the whole allocation with `std::fill`; `clearRange` likewise (`tapestry-buffer.h`) | A full-capacity clear on the audio thread is unbounded work | **Logical clear**: a valid-length of zero hides old material at once; the memory is wiped off the audio thread or in bounded pieces (as Dissolve's `forgetSome` does) |
| Splices in a `std::vector` with `push_back`, `insert`, `erase`, `clear`, and a sorted temporary vector (`tapestry-splice.h`) | Allocation and unbounded moves on the audio thread | Fixed-capacity marker array (proposal: 16 markers), bounded inserts and deletes, no heap |
| `kInternalSampleRate = 48000` and rate-dependent mappings (`tapestry-core.h`) | The device rate varies | Rates and time constants derived from the prepared rate; a loop captured at one rate is cleared, with a visible status, if the rate changes |
| Parameters in Rack voltages (`kSosCvMax = 8`, `kVariSpeedCvMax = 4`, `kGateTriggerThreshold = 2.5` …) | WILDBAND controls are 0..1 and its cables carry signed depth | Normalised parameters; WILDBAND's own smoothing; event inputs separate from continuous ones |
| Four grain voices, clock sync, overdub, sound-on-sound | More than the first audition needs | Start with clean loop + speed/reverse + slices; grains second; overdub and sync only if asked for |
| File import/export and reel slots (wrapper) | Forbidden by the product: no saved audio | Not ported. Nothing in LOOP serialises audio; saved state is settings only and restores EMPTY |

## Budget (proposal; the owner approves the duration after listening)

| Device rate | 8 s stereo float | Notes |
|---|---|---|
| 44.1 kHz | 2.8 MB | |
| 48 kHz | 3.1 MB | |
| 96 kHz | 6.1 MB | Worst case per instance; allocate in `prepare`, never in the callback |

Work per callback is bounded by: one stereo write while capturing, at most N interpolated reads (N = 1 for the plain
loop, up to 4 with grains), one seam cross-fade. Markers: 16. No waveform is computed on the audio thread; the UI
gets a small decimated overview through a bounded snapshot.

## Lifecycle

`EMPTY → CAPTURING → PLAYING ⇄ STOPPED`; CLEAR and PANIC from any state. Capture stops by itself at the cap. A new
capture replaces the old one. Retuning keeps a finished loop and, in the first prototype, **stops a capture in
progress** (so a loop never silently straddles two stations). Nothing is captured without the CAPTURE gesture: not
at start-up, not on restore, not from the microphone. On unload the memory is discarded. Synthesized tuner static is
captured only if it was what was playing when CAPTURE was pressed, and the status says so.

## Tests to register in the port's first commit

| ID | Gate | Test |
|---|---|---|
| LP-01 | L1 | Capture start and stop are sample-accurate: the loop's first and last frames are exactly the input frames at those instants, both channels |
| LP-02 | L1 | Auto-stop exactly at the 8 s cap; a capture of 0 or 1 frame is rejected or handled without reading invalid memory |
| LP-03 | L1 | Replace: after a second capture nothing from the first can be read; CLEAR and PANIC: silence from the next sample (after a bounded fade), and no old frame is ever returned afterwards |
| LP-04 | L1 | No hidden capture: with CAPTURE never pressed the buffer is never written (checked by a canary pattern) |
| LP-05 | L1 | The live radio path is sample-identical with LOOP present but EMPTY (the instrument is unchanged), and continues unchanged while a loop plays |
| LP-06 | L1 | Rate change: the loop is cleared with a status; no sample is reinterpreted at the wrong rate |
| LP-07 | L2 | Forward loop is seamless: no step above a threshold at the seam for sine, noise and DC-offset fixtures |
| LP-08 | L2 | Speed and reverse: a 1 kHz loop plays at the expected frequency at ×0.5, ×1, ×2, ×−1 (within a cent tolerance to be set) |
| LP-09 | L2 | Slices: 4 / 8 / 16 equal slices have exact bounds; NEXT wraps; marker insert/delete at the edges, duplicates and over capacity are safe |
| LP-10 | L2 | Scan and grain overlap stay inside the captured region for every parameter extreme; output finite and bounded |
| LP-11 | L1–L2 | No allocation on the audio thread in any state transition; deterministic; block-size independent |
| LP-12 | L3 | Each continuous control accepts signed depth; an unavailable WORLD source is neutral; events are edge-triggered and rate-limited |
| LP-13 | L4 | Saved state contains no audio (byte search for a known captured pattern in the settings and in plugin state) |
| LP-14 | L4 | CoreAudio sweep with radio + selected voice + LOOP at the grain maximum, with the late-callback positive control |

Reference fixtures are synthesized (the voice lab's): no recordings, no broadcast audio.

## Relationship to Dissolve

Dissolve (already in LAB) is a continuously overwritten two-second history with no capture gesture. LOOP is an
explicitly caught fragment that stays until replaced. They share a need for interpolated reads, a window, and
bounded erasing; the port should reuse Dissolve's reader and `forgetSome` pattern where they fit rather than carry a
second copy. Whether the instrument needs both is a listening decision.

## What is blocking, and what is not

- **Blocking the port:** the owner's confirmation of authorship for the five files (one question).
- **Not blocking:** everything above was done from reading. The first prototype's scope (plain loop, speed/reverse,
  slices) is small.
- **Owner decisions later:** 4 / 8 / 12 s cap; whether retune stops a capture; which controls LOOP gets.
