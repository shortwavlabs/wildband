# WILDBAND — Instrument Design & Experience Manifesto

**Date:** 2026-11-27  
**Status:** owner-articulated creative direction, recorded for subsequent design and engineering decisions. Candidate controls, layout and sequencing below remain proposals, not accepted implementation scope.  
**Working phrase:** Play the world.

## 1. The instrument we want to own

WILDBAND is a self-contained, semi-modular electronic musical instrument for people who make music on computers. It bridges the computer and the physical world of the performer. It has its own voice, character, constraints and playing techniques.

The owner describes the goal by analogy with a favorite guitar: an instrument you reach for repeatedly because you connect with it, it sounds wonderful, and playing it is rewarding in itself. WILDBAND should invite the same relationship, even when the musician has no intention of recording a piece.

**It must sound great and be fun. Neither is optional.**

WILDBAND is not VCV Rack, a general-purpose modular synthesizer environment, a collection of independent effects, a radio player with a kitchen sink of processors, or a tool whose principal activity is configuration. Patching is part of playing this particular instrument, not a substitute for its identity.

## 2. The world is a participant

The defining experience is the audible interaction between:
- **World:** live radio broadcasts, their dynamics and real connection state; optional remote environmental observations.
- **Place:** room activity and optional physical sensors, including the proposed iPhone probe.
- **Player:** tactile controls, mouse/trackpad, physical MIDI and optional phone gestures.

The outside world should be musically consequential, not decorative telemetry. The musician determines how strongly and in what ways it influences the sound. Internal random modulation complements actual environmental observations, not replaces them. Simulated tuner static, for example, is honest synthetic sound whose timing follows real connection and buffering conditions.

The same patch played in different surroundings or at a different time should invite a different performance, while retaining a recognizable musical character. Save the instrument's configuration, not the performance. No sample archive, timeline or full-performance recording/export workflow. Owner-approved exception (2026-11-27): [LOOP](TAPESTRY-LOOPER-RESEARCH.md) may capture a very short radio fragment into volatile memory for immediate chopping, looping and manipulation. It is a playing gesture, not an archive; captured audio is never serialized or restored. Historical P0 remains capture-free.

## 3. Borrow the engineering, create our instrument

TALORA is the precedent for a carefully authored, self-standing semi-modular instrument. Its typed patching and validated routing approach can inform WILDBAND without importing its Mutable Instruments module collection or assuming its cable UI and graph implementation are directly portable.

Similarly, selected algorithms from Shortwav Labs' own research can become integrated WILDBAND functions. Their original product boundaries need not be exposed as a collection of branded modules.

| Existing work | Potential contribution |
| --- | --- |
| TALORA | Typed audio/CV, fan-out, graph validation and safe topology changes |
| Korupt | Nonlinear CMOS/PLL-inspired transformations and feedback-related modulation |
| Drift | Organic, controllable pseudo-random movement |
| Wyrd | Feedback-aware delay, reverb, strength/follower, agitation and TOUCH interaction |
| AFTERIMAGE | Rolling granular-delay adaptation, TRACE-to-CLOUD dissolution and optional spectral guidance |
| WILDBAND P0 | Live radio receiver, resonant engine, room features, MIDI and patching foundation |

This is a candidate palette, not a mandatory bill of materials. Prior tests are useful starting evidence; integration, licensing, realtime performance and musical qualification remain necessary. AFTERIMAGE's draft realtime gates are not fully met. WILDBAND's release license remains undecided.

Every proposed function must answer: what unique musical gesture, sonic character, or meaningful world-to-sound relationship does it enable in this instrument? If the answer is only “more flexibility” or “the algorithm already exists,” defer it.

## 4. Industrial design: vintage, sophisticated, playful

The design should evoke a strange experimental radio instrument from an alternate timeline: the tactile charm of vintage analog radio and studio electronics combined with modern, legible, playful electronic-instrument interaction. Think 1978 meets 2026, not a reproduction of a particular device.

