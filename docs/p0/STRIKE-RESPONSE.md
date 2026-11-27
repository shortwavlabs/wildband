# WILDBAND — STRIKE response: analysis and three comparison candidates

**Branch:** `p0/strike-response`, on `p0/integration`. **Status:** the analysis and acceptance criteria below were
committed before the candidates were implemented; measured results are added at the bottom.

## Where the slowness comes from (analysis of the existing code, nothing changed)

A cable into STRIKE does two separate things:

| Part | Equation (control rate, every 16 samples) | Speed |
|---|---|---|
| **The strike itself** | excitation gains `+ 0.5 × 5 × COUPLE × m`, where `m` is `depth × source`, smoothed with a 20 ms one-pole; the excitation gain is then smoothed with a 2 ms one-pole | Fast. It follows the source: RADIO HITS decays in about 60 ms, ROOM HITS in about 250 ms |
| **The back-off** of the steady excitation | excitation loses `0.5 × 0.7 × COUPLE × backoff`; `backoff` follows `max(0, depth) × presence` (20 ms one-pole) | Depends entirely on `presence` |
| **Presence** (added by the STRIKE neutrality correction) | `seen = max(source, seen × r)` with `r = exp(−1 / (2 s × control rate))`, zero below 0.001; `presence = min(1, 10 × seen)` | **Slow by construction** |

After a hit of height `h`, presence stays at 1 while `10·h·e^(−t/2) ≥ 1`, i.e. for `2·ln(10h)` seconds — **4.6 s for a
full-scale hit, 2.2 s for a hit of 0.3** — then falls linearly with `seen` and reaches exactly zero at
`2·ln(1000h)` seconds: **13.8 s**. So the reported "4.6 s held, about 14 s to recover" is right, and it is entirely
the presence detector: a 2 s release behind a ×10 knee.

## Where response time belongs

- **Not the source.** The sources already carry their own timing (60 ms and 250 ms decays for the two HITS
  outputs, slower for LEVEL), and the same source may feed PITCH or TONE where a multi-second hold would be wrong.
  Slowing a source would slow everything patched from it.
- **Not the connection.** A cable has a depth; giving it a time constant too would turn every cable into a
  two-parameter object and the DEPTH strip into a small panel. It would also be meaningless for five of the six
  destinations today.
- **The destination.** The hold exists only because STRIKE trades steady excitation against hits. How long that
  trade lasts after the last hit is a property of *how STRIKE behaves*, like the 5 Hz DC blocker or the damping
  curve. That is where the candidates differ, and nowhere else.

So: no new faceplate control. The candidates are selected from a temporary **LAB** menu, and whichever the owner
prefers becomes the fixed behaviour (or, if none is right, the equations are tuned further).

## Candidates

| | A — Held (existing, the baseline) | B — Fast | C — Organic |
|---|---|---|---|
| Intended for | — | Rhythmic gestures, room transients: the steady excitation breathes back between hits | Ambient playing: the trade swells in and eases out, no hard edges |
| Presence | peak follower, 2 s release, ×10 knee | peak follower, 0.25 s release, ×3 knee | peak follower (1.5 s release) feeding a second follower with a 0.35 s rise and 1.2 s fall, then a smoothstep |
| Onset of the back-off | immediate | immediate | gradual |

Unchanged in all three: the strike itself, negative-depth ducking, DRIFT slowing (both still provisional), every
other destination, and neutrality for a silent source.

## Acceptance criteria (registered before implementation)

| ID | Test | Passes when |
|---|---|---|
| SR-01 | Baseline preserved | With response A selected (the default), every existing test passes unchanged, including sample-identical parity with the frozen engine where it was required before |
| SR-02 | Neutral when silent | For A, B and C: a silent source into STRIKE at any depth is sample-identical to no cable |
| SR-03 | Fast is fast | After a 0.2 s burst, B's back-off is below 10 % within 1.5 s and below 1 % within 2.5 s |
| SR-04 | Organic is gradual | C's back-off takes more than 0.3 s to reach 90 % (it swells in), is still above 50 % 2 s after the burst, and is below 10 % by 12 s |
| SR-05 | Rhythm | With a 50 ms hit every 0.5 s: A stays above 95 % between hits (no breathing); B falls below 70 % between hits (it breathes); C stays between the two extremes with a ripple under 15 points |
| SR-06 | Safety | All three: bounded, finite, no allocation, block-size independent; switching between them while playing adds no step above 0.03 |

Measured, not pass/fail: a table of times to 90 / 50 / 10 / 1 % for each candidate.

Not automatable (owner): which response suits rhythmic room playing, which suits ambient playing, and whether one
fixed behaviour can serve both.

---

## Results (added after implementation, 2026-11-27)

`wildband_tests`: **788 checks, 0 failed** (765 from the integration branch plus these), also under
`-O3 -ffp-contract=off`. **Numbers only; nobody has listened to the three responses.**

Back-off of the steady excitation after one second of activity, COUPLE at 1, depth +100 % (`wildband_tests --metrics`):

| Response | Rise to 90 % | Below 90 % after | Below 50 % | Below 10 % | Below 1 % | A 50 ms hit every 0.5 s |
|---|---|---|---|---|---|---|
| **A — Held** (existing) | 0.04 s | 4.83 s | 6.01 s | 9.23 s | 13.81 s | stays at 100 % |
| **B — Fast** | 0.04 s | 0.31 s | 0.46 s | 0.87 s | 1.44 s | swings 54 % … 100 % |
| **C — Organic** | 0.59 s | 2.75 s | 3.92 s | 5.74 s | 7.82 s | stays at 100 % |

| ID | Result | Notes |
|---|---|---|
| SR-01 | **Pass** | A is the default; all 765 earlier checks pass unchanged |
| SR-02 | **Pass** | 12 combinations |
| SR-03 | **Pass** | First tuning of B (0.3 s release) only fell to 72 % between hits at 120 bpm, missing the "breathes" criterion; the release is now 0.25 s |
| SR-04 | **Pass** | |
| SR-05 | **Pass** | Note what it shows: under a steady pulse **C behaves like A** (fully held); it differs in how the trade comes in and goes out, not in rhythm |
| SR-06 | **Pass** | All three (and a nonsense selector value) in the hostile-input and allocation runs; switching every 25 ms adds no step |

Two honest observations:

- C was specified as "slower, organic". It is slower than B and gentler than A, but it recovers **sooner** than the
  existing behaviour (8 s against 14 s). A response slower than the existing one did not seem worth building when
  the existing one is the one reported as sluggish; say so if a genuinely longer one is wanted.
- The strike itself is identical in all three. If the instrument still feels slow to respond to the room with B
  selected, the cause is elsewhere (the source's own 250 ms onset decay, or the 20 ms cable smoothing), and the next
  thing to try would be there.

### How to compare

**LAB** (top right) → *STRIKE response* → A, B or C. It is a menu, not a control: it exists for this comparison
and is remembered per settings profile. Suggested listening is in `OWNER-AUDITION.md`.
