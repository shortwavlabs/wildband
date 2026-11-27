// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Stephane P. Pericat
//
// WILDBAND voice experiment P1-R: an adapter around the Rings resonator DSP by Emilie Gillet (Mutable Instruments),
// which is MIT-licensed and is NOT part of this repository: it is compiled, unmodified, from upstream
// (pichenettes/eurorack at f4cfa76, stmlib at e3bd7c9). This file is WILDBAND's own and uses only upstream's public
// interface, rings::Part. Provenance, the upstream assumptions handled here, and the tests: docs/p1/RINGS-EXPERIMENT.md
//
// Upstream works at 48 kHz in blocks of at most 24 samples, so this adapter collects 24 samples, processes them and
// plays them out: 24 samples of latency whatever the host block size. No allocation after construction.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

#include "rings/dsp/part.h"

namespace wildband
{

class RingsVoice
{
public:
    static constexpr int kBlock = (int) rings::kMaxBlockSize;      // 24: also the latency in samples
    enum Model : int { kModal, kSympathetic, kString };

    struct Params
    {
        float note = 48.0f;          // MIDI note
        float structure = 0.4f, brightness = 0.6f, damping = 0.6f, position = 0.3f;   // 0..1, upstream's four macros
        float drive = 1.0f;          // input gain
        int model = kModal;
    };

    RingsVoice() : part (std::make_unique<rings::Part>()), reverb (std::make_unique<uint16_t[]> (kReverbSize)) { reset(); }

    /** `sampleRate` only sets the pitch correction: upstream itself always runs as if at 48 kHz. */
    void prepare (double sampleRate) noexcept { pitchOffset = 12.0f * (float) std::log2 (48000.0 / sampleRate); reset(); }

    void reset() noexcept
    {
        std::memset (reverb.get(), 0, kReverbSize * sizeof (uint16_t));
        std::memset (static_cast<void*> (part.get()), 0, sizeof (rings::Part));
        part->Init (reverb.get());
        part->set_polyphony (1);
        model = -1;
        std::fill (std::begin (in), std::end (in), 0.0f);
        std::fill (std::begin (outL), std::end (outL), 0.0f);
        std::fill (std::begin (outR), std::end (outR), 0.0f);
        fill = 0;
    }

    /** One sample in, one sample out (24 samples late). */
    void process (float input, float& left, float& right, const Params& p) noexcept
    {
        left = outL[fill];
        right = outR[fill];
        in[fill] = std::isfinite (input) ? std::min (4.0f, std::max (-4.0f, input * p.drive)) : 0.0f;
        if (++fill < kBlock) return;
        fill = 0;

        const int wanted = std::min (2, std::max (0, p.model));
        if (wanted != model)
        {
            model = wanted;
            part->set_model (model == kModal ? rings::RESONATOR_MODEL_MODAL : model == kSympathetic ? rings::RESONATOR_MODEL_SYMPATHETIC_STRING : rings::RESONATOR_MODEL_STRING);
        }
        rings::Patch patch;
        patch.structure = clamp01 (p.structure) * 0.9995f;
        patch.brightness = clamp01 (p.brightness);
        patch.damping = clamp01 (p.damping);
        patch.position = clamp01 (p.position);
        rings::PerformanceState state;
        state.strum = false;                  // the broadcast is the exciter: nothing is strummed internally
        state.internal_exciter = false;
        state.internal_strum = false;
        state.internal_note = false;
        state.tonic = 0.0f;
        state.note = std::min (108.0f, std::max (12.0f, p.note + pitchOffset));
        state.fm = 0.0f;
        state.chord = 0;
        part->Process (state, patch, in, outL, outR, (size_t) kBlock);
        for (int i = 0; i < kBlock; ++i)      // never let a non-finite sample out of the adapter
        {
            if (! std::isfinite (outL[i])) outL[i] = 0.0f;
            if (! std::isfinite (outR[i])) outR[i] = 0.0f;
        }
    }

    static size_t memoryBytes() noexcept { return sizeof (rings::Part) + kReverbSize * sizeof (uint16_t) + sizeof (RingsVoice); }

private:
    static constexpr size_t kReverbSize = 32768;
    static float clamp01 (float v) noexcept { return std::isfinite (v) ? std::min (1.0f, std::max (0.0f, v)) : 0.0f; }
    std::unique_ptr<rings::Part> part;
    std::unique_ptr<uint16_t[]> reverb;
    float in[kBlock], outL[kBlock], outR[kBlock];
    float pitchOffset = 0.0f;
    int fill = 0, model = -1;
};

} // namespace wildband
