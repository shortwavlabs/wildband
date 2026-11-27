# WILDBAND plugin — radio receiver lifecycle

**Branch:** `p1/plugin-lifecycle`, on `p1/plugin-proof`. **Status:** contract and acceptance criteria committed before
the change; results appended at the bottom.

## The defect

The first plugin proof started fetching the default station about 250 ms after the plugin object was **constructed**.
A host scanning its plug-ins, `auval`, a preset browser or a project that merely loads would each open a connection
to a third-party server. No external request may come from passive scanning or construction.

## Why `prepareToPlay` is not the signal

Hosts call `prepareToPlay` while scanning and validating (`auval` initialises and renders), when loading a project
whose track is never played, and before offline bounces. None of these is a decision to listen to a live station.

## Contract

The receiver has four states, decided on the message thread from what the audio thread reports (no locks, no
network work on the audio thread):

| State | Network | Entered when |
|---|---|---|
| **DORMANT** | none | construction, state restore, `releaseResources`, destruction |
| **ARMED** | none | `prepareToPlay` in real-time mode |
| **ACTIVE** | one stream, the station under TUNE | audio has been rendered **in real time** without a break for at least **1.5 s**: rendered audio time and wall-clock time agree within a factor of two over that window, with no gap between callbacks above 0.5 s |
| **SUSPENDED** | none (stream cancelled) | ACTIVE, then no audio callback for **5 s** (stopped engine, suspended or bypassed plugin, host in the background), or a switch to offline rendering |

- SUSPENDED → ACTIVE again by the same real-time rule; the selected station and all settings are kept.
- **Offline render** (`isNonRealtime()`): the receiver is never activated by it. A live session that switches to
  offline is suspended; the render contains the instrument's own response to whatever is already in the engine and
  ring (tails, then silence) — a live broadcast cannot be rendered faster than it arrives. Documented, not hidden.
- **Restoring state** sets the station and knobs; it never connects.
- **Cancellation** of an in-flight connection on any transition out of ACTIVE; **teardown** cancels, joins the
  receiver thread and leaves no connection open.
- The tuner static behaves as in the standalone, but only while ACTIVE (a dormant plugin has nothing to wait for).
- Test aid, compiled in but inert unless set: environment variable `WILDBAND_TEST_STATIONS` (comma-separated URLs)
  replaces the default station list, so a local server can count connections. It never adds network activity.

The rule is implemented in a JUCE-free class (`src/ReceiverGate.h`) so it can be unit-tested exactly.

## Acceptance criteria (registered before implementation)

Unit (`wildband_tests`, deterministic):

| ID | Passes when |
|---|---|
| RL-01 | Construction, prepare, restore: DORMANT/ARMED, never ACTIVE |
| RL-02 | Faster-than-real-time rendering (any amount): never ACTIVE |
| RL-03 | Real-time rendering for 1.4 s: not ACTIVE; for 1.6 s: ACTIVE |
| RL-04 | A gap above 0.5 s restarts the 1.5 s window |
| RL-05 | Offline rendering in real time: never ACTIVE; a switch to offline suspends |
| RL-06 | ACTIVE, then 5 s without callbacks: SUSPENDED; resumes only by the real-time rule |
| RL-07 | `releaseResources`: DORMANT at once |

Bundle level (`wildband_plugin_check` with `tools/stream_test_server.py --log`, which records every connection
opened and closed):

| ID | Passes when |
|---|---|
| RL-10 | Scan-like use — construct, query, destroy (both formats, three times): **0 connections** |
| RL-11 | Construct, prepare, release without rendering: 0 connections |
| RL-12 | Construct, prepare, render 4 s of audio as fast as possible (as validators do): 0 connections |
| RL-13 | Offline rendering, paced in real time, 4 s: 0 connections |
| RL-14 | Restore a saved state: 0 connections |
| RL-15 | Real-time rendering: exactly **one** connection, opened 1.5–2.5 s after rendering starts; static, then the station's audio |
| RL-16 | Stop rendering: the server sees the connection closed within 6 s; render again in real time: one new connection |
| RL-17 | `releaseResources` while ACTIVE: closed within 1 s. Destroying the plugin while ACTIVE: closed within 1 s, no connection left open |
| RL-18 | `auval` with the counting server's station list in its environment: 0 connections; `lsof` shows no remote TCP connection from the `auval` process during validation |
| RL-19 | Unchanged: `auval` passes; the frozen hosted parameter order; the existing headless checks |

