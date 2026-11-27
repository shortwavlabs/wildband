// Optional (cmake -DTALORA_DIR=/path/to/modulo): drives WILDBAND's Graph (src/Patch.h) and TALORA's real Graph
// (modulo/src/engine/Routing.cpp) with the same port registry and the same random edits, and requires the same
// verdict and the same resulting patch every time. Exit code = number of disagreements.
#include "../src/Patch.h"
#include "Routing.h"

#include <cstdio>
#include <random>

static wildband::ConnectError translate (modulo::ConnectError e)
{
    using M = modulo::ConnectError;
    using W = wildband::ConnectError;
    switch (e)
    {
        case M::None: return W::None;                     case M::UnknownPort: return W::UnknownPort;
        case M::OutputToOutput: return W::OutputToOutput; case M::InputToInput: return W::InputToInput;
        case M::Reversed: return W::Reversed;             case M::TypeMismatch: return W::TypeMismatch;
        case M::FanIn: return W::FanIn;                   case M::FanOut: return W::FanOut;
        case M::SelfConnection: return W::SelfConnection; case M::Duplicate: return W::Duplicate;
        case M::Full: return W::Full;                     case M::NotConnected: return W::NotConnected;
        default: return (W) 255;                          // Cycle, Reserved, ...: WILDBAND has no such verdict
    }
}

int main()
{
    modulo::PortSpec specs[wildband::kNumPorts];
    for (int i = 0; i < wildband::kNumPorts; ++i)
    {
        const auto& p = wildband::ports[i];
        specs[i] = { p.id, p.module, p.direction == wildband::PortDirection::Output ? modulo::PortDirection::Output : modulo::PortDirection::Input,
                     p.type == wildband::PortType::Audio ? modulo::PortType::Audio : modulo::PortType::CV, 0.0f, 1.0f, "FS",
                     p.maxConnections, modulo::PortSpec::kNoNormal, modulo::PortCapability::Active, 1 };
    }
    const modulo::PortRegistry registry { wildband::moduleNames, wildband::kNumModules, specs, wildband::kNumPorts };

    int failures = 0, checks = 0;
    std::mt19937 rng (11);
    for (int trial = 0; trial < 2000; ++trial)
    {
        modulo::Graph theirs (registry);
        wildband::Graph ours;
        for (int step = 0; step < 60; ++step)
        {
            const int from = (int) (rng() % (wildband::kNumPorts + 2)) - 1, to = (int) (rng() % (wildband::kNumPorts + 2)) - 1;   // includes out-of-range ports
            const bool connect = rng() % 4 != 0;
            // WILDBAND's capacity is 16 cables, TALORA's 64: "Full" is the one verdict allowed to differ, so stay below both
            if (connect && ours.size() >= wildband::Graph::kMaxConnections) continue;
            const auto a = connect ? ours.connect (from, to) : ours.disconnect (from, to);
            const auto b = translate (connect ? theirs.connect (from, to) : theirs.disconnect (from, to));
            ++checks;
            bool same = a == b && ours.size() == theirs.size();
            for (int in = wildband::kFirstInput; same && in < wildband::kNumPorts; ++in) same = ours.cableInto (in) == theirs.cableInto (in);
            if (! same && ++failures < 10) std::printf ("  DISAGREE trial %d step %d: %s %d -> %d: ours %d, TALORA %d\n", trial, step, connect ? "connect" : "disconnect", from, to, (int) a, (int) b);
        }
    }
    std::printf ("%d edits compared with TALORA's Graph, %d disagreements\n", checks, failures);
    return failures;
}
