# WILDBAND — Field Kit-Inspired Patch Utilities (Research Proposal)

**Status:** candidate directions only, not approved P0 scope. **Reference:** [KOMA Elektronik Field Kit](https://koma-elektronik.com/new/product/field-kit/). Study its *open-ended relationships between simple functions*, not its exact hardware design, panel layout, trade dress or firmware.

## Why it matters

WILDBAND uses live radio and local/remote environment signals to drive performances that cannot be replayed exactly. Patchable utilities should make those signals *musically steerable*, with feedback from the performer and meaningful outcomes. TALORA's established typed audio/CV ports, connection validation, fan-out and real-time-safe graph updates are the preferred starting point for assessing reuse, not a mandate to transplant the entire rack.

## Prioritized candidates

### 1. Auxiliary mixer and controlled feedback routing — first musical follow-up

**Reference concept:** Field Kit mixer and auxiliary sends.

**WILDBAND concept:** small patchable mixer with independent levels and an auxiliary return, able to form expressive loops such as RADIO → KORUPT → RESONATOR → AUX → processor input. Feed-forward routes and explicit delayed feedback paths should be differentiated. Enforce loop-delay or safe graph-cycle policy, finite gain, saturation/limiting, anti-click topology changes and a reliable PANIC function. Never route unrestricted feedback purely because the UI allows cables to connect.

**Why:** changes the instrument's behavior and playing technique; not merely another static effect.

**P0 boundary:** do not extend current P0 implementation until owner audition. Research and prototype on a separate follow-up branch after the current experience gates, unless strictly necessary to answer a blocker.

### 2. Configurable envelope follower + gate — first musical follow-up

**Reference concept:** convert incoming audio into control voltage and gate events.

**WILDBAND concept:** selectable RADIO/ROOM/EXTERNAL audio analysis with adjustable attack, release and threshold, providing independently patchable continuous ENVELOPE and discrete GATE/HIT signals. Route them into processor pitch, damping, strike rate, feedback or Korupt parameters. Rate-limit event signaling appropriately, suppress false triggers and smooth discontinuous controls.

**Why:** speech, rhythm and silence become actual performance structure.

**P0 boundary:** basic envelope/hits may already exist; inventory and extend rather than add a redundant competing module.

### 3. SENSE — environmental calibration / CV utility — later

**Reference concept:** sensor input conditioning and level/offset controls.

**WILDBAND concept:** normalize and calibrate environmental inputs with gain, offset, polarity, clamp, depth and smoothing. Explain sensor freshness/availability; fail gracefully to neutral values. Distinguish input-derived physical observations from synthetic/internal modulation.

**Possible sources:** microphone loudness/transients/brightness, optional weather, BLE or Wi-Fi metadata after verification. Wireless metadata is not raw EM-field measurement.

**Why:** helps subtle real-world changes become performable without granting outside signals uncontrolled authority.

### 4. Minimal patchable LFO — later

**Reference concept:** simple periodic modulation (square/triangle and blending).

**WILDBAND concept:** one light-weight LFO with clearly meaningful rate/depth and phase behavior. Allow audio/environment envelopes to modulate LFO rate/depth. No sequencer, large preset suite or elaborate modulation matrix required.

**Why:** creates controllable motion in partnership with unpredictable external signals.

## Future hardware-facing investigations (not P0)

- **Electromagnetic/contact pickup input:** external induction coils/contact mics through a compatible audio interface. Treat as actual audio input and document gain/noise/safety, not native Wi-Fi radio-spectrum access.
- **Motors/solenoids/physical actuators:** only with explicitly supported external hardware and bounded outputs, likely MIDI or dedicated protocol; not a laptop-only requirement.
- **Second live radio:** separate from Field Kit utilities but especially useful for cross-modulation and auxiliary sends after core playback and routing are solid.
- **Korupt integration:** see [WILDBAND Issue #2](https://github.com/shortwavlabs/wildband/issues/2) for independent CMOS/PLL port feasibility and audio/CV output proposals.

## Patch recipes instead of fixed musical outcomes

Inspired by KOMA's experiment-oriented education, eventually provide a *patch recipe book*: relationships and playing techniques, not prerecorded results or rigid playback presets.

Example ideas (unvalidated, no new implementation commitment):

- **Two Stations Arguing:** one radio signal modulates the other through follower, gating and resonance.
- **Ghost Conversation:** broadcast syllables generate gates; an envelope rides feedback/damping.
- **Storm Machine:** slow weather conditions shape resonance while radio activity provides excitation.
- **Room Creature:** room sound drives the response of a radio-excited instrument.

If configuration persistence is later added, save topology, parameter values, selected sources and controller mappings, **not a captured performance**. Patch recall should gracefully substitute missing radio/sensors.

## Design, playability and engineering checks

- Every utility adds a learnable *gesture* or interaction; no inert miniature equipment or decorative CV cabling.
- MIDI controllers are first-class *physical performance interfaces*: mouse/trackpad must also work.
- JUCE owns draggable patch cables and hit testing; Blender supplies consistent socket/knob/enclosure assets in **#F59E0B** Shortwav visual language.
- Distinguish typed audio vs control ports, modulation ranges and fan-out; verify the actual TALORA implementation before claiming portability.
- Modulation updates are bounded, smoothed and real-time safe; graph changes cannot glitch or allocate/block on the audio callback.
- Compare the new utilities against existing reference DSP in an owner performance session; do not assume more controls make the instrument more fun.

## Proposed decision order

1. Finish and audition the P0 radio + resonator + room + MIDI + patching path.
2. Try one auxiliary/feedback routing experiment and one improved follower/gate experiment. Report each separately, with owner feedback.
3. Only then choose whether SENSE, the LFO, Korupt, or additional physical inputs offer the most musical value.
4. Keep the optional future "50 ways to play" recipe collection as documentation, not a release requirement.