Reference lessons, not visual assets to copy:
- Vintage radio and car stereo: tuning scales, preset buttons, materiality, the ritual of finding a station, imperfect transitions.
- Teenage Engineering EP-133 K.O. II: compelling sound and playful, memorable interactions; a quirky illuminated segmented/icon display that conveys character without becoming a complicated dashboard.
- Make Noise Strega and related experimental hardware: deliberate constraints, tactile interventions, feedback and discoverable signal relationships.
- TALORA and experimental semi-modular instruments: coherent object identity and carefully selected functions, rather than an empty rack.

Use a distinct Shortwav Labs industrial design and **#F59E0B** accent. Explore typography, contrasting panel materials, instrument-scale knobs, switches, lamps, a substantial tuning dial, and original display iconography. Avoid reproducing recognizable third-party panel layouts, icons, product silhouettes, type treatment or trade dress.

The visual direction should reward picking up the instrument, not merely admiring a static render. Sophistication comes from purposeful organization, not from the number of controls. The design must work at practical laptop window sizes, Retina, reasonable scaling and with clear labels and contrast.

## 5. One coherent control surface

Explore a fixed integrated panel with a normalled musical signal path and a curated patch field. The instrument must make an interesting sound without the user wiring it from scratch. Patching can interrupt, redirect, modulate or extend selected internal relationships.

The following is a **conceptual organization**, not a locked module set or approved faceplate:

| Section | Musical role |
| --- | --- |
| TUNER | Select, sweep, hop and play radio transitions |
| WORLD | See, calibrate and couple optional live external signals |
| RESONANCE | Establish a distinct tuned, nonlinear sonic voice |
| CORRUPTION | Explore nonlinear tracking/instability if musically justified |
| DISSOLVE | Move between coherent echoes and dispersed granular textures |
| SPACE | Shape feedback, delay, absorption and atmosphere |
| SIGNAL PATCH FIELD | Discover meaningful audio/CV connections and visible signal activity |
| TOUCH / PERFORMANCE | Immediate interventions, MIDI mapping, temporary gestures and recovery |

These labels are exploratory. Some functions may merge, be removed or appear only in a secondary context if that produces a clearer instrument. Do not expose every inherited DSP parameter. Distinguish audio, continuous CV and triggers with shape, labels and color; keep connection rules understandable. Avoid duplicative menus, sprawling rack navigation and a matrix of indistinguishable knobs.

## 6. Discovery as an intentional design problem

### First contact
- Launch, tune a station, turn a main control and hear something compelling. No prerequisite patching, login, phone, microphone permission or MIDI hardware.
- The normalled path must be audible and recognizable, with optional external signals disabled by default.
- Provide a clear offline local test option, never mislabeled as live radio.

### Progressive discovery
- Give the user a small set of immediately discoverable musical gestures, with deeper interactions available through patching.
- Expose a few useful destinations and routes before adding a large connection surface. Make valid sockets and connection types obvious.
- Help users understand cause and effect via actual signal activity, responsive indicators and concise contextual hints.
- Offer optional short patch recipes and playing techniques, not a long tutorial gate or a collection of prerecorded outcomes.
- Design for productive accidents: experimentation can yield surprises, but should not normally lead to silence, uncontrolled output or a confusing dead end.

### Confidence and recovery
- Provide easy disconnection, clear defaults, recoverable configuration and reliable panic behavior. Evaluate undo/reset ergonomics; do not require a specific implementation before testing.
- Use bounded feedback, finite levels, smoothed changes, truthful status, and graceful unavailable-source handling.
- Preserve intentional sonic character and tails when changing stations or patch routes where safe; do not silently reset the whole instrument.
- Provide physical MIDI control of primary gestures as a first-class interaction; mouse/trackpad equally usable. Phone sensing is optional and separately consented.

### Personality and feedback
The compact quirky display is a central design opportunity. It can express tuning, lock/connection, actual room or phone activity, signal flow, modulation and feedback in an original illuminated visual language. It must remain operationally honest, legible and low overhead. Characterful animation should correspond to real signal state or explicitly simulated behavior; never fake telemetry or obscure critical errors. The performer should be able to play without continuously watching a screen. No decorative mini-game.

