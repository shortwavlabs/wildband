# Voice lab render report

Fixtures: synthesized, 12 s, 48 kHz. Every file is level-matched to -23 dBFS RMS; a render below -70 dBFS is left as it is and reported as silent. WORLD is a scripted stand-in.
For the WILDBAND voices, ns/sample is the whole engine with that voice selected, not the voice alone.
Cost is one thread, no audio device: a comparison between voices, not a CoreAudio measurement.

| voice | origin | fixture | ns/sample | memory kB | level before matching dBFS | crest dB | WORLD on vs off dB | finite | file stem |
|---|---|---|---|---|---|---|---|---|---|
| dry | WILDBAND | speech-like | 64 | 0 | -25.1 | 18.6 | no change | yes | `speech-like__dry` |
| dry | WILDBAND | percussion | 52 | 0 | -22.5 | 18.1 | no change | yes | `percussion__dry` |
| dry | WILDBAND | music | 62 | 0 | -17.2 | 11.7 | no change | yes | `music__dry` |
| dry | WILDBAND | static | 54 | 0 | -23.2 | 14.0 | no change | yes | `static__dry` |
| dry | WILDBAND | silence | 49 | 0 | -180.0 | 0.0 | silent | yes | `silence__dry` |
| dry | WILDBAND | sine-220 | 57 | 0 | -9.7 | 2.8 | no change | yes | `sine-220__dry` |
| reference | WILDBAND | speech-like | 51 | 124 | -34.5 | 16.1 | no change | yes | `speech-like__reference` |
| reference | WILDBAND | percussion | 54 | 124 | -12.5 | 11.2 | no change | yes | `percussion__reference` |
| reference | WILDBAND | music | 61 | 124 | -13.8 | 11.5 | no change | yes | `music__reference` |
| reference | WILDBAND | static | 51 | 124 | -26.0 | 14.2 | no change | yes | `static__reference` |
| reference | WILDBAND | silence | 49 | 124 | -180.0 | 0.0 | silent | yes | `silence__reference` |
| reference | WILDBAND | sine-220 | 57 | 124 | -10.5 | 8.1 | no change | yes | `sine-220__reference` |
| resonator | WILDBAND | speech-like | 52 | 96 | -27.8 | 17.3 | 4 | yes | `speech-like__resonator` |
| resonator | WILDBAND | percussion | 52 | 96 | -26.4 | 19.2 | 3 | yes | `percussion__resonator` |
| resonator | WILDBAND | music | 65 | 96 | -17.9 | 12.3 | -4 | yes | `music__resonator` |
| resonator | WILDBAND | static | 51 | 96 | -29.7 | 13.8 | 6 | yes | `static__resonator` |
| resonator | WILDBAND | silence | 49 | 96 | -180.0 | 0.0 | silent | yes | `silence__resonator` |
| resonator | WILDBAND | sine-220 | 57 | 96 | -18.1 | 6.9 | -2 | yes | `sine-220__resonator` |
| dissolve | WILDBAND (new, D2) | speech-like | 67 | 1025 | -28.5 | 18.7 | -5 | yes | `speech-like__dissolve` |
| dissolve | WILDBAND (new, D2) | percussion | 67 | 1025 | -25.5 | 19.3 | -6 | yes | `percussion__dissolve` |
| dissolve | WILDBAND (new, D2) | music | 76 | 1025 | -21.0 | 12.9 | -6 | yes | `music__dissolve` |
| dissolve | WILDBAND (new, D2) | static | 67 | 1025 | -26.8 | 15.2 | -6 | yes | `static__dissolve` |
| dissolve | WILDBAND (new, D2) | silence | 64 | 1025 | -180.0 | 0.0 | silent | yes | `silence__dissolve` |
| dissolve | WILDBAND (new, D2) | sine-220 | 74 | 1025 | -15.7 | 7.8 | -5 | yes | `sine-220__dissolve` |
| dissolve-into-resonator | WILDBAND (new, D2) | speech-like | 66 | 0 | -30.8 | 18.8 | 7 | yes | `speech-like__dissolve-into-resonator` |
| dissolve-into-resonator | WILDBAND (new, D2) | percussion | 67 | 0 | -29.2 | 21.5 | 6 | yes | `percussion__dissolve-into-resonator` |
| dissolve-into-resonator | WILDBAND (new, D2) | music | 75 | 0 | -21.0 | 13.2 | 0 | yes | `music__dissolve-into-resonator` |
| dissolve-into-resonator | WILDBAND (new, D2) | static | 66 | 0 | -33.2 | 14.4 | 9 | yes | `static__dissolve-into-resonator` |
| dissolve-into-resonator | WILDBAND (new, D2) | silence | 64 | 0 | -180.0 | 0.0 | silent | yes | `silence__dissolve-into-resonator` |
| dissolve-into-resonator | WILDBAND (new, D2) | sine-220 | 68 | 0 | -24.5 | 11.7 | 2 | yes | `sine-220__dissolve-into-resonator` |
| rings-modal | Rings (Emilie Gillet, MIT), upstream in place | speech-like | 108 | 172 | -25.9 | 17.4 | -3 | yes | `speech-like__rings-modal` |
| rings-modal | Rings (Emilie Gillet, MIT), upstream in place | percussion | 107 | 172 | -28.8 | 20.6 | -4 | yes | `percussion__rings-modal` |
| rings-modal | Rings (Emilie Gillet, MIT), upstream in place | music | 117 | 172 | -18.0 | 8.7 | -5 | yes | `music__rings-modal` |
| rings-modal | Rings (Emilie Gillet, MIT), upstream in place | static | 113 | 172 | -20.7 | 11.5 | -4 | yes | `static__rings-modal` |
| rings-modal | Rings (Emilie Gillet, MIT), upstream in place | silence | 103 | 172 | -180.0 | 0.0 | silent | yes | `silence__rings-modal` |
| rings-modal | Rings (Emilie Gillet, MIT), upstream in place | sine-220 | 108 | 172 | -30.0 | 15.0 | -2 | yes | `sine-220__rings-modal` |
| rings-sympathetic | Rings (Emilie Gillet, MIT), upstream in place | speech-like | 120 | 172 | -20.7 | 11.5 | -5 | yes | `speech-like__rings-sympathetic` |
| rings-sympathetic | Rings (Emilie Gillet, MIT), upstream in place | percussion | 120 | 172 | -21.6 | 13.8 | -6 | yes | `percussion__rings-sympathetic` |
| rings-sympathetic | Rings (Emilie Gillet, MIT), upstream in place | music | 129 | 172 | -20.6 | 10.8 | -3 | yes | `music__rings-sympathetic` |
| rings-sympathetic | Rings (Emilie Gillet, MIT), upstream in place | static | 121 | 172 | -20.6 | 11.5 | -6 | yes | `static__rings-sympathetic` |
| rings-sympathetic | Rings (Emilie Gillet, MIT), upstream in place | silence | 114 | 172 | -180.0 | 0.0 | silent | yes | `silence__rings-sympathetic` |
| rings-sympathetic | Rings (Emilie Gillet, MIT), upstream in place | sine-220 | 123 | 172 | -16.0 | 6.0 | -2 | yes | `sine-220__rings-sympathetic` |
| rings-string | Rings (Emilie Gillet, MIT), upstream in place | speech-like | 89 | 172 | -26.0 | 13.9 | 0 | yes | `speech-like__rings-string` |
| rings-string | Rings (Emilie Gillet, MIT), upstream in place | percussion | 94 | 172 | -19.3 | 11.4 | -5 | yes | `percussion__rings-string` |
| rings-string | Rings (Emilie Gillet, MIT), upstream in place | music | 96 | 172 | -20.9 | 10.5 | 0 | yes | `music__rings-string` |
| rings-string | Rings (Emilie Gillet, MIT), upstream in place | static | 82 | 172 | -28.1 | 13.8 | -1 | yes | `static__rings-string` |
| rings-string | Rings (Emilie Gillet, MIT), upstream in place | silence | 80 | 172 | -180.0 | 0.0 | silent | yes | `silence__rings-string` |
| rings-string | Rings (Emilie Gillet, MIT), upstream in place | sine-220 | 89 | 172 | -20.2 | 7.9 | -4 | yes | `sine-220__rings-string` |
| korupt | vcv-tapestry, in place (D1) | speech-like | 45 | 0 | -2.2 | 4.8 | -8 | yes | `speech-like__korupt` |
|  |  | speech-like: mean lock 0.04, tracking 1.00, glitch 0.984 (WORLD on) |  |  |  |  |  |  |  |
| korupt | vcv-tapestry, in place (D1) | percussion | 43 | 0 | -2.8 | 5.4 | -8 | yes | `percussion__korupt` |
|  |  | percussion: mean lock 0.03, tracking 1.00, glitch 0.995 (WORLD on) |  |  |  |  |  |  |  |
| korupt | vcv-tapestry, in place (D1) | music | 46 | 0 | -0.6 | 3.2 | -9 | yes | `music__korupt` |
|  |  | music: mean lock 0.03, tracking 1.00, glitch 0.992 (WORLD on) |  |  |  |  |  |  |  |
| korupt | vcv-tapestry, in place (D1) | static | 50 | 0 | -1.1 | 3.7 | -9 | yes | `static__korupt` |
|  |  | static: mean lock 0.00, tracking 1.00, glitch 1.000 (WORLD on) |  |  |  |  |  |  |  |
| korupt | vcv-tapestry, in place (D1) | silence | 40 | 0 | -2.5 | 0.7 | -6 | yes | `silence__korupt` |
|  |  | silence: mean lock 0.00, tracking 1.00, glitch 1.000 (WORLD on) |  |  |  |  |  |  |  |
| korupt | vcv-tapestry, in place (D1) | sine-220 | 46 | 0 | -0.5 | 3.1 | -9 | yes | `sine-220__korupt` |
|  |  | sine-220: mean lock 0.00, tracking 1.00, glitch 1.000 (WORLD on) |  |  |  |  |  |  |  |
| wyrd-timefilter | vcv-tapestry, in place (D3) | speech-like | 76 | 4413 | -31.6 | 15.1 | 1 | yes | `speech-like__wyrd-timefilter` |
| wyrd-timefilter | vcv-tapestry, in place (D3) | percussion | 73 | 4413 | -31.9 | 15.3 | -3 | yes | `percussion__wyrd-timefilter` |
| wyrd-timefilter | vcv-tapestry, in place (D3) | music | 77 | 4413 | -23.6 | 8.2 | 0 | yes | `music__wyrd-timefilter` |
| wyrd-timefilter | vcv-tapestry, in place (D3) | static | 89 | 4413 | -30.6 | 12.7 | 3 | yes | `static__wyrd-timefilter` |
| wyrd-timefilter | vcv-tapestry, in place (D3) | silence | 68 | 4413 | -105.1 | 0.0 | silent | yes | `silence__wyrd-timefilter` |
| wyrd-timefilter | vcv-tapestry, in place (D3) | sine-220 | 77 | 4413 | -17.1 | 2.1 | -3 | yes | `sine-220__wyrd-timefilter` |

Not exactly level-matched:
- `speech-like__dissolve` would have clipped at the matched level and was turned down by 0.5 dB.
- `percussion__dissolve` would have clipped at the matched level and was turned down by 1.9 dB.

## Korupt diagnostic

Core driven as in its own tests (400 Hz square, default parameters, 3 s): mean lock 0.000, highest lock 0.000, mean tracking 1.00. Then 4 s of zero input straight into the core: tracking 0.000. Its input stage fed 2 s of silence settles at audio = -0.277, comparator = 0.843, envelope = 0.000 (the core is given that 'audio' value, so with the input stage in front it does not see silence as zero).

## Drift as a control source (INTERNAL modulation, not WORLD)

Resonator with PITCH and TONE driven by: nothing, Drift (vcv-tapestry, in place), a plain 0.21 Hz sine.

| fixture | Drift vs none dB | sine vs none dB | Drift ns/sample (core alone) |
|---|---|---|---|
| speech-like | 3 | 1 | 32 |
| percussion | -7 | -7 | 32 |
| music | 3 | 1 | 32 |
| static | 5 | 5 | 33 |
| silence | 0 | 0 | 32 |
| sine-220 | -18 | -17 | 32 |
