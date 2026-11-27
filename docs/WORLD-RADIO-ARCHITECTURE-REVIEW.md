# WILDBAND — Worldwide catalog and dual receivers: architecture review and Radio Browser feasibility plan

**Date:** 2026-11-27. **Status:** review and plan only — **nothing here is implemented**. It reviews
[WORLD-RADIO-CATALOG.md](WORLD-RADIO-CATALOG.md) (PR #12, roadmap v1.3) against the code that exists on the
implementation stack (`p1/plugin-lifecycle`, PR #17, and its bases), and proposes the first, small provider spike.

## Owner requirements (as specified)

1. Worldwide station discovery through a directory such as Radio Browser.
2. Up to six selected stations per saved set or patch.
3. At most **two** independent simultaneous live receivers.
4. Independent audio and modulation signals for both receivers.
5. No additional stream sessions for previews or prebuffering beyond that limit.

PR #12 is consistent with all five. This review adds what the existing code implies.

## 1. What the current code assumes, and what has to change

Facts below were read from the code on `p1/plugin-lifecycle`.

| Area | Today | Change needed for A/B | Risk |
|---|---|---|---|
| Receiver ownership | `Instrument` owns one `Radio`; `Engine::process` takes one stereo radio input plus the local test source | `Instrument` owns `Radio radio[2]`; the engine takes two radio inputs. B **disabled** = no thread work, no socket, no ring reads | Low |
| Receiver memory | Each `Radio` allocates a ring of `192000 × 8` frames per channel: **12 MB per receiver** (sized for 6 s at up to 192 kHz × 8 headroom) | Two receivers would be **24 MB per instance**, multiplied by plugin instances. Size the ring from the prepared rate (6 s at 96 kHz stereo is about 4.6 MB) before adding B | Medium (memory in multi-instance sessions) |
| Session count | One `Radio` cancels its current stream before starting the next, so normally one socket per receiver. **But** a `.m3u`/`.pls` playlist is followed by a *recursive* call made while the playlist's `WebInputStream` is still alive, so for a playlist station a receiver briefly holds two sockets | Close the playlist request before opening the stream it points to; count every socket a receiver opens (playlist, redirect, stream) against the budget. Otherwise two receivers on playlist stations can exceed "two sessions" | **Real defect against requirement 5**; small fix |
| Session budget enforcement | None (only one receiver exists) | A tiny shared budget object (`max 2`) that a receiver must hold a token from before connecting; tune-B-while-A-retunes cannot momentarily make three. Test it at the server, as PR #17 does (connection-counting log) | Medium (races) |
| Engine radio features | One follower pair (`rFast`, `rSlow`), one tuner-static source, one `gap` / `request` / off-centre `detune` in `Controls` | Duplicate per receiver: followers, static layer (with its own 8 s limit and PANIC mute), detune. Static must follow **each** receiver's real gap | Low |
| Patch ports | `radio.audio_out`, `radio.level_out`, `radio.activity_out` (+ `local.audio_out`); `RoutingPlan` holds 7 inputs; one cable per input | Add `radioB.audio_out`, `radioB.level_out`, `radioB.activity_out` (new stable IDs; existing ones keep meaning "A", so saved patches stay valid). Feeding A **and** B into one resonator input needs the P2 mixer/fan-in; until then A and B reach different inputs or a mixer node | Medium (UI and routing scope) |
| Plugin parameters | Ten host parameters; AU hosted order frozen | TUNE B, STATIC B, B enable, A/B mix: **version hint 2** so they sort after the frozen ten (Logic addresses AU parameters by index) | Low, if the rule is followed |
| Plugin lifecycle | One `ReceiverGate` decides when the receiver may touch the network (PR #17) | One gate for both receivers: B never connects before the gate is ACTIVE; suspension and release cancel both | Low |
| Saved state | Standalone stores six station URLs; the plugin stores only its ten parameters | Plugin state v2: the six-slot set (`StationRef`s), A/B slot selections, B enabled. Restoring never connects (already enforced by the gate). v1 state migrates with B off | Low |
| Catalog requests | None | A separate, cancellable directory service, used **only on explicit user action in an editor**. The plugin has no editor yet, so the catalog depends on the shared editor work. Catalog browsing never starts audio (requirement 5) | Medium (schedule) |
| Local / private URLs | The test aid `WILDBAND_TEST_STATIONS` and the local test server use `127.0.0.1` | PR #12 asks to block loopback/private addresses for directory entries. Keep that rule for catalog-sourced entries only; manual and test URLs stay possible with explicit intent, otherwise the qualification tools stop working | Low |
| Cleartext `http://` | Allowed by the standalone's Info.plist; unknown inside hosts | Catalog results will contain many `http://` streams. Measure in Logic/Live before promising them | Medium |

**Recommended order (none of it started):** fix the playlist double socket and size the ring from the rate (both
useful even with one receiver) → session budget + server-counted tests → second receiver in `Instrument`/`Engine`
behind B-disabled-by-default with A-only parity tests → new ports/parameters → set model in state → catalog service
once an editor exists.

## 2. Radio Browser feasibility plan (spike R0 — small, read-only)

**Goal:** decide with evidence whether Radio Browser can supply playable stations for WILDBAND's decoder, and what
the adapter must handle. **Not** a catalog implementation.

What is already known (checked 2026-11-27): DNS SRV `_api._tcp.radio-browser.info` returned **one** mirror
(`de1.api.radio-browser.info`, port 443); `all.api.radio-browser.info` resolved to one address. So the "random
mirror, fail over" design currently has a single target — failover must degrade to "provider unavailable", with
saved sets still playable.

### Questions to answer

1. Do mirror discovery and HTTPS requests work from macOS with a descriptive User-Agent, within a few seconds?
2. For a spread of countries and languages, what share of `stations/search` results (with `hidebroken=true`) are
   **direct MP3 or AAC over HTTP(S)** that `Radio` actually plays? How many are HLS, Ogg/Opus, FLAC, playlists,
   dead, geo-blocked, or mislabelled?
3. Time to first audio and failure classes for those streams, using the existing receiver probe.
4. Are `stationuuid` lookups stable and is `url_resolved` usable, or must the adapter resolve itself?
5. Paging, response sizes, rate behaviour, text-field oddities (encoding, length, HTML) for the DTO validation.

### Method

- A throwaway command-line probe (JUCE `juce_core` only, like `wildband_radio_probe`): SRV lookup → mirror →
  `GET /json/stations/search?countrycode=XX&hidebroken=true&order=clickcount&limit=20` for about ten countries
  across continents, plus a few `name=` and `tag=` searches; `GET /json/stations/byuuid/{uuid}` for a sample.
- Feed each result's `url_resolved` to the existing `Radio` class for 8 s, as `wildband_radio_probe` does; record
  state, detail, time to first audio. No audio is written anywhere.
- **Politeness:** sequential requests, at most about one per second, total well under a few hundred requests;
  User-Agent `WILDBAND/0.x (feasibility spike; https://github.com/shortwavlabs/wildband)`.
- **No click reporting** (`/json/url/{uuid}`) during the spike: no genuine user selection is happening.
- Record only aggregates and the station UUIDs/codecs used; do not commit bulk directory dumps or any audio.

### Pass / revise / stop

| Outcome | Meaning |
|---|---|
| **Pass** | Discovery and search work reliably; at least about half of the sampled `hidebroken` results in most regions play with the current MP3/AAC decoder; failure classes are recognisable from metadata or a short probe |
| **Revise** | Playable share is low because of HLS or Ogg/Opus: decide whether to filter by `codec`/`hls` in search (fewer stations) or to qualify another decoder (scope) |
| **Stop / fallback** | Provider unreliable from macOS: keep curated factory sets plus manual URLs (already independent of any provider) and revisit |

### Deliverables

A short report (counts, shares, timings, failure classes, mirror behaviour, observed field problems), the exact
queries and timestamps, and a recommendation for the adapter's filters and validation. Estimated effort: about half
a day including the probe. Owner decision before any catalog UI or dual-receiver code.

### Constraints carried over

Directory data is described by the project as public domain; that does not license station logos, audio or
rebroadcast. Text metadata only; credit Radio Browser in the app. No account, no API key, no hosted backend.
