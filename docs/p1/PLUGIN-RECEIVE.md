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

### Thread ownership (final implementation, reconciled after review)

PR #17 had a race (a plain `double` sample rate shared by `prepareToPlay` and the timer) and a `SpinLock`
event queue a host thread could take. Both are gone.

| Datum | Writers | Readers | Mechanism |
|---|---|---|---|
| Sample rate | — | — | no longer shared: the timer does not need it (the PR #17 race is removed, not wrapped) |
| Prepared / prepared offline / release time (`prepared`, `preparedOffline`, `releasedAtMs`) | `prepareToPlay`, `releaseResources` (host threads, unspecified) | timer | `std::atomic` stores only; the timer derives the host state from them (`ReceiverControl::updateHost`) |
| Last callback time, offline now (`lastCallbackMs`, `offlineNow`) | audio thread | timer | relaxed atomics, written at the end of each block |
| MIDI RECEIVE toggles and PANIC (`ReceiveRequests`) | audio thread, once per block | timer | **one** atomic word: PANIC flag + toggles counted after the latest PANIC. The audio thread never acts on it. The timer takes it, and restore / explicit on/off / PANIC discard pending toggles, only under `receiverLock` |
| RECEIVE mapping and LEARN (`ReceiveMapping`) | LEARN capture (audio thread), restore and test aid (any non-audio thread), LEARN arm (editor) | audio thread, editor, `getStateInformation` | **one** atomic word: CC, note, LEARN flag. A restore stores a whole word, so it always wins; a capture uses compare-and-swap and lands only while LEARN is armed |
| Authorisation and connection state (`ReceiverControl`, `connected`, `tuned`) | timer, editor (`setReceive`), restore, destructor, PANIC | timer, editor status | under `receiverLock`, a `CriticalSection` the audio thread never takes. The timer holds it while it applies MIDI requests, decides and issues station requests, so nothing authorised before a revocation can connect after it |
| Station requests (`Radio::tune` / `resume`) | timer, revokers | receiver thread | always `receiverLock` first, then `Radio`'s own lock (one order everywhere); never called from `processBlock` |

### MIDI

A control is mapped to RECEIVE deliberately: editor → LEARN, then move a control (CC or note). The mapping is
configuration and is saved; **activation is not**. A mapped control toggles RECEIVE on a press (CC rising through 64,
or note-on). MIDI CC 120 (All Sound Off) acts as PANIC, and cannot itself be mapped. A test aid,
`WILDBAND_TEST_RECEIVE_CC=<n>`, maps CC *n* to RECEIVE at construction so the headless harness can make the gesture.
It maps; it does not activate.

### Authorisation ordering (narrow audit after the PR #19 review)

The review found four gaps in the first version. The rules below close them; each is tested (RX-07, RX-08,
RX-22..RX-25):

1. **PANIC outranks toggles.** A PANIC anywhere in an audio block discards every RECEIVE toggle in that block,
   whatever their order within it, and every toggle still pending from earlier blocks. Toggles in later blocks are
   new gestures and count. The precedence is deterministic because the audio thread publishes one word per block.
2. **Restore and explicit RECEIVE off/on supersede pending toggles.** They discard them under `receiverLock`, the
   lock under which the timer takes requests. A toggle made before a restore, PANIC or explicit setting can therefore
   never re-grant RECEIVE after it. (Before the fix, the timer ran PANIC and then applied pending toggles, and toggles
   survived a restore.)
3. **Offline MIDI cannot authorise.** While the host renders offline, the mapped control is consumed but never
   counts, and LEARN does not capture. An offline bounce that plays the mapped control neither connects (RX-18) nor
   leaves an authorisation behind for the next real-time pass (RX-24). PANIC still counts offline; it can only take
   RECEIVE away.
4. **Restore replaces the mapping.** A state's `receiveCc` / `receiveNote` become the mapping. A state without these
   fields (saved before RECEIVE existed, or not a WILDBAND state) leaves **no** mapping. Either way a pending LEARN is
   cancelled, and a LEARN capture still in progress cannot land after the restore.

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

**Added after the review** (ordering audit; not pre-registered, so each has a negative control against the
first version):

| ID | Passes when |
|---|---|
| RX-07 (unit) | `ReceiveRequests`: PANIC in a block outranks the block's toggles in either order; earlier pending toggles are discarded by a PANIC block, a restore or an explicit setting; a pending PANIC survives `discardToggles`; toggles after a PANIC block count |
| RX-08 (unit) | `ReceiveMapping`: restore without fields clears the mapping and LEARN; a capture after a restore cannot land; one capture per LEARN; out-of-range values (CC 120, note 200) map nothing |
| RX-22 | From RECEIVE off, a press and CC 120 in one block, in either order: 0 connections |
| RX-23 | A press in one block, then before any timer pass a CC 120 block or a state restore: 0 connections over 3 s |
| RX-24 | A press during an offline render, then 3 s in real time: 0 connections; a real-time press afterwards: 1 |
| RX-25 | Restoring a pre-RECEIVE state (`tests/fixtures/pre-receive-state`, saved by the PR #17 build) into an instance mapped to CC 102: that CC does nothing over 3 s; restoring a current state brings the mapping back (1 connection) |

Not established by any of this: what Logic and Live do; how the temporary editor feels.

## Results (2026-11-27, this branch, Apple Silicon, macOS 26)

Commands: `build/wildband_tests`, `tools/run_plugin_lifecycle_check.sh build`, `tools/run_plugin_lifecycle_check.sh
--control <PR #17 WILDBAND.vst3> build` (with the PR #17 AU installed), `build/.../wildband_plugin_check vst3=...`,
`tools/run_tuner_check.sh build`.

| Check | Result |
|---|---|
| RX-01..RX-08 | pass (part of 972 engine checks, 0 failed; RX-07/RX-08 added after the review) |
| RX-22..RX-25, VST3 and AU (ordering audit) | 16 of 16 pass. **Negative control:** the same harness against the first version's `Plugin.cpp` (79f0886) fails 14 of 16. RX-22 (both orders), RX-23 (PANIC and restore), RX-24 and RX-25 each opened a connection there; the 2 that pass there are RX-25's positive half. Full RX run after the fix: 52 of 52 plus auval (docs/p1/evidence/plugin-receive-check.txt) |
| RX-11 positive control (PR #17 build, same scenario) | VST3 1 connection, AU 1 connection: the scenario detects a build that connects without RECEIVE |
| RX-10..RX-20, VST3 and AU | 36 of 36 pass (first version; still passing after the ordering fix). RX-11: 0 connections over 10 s of real-time rendering with notes played. RX-12: first connection 0.02-0.03 s after the gesture. RX-13: closed 5.3 s after callbacks stopped, reconnected on resume with no gesture. RX-16 restore took < 1 ms and closed the stream. RX-17 off / restore / destroy mid-connect: no later connection, destroy < 10 ms. RX-19: one connection per receiving instance, destroying one closed only its own |
| RX-21 | auval: AU VALIDATION SUCCEEDED; 0 connections counted, 0 remote TCP seen. As in PR #17 this zero proves nothing: auval pumps no message loop, so the timer that makes requests never runs there |
| General headless checks | 24 of 24 (frozen order in both formats). The 6 s real-time check now makes the RECEIVE gesture and then reaches the default (real) stations |
| Tuner real-time checks | 28 of 28, including RL-23 (resume is not a retune) |

Not established: behaviour in Logic, Live or any other DAW; how the temporary editor and LEARN feel in use; VST3
validator; signing and notarisation; whether a host under App Transport Security allows the plain-http stations.
