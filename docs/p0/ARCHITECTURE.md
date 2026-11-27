# WILDBAND P0 — Implementation architecture (G0)

**Status:** prototype built to gather evidence. Nothing here is an accepted architecture; RFC-001 keeps its
"PROPOSAL / RESEARCH" status and the P0 research plan is not edited by this work.

## Musical promise (proposed, awaiting owner acceptance)

> A live broadcast strikes a small set of tuned resonators; your hands, the broadcast's own dynamics and the room
> decide how they ring.

Non-goals for P0: recording or export of any kind, station directories, plugin formats, Windows, any sensor other
than the microphone, the final look.

## What was built

```
            receiver thread                                   audio callback (CoreAudio)
URL -> HTTP(S) fetch -> AudioToolbox parse/decode/resample -> bounded ring -> Radio::read (fades, never blocks) -+
                                                                                                               |
WILDBAND Room helper app (started by ROOM; mic -> RoomAnalyser) -> loopback UDP -> EnvBus (level, hits, bright) -+--> Engine -> output
local test source (deterministic, labelled, never called "live") ----------------------------------------------+      ^
                                                                                                                       |
knobs / MIDI CC / MIDI notes -> atomics ------------------------------------------------------------------------------+
patch bay -> Graph (validated on the message thread) -> RoutingPlan in one atomic word -------------------------------+
```

| File | Role | Depends on |
|---|---|---|
| `src/Engine.h` | DSP engine, room analyser, environment bus, test source | C++ standard library only |
| `src/Patch.h` | Typed ports, validated `Graph`, `RoutingPlan` (TALORA-style adapter) | C++ standard library only |
| `src/Radio.{h,cpp}` | Stream receiver | `juce_core`, AudioToolbox |
| `src/RoomHelper.mm` | **WILDBAND Room**, a separate app: the only process that uses the microphone. Asks for consent, analyses the chosen input, sends three numbers to the instrument | CoreAudio, AVFoundation; no JUCE |
| `src/AudioIO.{h,cpp}` | Small CoreAudio layer: input capture for the helper, and the `--probe` diagnostic | CoreAudio, AudioToolbox |
| `src/MicPermission.{h,mm}` | macOS microphone consent (requested by the helper only) | AVFoundation |
| `src/Main.cpp` | App: audio/MIDI callbacks, placeholder UI, patch bay, settings, timing sweep | `juce_audio_utils` |
| `tests/Tests.cpp` | Deterministic engine and patch tests | nothing (plain `clang++`) |
| `tools/RadioProbe.cpp`, `tools/stream_test_server.py` | G1 evidence tools | `juce_core` / Python 3 |

The engine and the patch layer have no JUCE in them, which is the retained path to AU/VST3: a plugin would wrap
`Engine` in an `AudioProcessor`. The radio receiver inside a plugin is an open question (hosts, sandboxing), not a
solved one.

## Technology decisions

| Decision | Chosen | Why | Not chosen |
|---|---|---|---|
| Framework | JUCE 9.0.5, pinned by commit `d60b7a78` (same pin as AFTERIMAGE), fetched by CMake | Checked: builds and runs on macOS 26.6 / Xcode 27.2 / Apple Silicon. The local `/Applications/JUCE` is 9.0.3, so the pin is what makes the build reproducible | Unpinned system JUCE |
| App shape | `juce_add_gui_app` standalone | The brief asks for a standalone instrument; no plugin wrapper to fight | `juce_add_plugin` + Standalone |
| Stream decoding | macOS AudioToolbox (`AudioFileStream` + `AudioConverter`) | Ships with the OS: no codec library to license, vendor or build; the converter also resamples to the device rate | ffmpeg/mpg123/libopus (present on this Mac via Homebrew, but they would add dependencies and licence terms); JUCE's MP3 reader (file-oriented, off by default for licensing reasons) |
| Networking | `juce::WebInputStream` (NSURLSession underneath) | HTTPS, redirects and timeouts for free | libcurl |
| Mic consent | AVFoundation directly, in the helper | `juce::RuntimePermissions` does nothing on macOS | — |
| Where the microphone lives | A separate helper app beside the instrument | An app that declares microphone use is prompted by macOS as soon as it opens an output device that has inputs. The instrument therefore declares none (`EVIDENCE-LOG.md`) | In-process capture through JUCE's device manager (the first attempt) |
| Parameters | Plain atomics | There is no host; APVTS would be ceremony | APVTS |
| Patch hand-off | One `std::atomic<uint64_t>` | The whole plan is seven bytes | Lock, FIFO, triple buffer |

