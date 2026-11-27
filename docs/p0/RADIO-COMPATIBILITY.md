# WILDBAND P0 — Radio protocol and codec compatibility (G1)

**Claim:** a practical subset of Icecast/Shoutcast-style HTTP streams works. **Not claimed:** that any given
station will work, universal codec support, or a station directory.

A stream address is a server on the internet, not a frequency. TUNE chooses between six saved addresses.

## Supported and unsupported

| Item | Status | Evidence |
|---|---|---|
| MP3 (MPEG-1/2 Layer III), any sample rate | **Works** | 4 real stations (44.1 kHz and 24 kHz) |
| AAC-LC in ADTS (`audio/aac`, `audio/aacp`) | **Works** | 2 real stations |
| HE-AAC / HE-AAC v2 (`aacp`) | Coded for (format list), **not tested** — no test station used it | — |
| Ogg Vorbis, Ogg Opus, FLAC | **Not supported.** Refused with a message; no retry | Local server `/ogg` |
| HLS (`.m3u8`, `#EXT-X-…`) | **Not supported.** Refused with a message | Local server `/hls.m3u8` |
| MPEG-DASH, RTSP, RTMP, MMS | Not supported, not tested | — |
| `http://` and `https://` | **Works** (the app's Info.plist allows plain HTTP; many stations still use it) | `https` seen live inside the built app; `http` only in the command-line probe — see below |
| HTTP redirects (up to 5) | **Works** | Local server `/redirect` |
| `.m3u` and `.pls` playlists | **Works**, first address only, one level of indirection (two at most) | Local server `/list.m3u`, `/list.pls` |
| ICY in-band metadata (song titles) | Not requested (`Icy-MetaData: 0`), so not displayed. If a server interleaves it anyway, it is stripped | Local server `/meta` |
| Legacy Shoutcast v1 `ICY 200 OK` status line | **Works against a local imitation only**; assumed MP3 when no content type arrives. No real legacy server was tested | Local server `/icy` |
| Station name (`icy-name` header) | Shown in the status line | Real stations |
| HTTP 4xx | Reported, no retry. Other failures retry with back-off 0.5 → 8 s | Local server `/missing` |
| Connection closed by the server | Reconnects; with audio still buffered the join is inaudible in the PCM (0 underruns) | Local server `/drop`: 5 reconnects in 25 s, 25.0 s played |
| Connection stalls (socket open, no data) | Detected by the 8 s read timeout, then reconnects | Local server `/stall`: about 6 s of audio lost per stall |
| Authenticated or geo-blocked streams, custom headers, proxies | Not supported | — |
| Mono streams | Coded for (duplicated to both channels), not tested | — |
| Clock drift between station and Mac | **Not corrected.** A fast station is held back by the bounded ring (the socket waits); a slow one eventually underruns and re-buffers for 0.75 s. Not measured over a long session | — |

## Behaviour

- Decoded audio is resampled to the device rate by the same system converter and held in a ring of at most 6 s.
- Playback starts once 0.75 s is buffered. Servers usually send a burst first, so the ring then sits near its
  limit: **the radio you hear is typically several seconds behind the broadcast**, on top of the station's own delay.
- On a retune the old station fades out within 20 ms of the gesture, and the resonators ring on over the gap.
- On a sample-rate change the stream is restarted at the new rate.
- Nothing is written to disk. The ring is the only copy of the audio and it is overwritten continuously.

## Evidence: real stations

`build/wildband_radio_probe_artefacts/Release/wildband_radio_probe --seconds 8 <urls>` on 2026-11-27. The probe is
the same `Radio` class as the app, read at real-time pace by a thread standing in for the audio callback. The
stations were tuned one after another in a single run, so every row after the first is also a **station switch**;
"first audio" is the time from the tune request to playing PCM.

| Station (operator) | Address | Result | Format | First audio | Played of 8 s | RMS dBFS | Underruns | Reconnects |
|---|---|---|---|---|---|---|---|---|
| Groove Salad (SomaFM) | `https://ice2.somafm.com/groovesalad-128-mp3` | live | MP3 44.1 kHz stereo | 0.47 s | 7.5 s | −17.8 | 0 | 0 |
| KEXP 90.3 (KEXP, Seattle) | `https://kexp.streamguys1.com/kexp160.aac` | live | AAC 44.1 kHz stereo | 1.72 s | 6.3 s | −13.9 | 0 | 0 |
| WFMU Freeform (WFMU) | `http://stream0.wfmu.org/freeform-128k` | live | MP3 44.1 kHz stereo | 0.45 s | 7.5 s | −23.1 | 0 | 0 |
| Radio Paradise | `https://stream.radioparadise.com/aac-128` | live | AAC 44.1 kHz stereo | 0.16 s | 7.8 s | −11.3 | 0 | 0 |
| BBC World Service (BBC) | `http://stream.live.vc.bbcmedia.co.uk/bbc_world_service` | live | MP3 24 kHz stereo | 1.28 s | 6.7 s | −25.8 | 0 | 0 |
| SF 10-33 (SomaFM) | `https://ice2.somafm.com/sf1033-128-mp3` | live | MP3 44.1 kHz stereo | 0.37 s | 7.6 s | −19.3 | 0 | 0 |

Five independent operators; music, speech (BBC) and a noisy speech/ambient mix (SF 10-33). Radio Paradise's decoded
peak was 1.07, above full scale; the engine clamps its input.

Inside the built, ad-hoc-signed app: SomaFM Groove Salad (`https`) reached LIVE with a 6.0 s buffer at 48 kHz
(seen in a capture of the app's status line, on the first muted launch). That is the only in-app observation of a
playing stream: the timing sweeps did not log the radio state. **Plain `http://` stations have not been confirmed inside the app** (the attempt was blocked by the
microphone prompt described in `EVIDENCE-LOG.md`); the Info.plist exception that permits them is present.

## Evidence: awkward servers

`python3 -I tools/stream_test_server.py clip.mp3` (a 20 s sine-plus-noise MP3 made with ffmpeg, served in a loop
at real-time pace on 127.0.0.1), then the probe against each path. Results are in the table at the top. A local
imitation is repeatable, but it is not a real legacy server or a real network failure.

## Not tested

Wi-Fi loss, a captive portal, sleep/wake, VPNs, IPv6-only networks, very low bitrates, long sessions (hours), and
any station outside the six above.

## Rights

Only the stations' public addresses are in the source, as editable defaults. No broadcast audio is stored, bundled
or redistributed, and there is no capture path. A publicly reachable stream is not permission to rebroadcast or to
use a station's name in a demo; that has not been looked into.
