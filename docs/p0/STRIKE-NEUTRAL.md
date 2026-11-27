# WILDBAND — STRIKE correction: a silent source has no influence

**Branch:** `p0/strike-neutral`, on top of `p0/attenuverter` (PR 4 is left as it was for audition).
**Status:** prepared for owner audition. It is an **intentional deviation from sample-identical compatibility** with
the build auditioned on 2026-11-27, limited to cables plugged into STRIKE.

## What was wrong

In the auditioned engine, plugging a cable into STRIKE made the steady excitation back off by up to 70 %, so the
control signal's hits would stand out — whether or not the source was producing anything. A ROOM → STRIKE cable with
ROOM off, denied or stale therefore dimmed the instrument. Test ST-01 shows this on the frozen engine: the resonator
output falls to less than half.

## What changed

One block in `src/Engine.h`. The back-off now requires a source that has actually been active recently:

- `strikeSeen` follows the source's recent peak: instantly up, about 2 s time constant down, exactly zero below 0.001.
- The back-off is `max(0, depth) × min(1, 10 × strikeSeen)`: full once the source has reached 0.1, none for a
  source that reads zero.

Nothing else moved: the strike itself (`5 × COUPLE × depth × source`), negative-depth ducking, DRIFT slowing and
every other destination are untouched, and the frozen P0 engine stays in `tests/baseline/` as the reference.

## Behaviour

| Situation | Auditioned build | This branch |
|---|---|---|
| Source silent / ROOM off, denied or stale, any depth | Dimmed at positive depth | **No influence** (sample-identical to no cable) |
| Source active from the start (≥ 0.1) | — | **Sample-identical** to the auditioned build at +100 % |
| Source active, then stops | Stays dimmed for ever | **The dimming is held, then released.** After a full-scale hit it stays complete for about 4.6 s, then tapers, and is exactly gone at about 14 s (a weaker last hit shortens both). Measured: still dimmer than the bare patch 3 s into a silence; within 1 % of "no cable" after 15.5 s |
| Source resumes | — | Back-off returns at once |
| Default patch (RADIO HITS → STRIKE) on the local test source | — | Differs by −73 dB relative to the signal; level change 0.00 dB (measured, 8 s) |
| Negative depth | Ducks | Ducks (unchanged; provisional pending audition) |

The −73 dB figure is for the built-in test pattern, whose hits are frequent. On a broadcast with long quiet
passages (more than a few seconds without a transient) the steady excitation will come back up between hits, where
the auditioned build kept it down. **That is the audible consequence to listen for.**

## Tests (`wildband_tests`, in `depth::strikeNeutral`)

| ID | Test | Result |
|---|---|---|
| ST-01 | Three ROOM sources × five depths into STRIKE with ROOM silent: sample-identical to no cable. The frozen engine is shown to dim in the same situation | Pass |
| ST-02 | A source active from the first sample, at +100 %: sample-identical to the frozen engine | Pass |
| ST-03 | Hits for 2 s, silence for 16 s, hits again: louder than the bare patch while struck (> 2×), still dimmed 3 s into the silence, within 1 % of the bare patch 15.5 s into it, louder again on resuming | Pass |
| ST-04 | Default patch against the frozen engine: not identical (the deviation exists); overall level within 3 dB | Pass (−73 dB difference, +0.00 dB level) |
| AT-07 | Now holds for all six destinations with no exception | Pass |
| AT-01 | Exact parity with the frozen engine is now required for patches with **no cable in STRIKE**; STRIKE is covered by ST-02 and ST-04 instead | Pass |

Whole suite: 688 checks, 0 failed; also passes under
`-O3 -ffp-contract=off`.

## For the owner to judge

- Does the default patch still feel the same on real stations, especially speech with pauses?
- Is "about 2 seconds" the right memory for the back-off? Shorter makes the steady excitation breathe between hits;
  longer keeps the old punchy contrast.
- STRIKE ducking at negative depth and DRIFT slowing remain provisional.
