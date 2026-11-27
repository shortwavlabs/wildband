# P0 — Owner first audition and approved follow-up

**Date:** 2026-11-27. **Auditioned baseline:** post-microphone-fix prototype, PR #3 head `9c0a40900d084dc006398fcf443ac18d4138ffe6` at handover. Exact installed binary hash not supplied. **Verdict: encouraging first impression; REVISE the voice and modulation ergonomics; formal P0 acceptance remains open.**

## Owner response (verbatim)

| Question | Response |
| --- | --- |
| Does it sound interesting? | Yes |
| Does it feel like an instrument rather than a radio effect? | Yes |
| Can you intentionally create different musical textures? | Somewhat |
| Does patching encourage experimentation? | Yes |
| Does the outside world meaningfully influence the sound? | Somewhat |
| Do you want to keep playing? | Yes |

> ok this is fun and a good starting point. one feature i find interesting that claude added is the off centre when you rotate the station selection knob; the audio starts degrading in a musical manner. the resonator isn't particularly expressive. also i think we should add ways to control how much a source like the room influence the destination; like an attenuverter in a modular rack. but this is very promising!!

Owner subsequently approved the proposed next steps: “next steps approved; go ahead :)”.

## Interpretation and gate limits

- The owner reports the instrument is fun, patching invites exploration, and the off-centre TUNE degradation has musical value. Preserve this successful behavior.
- The resonator is not especially expressive; do not mistake concept enthusiasm for preference over REFERENCE. Explore stronger candidate voices instead of committing to the present resonator.
- Environmental influence is promising but only somewhat convincing; per-connection depth and inversion become an immediate playability priority.
- Session duration, exact station set, ENGINE/DRY/REFERENCE comparisons, controller used, microphone input/permission, headphones, glitches and two-session protocol are not specified. Do not infer them. G1/G2/G3/G4/G5 remain open; G0 creative direction is supported by the manifesto but licensing still undecided. Historical automated/muted evidence remains unchanged.

## Approved next steps (sequenced)

### A — Close remaining baseline evidence

Keep PR #3 draft and preserve the tested baseline. Re-run the post-helper CoreAudio timing sweep with verified live-radio activity, ROOM off/on where hardware and explicit consent permit, positive late-callback control, and known sample rates/buffers. Test output switching, input choice, helper lifecycle/permission denial, revoke/unplug, sleep/wake, network loss and long sessions. Keep actual owner/manual actions explicit. Provide a short targeted checklist for two 15-minute sessions, MIDI Learn with the Launch Control XL, music/speech/noise and matched ENGINE/DRY/REFERENCE. Separate automated, physical-hardware and listening evidence. No merge or formal gate pass without owner review.

### B — Per-connection modulation depth (first interaction iteration)

Create an isolated follow-up branch from the recorded prototype/docs integration baseline. Do not overwrite the auditioned build or rewrite PR #3 evidence.

- Each continuous CV cable has independent signed amount in [-1,+1]. Zero means neutral/no modulation, +1 reproduces the original cable behavior, -1 reverses its influence relative to the unmodulated parameter. One source fanned out to multiple destinations has independent depths. Retain global COUPLE as master; depth multiplies the normalized source before destination-specific mapping. No source rebiasing by default; zero/stale/denied ROOM stays neutral at every depth.
- First iteration retains typed audio/CV and one cable per destination; no general mixer/fan-in or new trigger type required. Audio cable semantics unchanged. Existing port IDs and patches remain compatible: missing depth defaults to +1. Persist configuration only; validate range/non-finite values on restore.
- Prototype cable selection with a clearly associated bipolar DEPTH control, center-zero reset, signed value and route label; preserve cable dragging/disconnect and readable hit targets. No permanent bank of tiny attenuverters. Consider MIDI Learn for selected route only after stable, unambiguous assignment semantics; no accidental retargeting.
- Existing `src/Patch.h` packs seven source bytes into a single atomic uint64. Adding depths changes this capacity contract. Choose a bounded coherent realtime handoff (inspect TALORA's existing snapshot approach; no copying without rights review), avoid independently published inconsistent source/depth pairs, no blocking/allocations in callback. Smooth signed effective CV on route or depth changes.
- Audit **every destination** for negative modulation. Existing equations assume positive inputs; in particular STRIKE excitation, DRIFT rate and FEEDBACK loop gain need explicit valid ranges. Retain +1 legacy behavior and zero neutral, avoid negative excitation/gain or invalid filter/rate unless intentionally designed and qualified. Bound all feedback/output; PANIC must remain authoritative.
- Register tests before implementation: default and migrated patch parity; signed depth endpoints and zero; independent fan-out; global COUPLE=0; unavailable sources; rapid edits/concurrent handoff; malformed state; numerical and block-size safety; click-free changes, no allocation, current graph/TALORA topology parity. Benchmark worst-case callback with live inputs; owner audition ROOM influence at 0, subtle positive, full positive and negative depths. Do not claim sound quality from automated tests.

### C — Playable tuner follow-up

Protect the exact existing off-centre detuning sound with regression fixtures and owner A/B. Read the newer docs-branch `RADIO-TUNER-PERFORMANCE.md`. Prototype manual NEXT/PREV and preset selection, MIDI-ready mapping, and bounded synthesized static spanning actual connecting/buffering states. Fast readiness must not be delayed for a fake transition. Retune request epochs/cancellation, short safe fades, honest FAILED status and finite fallback, no feedback-state reset except explicit safety/clear. Keep detune and buffering texture distinct and complementary. Automatic environmental hopping remains later research. Test static off, existing detune vs modified transition and slow/stalled/rapid switches; audition before committing.

### D — Comparative voice exploration

Use a separate experiment branch, retain existing ENGINE/DRY/REFERENCE as baseline. Review `INSTRUMENT-DESIGN-MANIFESTO.md`, `WYRD-REUSE-RESEARCH.md`, `AFTERIMAGE-DSP-REUSE.md`, `FIELD-KIT-INSPIRED-UTILITIES.md`, and Korupt issue #2 on the docs branch. First compare narrow candidates (Korupt nonlinear/PLL, AFTERIMAGE live granular delay, Wyrd delay/space); Drift is CV movement rather than a competing audio voice. Start with smallest feasible primitives, inspect source/license and prior failed realtime gates, no blanket transplant. Use same lawful local fixtures and comparable live sources, matched listening levels, actual environmental CV on/off, report cost, safety, expressiveness and owner preference separately. Keep successful tuner and patch gestures common across candidates. Select one cohesive musical direction rather than adding all engines. No production faceplate or broad feature implementation until listening supports the control set.

**Owner review checkpoints:** baseline evidence → depth audition → tuner audition → candidate voice decision. Scope is approved for staged exploration, not release, merge or claim of qualification.
