# WILDBAND plugin — radio receiver lifecycle

**Branch:** `p1/plugin-lifecycle`, on `p1/plugin-proof`. **Status:** contract and acceptance criteria committed before
the change; results appended at the bottom.

> **Superseded in part** by [PLUGIN-RECEIVE.md](PLUGIN-RECEIVE.md) (`p1/plugin-receive`): real-time rendering no longer
> authorises a connection; only an explicit RECEIVE does. The evidence below describes the PR #17 build as it was.

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

## Results (added after implementation, 2026-11-27; revised after review the same day)

**What unit tests, the headless bundle check and `auval` established. Nothing was tried in a DAW.**

Implementation: `src/ReceiverGate.h` (the rule, no JUCE) and `src/Plugin.cpp` (the audio thread only records how much
it rendered and when; the 30 Hz message-thread timer drains prepare/release events, updates the gate, requests the
station only while ACTIVE, and cancels the stream on every transition out of ACTIVE and in the destructor).
`tools/stream_test_server.py --log` records every connection; `tools/run_plugin_lifecycle_check.sh` runs the bundle
checks and `auval` against it, with the plugin's stations pointed at it.

### Changes made during qualification (each found by a check)

1. **`releaseResources` did not cancel the stream** — the timer asked "was it active?" after the release event had
   already been applied. Fixed (asked first).
2. **A host re-initialising a live plugin dropped and redialled the station, with tuner static.** JUCE's AU hosting
   does `release` + `prepare` back to back on a re-prepare (an AU host changing buffer size does the same), and
   `prepareToPlay` also reset the "last callback" time, which looked like silence. Now: the last-callback time
   survives a prepare; a release while ACTIVE takes effect after a **0.5 s grace period** unless a real-time prepare
   arrives first; and reconnecting to the station that was playing (after construction, a suspension or a release)
   uses `Radio::resume`, which buffers **without tuner static** and is not counted as a station request. Moving TUNE
   while live is still a retune, with static. Consequence: the release limit in RL-17 was raised from 1.0 s to
   1.5 s (grace + one timer tick + the test server noticing the closed socket).
3. **A playlist station (`.pls`, `.m3u`) held two connections for the whole session** — the stream was opened from
   inside the frame that still owned the playlist request. Measured with `netstat` on the receiver probe: 2 for
   SomaFM's `.pls` (HTTPS and HTTP), 1 for a direct stream. Fixed by closing the playlist request first: **1** for
   all three afterwards. (Relevant to the two-session limit in PR #12; affects the standalone as well.)
4. JUCE's AU hosting does not forward `setNonRealtime`; the check sets the AU's own `kAudioUnitProperty_OfflineRender`,
   as Logic does for a bounce.

### Results

| ID | Result | Measured |
|---|---|---|
| RL-01 … RL-09, RL-07b | **Pass** | `wildband_tests`: 958 checks with Rings configured. Includes: offline prepare stays dormant until a real-time prepare (RL-08); re-prepare while active stays active (RL-09); release + prepare back to back stays active, but not into offline (RL-07b) |
| RL-10 | **Pass** | 0 connections, both formats, three scan-like cycles each |
| RL-11 … RL-14 | **Pass** | 0 connections each (prepare/release; 4 s rendered as fast as possible; offline rendering paced in real time — VST3 process mode, AU offline property; restore) |
| RL-15 | **Pass** | exactly 1 connection, opened 1.8 s after real-time rendering began; sound in the last second |
| RL-16 | **Pass** | closed 5.3 s after rendering stopped; one new connection on resuming |
| RL-17 | **Pass** | release: closed 0.8 s later (with the 0.5 s grace); destroy: 0.3 s; nothing left open |
| RL-20 (added) | **Pass** | `prepareToPlay` again while live: 0 opened, 0 closed (both formats) |
| RL-21 (added) | **Pass** | `release` + `prepare` back to back while live: stream kept (both formats) |
| RL-22 (added) | **Pass** | mid-connect to a server that never answers: `releaseResources` returns at once; destruction in 5–9 ms |
| RL-23 (added) | **Pass** | `Radio::resume`: LIVE without tuner static, request count unchanged (`wildband_tuner_check`, now 28 checks) |
| RL-18 | **Passes, but proves nothing** | see below |
| RL-19 | **Pass** | `auval` succeeds; the general headless check (24 checks, frozen hosted parameter order unchanged) passes |

### RL-18 has no working positive control

`auval` with the counting server's stations in its environment saw 0 connections, and `netstat` saw no remote
connection from the `auval` process. **But the same test with the old PR #15 AU — which does connect shortly after
construction — also saw none.** `auval` loads the AU in its own process (checked), yet it does not run a message loop,
and the receiver is driven from the plugin's message-thread timer: under `auval` neither the old nor the new build
ever reaches the code that connects. So RL-18 cannot tell the fix from the defect. (The first attempt used `lsof`,
which turned out to be blind to these connections altogether: macOS's user-space networking stack does not show up
there. `netstat -anv`, which lists the owning process, does.)

The evidence for "no network from scanning, construction or restore" is therefore **RL-10 … RL-14**, which pump the
message loop so that the timer runs; RL-15 … RL-17 show the same harness does see connections when they happen.
What real hosts do while scanning remains to be observed in Logic and Live.

### Limits of this evidence

- The rule is inferred, not confirmed against real hosts.
- **"Restore never connects" holds only while nothing renders.** A project that opens onto a live, monitored track
  renders in real time and so connects after 1.5 s — that is the heuristic working as designed, but it means opening
  a project can make a network request without a deliberate gesture. An explicit RECEIVE switch in the future editor
  would remove the guess. **Owner decision.**
- A stopped transport that keeps calling `processBlock` (as most hosts do) keeps the stream: intended (the instrument
  stays live while monitored).
- The first connection after construction now buffers without static (it is a start, not a retune). The standalone
  still plays static on its first connection.
