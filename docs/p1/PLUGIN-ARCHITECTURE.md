# WILDBAND — Shared AU / VST3 plugin architecture: plan and first proof

**Branch:** `p1/plugin-proof`, on `p1/rings`. **Status:** the plan and the proof's acceptance criteria were committed
before the plugin target existed; results are appended at the bottom. Roadmap: P0 "AUv2/VST3 proof" and "VST3
validation". The contest needs a signed, notarized AU, VST3 or CLAP; a standalone alone does not qualify.

**A plugin that compiles is not a qualified plugin.** This document separates what was built, what a validator
checked, and what only a real host and the owner can establish.

## Where the standalone is today

Everything is in `src/Main.cpp`: an `Instrument` (engine, radio, MIDI maps, timing counters; the audio and MIDI
callbacks) and a `MainComponent` that is both the editor and the application shell — it owns the
`AudioDeviceManager`, the settings file, the ROOM helper, the tuner timer and the LAB menu.

## Target shape (one DSP, one UI, three shells)

```
src/Engine.h, Patch.h, Dissolve.h, RingsVoice.h      DSP, no JUCE                      (as now)
src/Radio.*                                           receiver, juce_core               (as now)
src/Instrument.h        NEW   everything audio/MIDI threads touch; no device, no window
src/Services.h          LATER what the shell provides: settings store, ROOM sensing, "which station" requests
src/Editor.*            LATER MainComponent minus device ownership; talks to Instrument + Services only
src/Main.cpp                  standalone shell: AudioDeviceManager, settings file, ROOM helper, test flags
src/Plugin.cpp          NEW   plugin shell: juce::AudioProcessor around Instrument; host owns device, state, MIDI
```

| Concern | Standalone | Plugin (AU / VST3) |
|---|---|---|
| Audio device, sample rate, block size | App's `AudioDeviceManager` | Host: `prepareToPlay` / `processBlock`. Rate changes restart the receiver exactly as now |
| Bus layout | stereo out | **Instrument/generator: no audio input**, stereo out; MIDI in. AU type `aumu` so Logic lists it as an instrument and sends notes |
| MIDI | Opens every input itself | Host delivers MIDI in `processBlock`; MIDI learn works on that stream. Host automation is separate (parameters) |
| Parameters | atomics + settings file | The same atomics, mirrored by host parameters with stable IDs and a fixed order (AU automation depends on order: new parameters only at the end) |
| State | `WILDBAND-<profile>.settings` | Host project/preset via `getStateInformation`; **never any audio**; LOOP restores EMPTY |
| Station requests | Timer on the message thread | Same timer in the processor (never `tune()` from the audio thread: it takes a lock) |
| Network | App's Info.plist allows plain HTTP | **Risk:** inside a host, App Transport Security is the *host's*. `http://` stations may be refused there while `https://` works. To be measured in Logic and Live; if confirmed, plain-HTTP streams need a socket path that is not `NSURLSession` |
| ROOM | Sibling helper app launched with `open` | Same idea, but the helper must be found relative to the plugin bundle and may not be launchable from a sandboxed host. **No microphone request at instantiation or restore, ever**; ROOM off by default in every project. AUv3 cannot spawn a sibling at all (separate spike) |
| Instances | one | Several: each has its own engine, receiver thread and 6 s ring; nothing static or shared. Memory and connection count scale with instances |
| Offline bounce | n/a | A live broadcast cannot be reproduced; offline render processes whatever arrives in wall-clock time. Define and document; do not pretend |
| Test flags, LAB | command line, menu | LAB as hidden state only; no test flags |

## Proof scope (this branch)

Smallest thing that shows the path is real:

1. `src/Instrument.h` extracted from `Main.cpp`, unchanged in behaviour; the standalone uses it.
2. `src/Plugin.cpp`: an `AudioProcessor` around `Instrument` — instrument bus layout, ten host parameters mirroring
   the knobs, host MIDI in, station follows TUNE, state = the parameters. **No custom editor yet** (the host shows
   its generic one). No ROOM, no patch-bay state, no LAB.
3. One CMake `juce_add_plugin` target producing **AU and VST3** from that file.

Out of scope here: the shared editor, ROOM in a plugin, signing and notarization, AUv3, CLAP, pluginval, Windows.

## Acceptance criteria (registered before implementation)

| ID | Test | Passes when |
|---|---|---|
| PA-01 | Standalone unchanged | After the extraction, `wildband_tests`, `wildband_tuner_check` and the standalone build all pass as before |
| PA-02 | Builds | AU (`.component`) and VST3 bundles are produced by the same target from the shared sources; ad-hoc signatures verify |
| PA-03 | AU validation | `auval -v aumu <code> <manufacturer>` reports **PASS** |
| PA-04 | Instrument contract | Reported as an instrument with no audio input, stereo output, MIDI input; ten parameters with stable IDs |
| PA-05 | No surprise permissions | Instantiating and validating the plugin raises **no microphone prompt** (the plugin binary declares no microphone use and contains no capture code path) |
| PA-06 | Engine in a plugin shell | A headless check instantiates the processor, prepares it at 44.1/48/96 kHz, renders with parameter changes and MIDI, saves and restores state: output finite and bounded, state round-trips, no audio in the state |
| PA-07 | Notices | The third-party notices list what the plugin targets add (VST3 SDK, AudioUnit SDK) |

Explicitly **not** established by this proof, whatever the result: behaviour in Logic Pro or Live (instantiation,
streaming under the host's network policy, automation, bypass, multiple instances, offline bounce, project
save/reload), VST3 conformance (no validator is installed here), signing, notarization, clean-machine install.
