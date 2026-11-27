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

---

## Results (added after implementation, 2026-11-27)

**What was built and what a validator or the headless check established. Nothing was tried in a DAW.**

| ID | Result | Notes |
|---|---|---|
| PA-01 | **Pass** | `src/Instrument.h` extracted from `Main.cpp` with two host helpers added (`prepareForHost`, `renderForHost`); the standalone builds, `wildband_tests` 884 checks and `wildband_tuner_check` pass as before |
| PA-02 | **Pass** | One `juce_add_plugin` target produces `WILDBAND.component` (AU v2) and `WILDBAND.vst3` from `src/Plugin.cpp` + the shared sources; ad-hoc signatures verify. **Found on the way:** my post-build copy of the licence files failed on the AU (no `Resources` folder yet) and made the build delete the binary; fixed |
| PA-03 | **Pass** | `auval -v aumu Wldb SwLb`: **AU VALIDATION SUCCEEDED** ([`evidence/auval.txt`](evidence/auval.txt)). Installed for this user only (`~/Library/Audio/Plug-Ins/Components`) to run it |
| PA-04 | **Pass** | Both formats: 0 audio inputs, 2 outputs, accepts MIDI, described as an instrument, the ten parameters. VST3 also has a bypass parameter and 2 080 hidden parameters that JUCE uses to carry MIDI CCs (VST3 has no CC events) — that is what lets MIDI learn work in a VST3 host |
| PA-05 | **Pass** | The plugin bundle declares no microphone use, its binary references no capture API (`AVCaptureDevice`, `requestAccessForMediaType`: 0 hits), and no system prompt appeared while validating and loading it |
| PA-06 | **Pass** | `wildband_plugin_check` loads the built bundles through JUCE's hosting code ([`evidence/plugin-check.txt`](evidence/plugin-check.txt), 24 checks). Six seconds in real time: sound while the station connects (static) and from the broadcast, under the ceiling. Faster than real time at 44.1/48/96 kHz with automation and MIDI: finite and bounded (mostly silent — no audio can arrive faster than real time; see "Offline bounce"). State round-trips into a second instance; about 500 bytes as stored by each host format |
| PA-07 | **Pass** | VST3 SDK (MIT) and AudioUnitSDK (Apache-2.0) added to the notices and `licenses/`; the bundles carry the licence files |

**Two real defects found by the headless check and fixed:**

1. **Saved state could miss the latest change.** The plugin wrote its state from the engine's values, which are only
   updated in `processBlock`. A host that saves after automating without having processed a block in between
   (stopped transport, suspended plugin) would have saved stale values. State is now written from the host
   parameters, and knob moves made through MIDI learn are mirrored into the host parameters on the message thread
   (unless the host moved that parameter itself in the meantime).
2. **AU parameter order is not declaration order.** JUCE's AU wrapper sorts parameters by hashed ID, and Logic
   addresses AU parameters by that index. The hosted order —
   `MIX, TONE, TUNE, DRIFT, LEVEL, PITCH, SHAPE, COUPLE, STATIC, FEEDBACK` — is now frozen by the check; any
   parameter added later must carry version hint 2 or higher so that it lands after these.

### Still open, in the order they matter for the contest

1. **Logic Pro and Live**: instantiate, play, automate, save and reload a project, bypass, two instances, offline
   bounce. Owner or a scripted session; nothing here says it works.
2. **Plain-`http://` stations inside a host** (the host's App Transport Security applies, not ours). Untested.
3. **A shared editor**: the plugin has no UI of its own yet (hosts show a generic slider panel). The next step is to
   lift `MainComponent` minus device ownership into `src/Editor.*`.
4. **ROOM in a plugin**: not attempted; the helper must be found from the plugin bundle, and sandboxed hosts may
   refuse to launch it.
5. **VST3 conformance**: no validator is installed (`pluginval` and Steinberg's validator absent).
6. **Signing, notarization, installer**: not started; needs the owner's Developer ID.
7. **Offline bounce of a live broadcast** cannot reproduce anything; it renders whatever has arrived. To be
   documented in the field guide and the host behaviour defined (most likely: plays the ring's contents, then
   static/silence).
8. AUv3: separate spike, not started.
9. **Network at instantiation.** The plugin starts fetching the station about 250 ms after it is created, so `auval`,
   a host's plug-in scan and every new instance open a connection to the default station. Better to wait for
   `prepareToPlay` (a scan never calls it); not changed yet.
10. **Shared random state in Rings' string model: a data race.** `stmlib::Random` keeps one *static* state for the
    whole process (`rng_state_`), read and written without synchronisation by Rings' string model. Two instances in
    one host run that code on different threads at once: that is a data race and undefined behaviour, and it
    contradicts "nothing is shared" above. Recorded as a **mandatory production fix if Rings is selected**
    (`RINGS-EXPERIMENT.md`). Relevant only to builds with Rings.

The final build of this branch (`910199d`, after the review fixes) was launched: muted live navigation behaved as
before and left the shared settings untouched; the AU was reinstalled from it, `auval` passes again, and the
headless check passes again.
