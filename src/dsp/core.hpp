// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#pragma once
#include "model/parameters.hpp"
#include "dsp/neural.hpp"
#include "dsp/nr_modes.hpp"
#include "dsp/wiener.hpp"
#include "dsp/speex.hpp"
#include "dsp/deep_filter.hpp"
#include "dsp/agc.hpp"
#include <array>
namespace qrmforge {
Parameters sanitize(Parameters values, float sampleRate) noexcept;
Parameters preset(int selection) noexcept;
struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    float process(float value) noexcept;
    void configure(int type, float frequency, float rate, float quality = 0.70710678f) noexcept;
};
std::array<Biquad, 4> bandwidthFilters(const Parameters& parameters, float sampleRate) noexcept;
class NoiseBlanker {
public:
    explicit NoiseBlanker(float sampleRate) noexcept;
    float process(float sample, float threshold) noexcept;
private:
    float envelope = 0;
    float previous = 0;
    float averaging;
    unsigned warmup;
    unsigned maximumBlank;
    unsigned blanked = 0;
};
class Core {
public:
    explicit Core(float sampleRate) noexcept : rate(sampleRate), blanker(sampleRate), neural(sampleRate), speech(sampleRate), agc(sampleRate) {}
    bool neuralAvailable() const noexcept { return neural.available(); }
    std::uint32_t availableModes() const noexcept;
    bool deepFilterStarting() const noexcept { return deep && deep->starting(); }
    bool deepFilterSelectable() const noexcept { return deep && (deep->starting() || deep->available()); }
    bool prepareDeepFilter() noexcept;
    std::uint32_t droppedFrames() const noexcept { return deep ? deep->droppedFrames() : 0; }
    Parameters effectiveParameters(Parameters parameters) const noexcept;
    void process(const float* input, float* output, unsigned long count,
                 Parameters parameters, Telemetry* telemetry = nullptr) noexcept;
private:
    float rate;
    NoiseBlanker blanker;
    Neural neural;
    Wiener wiener;
    SpeexDenoiser speech;
    std::unique_ptr<DeepFilter> deep;
    AudioAgc agc;
    NoiseReduction activeMode = NoiseReduction::Off;
    std::array<Biquad, 6> filters{};
    std::array<Biquad, 6> targets{};
    std::array<float, 2> steepMix{};
    float preGain = 1, postGain = 1;
    Parameters previous{};
    bool configured = false;
    std::array<float, 96> weights{};
    std::array<float, 120> delay{};
    unsigned position = 0;
    float reduce(float value, float level) noexcept;
};
}
