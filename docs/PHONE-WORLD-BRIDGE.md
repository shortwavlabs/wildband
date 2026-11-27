# WILDBAND — iPhone WORLD Bridge

**Status:** proposed post-P0 feasibility experiment. No iOS application, pairing implementation or sensor performance is claimed. Keep the current macOS permission/output fix and owner audition on their existing track.

## Musical intent

An iPhone can become both a portable probe of the physical world and a handheld gestural controller. Place it near ordinary objects to observe local magnetic-field variations, move or tilt it to perform, or leave it stationary as an environmental source. A phone is optional: the Mac instrument, radio, MIDI and output must remain independently usable. No phone is required to play WILDBAND.

The central distinction is **WORLD (observed physical conditions)** versus **PLAYER (intentional intervention)**. The same sensor may be used in either role, but the instrument must not label synthetic randomness as a measured physical signal. The patch preserves relationships, not the performance.

## Candidate signals

| Source | Proposed patchable signals | Meaning |
| --- | --- | --- |
| Magnetometer | MAG X/Y/Z, MAG FIELD, MAG CHANGE | Device-axis magnetic flux density in microtesla, magnitude and calibrated variation |
| Accelerometer / processed device motion | ACCEL X/Y/Z, MOTION | Movement and optionally gravity-removed acceleration; distinguish raw acceleration from processed data |
| Gyroscope | ROT X/Y/Z | Angular velocity; expressive rotation gestures |
| Device attitude | TILT, ROLL, PITCH | Relative orientation; calibrate neutral pose, avoid wrap discontinuities |
| Barometer (optional later) | PRESSURE, PRESSURE CHANGE | Slow local environmental input when supported; not required for the initial spike |

Check device/API availability at runtime via Core Motion; no universal sensor availability assumption. Use relative magnetic magnitude/baseline calibration to reduce orientation and device bias where appropriate; axis values are device-frame readings, not an absolute world-coordinate field. Magnetometers measure magnetic flux density, not a broadband RF spectrum, Wi-Fi packets or an audio-frequency electromagnetic pickup. Do not present them as such. Avoid experiments involving strong magnets or sensitive equipment.

## First prototype: reuse before building an app

1. After the current P0 owner listening gate, evaluate a generic iOS sensor-to-OSC sender that actually exposes magnetometer and motion readings. Confirm available signal fields, update rate, license and privacy behavior. Do not assume a particular existing app supports all fields.
2. Add a narrowly scoped local OSC receiver spike to macOS, with a versioned signal schema and normalized WORLD ports. Try one magnetic-field channel and one motion channel first. An existing sender may be enough to test the music without a bespoke iOS build.
3. Compare a stationary environmental probe with intentional handheld gestures; use an unchanged live-radio patch with phone influence on/off. Measure delivery latency, jitter, packet loss, and rate stability; start with a candidate 20–50 Hz control update rate, not a guaranteed sensor or network rate.
4. Only if owner audition establishes clear musical value, evaluate a minimal Swift/SwiftUI companion using Core Motion and local networking. Keep iOS app build/signing/distribution decisions separate; no App Store release commitment.

## Candidate companion experience

A tiny physical instrument interface: **PAIR / START / STOP**, explicit sensor selection, neutral-pose/baseline calibration, sensitivity and live status. No accounts, sample library, location tracking, microphone/camera collection or cloud relay in this experiment. A playful visual language can echo WILDBAND's proposed illuminated instrument display, but prioritize discoverable connection and safety states.

Two modes: **PROBE** (leave the phone in a place and use calibrated field changes) and **GESTURE** (move/tilt the phone to steer patchable controls). These are UX modes, not a claim of new sensor capabilities. Optional touchscreen controls are later research; existing physical MIDI remains first-class.

## Architecture and transport

iPhone Core Motion / verified OSC sender → explicit opt-in local connection → non-audio-thread receiver → validated latest-value snapshots → SENSE calibration (offset, sensitivity, polarity, clamp, smoothing) → typed WORLD CV/gates → DSP.

For the spike, OSC over local unicast UDP with manual host/port and explicit source allow-list is a candidate; plain OSC/UDP is not authenticated or encrypted, and IP filtering is not strong authentication. Use only on an appropriate trusted test LAN, with receiver disabled by default. A product companion must implement deliberate pairing (e.g. short-lived code/QR plus authenticated session and appropriate secure transport), explicit connect/disconnect, and no unauthenticated LAN control. Bonjour discovery is optional after permission and entitlement review; do not auto-connect to nearby devices. Evaluate latency vs reliability before fixing a transport.

Keep packets finite and size/rate bounded. Validate protocol version, schema, units, source/session identity, sequence numbers, timestamp plausibility and finite values. Use local monotonic receipt time for freshness; do not compare unsynchronized clocks as though identical. Drop old/out-of-order samples; smooth values at the control/DSP boundary; ramp unavailable signals to neutral after a configurable timeout. All networking, parsing, discovery, permission and allocation work stays off the realtime audio thread. Display CONNECTED / STALE / DISCONNECTED / DENIED; show actual activity, not fabricated readings. No raw sensor history or device identifiers in saved patches. A saved patch may remember a generic phone port mapping and calibration settings; never auto-start phone sensing on recall.

Apple references:
- [Core Motion](https://developer.apple.com/documentation/coremotion/)
- [CMMotionManager](https://developer.apple.com/documentation/coremotion/cmmotionmanager)
- [TN3179: Local network privacy](https://developer.apple.com/documentation/technotes/tn3179-understanding-local-network-privacy)
- [NSLocalNetworkUsageDescription](https://developer.apple.com/documentation/bundleresources/information-property-list/nslocalnetworkusagedescription)

Review OS-specific permissions and usage descriptions on both platforms, including Bonjour declarations if used. Test foreground/background, lock, interruption, Wi-Fi changes and permission denial on actual devices. Do not promise reliable background streaming before validation.

## Musical experiments (unvalidated)

- **Magnetic wand:** MAG CHANGE → resonator coupling / Korupt tracking rate, live radio as audio excitation. Move the phone near ordinary low-field objects and listen for a deliberate, reversible response.
- **Tilt the world:** relative tilt → Wyrd delay time or reverb damping; MIDI fader controls depth so the player can intervene without looking at either screen.
- **Room compass:** stationary phone magnetic variation → Drift's external CV blend, with radio envelope also participating. Distinguish live magnetic measurements from Drift's pseudo-random output.
- **Shaken radio:** motion/onset detector → agitation/strike gate with debouncing and bounded event rate. Gentle intentional movement only; no physical contact or force required.

## Acceptance and non-goals

- Demonstrate real sensor measurements and causality: matched live-radio patch, phone route connected/disconnected, stationary vs intentional gesture, at least two clearly controllable musical outcomes. Owner wants to continue playing.
- Phone off/denied/disconnected/stale/locked or malformed input must not disable radio, MIDI, output or the Mac app. No automatic permission prompt on Mac startup.
- Prove actual-device sensor support, usable calibration, network latency/jitter, recovery, safe modulation ranges, no audio-thread network work, and correct UI state. Measure on real hardware rather than marking tests passed from a simulated sender.
- Do not build a second standalone instrument, a universal sensor service, full Bluetooth pairing, remote internet control, raw RF receiver, recorder, or iOS release as a prerequisite.
- Reuse existing OSC tooling and WILDBAND's typed WORLD/CV plumbing where qualified; no duplication of DSP on iOS. Promote only if the outside-world/performer relationship becomes more compelling.
