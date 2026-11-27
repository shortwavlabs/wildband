// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Stephane P. Pericat
// FROZEN COPY of src/Patch.h at 9c0a409, in namespace wildband_baseline. Used only by the tests. Do not edit.
// WILDBAND P0 patch graph: a narrow adapter modelled on TALORA's routing layer (shortwavlabs/modulo,
// src/engine/Routing.h at 91cc748). Same concepts, names and rules - typed ports with stable "module.port" ids, a
// fixed-capacity value-type Graph that validates every cable off the audio thread, inputs take one cable, outputs fan
// out without limit, and the audio thread only ever sees a plain-data RoutingPlan. Graph::check below is re-typed from
// TALORA's, rule for rule; no TALORA file is included (modulo is a private repository without a licence file, this
// one is public - whether even this much may be published is the owner's decision). What differs and why:
// docs/p0/TALORA-REUSE.md. tests/TaloraParity.cpp checks this Graph against the real one when TALORA is on disk.
#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>

namespace wildband_baseline
{

enum class PortType : uint8_t { Audio, CV };
enum class PortDirection : uint8_t { Input, Output };

struct PortSpec
{
    static constexpr int kUnlimited = -1;
    const char* id;       // stable: what a saved patch stores. Never rename one.
    const char* label;
    int module;
    PortDirection direction;
    PortType type;
    int maxConnections;   // inputs 1 (no fan-in), outputs kUnlimited
};

enum Module : int { kModRadio, kModLocal, kModRoom, kModResonator, kNumModules };
inline constexpr const char* moduleNames[kNumModules] = { "RADIO", "LOCAL", "ROOM", "RESONATOR" };

enum Port : int
{
    // outputs. CV here is unipolar 0..1, already bounded and smoothed by its source.
    kRadioAudioOut, kLocalAudioOut,
    kRadioLevelOut, kRadioActivityOut, kRoomLevelOut, kRoomActivityOut, kRoomBrightnessOut,
    // inputs
    kResAudioIn, kResPitchIn, kResShapeIn, kResDampIn, kResExciteIn, kResDriftIn, kResFeedbackIn,
    kNumPorts
};
constexpr int kFirstInput = kResAudioIn, kNumInputs = kNumPorts - kFirstInput;
constexpr int kFirstCvOut = kRadioLevelOut, kNumCvOuts = kFirstInput - kFirstCvOut;

inline constexpr PortSpec ports[kNumPorts] = {
    { "radio.audio_out", "AUDIO", kModRadio, PortDirection::Output, PortType::Audio, PortSpec::kUnlimited },
    { "local.audio_out", "AUDIO", kModLocal, PortDirection::Output, PortType::Audio, PortSpec::kUnlimited },
    { "radio.level_out", "LEVEL", kModRadio, PortDirection::Output, PortType::CV, PortSpec::kUnlimited },
    { "radio.activity_out", "HITS", kModRadio, PortDirection::Output, PortType::CV, PortSpec::kUnlimited },
    { "room.level_out", "LEVEL", kModRoom, PortDirection::Output, PortType::CV, PortSpec::kUnlimited },
    { "room.activity_out", "HITS", kModRoom, PortDirection::Output, PortType::CV, PortSpec::kUnlimited },
    { "room.brightness_out", "BRIGHT", kModRoom, PortDirection::Output, PortType::CV, PortSpec::kUnlimited },
    { "resonator.audio_in", "IN", kModResonator, PortDirection::Input, PortType::Audio, 1 },
    { "resonator.pitch_cv_in", "PITCH", kModResonator, PortDirection::Input, PortType::CV, 1 },
    { "resonator.shape_cv_in", "SHAPE", kModResonator, PortDirection::Input, PortType::CV, 1 },
    { "resonator.damp_cv_in", "TONE", kModResonator, PortDirection::Input, PortType::CV, 1 },
    { "resonator.excite_cv_in", "STRIKE", kModResonator, PortDirection::Input, PortType::CV, 1 },
    { "resonator.drift_cv_in", "DRIFT", kModResonator, PortDirection::Input, PortType::CV, 1 },
    { "resonator.feedback_cv_in", "FEEDBACK", kModResonator, PortDirection::Input, PortType::CV, 1 } };

inline int findPort (const char* id) noexcept
{
    for (int i = 0; i < kNumPorts; ++i) if (std::strcmp (ports[i].id, id) == 0) return i;
    return -1;
}

// TALORA's ConnectError names. Cycle and Reserved are absent: no WILDBAND module has both inputs and outputs, and
// there are no reserved ports, so neither can occur with this registry.
enum class ConnectError : uint8_t { None, UnknownPort, OutputToOutput, InputToInput, Reversed, TypeMismatch, FanIn, FanOut, SelfConnection, Duplicate, Full, NotConnected };
inline const char* describe (ConnectError e) noexcept
{
    static constexpr const char* text[] = { "ok", "unknown port", "two outputs", "two inputs", "drag from an output to an input",
        "audio and control signals do not mix", "that input already has a cable", "that output is full", "same module", "already connected", "no free cables", "not connected" };
    return text[(int) e];
}

struct Connection { int8_t from = -1, to = -1; };

/** The patch. A fixed-size value: copying or editing one never allocates. Edited on the message thread only. */
class Graph
{
public:
    static constexpr int kMaxConnections = 16;