No cloud service, account, hosted backend or API key is involved anywhere.

## The engine

Six delay-line resonators. Each loop is: fractional delay (4-point Hermite) → one-pole low-pass (frequency-dependent
damping) → 5 Hz high-pass (stops a DC latch above unity gain) → `tanh` saturator → back into the line together with
the excitation. The six loops share energy through the matrix `I − k·(2/N)·ones`, whose eigenvalues are 1 and
`1 − 2k`, so for `k ≤ 0.45` the coupling can never add energy. Every state is therefore bounded by construction;
the output then passes a fixed soft clip at 0.89 (about −1 dBFS).

| Control | What it does | Range notes |
|---|---|---|
| TUNE | A dial over six station slots. The middle 40 % of each slot is clean reception; toward the edges the radio is ring-modulated (20–400 Hz) and attenuated. Resting in a new slot for 250 ms fetches that station | It is a dial position, not an RF frequency, and the UI says so |
| PITCH | Root note of the resonators, C2–C6. MIDI notes set it | Tuned to within 10 cents (tested) when DRIFT is 0 |
| SHAPE | Morphs the six ratios: near-unison → fifths/octaves → harmonic series → inharmonic bar | Continuous |
| TONE | Damping cutoff, 200 Hz – 16 kHz | |
| COUPLE | Depth of every control cable in the patch at once | At 0 no cable has any effect (tested) |
| DRIFT | Slow pitch wander of each resonator (six phases, each bending its neighbour's) and how much the resonators bleed into each other | Deterministic: no random numbers anywhere in the engine |
| FEEDBACK | Loop gain. Unity is reached near 0.7; above that the resonators sustain themselves, held by the saturators | Tested at the stop with hostile input |
| MIX, LEVEL | Source/resonator balance, output level | |

Control cables (see `TALORA-REUSE.md` for the patch layer) carry five 0..1 signals — radio LEVEL and HITS, room
LEVEL, HITS and BRIGHT — into six resonator inputs: PITCH (up to +1 semitone), SHAPE, TONE (up to +3 octaves of
cutoff), STRIKE (excitation follows the signal instead of being constant), DRIFT (speed), FEEDBACK (up to +0.2 loop
gain). Unpredictability comes only from these interactions: the broadcast, the room and the player's hands.

Comparison modes: **DRY** (the source alone) and **REFERENCE** (an ordinary resonant filter into a feedback delay,
on the same PITCH/SHAPE/TONE/DRIFT/FEEDBACK knobs). Mode changes cross-fade.

## Real-time rules and how each is met

| Rule | How |
|---|---|
| No network or decoding on the audio thread | Both live on `Radio`'s own thread. `Radio::read` only copies from the ring |
| No allocation or locks in the callback | Engine state is sized in `prepare`. Test `no-allocation` counts mallocs during `process` (0) with a positive control. `Radio::read` takes no lock |
| Bounded queues | PCM ring: at most 6 s; the receiver thread waits when it is full (TCP back-pressure) |
| Smoothed modulation | Every parameter, control cable and mode weight is one-pole smoothed; delay lengths glide |
| Underrun / disconnect without clicks | The reader fades out 20 ms before the ring runs dry and fades in after 0.75 s is buffered again |
| Retune responds at once | The old station starts fading on the `tune()` call; the resonators ring on while the new one connects |
| Sample-rate change | The receiver restarts its session at the new rate; the callback is not involved |
| NaN / runaway | Inputs sanitised, loops saturate, output clipped; a non-finite output block resets the engine and is counted (`faults`, shown in the status line; never seen) |
| Mic denied / absent | ROOM switches itself off and stays neutral (0); playback is a different process and is unaffected. The environment bus also reads neutral when a source goes stale for 1.5 s |

## Environment abstraction

`EnvBus` has one slot per source (`microphone`, `weather`, `bluetooth_rssi`, `wifi`, `external_rf`). Each source
publishes the same three bounded values with a timestamp; a stale slot reads as neutral. **Only the microphone slot
is implemented.** The other names are placeholders, not features. Wi-Fi and Bluetooth metadata, if ever added, are
observations about nearby devices, not measurements of electromagnetic fields.

## AFTERIMAGE: what was considered

Reviewed `shortwavlabs/after-image` at `c38b037` (AGPL-3.0-only, same owner).

| Area | Finding | Decision |
|---|---|---|
| Real-time architecture (engine owned by the callback, command FIFO, status atomics) | Sound, but built around capture commands and voices | Same discipline, none of the code |
| FFT/STFT utilities, spectral analysis | Tied to `juce::dsp::FFT` and the ghost/cloud reconstruction | **Rejected**: this engine is time-domain. Room "brightness" is a one-pole band split, not an FFT |
| Audio history / granular capture | The opposite of an ephemeral instrument: it exists to keep audio | **Rejected** |
| Parameter smoothing (`Ramp`) | Linear ramps to a target | Not reused; one-pole smoothing fits continuously modulated controls better |
| Modulation building blocks | None generic | — |
| Test harness (`CHECK` macro, no framework, exit code = failures; `malloc_logger` allocation gate) | Directly useful | **Pattern reused** (re-typed, ~15 lines) |
| CoreAudio measurement lesson (P1-B E3: JUCE's xrun counter never moved on this macOS) | Important | **Reused as a method**: WILDBAND counts host-time gaps and runs a positive control |
| Build (pinned JUCE, ad-hoc signing after build) | Useful | **Pattern reused** in `CMakeLists.txt` |

No AFTERIMAGE source file was copied.

## Licensing (open — owner decision)

The repository README says no licensing decision has been made, and this work does not make one: no `LICENSE` file
was added. Facts the decision has to account for:

- JUCE is AGPLv3 or commercial. Distributing a WILDBAND binary without a JUCE commercial licence means the whole
  program is AGPLv3. AFTERIMAGE already chose AGPL-3.0-only; WILDBAND has not chosen.
- JUCE vendors third-party code (zlib, HarfBuzz, libpng, …). A third-party notice derived from JUCE's SPDX file is
  needed before any distribution; none has been written.
- MP3/AAC decoding uses Apple's system decoders, so no codec library is distributed.
- TALORA (`modulo`) is private and has no licence file. None of its files is in this repository, but `src/Patch.h`
  is re-typed from its routing layer (see `TALORA-REUSE.md`), which the eventual licence has to cover.
- Station streams are third parties' broadcasts. Only their public addresses are in the source, as editable
  defaults. Whether they may appear in demos or marketing has not been investigated.

## Prior art

A light check only, not a survey: KOMA Field Kit (hardware, has an AM/FM/SW receiver as a signal source) and Music
Thing Modular's Radio Music (a sample player imitating a radio) are the closest references found. Software that
runs live internet radio through effects certainly exists. **No novelty claim is made**; the question P0 asks is
whether this particular instrument is good to play.

## Unresolved risks

1. Nobody has listened to it. Everything about the sound is unproven until the owner plays it.
2. The microphone-prompt defect is fixed by moving capture into a helper app; that adds a second bundle, a
   loopback UDP link (about 35 ms of extra latency on room values) and a re-prompt after each unsigned rebuild.
3. Microphone plus speakers is a real acoustic loop through the control path, even with no mic audio in the output.
4. The station and the Mac run on different clocks; nothing corrects the drift (see `RADIO-COMPATIBILITY.md`).
5. Retuning takes 0.2–2 s of network time. The instrument covers it musically (fade, ring-out); whether that feels
   like tuning is a listening question.
6. Stream reception inside a plugin is unexplored.
