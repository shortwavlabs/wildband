# WILDBAND P0 — Baseline qualification, second pass (after the microphone-helper fix)

**Date:** 2026-11-27. **Build:** `p0/prototype`; the engine, radio receiver and helper are those the owner
auditioned (`9c0a409`). Commits since then add measurement and test flags to the app only (`--measure` columns,
`--log`, `--switch-test`); no DSP, routing or helper code changed. Earlier evidence in `EVIDENCE-LOG.md` is left as
it was. **Every run here was muted. None of this is listening evidence.**

Machine: MacBook Pro `Mac17,6`, macOS 26.6.2, output Arturia MiniFuse 1 (USB), JUCE 9.0.5.

## 1. CoreAudio callback timing, with radio excitation confirmed

The first sweep could not say whether the radio was playing in each window. The sweep now samples, 30 times a
second, whether the receiver is LIVE, the radio level the engine sees, and whether ROOM is on. Knobs: FEEDBACK 0.85,
DRIFT 0.7, COUPLE 0.8; 20 s per setting after 5 s settling; time around the whole callback.

| | ROOM off | ROOM on (helper running, MiniFuse input) |
|---|---|---|
| Full table | [`evidence/coreaudio-minifuse-2-room-off.md`](evidence/coreaudio-minifuse-2-room-off.md) | [`evidence/coreaudio-minifuse-2-room-on.md`](evidence/coreaudio-minifuse-2-room-on.md) |
| Radio LIVE, share of each window | 100 % in all 12 | 100 % in all 12 |
| Mean radio level (0..1) | 0.68 – 0.78 | 0.66 – 0.73 |
| ROOM on, share of each window | 0 % | 100 % in all 12 |
| Mean room level | 0 | 0.09 in the first window, 0.00 after: **the input was silent**, so ROOM-driven modulation was connected but not exercised |
| Worst callback, share of the buffer period | **4.0 %** (96 kHz / 64) | **4.5 %** (96 kHz / 32 and / 64) |
| p99.9, worst setting | 25 µs of 667 µs | 30 µs of 667 µs |
| Late callbacks (host-time gap > 1.5 periods) | 0 | 0 |
| Radio underruns / reconnects / engine faults | 0 / 0 / 0 | 0 / 0 / 0 |
| Positive control (one callback held for 3 periods) | counted, gap 21.4 ms | counted, gap 21.3 ms |

Settings: 44.1 / 48 / 96 kHz × 32 / 64 / 128 / 512 samples, all granted by the device.

What this settles: the earlier doubt (an idle engine) is gone — the engine was fed a live broadcast in every window.
What it leaves open: the cheap-looking rows (for example 2.8 µs mean at 96 kHz / 32, about 0.09 µs per sample
against 0.2–0.3 elsewhere) persist with the radio live, so they are not an idle engine; the cause is still unknown.
One caveat on the ROOM-off run: two short test compilations ran on the same machine during its last minute.

Limits: one interface, one Mac, 20 s windows, no other load, window open.

## 2. Reliability checks performed

Log: [`evidence/output-switch-and-helper-kill.txt`](evidence/output-switch-and-helper-kill.txt) (one status line
every 5 s; muted; ROOM on).

| Check | How | Result |
|---|---|---|
| Switch output while playing | The call the AUDIO dialog makes, issued by `--switch-test`: MiniFuse → built-in speakers at 15 s → MiniFuse at 30 s | **Pass.** Both switches succeed; radio stays LIVE with a 6.0 s buffer; 0 underruns, reconnects, late callbacks, faults; no microphone prompt; ROOM keeps listening on the MiniFuse input throughout |
| Helper dies while ROOM is on | `kill -9` on WILDBAND Room | **Pass.** Within 5 s ROOM reads off with "the WILDBAND Room helper is not running"; radio LIVE; no helper left |
| Instrument quits while ROOM is on | End of the ROOM-on sweep | **Pass.** No WILDBAND or helper process left |
| Helper permission still valid after rebuilding only the app | ROOM-on sweep and the log above, after app-only rebuilds | **Pass.** No prompt (the helper binary is unchanged) |
| Stream closed / stalled by the server | Local test server, first pass (`RADIO-COMPATIBILITY.md`) | Pass then; not repeated |

## 3. Not performed (needs the owner or hardware actions)

Network loss on the real connection; sleep/wake; unplugging the MiniFuse while playing; unplugging the ROOM input
while ROOM is on; revoking WILDBAND Room's permission mid-session; choosing output and ROOM input by hand in the
dialog; any MIDI controller; sessions longer than a few minutes; anything involving ears. See
`OWNER-CHECKLIST.md`.

## 4. Gate status after this pass

| Gate | Status |
|---|---|
| G0 | Open: licence undecided; creative direction recorded in the manifesto |
| G1 | Open: technical evidence complete for six stations; owner reliability-by-ear outstanding |
| G2 | Open: permission and capture verified; ROOM's musical usefulness "somewhat" per the first audition |
| G3 | Open: first audition "fun, feels like an instrument", resonator "not particularly expressive"; no matched ENGINE/DRY/REFERENCE comparison recorded |
| G4 | Open: one informal session reported; the two 15-minute sessions and MIDI Learn on hardware outstanding |
| G5 | Open: timing and the checks above done; the items in §3 outstanding |
