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
| Presence | peak follower, 2 s release, ×10 knee | peak follower, 0.3 s release, ×3 knee | peak follower (1.5 s release) feeding a second follower with a 0.35 s rise and 1.2 s fall, then a smoothstep |
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
