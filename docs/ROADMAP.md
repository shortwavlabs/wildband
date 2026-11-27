# WILDBAND — Product Roadmap

**Version 1.1 | 2026-11-27 | Owner-agreed planning direction.** Candidate implementations, DSP choices, design and release gates still require evidence. Includes Rings as a priority resonator experiment, ATMOSPHERE and macOS AUv3 feasibility.

**Play the world. The world is your oscillator.**

## Product principles

The [Instrument Design & Experience Manifesto](INSTRUMENT-DESIGN-MANIFESTO.md) governs the roadmap. WILDBAND is one cohesive semi-modular instrument, not an empty rack or collection of effects. WORLD (live broadcasts and surroundings), SOUND (distinctive voice), DISCOVERY (meaningful patching), PLAY (tactile musical agency). The favorite guitar test: does the musician return simply to play?

Compelling normalled sound first; patching opens deeper possibilities. Real external conditions must have audible, useful influence. Save the instrument, not the performance: no full-performance recording, sample library, timeline or audio export. The sole deliberate capture exception is LOOP: a tiny explicitly captured, RAM-only fragment for live manipulation, never saved with a patch. No mandatory peripheral, cloud account or location permission.

## Schedule and contest requirements

[Official Plugin Jam rules](https://www.theaudioprogrammer.com/events/plugin-jam-rules), verified November 27: register and submit via Freqlab by **January 31, 2027, 11:59 pm Pacific Time**. Working signed AU, VST3 or CLAP on Mac and/or Windows; notarized on Mac; public GitHub repository with OSI-approved license; demo of at most three minutes on YouTube/Vimeo; disclose reused code and license/credit third-party material. Developer accounts are needed for signing. Standalone alone is not eligible. Recheck rules, marketplace and supported upload formats before delivery. Finale February 12.

**Target:** macOS standalone + conventional AU (AUv2). macOS AUv3 is an early feasibility target and desired additional format if qualified; VST3 additionally if time permits. Do not make AUv3, VST3 or iPadOS a dependency for the contest entry. Confirm AUv3 acceptance with Freqlab before counting it as the qualifying binary.

| Week | Dates | Objective |
| --- | --- | --- |
| 1 | Nov 27–Dec 3 | Consolidation, licensing, AUv2 proof, AUv3 feasibility |
| 2 | Dec 4–10 | Comparative DSP including Rings |
| 3 | Dec 11–17 | Select voice architecture |
| 4 | Dec 18–24 | Patching and WORLD |
| 5 | Dec 25–31 | Phone sensor spike, ATMOSPHERE, UI |
| 6 | Jan 1–7 | Integration and production visual design |
| 7 | Jan 8–14 | Musical refinement, reliability, feature freeze |
| 8 | Jan 15–21 | Release qualification, field guide and film |
| Buffer | Jan 22–31 | Package and film by Jan 24; fixes and early submission |

Ambitious planning targets, not guarantees. Design exploration and wrapper work run in parallel; production enclosure follows accepted control set. Cut features before sacrificing sonic quality, playability or safety.

## P0 — Consolidated foundation (Dec 3)

[PR #3](https://github.com/shortwavlabs/wildband/pull/3) owner audition encouraging: fun, instrument-like, patching engaging, wants to continue; resonator lacks expressiveness, environmental influence somewhat convincing, off-centre degradation liked. Post-fix live-radio CoreAudio worst callback 4.0% ROOM off, 4.5% ROOM on (silent input); output switching/helper loss demonstrated. [PR #4](https://github.com/shortwavlabs/wildband/pull/4) depth: 670 checks, 3.8% worst callback; [PR #5](https://github.com/shortwavlabs/wildband/pull/5) STRIKE: 688 checks; [PR #6](https://github.com/shortwavlabs/wildband/pull/6) tuner: 747 engine and 24 receiver checks, 4.1% worst callback, one window not fully live. Follow-up builds not auditioned; no formal owner gate promoted. See [owner evidence](https://github.com/shortwavlabs/wildband/blob/p0/prototype/docs/p0/OWNER-FIRST-AUDITION.md).

- Consolidate PRs 3–6 on separate branch; preserve historical evidence and builds, isolate/backup settings. Compare STRIKE response times; preserve detune; distinguish intentional retune from device restart. Audition depth/static/navigation.
- Complete MIDI, mouse, ROOM, device switch/unplug, permission revocation, sleep/wake, network loss and soak checks. ROOM helper packaging/signing/lifecycle, input selection and built-in microphone verification open.
- Licensing: AGPL-3.0-only proposed; confirm JUCE route, complete delegated inventory verification, GPL/AGPL notices, Vult generated code provenance and reproducibility. Owner authorizes TALORA concepts; specific PlanMailbox reuse addressed in follow-up; no blanket publication of private implementation or third-party material.
- AUv2 proof: share processor and UI with standalone, separate audio-device ownership, validate instrument/generator bus type and MIDI, instance isolation, transport, state restoration, offline rendering and helper permissions. No automatic microphone request at instantiation/restore.
- **AUv3 spike:** JUCE supports AUv3 ([CMake API](https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md)); check exact pinned JUCE/Xcode generator and packaging requirements. AUv3 is an app extension, not a drop-in .component. Prototype a signed containing app + extension, Logic discovery/instantiation, network client entitlement for streaming/weather, sandbox/local phone access, resource loading and settings, multiple instances and extension lifecycle. ROOM cannot assume it can spawn a sibling helper; investigate containing-app-managed sensing and authorized IPC with explicit opt-in, no host microphone prompt. Keep radio decode/network off callback. Test actual extension host; compilation alone is not qualification. See [Apple AUv3 docs](https://developer.apple.com/documentation/audiotoolbox/audio-unit-v3-plug-ins). Use shared DSP/UI/service interfaces; no parallel DSP fork. iPadOS AUv3 is future work requiring a separate touch/mobile/network/performance and distribution qualification.

**Exit:** consolidated playable baseline, remaining gaps explicit, credible conventional AU path. AUv3 feasibility result documented without blocking AUv2. Draft PRs until review.

## P1 — Sonic identity (Dec 17)

Common experimental voice slot, baseline resonator and DRY/REFERENCE retained. [Comparative DSP plan](https://github.com/shortwavlabs/wildband/blob/p0/prototype/docs/p0/PLAN-D-COMPARATIVE-DSP.md). Stage small eligible primitives and owner listening checkpoints, not wholesale transplantation.

### P1-R — Rings (priority early resonator experiment)

Evaluate Mutable Instruments Rings as a replacement or complement for the current resonator. Prior TALORA integration experience is relevant. Prefer original MIT-licensed upstream DSP with required notices and pinned dependencies; inspect existing TALORA adaptation/test evidence without copying private code beyond authorization. [Upstream Rings](https://github.com/pichenettes/eurorack/tree/master/rings), [manual](https://pichenettes.github.io/mutable-instruments-documentation/modules/rings/manual/), [open source](https://pichenettes.github.io/mutable-instruments-documentation/modules/rings/open_source/).

Compare modal resonator, sympathetic strings and inharmonic/nonlinear string model on live speech, music, transients and tuning static. External continuous excitation and internally triggered excitation evaluated separately. Curate pitch, structure, brightness, damping, position and possible polyphony. Audit fixed block/sample-rate assumptions, resampling, latency, strum/exciter semantics, output/stereo mapping, parameter smoothing, tail preservation, CPU and safety. Compare against upstream reference and original WILDBAND resonator at matched levels, with WORLD modulation. Reuse the DSP, not the virtual hardware module, branding or panel. Select only if expressive benefit survives owner audition.

| Candidate | Musical intent | Constraints |
| --- | --- | --- |
| P1-A Korupt | CMOS/PLL tracking, subharmonics, nonlinear corruption | Rack-free equivalents, rights review, aliasing/lock/cost checks; [issue #2](https://github.com/shortwavlabs/wildband/issues/2) |
| P1-B AFTERIMAGE | Rolling granular delay, TRACE-to-CLOUD DISSOLVE | No IMPRINT/freeze/capture; overwrite temporary DSP history; broadband/filter-first; previous realtime gates unresolved; [research](AFTERIMAGE-DSP-REUSE.md) |
| P1-C Wyrd | Feedback delay/filter, atmosphere, strength and TOUCH | Select complementary primitives, bound feedback/memory, review Vult; [research](WYRD-REUSE-RESEARCH.md) |
| P1-L LOOP (Tapestry) | Short volatile radio fragments, chop, reverse and granular microsound | High priority; proposed 8 s cap, fixed capacity, no saved reels; [technical research](TAPESTRY-LOOPER-RESEARCH.md) |
| P1-D Drift | Organic random CV motion | CV only, not external randomness; Vult audit; resolve DRIFT naming clash; [research](FIELD-KIT-INSPIRED-UTILITIES.md) |

Common listening harness, lawful comparable speech/music/percussion/noise/silence/static, matched levels, actual WORLD route enabled/disabled. Separate numerical, realtime, safety and owner preference evidence. No audio-thread allocation, blocking or network. Test worst-case callbacks, sample rates/buffers, stereo/mono, feedback, aliasing, resets and PANIC. No unnecessary FFT path. Rights issues block affected ports, not independent experiments.

P1-L is a parallel playable source fed by radio; audition a small Tapestry-derived loop before committing to separate granular engines. Compare with continuous AFTERIMAGE DISSOLVE and share qualified primitives if useful. Preserve live radio and completed loop across retunes; choose final controls before Blender geometry.

P1-E possible normalled path: RADIO/TUNER → excitation/resonance/corruption → dissolution → space → stereo output. WORLD/DRIFT/TOUCH/MIDI modulate selected stages. Not a locked topology. Each algorithm earns its place; do not ship all four or expose branded modules just because they exist.

**Exit:** recognizable voice, intentional contrasting textures, meaningful environmental influence, owner preference over generic processing, realtime/safety qualification. Stabilize primary control contract.

## P2 — Semi-modular patching (Dec 24)

| Capability | Priority |
| --- | --- |
| Normalled path, typed audio/CV, output fan-out, independent signed depth, COUPLE | Essential |
| Curated audio routing between processors | Essential |
| CV mixing, auxiliary mixer, bounded feedback routing | High |
| Adjustable envelope follower/gate | High |
| Undo/recovery and clear valid socket affordances | High |
| LOOP audio routes, signed CV and bounded slice/retrigger events | High |
| General clock/trigger routing | Stretch |

[TALORA-inspired/Field Kit research](FIELD-KIT-INSPIRED-UTILITIES.md). Independent depths multiply source before destination-specific mapping; smooth, clamp, unavailable source neutral. Extend one-input policy only with tested mixer/fan-in. Explicit delayed bounded feedback cycles, reliable PANIC, coherent nonblocking snapshots. Compact selected-cable DEPTH rather than bank of knobs.

**Exit:** meaningful playable patches, visible cause/effect, recovery, safe live routing.

## P3 — WORLD (Dec 31)

### RADIO
Essential saved URLs, preset buttons/NEXT/PREV, MIDI navigation, off-centre degradation, adjustable synthesized static during actual deliberate buffering, broadcast envelope/transients. Static excites patch, preserves tails, no artificial delay or endless hiss, truthful failure status. Distinguish broadcast-only features from combined tuner activity. High: manual random HOP, rate-limited patchable station-hop trigger (ROOM hit, etc.), prevent synthetic-static trigger loops. Stretch: clock hopping, second independently tuned receiver, multi-stream prebuffering. [Tuner research](RADIO-TUNER-PERFORMANCE.md).

### ROOM
Essential opt-in LEVEL/HITS/BRIGHTNESS, no raw monitoring by default, sensitivity and useful response. Audition clap/speech/taps on headphones; stale/denied/off neutral, playback uninterrupted. Helper lifecycle/signing and plugin permissions release concerns.

### SENSE
High: calibration, gain, offset/baseline, polarity, smoothing, threshold/event extraction; complements per-route depth, avoid duplicate controls. Source freshness and safe neutral.

### iPhone WORLD bridge
High priority. PROBE observes conditions; GESTURE makes phone a physical controller. Magnetometer field change (not RF), acceleration, rotation and relative tilt. Pressure stretch. Validate existing sensor-to-OSC sender on trusted LAN, then narrow receiver, musical mappings, latency/jitter/loss and stale behavior. Production explicit authenticated pairing, consent, bounded validation, background/lock handling, no cloud or tracking. Dedicated Swift companion stretch, App Store not contest dependency. [Phone research](PHONE-WORLD-BRIDGE.md).

### ATMOSPHERE
High priority. ELSEWHERE manual city entry first, with geocoding and city/region/country disambiguation. HERE optional approximate location via Core Location only with explicit consent and host-safe behavior. Weather location need not match radio or performer's surroundings. No IP tracking requirement. Save selected location configuration, not history.

Candidate [Open-Meteo](https://open-meteo.com/), [geocoding](https://open-meteo.com/en/docs/geocoding-api), [terms](https://open-meteo.com/en/terms), [pricing](https://open-meteo.com/en/pricing). Verify hosted service eligibility (free non-commercial tier), quotas, data/geocoding attribution and terms separately. Provider adapter; lawful alternate/provider configuration if needed; no embedded secrets. Candidate patchable outputs temperature, wind/gusts, humidity, pressure, cloud cover, precipitation and day/night. Example mappings wind→grains, pressure→pitch, humidity→space, temperature→tone; no hardcoded mappings. Poll off callback roughly 10–15 min subject to actual update cadence and quota; deduplicate/cache, timestamp, smooth between valid samples, stale neutral ramp, backoff, display source/location/units/freshness/attribution. Forecast/model values not direct local measurements. No fake live data. Offline and malformed data tests; no core playback dependence.

### Other WORLD
Ambient light/BLE RSSI/Wi-Fi research/post-contest; external induction/contact pickup and SDR future. Never equate wireless metadata/magnetometer with raw RF.

**Exit:** aim for ROOM and iPhone as two physical-world influences beyond radio, ATMOSPHERE slow modulation; prove audible enabled/disabled effects. If phone blocked, consider ROOM+weather and record gap. Cut optional scope if needed.

## P4 — Industrial design and UX (parallel; production Jan 7)

One cohesive enclosure, vintage experimental radio meets playful modern electronics. Original Shortwav Labs #F59E0B accent, tactile tuning scale/buttons, readable labels. Explore receiver, field instrument and laboratory device directions. References vintage radio, EP-133 K.O. II, Strega and TALORA as principles, not copies.

Candidate sections TUNER, WORLD, RESONANCE/CORRUPTION, DISSOLVE, SPACE, TOUCH, LOOP, PATCH FIELD. Final set follows sonic evidence. Immediate gratification, progressive discovery, visible signal flow, productive accidents, easy recovery. Signature original compact illuminated segmented/icon display: real tune/lock/connection, room/phone/weather activity, qualified DSP telemetry, MIDI, feedback. Optional responsive character only if meaningful; no fabricated telemetry or safety obstruction.

Blender static enclosure, textures, filmstrips, glass, illumination/shadows; JUCE dynamic labels, controls, cables, hit testing, animation, MIDI, accessibility/scaling. Functional prototype first, no final geometry before stable control set. No realtime 3D. Test laptop sizing, Retina, keyboard, contrast/non-color cues, reduced motion and CPU.

## P5 — Performance and refinement (Jan 14)

Essential MIDI learn, navigation, saved maps, actual hardware testing (Launch Control XL, BeatStep, Keystep). High: TOUCH, stable MIDI mapping of connection depth. Stretch: footswitch, MIDI clock. Mouse equally usable. Persist only stations, controls, routes/depth, MIDI and generic source settings, not captured LOOP audio, recordings, sensor history or delay contents. Restore LOOP empty; configuration only. Restore gracefully, no unsolicited permission or network surprise.

Two owner 15-minute sessions, matched ENGINE/DRY/REFERENCE, MIDI and ROOM evidence, unfamiliar musicians if feasible. First minute compelling sound, understandable tuner/patch, meaningful WORLD causality, contrasting states, recovery, voluntary return. Separate subjective and technical verdicts.

**Hard feature freeze January 14.** Fixes, musical tuning, usability and release only afterward.

## P6 — Field Guide (Jan 21)

Illustrated creative experiments rather than dry parameter reference: quick start, tuner, patch/depth, WORLD, MIDI, safety/recovery. Recipes only if supported: Between Stations, Ghost Conversation, Magnetic Cloud, Room Creature, Weathering, Broken Transmission, Self-Exciting Radio, Two Stations Arguing, Caught Transmission (short loop against retuned radio). No prerecorded outcomes.

## P7 — Release qualification (Jan 21–24)

Public README with screenshots/video, manifesto, roadmap, field guide, architecture and transparent evidence/reuse disclosures. Complete license/notices/provenance, reproducible builds, signed/notarized conventional AU and standalone; AUv3/VST3 only if separately qualified. AUv3 containing app/extension packaging, entitlements, host discovery, multiple instances and IPC validation if included. Logic Pro, auval and applicable pluginval/host validation, clean-machine install, explicit supported architectures. Host transport, bypass, rates/buffers, state, offline bounce, helper permissions and lifecycle. Live broadcast cannot reproduce identically offline; define behavior. No hidden recording. Verify Freqlab build/upload and signing early.

## P8 — Three-minute performance film (Jan 24)

Fennesz-inspired intimate performance, not feature checklist. Show qualifying plugin in DAW; standalone may also appear. Use only qualified features and audio/visual material with promotional rights; own temporary live stream with original/cleared content is an option.

| Time | Scene |
| --- | --- |
| 0:00–0:20 | The world is already playing: broadcast becomes musical |
| 0:20–0:50 | Tune and transform: detune, hop, static |
| 0:50–1:20 | Discover: patch and shape sound |
| 1:20–1:50 | Connect the physical world: ROOM and phone if qualified |
| 1:50–2:45 | Play: compelling music |
| 2:45–3:00 | Let go: decay, logo |

## Scope and decision gates

Essential: distinctive voice, live tuner/static/detune, semi-modular audio/CV, independent depth, ROOM, MIDI, polished original UI/display, safe feedback/recovery, configuration persistence, signed/notarized contest-eligible plugin, source and demo.

High: tiny Tapestry-derived LOOP with volatile capture/chop/mangle, Rings early audition, selected Korupt/AFTERIMAGE/Wyrd/Drift candidates (not guaranteed inclusions), auxiliary mixer/feedback, follower/gate, SENSE, environmental hop, iPhone bridge, ATMOSPHERE, AUv3 feasibility and additional qualified macOS format.

Stretch: second radio, custom phone companion, advanced spectral processing, general clock/trigger, VST3 if time. Post-contest: Windows, iPadOS/mobile AUv3, broader sensing, external SDR/pickups and hardware.

Critical risks: unproven musical voice, licensing/provenance, plugin service lifecycle, complex interface, holidays and audition availability, broadcaster terms and signing/packaging. Keep work staged in draft PRs. Review at each listening gate; no unsupported pass, no merge/release without approval. Roadmap is living planning direction, not a claim of finished capabilities.
