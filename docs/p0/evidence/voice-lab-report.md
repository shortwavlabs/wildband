# Voice lab render report

Fixtures: synthesized, 12 s, 48 kHz. Every file is level-matched to -23 dBFS RMS; a render below -70 dBFS is left as it is and reported as silent. WORLD is a scripted stand-in.
For the WILDBAND voices, ns/sample is the whole engine with that voice selected, not the voice alone.
Cost is one thread, no audio device: a comparison between voices, not a CoreAudio measurement.

| voice | origin | fixture | ns/sample | memory kB | level before matching dBFS | crest dB | WORLD on vs off dB | finite | file stem |
|---|---|---|---|---|---|---|---|---|---|
| dry | WILDBAND | speech-like | 62 | 0 | -25.1 | 18.6 | no change | yes | `speech-like__dry` |
| dry | WILDBAND | percussion | 51 | 0 | -22.5 | 18.1 | no change | yes | `percussion__dry` |
| dry | WILDBAND | music | 61 | 0 | -17.2 | 11.7 | no change | yes | `music__dry` |
| dry | WILDBAND | static | 53 | 0 | -23.2 | 14.0 | no change | yes | `static__dry` |
| dry | WILDBAND | silence | 48 | 0 | -180.0 | 0.0 | silent | yes | `silence__dry` |
| dry | WILDBAND | sine-220 | 56 | 0 | -9.7 | 2.8 | no change | yes | `sine-220__dry` |
| reference | WILDBAND | speech-like | 51 | 124 | -34.5 | 16.1 | no change | yes | `speech-like__reference` |
| reference | WILDBAND | percussion | 53 | 124 | -12.5 | 11.2 | no change | yes | `percussion__reference` |
| reference | WILDBAND | music | 60 | 124 | -13.8 | 11.5 | no change | yes | `music__reference` |
| reference | WILDBAND | static | 50 | 124 | -26.0 | 14.2 | no change | yes | `static__reference` |
| reference | WILDBAND | silence | 48 | 124 | -180.0 | 0.0 | silent | yes | `silence__reference` |
| reference | WILDBAND | sine-220 | 56 | 124 | -10.5 | 8.1 | no change | yes | `sine-220__reference` |
| resonator | WILDBAND | speech-like | 51 | 96 | -27.8 | 17.3 | 4 | yes | `speech-like__resonator` |
| resonator | WILDBAND | percussion | 51 | 96 | -26.4 | 19.2 | 3 | yes | `percussion__resonator` |
| resonator | WILDBAND | music | 64 | 96 | -17.9 | 12.3 | -4 | yes | `music__resonator` |
| resonator | WILDBAND | static | 50 | 96 | -29.7 | 13.8 | 6 | yes | `static__resonator` |
| resonator | WILDBAND | silence | 48 | 96 | -180.0 | 0.0 | silent | yes | `silence__resonator` |
| resonator | WILDBAND | sine-220 | 56 | 96 | -18.1 | 6.9 | -2 | yes | `sine-220__resonator` |
| dissolve | WILDBAND (new, D2) | speech-like | 66 | 1025 | -28.5 | 18.6 | -5 | yes | `speech-like__dissolve` |
| dissolve | WILDBAND (new, D2) | percussion | 66 | 1025 | -25.5 | 19.4 | -6 | yes | `percussion__dissolve` |
| dissolve | WILDBAND (new, D2) | music | 74 | 1025 | -21.0 | 12.7 | -5 | yes | `music__dissolve` |
| dissolve | WILDBAND (new, D2) | static | 65 | 1025 | -26.8 | 15.2 | -5 | yes | `static__dissolve` |
| dissolve | WILDBAND (new, D2) | silence | 63 | 1025 | -180.0 | 0.0 | silent | yes | `silence__dissolve` |
| dissolve | WILDBAND (new, D2) | sine-220 | 72 | 1025 | -15.7 | 7.8 | -4 | yes | `sine-220__dissolve` |
| dissolve-into-resonator | WILDBAND (new, D2) | speech-like | 65 | 0 | -30.8 | 18.8 | 7 | yes | `speech-like__dissolve-into-resonator` |
| dissolve-into-resonator | WILDBAND (new, D2) | percussion | 65 | 0 | -29.2 | 21.5 | 6 | yes | `percussion__dissolve-into-resonator` |
| dissolve-into-resonator | WILDBAND (new, D2) | music | 73 | 0 | -21.0 | 13.3 | 0 | yes | `music__dissolve-into-resonator` |
| dissolve-into-resonator | WILDBAND (new, D2) | static | 64 | 0 | -33.2 | 14.3 | 9 | yes | `static__dissolve-into-resonator` |
| dissolve-into-resonator | WILDBAND (new, D2) | silence | 63 | 0 | -180.0 | 0.0 | silent | yes | `silence__dissolve-into-resonator` |
| dissolve-into-resonator | WILDBAND (new, D2) | sine-220 | 66 | 0 | -24.5 | 11.7 | 2 | yes | `sine-220__dissolve-into-resonator` |
| korupt | vcv-tapestry, in place (D1) | speech-like | 44 | 0 | -2.2 | 4.8 | -8 | yes | `speech-like__korupt` |
|  |  | speech-like: mean lock 0.04, tracking 1.00, glitch 0.984 (WORLD on) |  |  |  |  |  |  |  |
| korupt | vcv-tapestry, in place (D1) | percussion | 42 | 0 | -2.8 | 5.4 | -8 | yes | `percussion__korupt` |
|  |  | percussion: mean lock 0.03, tracking 1.00, glitch 0.995 (WORLD on) |  |  |  |  |  |  |  |
| korupt | vcv-tapestry, in place (D1) | music | 45 | 0 | -0.6 | 3.2 | -9 | yes | `music__korupt` |
|  |  | music: mean lock 0.03, tracking 1.00, glitch 0.992 (WORLD on) |  |  |  |  |  |  |  |
| korupt | vcv-tapestry, in place (D1) | static | 49 | 0 | -1.1 | 3.7 | -9 | yes | `static__korupt` |
|  |  | static: mean lock 0.00, tracking 1.00, glitch 1.000 (WORLD on) |  |  |  |  |  |  |  |
| korupt | vcv-tapestry, in place (D1) | silence | 38 | 0 | -2.5 | 0.7 | -6 | yes | `silence__korupt` |
|  |  | silence: mean lock 0.00, tracking 1.00, glitch 1.000 (WORLD on) |  |  |  |  |  |  |  |
| korupt | vcv-tapestry, in place (D1) | sine-220 | 45 | 0 | -0.5 | 3.1 | -9 | yes | `sine-220__korupt` |
|  |  | sine-220: mean lock 0.00, tracking 1.00, glitch 1.000 (WORLD on) |  |  |  |  |  |  |  |
| wyrd-timefilter | vcv-tapestry, in place (D3) | speech-like | 74 | 4413 | -28.5 | 17.2 | -2 | yes | `speech-like__wyrd-timefilter` |
| wyrd-timefilter | vcv-tapestry, in place (D3) | percussion | 71 | 4413 | -26.5 | 17.8 | -8 | yes | `percussion__wyrd-timefilter` |
| wyrd-timefilter | vcv-tapestry, in place (D3) | music | 75 | 4413 | -20.3 | 12.1 | -4 | yes | `music__wyrd-timefilter` |
| wyrd-timefilter | vcv-tapestry, in place (D3) | static | 87 | 4413 | -26.8 | 15.1 | -1 | yes | `static__wyrd-timefilter` |
| wyrd-timefilter | vcv-tapestry, in place (D3) | silence | 66 | 4413 | -105.1 | 0.0 | silent | yes | `silence__wyrd-timefilter` |
| wyrd-timefilter | vcv-tapestry, in place (D3) | sine-220 | 75 | 4413 | -13.2 | 5.8 | -7 | yes | `sine-220__wyrd-timefilter` |

Not exactly level-matched:
- `percussion__dissolve` would have clipped at the matched level and was turned down by 1.5 dB.

## Drift as a control source (INTERNAL modulation, not WORLD)

Resonator with PITCH and TONE driven by: nothing, Drift (vcv-tapestry, in place), a plain 0.21 Hz sine.

| fixture | Drift vs none dB | sine vs none dB | Drift ns/sample (core alone) |
|---|---|---|---|
| speech-like | 3 | 1 | 32 |
| percussion | -7 | -7 | 31 |
| music | 3 | 1 | 32 |
| static | 5 | 5 | 32 |
| silence | 0 | 0 | 32 |
| sine-220 | -18 | -17 | 32 |
