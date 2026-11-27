# WILDBAND — P0 Research & Feasibility Gates

**State:** NOT STARTED. This plan is evidence-seeking, not permission to implement a full product.

## Gate G0 — Define the instrument

Write and accept a single-sentence musical promise and non-goals. Validate product research competitors (e.g. internet radio sampling and radio-granular applications) before stating novelty. Recording is explicitly not part of P0.

## Gate G1 — Source feasibility

A standalone macOS spike successfully receives audio from two independently operated, compatible Icecast/Shoutcast stream URLs; report codecs, redirects, startup/re-tune behavior, gaps and failure recovery. Do not claim all streams work or that internet station directory search is implemented.

## Gate G2 — Environmental sensing

Microphone activity with explicit permission drives a smooth, bounded control signal. Document outcome of permission denial and absence. Investigate BLE and Wi-Fi only as technical options; report actual tested macOS behavior, not API assumptions.

## Gate G3 — Musical engine

Implement one safe resonant or nonlinear network. Compare (a) dry radio, (b) generic filter/delay, (c) proposed coupled engine. Document listening evidence across speech, music and noise streams. Prove meaningful influence from a performer gesture and an environmental source.

## Gate G4 — Performance interface

Create a *functional* JUCE placeholder UI and hardware MIDI mapping. Owner performs two unrecorded 15+ minute sessions, logging observations about responsiveness, learnability, surprise and whether they want to play again. A Blender-rendered interactive knob integration spike may be done here; polished full interface comes later.

## Gate G5 — Reliability and scope

Test internet loss, source switches, mic permission denied/revoked, system sleep/wake, audio output switching, Apple Silicon CoreAudio buffer/rate modes, feedback limiting and audio-thread scheduling. Record repeatable evidence. Decide standalone vs plugin qualification and any added environmental features only after earlier gates.

## Deliverables

- Evidence log with machines, OS version, sample rates, network and permission conditions.
- Source-protocol and codec compatibility matrix.
- Musical experiment report and honest owner listening notes.
- P0 design proof with one Blender-rendered interactive control.
- Go / revise / stop decision, without automatically escalating to full implementation.

## Risks

Audio stream copyrights and terms; station availability and latency; network/codec complexity; permissions; DSP instability; shallow gimmick design; too many environmental sensors; recording-first scope creep; and a polished design that hides an uninspiring interaction.

The January 2027 contest is a **deadline constraint**, not a reason to weaken the musical product concept. Confirm current official rules separately before submission decisions.