    ConnectError check (int from, int to) const noexcept
    {
        if (from < 0 || from >= kNumPorts || to < 0 || to >= kNumPorts) return ConnectError::UnknownPort;
        const auto& a = ports[from];
        const auto& b = ports[to];
        const bool aOut = a.direction == PortDirection::Output, bOut = b.direction == PortDirection::Output;
        if (aOut && bOut) return ConnectError::OutputToOutput;
        if (! aOut && ! bOut) return ConnectError::InputToInput;
        if (! aOut) return ConnectError::Reversed;
        if (a.type != b.type) return ConnectError::TypeMismatch;
        if (a.module == b.module) return ConnectError::SelfConnection;
        if (contains (from, to)) return ConnectError::Duplicate;
        int fanIn = 0, fanOut = 0;
        for (int i = 0; i < count; ++i) { fanIn += conns[i].to == to; fanOut += conns[i].from == from; }
        if (b.maxConnections != PortSpec::kUnlimited && fanIn >= b.maxConnections) return ConnectError::FanIn;
        if (a.maxConnections != PortSpec::kUnlimited && fanOut >= a.maxConnections) return ConnectError::FanOut;
        if (count == kMaxConnections) return ConnectError::Full;
        return ConnectError::None;
    }
    ConnectError connect (int from, int to) noexcept
    {
        const auto e = check (from, to);
        if (e == ConnectError::None) conns[count++] = { (int8_t) from, (int8_t) to };
        return e;
    }
    ConnectError disconnect (int from, int to) noexcept
    {
        for (int i = 0; i < count; ++i)
            if (conns[i].from == from && conns[i].to == to) { conns[i] = conns[--count]; return ConnectError::None; }
        return ConnectError::NotConnected;
    }
    void clear() noexcept { count = 0; }
    int size() const noexcept { return count; }
    const Connection& operator[] (int i) const noexcept { return conns[i]; }
    bool contains (int from, int to) const noexcept
    {
        for (int i = 0; i < count; ++i) if (conns[i].from == from && conns[i].to == to) return true;
        return false;
    }
    int cableInto (int input) const noexcept
    {
        for (int i = 0; i < count; ++i) if (conns[i].to == input) return conns[i].from;
        return -1;
    }

    /** The instrument as it starts: every cable is real and can be pulled out. radio.level_out feeds two inputs. */
    static Graph defaultPatch() noexcept
    {
        Graph g;
        g.connect (kRadioAudioOut, kResAudioIn);
        g.connect (kRadioActivityOut, kResExciteIn);
        g.connect (kRadioLevelOut, kResDampIn);
        g.connect (kRadioLevelOut, kResDriftIn);
        g.connect (kRoomLevelOut, kResFeedbackIn);
        g.connect (kRoomActivityOut, kResPitchIn);
        g.connect (kRoomBrightnessOut, kResShapeIn);
        return g;
    }

private:
    Connection conns[kMaxConnections];
    int count = 0;
};

/** What the audio thread runs: the source port of each input, -1 = nothing plugged in. Seven bytes of plain data, so
    it travels in one atomic word (TALORA's larger plan uses a triple buffer; the hand-off is wait-free either way). */
struct RoutingPlan
{
    int8_t source[kNumInputs] = { -1, -1, -1, -1, -1, -1, -1 };

    static RoutingPlan fromGraph (const Graph& g) noexcept
    {
        RoutingPlan p;
        for (int i = 0; i < kNumInputs; ++i) p.source[i] = (int8_t) g.cableInto (kFirstInput + i);
        return p;
    }
    int sourceOf (int input) const noexcept { return source[input - kFirstInput]; }
    uint64_t pack() const noexcept { uint64_t v = 0; std::memcpy (&v, source, sizeof (source)); return v; }
    static RoutingPlan unpack (uint64_t v) noexcept { RoutingPlan p; std::memcpy (p.source, &v, sizeof (p.source)); return p; }
};
static_assert (sizeof (RoutingPlan::source) <= sizeof (uint64_t));

} // namespace wildband_baseline
