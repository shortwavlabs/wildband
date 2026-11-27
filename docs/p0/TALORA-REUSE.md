# WILDBAND P0 — TALORA patching reuse assessment

**Inspected:** `shortwavlabs/modulo` (the repository that contains TALORA), branch `development` at `91cc748`
("Phase P2-beta"), on this Mac. Files read: `src/engine/Routing.{h,cpp}`, `src/control/PatchController.{h,cpp}`,
the plan hand-off in `src/engine/ModuloEngine.{h,cpp}`, the two editors' headers, `README.md`,
`docs/02-rfc-decisions.md`, and the test list.

## What TALORA has

| Component | Where | Dependencies | Portable as it stands? |
|---|---|---|---|
| Typed ports (`PortType` Audio/CV/Gate/Trigger, direction, capability, channel count, normals, stable `module.port` ids) | `Routing.h` | `<bitset>`, `<cstdint>` | The types, yes. The registries (`moduloPorts`, `taloraRackPorts`, the port-id enums) are TALORA's own modules |
| `Graph`: fixed-capacity value type, `check/connect/disconnect`, one cable per input, unlimited fan-out, type check, cycle check, normalling | `Routing.{h,cpp}` | none beyond the above | **Yes** — it takes any `PortRegistry`. Confirmed by building it against a WILDBAND registry (below) |
| `RoutingPlan`: what the audio thread runs | `Routing.h` | — | **No.** Its fields are TALORA's inputs (`timbreSource`, `ringsInSource`, `elements…`) and module order |
| Wait-free plan hand-off (three slots, one atomic exchange) | `ModuloEngine.{h,cpp}` | inside `ModuloEngine` | The idea, yes; the code is part of the engine class |
| `PatchController`: revisioned commands, atomic multi-edit transactions, undo/redo, snapshots | `PatchController.{h,cpp}` | `ModuloEngine`, `std::mutex`, `std::deque`, `std::string` | Not without `ModuloEngine`; it would need an engine interface extracted first |
| UI → engine protocol (JSON patch commands) | `src/plugin/PatchJson.*`, `PluginEditor3D.*` | JUCE WebView | Tied to the WebView editor |
| Cable UI | `ui/prototype-3d` (Three.js in a JUCE WebView); the native editor only has buttons | Node toolchain, WebView, GLB assets | **No** native cable component exists to reuse |
| Tests | `tests/patch_protocol_tests.cpp`, `graph_capacity_tests.cpp`, `p1_tests.cpp` (1 844 lines) | TALORA engine | They test TALORA's registry |

## Decision: a narrow, compatible adapter — not a file copy, not yet a shared library

`src/Patch.h` (about 150 lines) is **re-typed from TALORA**: it keeps TALORA's names and rules, and its
`Graph::check` follows TALORA's rule for rule. It is derived work, not independent work. It covers: `PortType`,
`PortDirection`, `PortSpec`, `ConnectError`, `Connection`, `Graph::check/connect/disconnect/cableInto`, one cable
per input, unlimited fan-out, a plain-data `RoutingPlan` built off the audio thread, and the `from|to` cable id
format for saved patches.

Why not copy `Routing.{h,cpp}` into this repository:

- **`modulo` is private and has no licence file; `wildband` is public.** Copying would publish private source, and
  under whichever licence WILDBAND ends up with. That is the owner's call, not something to do in passing.
  The same question applies, on a smaller scale, to what *is* here: the re-typed validation rules in `src/Patch.h`,
  `tests/TaloraParity.cpp` (which encodes TALORA's `PortSpec` layout), and this document's description of the
  private repository's structure. **The owner was asked before any of it was pushed to the public repository.**
- `Routing.cpp` carries TALORA's whole rack registry; WILDBAND would compile a port table it never uses.

Why not extract a shared library now: it means changing `modulo` (splitting the registries and the plan out of
`Routing`, giving `PatchController` an engine interface), re-running TALORA's qualification, and choosing a home and
licence for the library. That is a project of its own and would have displaced the proof of concept.

**Tradeoff accepted:** the validation rules now exist twice. To keep them honest there is a parity test.

### Parity evidence

`tests/TaloraParity.cpp` builds TALORA's real `Routing.cpp` with a `PortRegistry` mirroring WILDBAND's ports and
applies the same random connects and disconnects (including out-of-range ports) to both graphs.

```
cmake -S . -B build -DTALORA_DIR=/path/to/modulo && cmake --build build --target wildband_talora_parity
120000 edits compared with TALORA's Graph, 0 disagreements          (modulo 91cc748, 2026-11-27)
```

The test is optional because it needs the private repository; WILDBAND builds without it.

## Differences from TALORA, and why

| Topic | TALORA | WILDBAND P0 | Reason |
|---|---|---|---|
| Port types | Audio, CV, Gate, Trigger | Audio, CV | No gates yet; "HITS" is a CV |
| CV convention | Volts, bipolar, 1 V/oct pitch | Unipolar 0..1 | Envelope-like sources only. **A real incompatibility** if modules are ever shared |
| Normalling | Inputs have built-in sources; normals can be broken | None: the default patch is made of real cables | So that every connection, including radio → resonator, can be pulled out by hand |
| Cycle check | Yes | Not needed: no module has both inputs and outputs | Must be added with the first such module |
| Capacity | 64 cables, 256 ports | 16 cables, 14 ports | — |
| Plan hand-off | Triple buffer, adopted at a block boundary | One atomic 64-bit word, read each callback | The plan is 7 bytes. Both are wait-free |
| Glitch-free re-patching | Plan swap at an island boundary | Every input cross-fades or glides to its new source (20–30 ms) | Tested: `patching`, 40 random re-patches, no step above 0.03 |
| Undo, revisions, transactions | Yes | No | Not needed for one small patch bay |
| A cable dropped on an occupied input | Rejected (`FanIn`) | The UI pulls the old cable (a validated disconnect) and plugs the new one | Performance gesture; the `Graph` rule itself is unchanged |
| UI | Three.js WebView, Blender GLB rack | Native JUCE component, drawn sockets and cables | Small and immediate; no WebView in the audio app |

## The patch bay in the prototype

Outputs: `radio.audio_out`, `local.audio_out`, `radio.level_out`, `radio.activity_out`, `room.level_out`,
`room.activity_out`, `room.brightness_out`. Inputs: `resonator.audio_in` and the six control inputs.

The four requested demonstrations, all present in the default patch or one drag away:

| Requested | In the prototype |
|---|---|
| Radio audio → resonator audio input | `radio.audio_out → resonator.audio_in` (default; pull it and the resonators ring out) |
| Radio-derived envelope → DSP modulation input | `radio.level_out → resonator.damp_cv_in`, `radio.activity_out → resonator.excite_cv_in` (default) |
| Microphone-derived envelope → feedback or damping | `room.level_out → resonator.feedback_cv_in` (default) |
| One source → several destinations | `radio.level_out → damp` **and** `→ drift` (default); test `patching` checks a three-way fan-out |

Interaction: drag from an output to an input; compatible inputs light up while dragging; audio sockets are square
and control sockets round, and a mismatched drop is refused with the reason shown. Drag a cable off its input to
move it, or drop it in empty space to remove it. Cables and source sockets brighten with the live signal.

## Assumptions still to be tested

- That the 0..1 CV convention is the right one musically (no negative modulation is possible).
- That one cable per input is enough; mixing two sources into one input needs a mixer module or a rule change.
- Mouse dragging has been exercised only through the `Graph` API and tests, not by a person at the UI.
- Blender-rendered sockets: not started. The component already separates hit-testing and cable drawing from how a
  socket is painted.
