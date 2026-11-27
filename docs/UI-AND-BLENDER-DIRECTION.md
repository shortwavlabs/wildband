# WILDBAND — Interactive Object & Blender Direction

**Status:** early art/interaction brief, not an approved mockup.

See the [Instrument Design & Experience Manifesto](INSTRUMENT-DESIGN-MANIFESTO.md) for the overarching owner direction: one integrated, self-standing instrument with a compelling normalled path, progressive discovery, vintage radio character and an original playful display. Candidate panel sections are exploratory, not fixed geometry or a commitment to a rack of independent modules.

## Goal

Create the *desire to touch* that the user associates with Teenage Engineering, Make Noise and experimental electronic instruments, but translate it into a practical computer instrument. Avoid making a beautiful static faceplate that forces menu-diving or obscures interaction.

## Workflow proposal

1. **Interaction prototype first:** clickable/drag-able placeholder controls in JUCE, mapped to real DSP and MIDI; evaluate response while playing.
2. **Blender visual language:** model a distinctive Shortwav Labs object/enclosure; render consistent orthographic front-panel assets, illumination states, physical knobs, switches and shadow/occlusion layers.
3. **Export layered assets:** device faceplate separate from controls and indicators. Render knob sprite/angle sets or derive rotated assets; retain precise pivot coordinates and hit regions in a sidecar asset manifest.
4. **JUCE composition:** actual text, connection status, metering, patch feedback and animated data displays are generated dynamically. JUCE owns pointer, focus, automation, MIDI mapping, tooltips, accessibility and states.
5. **Validation:** scale 100–200%, Retina, light/dark desktop surroundings, keyboard navigation, colorblind readability, low CPU consumption while audio is active.

## Design constraints

- **Shortwav orange #F59E0B**, paired with an intentionally distinct, accessible palette for each signal class (radio / room / player), using shape and labels as well as hue.
- A **large tuning control** should genuinely respond as a performance gesture, not be an imitation frequency dial that skips through internet stream endpoints invisibly. Communicate buffering and source change.
- A minimal collection of **tactile modules**: source, environment, coupling, motion/instability, output. Avoid a huge rack of tiny equally prominent knobs.
- Every decorative animation must correlate to real signal state or a meaningful simulation rule. No inert microgame.
- Design surprise gestures carefully; transformations must be bounded, reversible where appropriate, and musically relevant.
- Support mouse/trackpad and MIDI controllers equally; no requirement to own external hardware.
- Use Blender for high-quality **static objects and state sprites**, not full real-time 3D at audio rate.
- Avoid direct copies of OP-1, Field Kit, 0-Coast or Musyc industrial design or trade dress.
- **No full-performance recording/export controls.** The post-P0 [LOOP](TAPESTRY-LOOPER-RESEARCH.md) exception may expose a compact CAPTURE/PLAY/CHOP/CLEAR interaction, truthful waveform/playhead and volatile-memory status. Final geometry follows audition.

## Prototype art directions to explore only after interaction proof

- **Field instrument:** compact rugged exploratory device, colored tactile modules, prominent radio tuner and small signal windows.
- **Boutique studio box:** brushed-metal device with a quirky, inviting control hierarchy, recognizable from its silhouette.
- **Playful workbench:** instrument built around manipulable signal relationships, rather than fake patch cables everywhere.

## Art acceptance

A user should be able to point to the five central gestures and correctly predict their effect. The faceplate must be readable and genuinely interactive; model/render quality alone does not pass this gate.

## Illuminated performance display — future UI research

**Status:** proposed design direction, not an approved or required P0 feature. **Reference:** Teenage Engineering EP-133 K.O. II's playful segmented/icon display behavior, *not* its actual icons, screen graphics, typography, panel layout or trade dress. Build a distinct Shortwav Labs display language.

### Product intent

WILDBAND should have a small, charming **live signal display** that makes the instrument feel like a tactile electronic object. It is a source of secondary performance feedback and personality, not a replacement for ears or physical MIDI controls. A performer should still be able to improvise **without looking at the computer**.

### Visual concept

- A compact smoked-glass or dark translucent panel with individually illuminated, legible iconography, compact segmented-style numerals, and sparse motion. Prefer characterful low-information presentation over a high-density desktop dashboard.
- WILDBAND-specific pictograms for radio, room, weather, patching, control/gesture, oscillation, and feedback; distinguish from the EP-133's actual artwork and arrangement.
- Use Shortwav Labs orange **#F59E0B** as a recognizably branded accent, paired with a small number of supporting high-contrast signal colors. Icons need non-color cues.
- Blender supplies enclosure, bezel, glass/reflections and optionally inert illumination masks. **JUCE renders changing icons, numbers, meters, animations, real source state and interaction/accessibility.** The audio callback must never draw UI or dispatch UI work directly.

### Candidate states and animations (behavioral ideas, not UI contracts)

| Display mode | Signal-driven content |
| --- | --- |
| RADIO | Station/source indicator, buffering/connected/error status, activity animation derived from real input energy |
| ROOM | Microphone permission/status and an expressive activity glyph responding to valid local audio features |
| PATCH | Compact view of active typed routes; actual audio/CV activity, not fabricated cable flow |
| KORUPT (future) | PLL lock/tracking confidence and glitch activity when Korupt is implemented; no fake lock telemetry |
| ENVIRONMENT (future) | Available/stale status for weather and other optional sensors; no implication that Wi-Fi/BLE are raw EM measurements |
| FEEDBACK | Readable feedback intensity and safety intervention state; do not glamorize or obscure unsafe levels |
| MIDI | Brief learn/assignment feedback and controller event indication when events are actually received |

A tiny playful **responsive character / creature** is an optional explorative treatment for ROOM or overall signal activity, provided it has a clear mapping to real inputs and never interferes with critical status visibility. Avoid a mandatory mascot or distracting screen-wide animations.

### Interaction requirements and implementation gates

1. **Prioritize musical affordances:** gestures and MIDI mappings work with the display closed, dimmed or ignored. Display modes must not block actual parameter access.
2. **Semantically honest status:** live, buffered, silent, permission denied, source unavailable and stale environmental data must be distinguishable; do not present decorative animation as measured data.
3. **Low overhead:** cap redraw frequency, avoid graphics allocations/locks in the real-time callback, interpolate meter snapshots on the UI thread, and validate CPU/GPU usage while DSP runs.
4. **Accessibility:** readable text at Retina and 100–200% scale, contrast and reduced-motion alternative; do not rely solely on color or tiny pictograms for operational information.
5. **Prototype first:** validate one JUCE-rendered functional display panel using real stream envelope and MIDI/patch status in the interaction prototype. Only after owner acceptance consider Blender treatment, expanded icon set, and animated personalities.
6. **Distinctiveness:** evoke playful segmented electronic-instrument displays without copying identifiable EP-133 screen composition, icons, specific typography or product silhouettes.

### Explicit scope boundary

This is **design research and eventual UI roadmap work**, not authorization to interrupt the ongoing P0 radio/DSP/MIDI/patching implementation, add a miniature game, or spend time on full Blender rendering before owner playability acceptance.