## 7. Sound quality and fun are release criteria

Do not confuse numerical correctness, feature count, visual fidelity or algorithm novelty with musical success. Assess both audible quality and interaction with real users, beginning with the owner.

| Experience question | Proposed evidence |
| --- | --- |
| Does the instrument make a compelling sound immediately? | First-contact audition from the normalled path; compare with dry radio and a conventional filter/delay |
| Does it have a recognizable sonic identity? | Musical audition across speech, music and noisy sources, distinct from reference processing |
| Can the world be played? | Matched patch with a valid environmental route connected vs disconnected; observable external changes cause audible, useful responses |
| Is it learnable? | New user can identify the main gestures, patch a meaningful connection and predict or recover from its effect without a lengthy explanation |
| Is exploration rewarding? | User intentionally discovers several contrasting musical states and can return to a preferred one |
| Does it feel tactile and expressive? | Mouse/trackpad and actual physical MIDI tests; optional phone only if implemented |
| Does it encourage return? | Two owner play sessions of at least 15 minutes, desire to continue, voluntary return outside development |
| Is it safe and reliable enough to trust? | Actual device/session testing, bounded feedback, reliable PANIC, denied mic and offline behavior; no unsupported pass claims |

### The favorite guitar test

After playing, ask: would I open this tomorrow just to play? Did I discover a sound I wanted to pursue? Did I lose track of time? Did the outside world and my gestures genuinely shape the result? Did I make music rather than spend my time configuring software?

These are qualitative design questions alongside the existing P0 gates, not replacement numerical thresholds or invented listening evidence. Use the answers to cut features as readily as to add them.

## 8. Time and scope discipline

The owner has approximately two months for the contest. The following eight-week sequence is an aggressive planning sketch from the discussion, not a deadline verification, delivery guarantee or authorization to implement every candidate:
- Week 1: restore output-only startup, finish P0 owner audition and decide sonic direction.
- Weeks 2–3: selectively reuse DSP and prototype playable tuner transitions.
- Week 4: focus on meaningful WORLD influence and physical performance.
- Weeks 5–6: Blender visual system, JUCE controls and original display; refine interaction.
- Week 7: playability, safety and reliability.
- Week 8: packaging, documentation and three-minute performance demo.

Explore industrial design in parallel with listening and DSP research, but do not lock production enclosure geometry before the control set stabilizes. Prototype interaction in JUCE first, use Blender for high-quality static and state assets, and JUCE for live composition and interaction. Keep the exact deadline and contest submission requirements subject to separate verification.

The three-minute demo should be a real musical performance that reveals the instrument, not a feature checklist. Use only qualified features, verify broadcast audio rights before publication, and show audible environmental causality. Cut complexity before compromising sound quality, playability or reliability. The current P0 implementation and its permission/output blocker remain on their own track; this manifesto does not change P0 acceptance or authorize merging any draft.

## 9. Related research

- [Product vision](PRODUCT-VISION.md)
- [UI and Blender direction](UI-AND-BLENDER-DIRECTION.md)
- [Architecture RFC](RFC-001-MACOS-ARCHITECTURE.md)
- [Field Kit-inspired utilities](FIELD-KIT-INSPIRED-UTILITIES.md)
- [Playable radio tuner](RADIO-TUNER-PERFORMANCE.md)
- [iPhone WORLD bridge](PHONE-WORLD-BRIDGE.md)
- [Wyrd reuse](WYRD-REUSE-RESEARCH.md)
- [AFTERIMAGE reuse](AFTERIMAGE-DSP-REUSE.md)
- [Korupt feasibility issue](https://github.com/shortwavlabs/wildband/issues/2)

**Decision principle:** Sound exceptional. Feel alive. Invite curiosity. Reward experimentation. Make an instrument the musician wants to play again.
