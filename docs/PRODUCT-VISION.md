# WILDBAND — Product Vision

**Status:** exploration, not product approval. **Working phrase:** Play the world.

WILDBAND is a macOS-first live electronic instrument. Its sound source is a live radio broadcast; its behavior is influenced by the performer's gestures and optionally by their physical and remote environment. It is for *performing in the moment*, not archiving, exporting or re-creating a fixed musical result. A tiny volatile radio loop is an explicit live-playing exception, never saved as audio.

The owner's detailed creative direction is recorded in [Instrument Design & Experience Manifesto](INSTRUMENT-DESIGN-MANIFESTO.md): a cohesive semi-modular instrument, distinctive sound, meaningful WORLD coupling, playful discovery and the favorite guitar test. Candidate functions are curated, not a general-purpose modular rack.

The agreed [Product Roadmap](ROADMAP.md) defines the staged development and contest delivery priorities.

## Creative thesis

A guitar or modular synthesizer is fun because the musician interacts with a responsive system whose outcome is shaped by performance and context. WILDBAND tests whether computer-based instruments can deliver a similar relationship. The same configuration should preserve a recognizable character while the incoming broadcast, room activity and gestures change. **The patch is a set of conditions, not a finished composition.**

Three participants: **World** (live radio and potentially weather), **Place** (microphone and optional macOS-observable sensor signals), **Player** (hands-on control, MIDI, listening, improvisation).

## Experience pillars

1. **Tune** into the present: continuous live broadcasts, not sample libraries.
2. **Touch** the sound: a small set of expressive controls with immediate audible consequences.
3. **Couple** signals: radio-derived control signals interact with processing and environment-derived control signals.
4. **Discover** emergent patterns: controllable surprises rather than arbitrary randomization.
5. **Let go**: performances can fade and disappear; no obligation to record.
6. **Return**: saving the setup is acceptable but must not imply identical playback.

## Reference lessons, not assets to copy

- [Fingerlab Musyc](https://www.fingerlab.net/portfolio/musyc): construction, play, and emergent behavior.
- Teenage Engineering: playful tactile controls, quirks and a memorable interface.
- [Make Noise 0-Coast](https://www.makenoisemusic.com/synthesizers/0-coast): compact functions with deep interactions.
- [KOMA Field Kit](https://koma-elektronik.com/new/product/field-kit/): environmental and radio signals as patchable musical materials.
- VCV Rack: the patching experience is an end in itself.

## Visual identity

Distinctly Shortwav Labs; official orange **#F59E0B**. A Blender-authored physical-object visual system can make hardware-like controls attractive, but visuals must be coupled to real interaction. Avoid recreating reference products' enclosure silhouettes, layout or trade dress. Prioritize readable labeling, MIDI control, accessibility and feedback.

## Firm boundaries

- The primary product is an **instrument**, not an audio recording or productivity application.
- No built-in record, capture, rip, render or sample-library workflow in historical P0. Post-P0, the owner approved [LOOP](TAPESTRY-LOOPER-RESEARCH.md): short explicitly captured, RAM-only fragments for live manipulation; no saved audio, full-performance recording, sample library or export.
- Standalone macOS first. AU/VST3 and Windows are optional decisions after P0.
- External sensors must be opt-in; the instrument remains useful if all are unavailable.
- Wi-Fi/Bluetooth metadata is *not* general RF field sensing; raw spectrum requires external hardware.
- Playability matters more than whether the music can literally never repeat.
- A minimal local fallback sound source is allowed for offline rehearsals and tests.

## Success question

Can a musician learn to influence the instrument, improvise for 15–30 minutes, and want to return—without making a recording? If it only feels like a radio player with effects, the concept has failed.
