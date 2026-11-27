# WILDBAND — Interactive Object & Blender Direction

**Status:** early art/interaction brief, not an approved mockup.

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
- **No recording/capture/export button** as a primary visual element.

## Prototype art directions to explore only after interaction proof

- **Field instrument:** compact rugged exploratory device, colored tactile modules, prominent radio tuner and small signal windows.
- **Boutique studio box:** brushed-metal device with a quirky, inviting control hierarchy, recognizable from its silhouette.
- **Playful workbench:** instrument built around manipulable signal relationships, rather than fake patch cables everywhere.

## Art acceptance

A user should be able to point to the five central gestures and correctly predict their effect. The faceplate must be readable and genuinely interactive; model/render quality alone does not pass this gate.
