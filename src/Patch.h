// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Stephane P. Pericat
// WILDBAND P0 patch graph: a narrow adapter modelled on TALORA's routing layer (shortwavlabs/modulo,
// src/engine/Routing.h at 91cc748). Same concepts, names and rules - typed ports with stable "module.port" ids, a
// fixed-capacity value-type Graph that validates every cable off the audio thread, inputs take one cable, outputs fan
// out without limit, and the audio thread only ever sees a plain-data RoutingPlan. Graph::check below is re-typed from
// TALORA's, rule for rule, and PlanMailbox follows TALORA's plan hand-off; no TALORA file is included. TALORA is a
// private repository by the same author, who authorized reuse of its architectural concepts here (2026-11-27) and
// releases this file under AGPL-3.0-only. That authorization does not extend to TALORA's codebase. What differs and why:
// docs/p0/TALORA-REUSE.md. tests/TaloraParity.cpp checks this Graph against the real one when TALORA is on disk.
#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace wildband
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
// NoDepth is WILDBAND's own addition (appended, so the shared values keep their numbers): a depth was given for a
// cable that cannot have one.
enum class ConnectError : uint8_t { None, UnknownPort, OutputToOutput, InputToInput, Reversed, TypeMismatch, FanIn, FanOut, SelfConnection, Duplicate, Full, NotConnected, NoDepth };
inline const char* describe (ConnectError e) noexcept
{
    static constexpr const char* text[] = { "ok", "unknown port", "two outputs", "two inputs", "drag from an output to an input",
        "audio and control signals do not mix", "that input already has a cable", "that output is full", "same module", "already connected", "no free cables", "not connected", "audio cables have no depth" };
    return text[(int) e];
}

/** A cable. `depth` is the attenuverter of a control cable: -1 .. +1, where +1 is the original behaviour, 0 is no
    influence and negative values reverse it. Audio cables always carry +1 (they have no depth). */
struct Connection { int8_t from = -1, to = -1; float depth = 1.0f; };

inline float validDepth (float depth) noexcept { return std::isfinite (depth) ? std::min (1.0f, std::max (-1.0f, depth)) : 1.0f; }

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
    ConnectError connect (int from, int to, float depth = 1.0f) noexcept
    {
        const auto e = check (from, to);
        if (e == ConnectError::None) conns[count++] = { (int8_t) from, (int8_t) to, ports[to].type == PortType::CV ? validDepth (depth) : 1.0f };
        return e;
    }
    /** Sets a control cable's depth (clamped to -1 .. +1). Non-finite values and audio cables are refused. */
    ConnectError setDepth (int from, int to, float depth) noexcept
    {
        for (int i = 0; i < count; ++i)
            if (conns[i].from == from && conns[i].to == to)
            {
                if (ports[to].type != PortType::CV || ! std::isfinite (depth)) return ConnectError::NoDepth;
                conns[i].depth = validDepth (depth);
                return ConnectError::None;
            }
        return ConnectError::NotConnected;
    }
    /** Depth of the cable into `input`; +1 if there is none. */
    float depthInto (int input) const noexcept
    {
        for (int i = 0; i < count; ++i) if (conns[i].to == input) return conns[i].depth;
        return 1.0f;
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

/** What the audio thread runs: for each input, the port plugged into it (-1 = nothing) and that cable's depth.
    Plain data, built from a validated Graph off the audio thread. */
struct RoutingPlan
{
    int8_t source[kNumInputs] = { -1, -1, -1, -1, -1, -1, -1 };
    float depth[kNumInputs] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };

    static RoutingPlan fromGraph (const Graph& g) noexcept
    {
        RoutingPlan p;
        for (int i = 0; i < kNumInputs; ++i)
        {
            p.source[i] = (int8_t) g.cableInto (kFirstInput + i);
            p.depth[i] = g.depthInto (kFirstInput + i);
        }
        return p;
    }
    int sourceOf (int input) const noexcept { return source[input - kFirstInput]; }
    float depthOf (int input) const noexcept { return depth[input - kFirstInput]; }
};

/** Hands the latest plan from one writer thread (the UI) to one reader thread (audio), whole: a reader never sees one
    cable's source with another plan's depth. Wait-free on both sides, no allocation. This is the standard three-slot
    "latest value" exchange (TALORA hands its plans over the same way); with depths the plan no longer fits in the
    single atomic word P0 used. */
class PlanMailbox
{
public:
    /** Writer thread only. */
    void publish (const RoutingPlan& plan) noexcept
    {
        slots[back] = plan;
        back = middle.exchange (back | kFresh, std::memory_order_acq_rel) & kIndex;
    }
    /** Reader thread only: the most recently published plan. */
    const RoutingPlan& read() noexcept
    {
        if ((middle.load (std::memory_order_relaxed) & kFresh) != 0)
            front = middle.exchange (front, std::memory_order_acq_rel) & kIndex;
        return slots[front];
    }

private:
    static constexpr int kIndex = 3, kFresh = 4;
    RoutingPlan slots[3];
    std::atomic<int> middle { 1 };
    int back = 0, front = 2;      // owned by the writer / by the reader
};

// ---- persistence. Two strings, on purpose:
//   cables: "from|to;from|to;..."            exactly what P0 wrote, so the P0 build still reads a patch saved here
//   depths: "from|to|depth;from|to|depth;..." read only by builds that know about depths
// (P0 takes everything after the first '|' as the input's id, so a depth inside `cables` would make it drop every
// cable and then save the empty patch.) A patch with no `depths` loads at +1. Anything invalid is dropped or
// corrected; the result is always a valid Graph.
inline std::string savePatch (const Graph& g, bool withDepths = false)
{
    std::string text;
    for (int i = 0; i < g.size(); ++i)
    {
        char depth[32] = "";
        if (withDepths) std::snprintf (depth, sizeof (depth), "|%.4f", (double) g[i].depth);
        text += std::string (i ? ";" : "") + ports[g[i].from].id + "|" + ports[g[i].to].id + depth;
    }
    return text;
}

inline Graph loadPatch (const std::string& text)
{
    Graph g;
    for (size_t at = 0; at <= text.size();)
    {
        const size_t end = std::min (text.find (';', at), text.size());
        const std::string cable = text.substr (at, end - at);
        at = end + 1;
        const size_t bar1 = cable.find ('|'), bar2 = bar1 == std::string::npos ? bar1 : cable.find ('|', bar1 + 1);
        if (bar1 == std::string::npos) continue;
        const std::string from = cable.substr (0, bar1), to = cable.substr (bar1 + 1, bar2 == std::string::npos ? bar2 : bar2 - bar1 - 1);
        float depth = 1.0f;
        if (bar2 != std::string::npos)
        {
            char* parsed = nullptr;
            const double value = std::strtod (cable.c_str() + bar2 + 1, &parsed);
            if (parsed != cable.c_str() + bar2 + 1) depth = validDepth ((float) value);     // unreadable: keep +1
        }
        g.connect (findPort (from.c_str()), findPort (to.c_str()), depth);                  // every rule of connect() applies
    }
    return g;
}

/** Loads `cables` (topology) and then applies `depths` to the cables that exist in it. */
inline Graph loadPatch (const std::string& cables, const std::string& depths)
{
    Graph g = loadPatch (cables);
    const Graph wanted = loadPatch (depths);
    for (int i = 0; i < wanted.size(); ++i) g.setDepth (wanted[i].from, wanted[i].to, wanted[i].depth);   // ignored unless that cable is there
    return g;
}

} // namespace wildband
