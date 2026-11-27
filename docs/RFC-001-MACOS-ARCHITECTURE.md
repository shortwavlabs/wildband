# RFC-001 — macOS Live Signal and Environment Architecture

**Status:** PROPOSAL / RESEARCH — not an accepted architecture.

## Scope and target

macOS standalone prototype first; JUCE 9 considered for shared audio/MIDI UI engine. macOS-specific permission and sensing services should live behind interfaces, independent from real-time DSP and the rendering layer. Never assume Windows equivalence.

## Sketch

```
Live radio URL -> async fetch / codec -> bounded PCM ring -> source switch / fade -+
                                                                          |
Microphone -> native capture / meter / features -> bounded control data ---+--> DSP coupling & resonance --> output
                                                                          |
Optional weather / BLE / Wi-Fi -> low-rate sanitized values --------------+
                                                                          ^
MIDI knobs / on-screen gestures ------------------------------------------+
```

All network, parsing, codec negotiation, sensor discovery, permissions and graphics run off the audio callback. The DSP receives lock-free or otherwise bounded real-time-safe samples/control snapshots. Retunes and disconnects have finite fades; no unbounded feedback or output levels.

## Sources

- **Internet radio:** validate direct stream URL handling for common HTTP Icecast/Shoutcast deployments and relevant MP3/AAC/Opus/Ogg variants individually. Playlist indirection, redirects, HTTPS, metadata, station failures, buffering jitter and format changes need testing. A URL is not guaranteed to decode. Do not rely on a vendor-operated proxy.
- **Microphone:** explicit system permission. Analyze audio activity locally and default to control features (RMS, onset, spectral centroid) instead of automatically routing raw microphone audio to speakers.
- **Weather:** optional, low-rate, stale-tolerant source; no mandatory API key or dependency for performance.
- **Bluetooth:** CoreBluetooth may provide discovered advertising peripheral RSSI, with hardware/permission/privacy limitations; not a broad-spectrum EM sensor. Do not persist device identifiers.
- **Wi-Fi:** CoreWLAN and OS entitlements/permissions constrain accessible scan/signal info; feasibility unknown.
- **Raw RF:** not available with standard Wi-Fi/BLE hardware; investigate optional external SDR separately if musical value is established.
- **Ambient light:** availability and public API support are device dependent; research only.

## DSP candidate

Start with a *small* network: streaming audio excitation -> resonant/nonlinear voice -> controllable feedback/damping -> output. Add follower-derived control from incoming broadcast and microphone. Modulations must have smoothing, clamping and understandable depth controls. Sound can decay and go silent; this is musically valid.

Do not overcommit to spectral, granular or complex physically modeled DSP until listening tests support it. Conduct an A/B against a generic filter-and-delay chain.

## Follow-up patch utilities (research only)

After P0 owner audition, explore an auxiliary mixer with deliberately bounded feedback paths and a configurable audio envelope follower/gate. Later consider an environmental SENSE scale/offset/polarity/smoothing utility and a Drift-derived organic random CV modulator (with a simple LFO as a separate fallback if musically needed). Reuse TALORA's typed audio/CV and real-time-safe graph patterns only after inspecting actual implementation and porting costs. A field pickup connected via an audio interface is a potential future true environmental *audio* source; motors/solenoids require external hardware. Drift's current implementation uses a Vult-generated, stateful pseudo-random algorithm and requires license, dependency, scaling and real-time qualification before porting. See [`FIELD-KIT-INSPIRED-UTILITIES.md`](FIELD-KIT-INSPIRED-UTILITIES.md). This proposal does not modify the current P0 gate or imply these features work.

## Real-time and reliability design

- No network calls, locks with uncertain latency, decoding, file access or GUI in processBlock.
- Rate-limited sensor measurements; timestamp, clamp and smooth; stale values ramp to neutral.
- Audio device switches and sample-rate changes must leave the engine recoverable.
- Streaming underrun triggers predictable decay/fallback, not discontinuities.
- Controlled gain/feedback bounds and fail-safe mute.
- macOS sleep/wake, internet outages, captive portals, denied mic permission and headphones disconnect must all have explicit behavior.
- Preserve only configuration, not private observations or stream audio.

## Open research questions

1. Which stream codecs can be shipped reliably and legally with chosen libraries?
2. Can changes between internet streams be made musically responsive despite network latencies?
3. Does microphone-derived control signal meaningfully change playability?
4. What form of coupled resonator remains bounded and sounds compelling?
5. Which macOS sensing services have stable public APIs on Apple Silicon?
6. Can the standalone performance interface and MIDI mapping be qualified under real-world operation?

**Exit gate:** no architectural promotion until P0 evidence and owner listening report.