Not established by this, still owner/host tasks: what Logic and Live actually do when scanning, loading and playing.

---

## Results (added after implementation, 2026-11-27)

**What a unit test, the headless bundle check and `auval` established. Nothing was tried in a DAW.**

Implementation: `src/ReceiverGate.h` (the rule, no JUCE) and `src/Plugin.cpp` (the audio thread only records how much
it rendered and when; the 30 Hz message-thread timer drains prepare/release events, updates the gate, requests the
station only while ACTIVE, and cancels the stream on every transition out of ACTIVE and in the destructor).
`tools/stream_test_server.py --log` records every connection; `tools/run_plugin_lifecycle_check.sh` runs the
bundle checks and `auval` against it.

| ID | Result | Measured |
|---|---|---|
| RL-01 … RL-07 | **Pass** | `wildband_tests` (950 checks with Rings configured) |
| RL-10 | **Pass** | 0 connections, both formats, three scan-like cycles each |
| RL-11 | **Pass** | 0 |
| RL-12 | **Pass** | 0 |
| RL-13 | **Pass** | 0 (VST3 offline process mode; AU offline-render property, set directly — see below) |
| RL-14 | **Pass** | 0 |
| RL-15 | **Pass** | exactly 1 connection, opened 1.8 s after real-time rendering began (both formats); sound in the last second |
| RL-16 | **Pass** | closed 5.3 s after rendering stopped; one new connection on resuming |
| RL-17 | **Pass** | release: closed 0.3–0.5 s later; destroy: closed 0.3 s later; nothing left open |
| RL-18 | **Pass** | `auval` (with the counting server's stations in its environment): 0 connections to the server; no remote TCP connection seen from the `auval` process |
| RL-19 | **Pass** | `auval` succeeds; the general headless check (24 checks, frozen hosted parameter order unchanged) passes; 27 real-time tuner checks pass |

Full output: [`evidence/plugin-lifecycle-check.txt`](evidence/plugin-lifecycle-check.txt),
[`evidence/plugin-check.txt`](evidence/plugin-check.txt), [`evidence/auval.txt`](evidence/auval.txt).

**Found by these checks and fixed:**

1. On `releaseResources` the stream was **not** cancelled: the timer looked at "was it active?" after the release
   event had already made it dormant, so the transition went unnoticed and the connection stayed open (it closed
   only when the plugin was destroyed). The check is now taken before events are applied.
2. JUCE's AU *hosting* code does not pass `setNonRealtime` on to an Audio Unit, so the first AU offline test was
   not offline at all. The check now sets the AU's own `kAudioUnitProperty_OfflineRender`, as Logic does for a bounce.
   The plugin side (JUCE's AU wrapper) already honours that property.

**Limits of this evidence:**

- The rule is inferred, not confirmed against real hosts: what Logic and Live do while scanning, loading a project
  and playing must still be observed (the counting server and `WILDBAND_TEST_STATIONS` make that easy: point the
  stations at it, launch the host, count).
- A host that renders "in real time" for 1.5 s without the user meaning to play (for example a long live preview
  while browsing) would activate the receiver. That is the price of not having an explicit control; an explicit
  "receive" switch in the future editor would remove the guess.
- Suspension after 5 s without callbacks means a stopped transport that keeps calling `processBlock` (as most hosts
  do) keeps the stream: that is intended — the instrument stays live while the track is armed or monitored.
