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
