# WILDBAND plugin — explicit RECEIVE

**Branch:** `p1/plugin-receive`, on `p1/plugin-lifecycle` (PR #17). **Status:** contract and acceptance criteria
committed before the change; results appended at the bottom. Supersedes the authorisation part of
`PLUGIN-RECEIVER-LIFECYCLE.md`: inferred intent (1.5 s of real-time rendering) **no longer authorises a network
connection.** Only an explicit RECEIVE does.

## Contract (owner's, 2026-11-27)

- Construction, scanning, `prepareToPlay`, state restoration and passive real-time processing never start a stream.
- **RECEIVE is off by default.** Only an explicit gesture turns it on: the RECEIVE button in the (temporary) editor, or
  a MIDI control the user has deliberately mapped to RECEIVE.
- RECEIVE is **runtime state**: never saved in, nor restored from, a preset or project.
- Closing the editor does not stop an activated receiver. Stopping the transport does not stop it while the host keeps
  processing in real time (monitoring).
- **Suspension** (host stops processing, or switches to offline) releases the network but keeps the intent within the
  instance; **resume** reconnects only if this instance was explicitly activated.
- **Revocation**: RECEIVE off, state restoration, destruction and PANIC turn RECEIVE off and cancel any request,
  including one in flight.
- Offline rendering never starts a connection.
- A deliberate retune (TUNE moved while receiving) keeps the buffer-driven tuner static; connecting after RECEIVE,
  and reconnecting after a suspension, buffer without static.
- **No artificial delay**: if the host is already processing in real time, RECEIVE connects at once (next timer tick).
- The ten host parameters and their frozen AU order are unchanged. RECEIVE is not a host parameter (a host parameter
  would be saved, restored and automatable).

## Design

Two independent questions, two parts:

| Part | Question | Who changes it |
|---|---|---|
| `ReceiverControl::Authorisation[i]` | Has the user asked receiver *i* to receive? | Only RECEIVE on/off, PANIC, restore, destruction |
| `ReceiverControl::Host` | Is the host processing in real time right now, so a connection is useful? | `prepareToPlay`, `releaseResources`, audio callbacks, offline flag |

A receiver holds a connection **iff** it is authorised **and** the host is live. "Host live" = prepared for real time,
not offline, an audio callback within the last 0.5 s (no waiting window), and not in the 5 s idle or 0.5 s release
grace states inherited from PR #17. Written for an array of receivers; the plugin uses receiver 0 until receiver B exists.

### Thread ownership (audit result)

| Datum | Writers | Readers | Before | After |
|---|---|---|---|---|
| Sample rate used by the timer | `prepareToPlay` (host thread, unspecified) | message-thread timer | **plain `double` — a data race** | `std::atomic<double>` |
| Prepare / release events | host threads | timer | small queue under a `SpinLock`, also taken by a host thread that may be the audio thread | **wait-free**: atomic counters and flags (`prepared`, `preparedOffline`, `releaseCount`, release time); the timer derives transitions |
| Rendered samples, last callback time, offline flag | audio thread | timer | atomics | unchanged |
| RECEIVE requests | editor (message thread), MIDI (audio thread, via `processBlock`) | timer | — | atomic request counter + desired value; never acted on from the audio thread |
| Revocation from `setStateInformation` / destructor / PANIC | host threads, message thread | — | — | under `receiverLock` (a `CriticalSection` never taken by the audio thread), which the timer also holds while it decides and issues receiver requests — so a timer that read "authorised" cannot open a connection after a revocation |
| Station requests (`Radio::tune` / `resume`) | timer, revokers | receiver thread | already lock-protected inside `Radio` | always under `receiverLock` first, then `Radio`'s lock (one order everywhere) |

### MIDI

A control is mapped to RECEIVE deliberately: editor → LEARN, then move a control (CC or note). The mapping is
configuration and is saved; **activation is not**. A mapped control toggles RECEIVE on a press (CC rising through 64,
or note-on). MIDI CC 120 (All Sound Off) acts as PANIC. A test aid, `WILDBAND_TEST_RECEIVE_CC=<n>`, maps CC *n* to
RECEIVE at construction so the headless harness can make the gesture; it maps, it does not activate.

## Acceptance criteria (registered before implementation)

Unit (`wildband_tests`): RX-01 nothing is authorised by default, by restore, or by any amount of real-time host
activity; RX-02 authorised + live = connect without delay; RX-03 suspension keeps intent, resume reconnects; RX-04
revocation from every cause clears intent; RX-05 offline never connects; RX-06 receivers are independent (A/B ready).

Bundle (`wildband_plugin_check` + counting server; both formats):

| ID | Passes when |
|---|---|
| RX-10 | Scan-like, prepare/release, fast rendering, offline rendering, restore: 0 connections |
| RX-11 | **A monitored project rendering in real time for 10 s without RECEIVE: 0 connections.** Positive control: the PR #17 build, same scenario, makes ≥ 1 |
| RX-12 | RECEIVE (mapped CC) while rendering: one connection, opened within 0.6 s of the gesture |
| RX-13 | Host stops calling back (transport stopped and not monitoring) for 6 s: closed; callbacks resume: reconnects without a new gesture |
| RX-14 | RECEIVE off: closed; no new connection over 3 s of rendering |
| RX-15 | Five on/off cycles: five opens, five closes |
| RX-16 | Restore a state into an instance that is receiving: closed, RECEIVE off, no reconnection over 3 s |
| RX-17 | In flight: RECEIVE on toward a server that never answers, then off / restore / destroy while connecting: each returns promptly, no later connection |
| RX-18 | Offline: RECEIVE gesture during an offline render: 0 connections |
| RX-19 | Two instances: RECEIVE on one → exactly one connection; on both → two; destroying one closes only its own |
| RX-20 | PANIC (CC 120) while receiving: closed, RECEIVE off |
| RX-21 | Unchanged: `auval` succeeds (its network evidence still proves nothing, see PR #17); frozen parameter order; the general headless checks |

Not established by any of this: what Logic and Live do; how the temporary editor feels.
