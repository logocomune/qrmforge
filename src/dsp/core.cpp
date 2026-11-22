// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#include "dsp/core.hpp"
#include <algorithm>
#include <cmath>
namespace qrmforge {
Parameters sanitize(Parameters values, float rate) noexcept {
    constexpr Parameters minimum{0,-20,50,400,0,200,0,300,0,0.1f,-20,0,2,0,0,0,0,0};
    constexpr Parameters maximum{1,20,1500,4500,1,3500,1,1200,float(noiseReductionCount-1),10,20,1,20,1,1,1,1,1};
    for (int i = 0; i < ParameterCount; ++i)
        values[i] = std::clamp(std::isfinite(values[i]) ? values[i] : defaults[i], minimum[i], maximum[i]);
    float upper = std::max(1.0f, rate * 0.45f);
    for (auto parameter : {LowCut, HighCut, NotchHz, ApfHz})
        values[parameter] = std::min(values[parameter], upper);
    values[NrMode] = std::round(values[NrMode]);
    for (auto parameter : {LowCutSteep, HighCutSteep, AgcOn})
        values[parameter] = values[parameter] >= 0.5f ? 1 : 0;
    values[LowCut] = std::min(values[LowCut], values[HighCut]);
    return values;
}
NoiseBlanker::NoiseBlanker(float sampleRate) noexcept
    : averaging(1 - std::exp(-1 / (sampleRate * 0.02f))),
      warmup(unsigned(sampleRate * 0.02f)),
      maximumBlank(std::max(1u, unsigned(sampleRate * 0.0005f))) {}
float NoiseBlanker::process(float sample, float threshold) noexcept {
    const float magnitude = std::abs(sample);
    if (warmup == 0 && magnitude > std::max(envelope, 0.0001f) * threshold
        && blanked < maximumBlank) {
        ++blanked;
        return previous;
    }
    if (warmup > 0) --warmup;
    blanked = 0;
    envelope += averaging * (magnitude - envelope);
    previous = sample;
    return sample;
}
Parameters preset(int selection) noexcept {
    auto result = defaults;
    if (selection == 0) { result[LowCut] = 100; result[HighCut] = 4500; }
    else if (selection == 1) { result[LowCut] = 300; result[HighCut] = 3000; }
    else { result[LowCut] = 300; result[HighCut] = 2700; }
    return result;
}
float Biquad::process(float value) noexcept {
    float result = b0 * value + z1;
    z1 = b1 * value - a1 * result + z2;
    z2 = b2 * value - a2 * result;
    if (std::abs(z1) < 1e-20f) z1 = 0;
    if (std::abs(z2) < 1e-20f) z2 = 0;
    return result;
}
void Biquad::configure(int type, float frequency, float rate, float quality) noexcept {
    const float angle = 6.283185307179586f * frequency / rate;
    float cosine = std::cos(angle);
    quality = type == 2 ? 12 : (type == 3 ? 18 : quality);
    float alpha = std::sin(angle) / (2 * quality);
    float amplitude = std::pow(10.0f, 10.0f / 40);
    float denominator = 1 + (type == 3 ? alpha / amplitude : alpha);
    a1 = -2 * cosine / denominator;
    a2 = (1 - (type == 3 ? alpha / amplitude : alpha)) / denominator;
    if (type < 2) {
        b0 = (1 + (type == 0 ? cosine : -cosine)) / (2 * denominator);
        b1 = (type == 0 ? -2 : 2) * b0;
        b2 = b0;
    } else if (type == 2) {
        b0 = b2 = 1 / denominator; b1 = a1;
    } else {
        b0 = (1 + alpha * amplitude) / denominator;
        b1 = a1; b2 = (1 - alpha * amplitude) / denominator;
    }
}
std::array<Biquad, 4> bandwidthFilters(const Parameters& parameters, float rate) noexcept {
    std::array<Biquad, 4> result;
    for (int index = 0; index < 2; ++index) {
        const bool steep = parameters[index == 0 ? LowCutSteep : HighCutSteep] >= 0.5f;
        const float frequency = parameters[index == 0 ? LowCut : HighCut];
        // Fourth-order Butterworth pole pairs keep the selected cutoff at -3 dB.
        result[index].configure(index, frequency, rate, steep ? 0.54119610f : 0.70710678f);
        if (steep) result[index+2].configure(index, frequency, rate, 1.30656296f);
    }
    return result;
}
float Core::reduce(float value, float level) noexcept {
    delay[position] = value;
    float result = 0, norm = 1e-6f;
    for (unsigned k = 0; k < weights.size(); ++k) {
        float sample = delay[(position + 120 - 24 - k) % 120];
        result += weights[k] * sample; norm += sample * sample;
    }
    float step = level * 0.008f * (value - result) / norm;
    for (unsigned k = 0; k < weights.size(); ++k)
        weights[k] = weights[k] * 0.9996f + step * delay[(position + 120 - 24 - k) % 120];
    position = (position + 1) % 120;
    return result;
}
std::uint32_t Core::availableModes() const noexcept {
    auto result = modeBit(NoiseReduction::Off)|modeBit(NoiseReduction::Nlms)|modeBit(NoiseReduction::Wiener);
    if (neural.available()) result |= modeBit(NoiseReduction::Rnnoise);
    if (speech.available()) result |= modeBit(NoiseReduction::Speex);
    if (deep && deep->available()) result |= modeBit(NoiseReduction::DeepFilter);
    return result;
}
bool Core::prepareDeepFilter() noexcept {
    if (!deep) {
        try { deep = std::make_unique<DeepFilter>(rate); }
        catch (...) { return false; }
    }
    return deepFilterSelectable();
}
Parameters Core::effectiveParameters(Parameters values) const noexcept {
    values = sanitize(values, rate);
    const auto mode = NoiseReduction(unsigned(values[NrMode]));
    // APF is the explicit CW mode: speech suppression must not remove its tones.
    if (!modeAvailable(availableModes(),mode) || (voiceOnly(mode) && values[ApfOn] >= 0.5f))
        values[NrMode] = 0;
    return values;
}
void Core::process(const float* input, float* output, unsigned long count,
                   Parameters values, Telemetry* telemetry) noexcept {
    values = effectiveParameters(values);
    const bool useAgc = values[AgcOn] >= 0.5f && values[Bypass] < 0.5f && values[Mute] < 0.5f;
    if (!useAgc) agc.clear();
    const auto selected = values[Bypass] < 0.5f ? NoiseReduction(unsigned(values[NrMode])) : NoiseReduction::Off;
    if (activeMode != selected) {
        if (activeMode == NoiseReduction::Rnnoise || selected == NoiseReduction::Rnnoise) neural.clear();
        if (activeMode == NoiseReduction::Wiener || selected == NoiseReduction::Wiener) wiener.clear();
        if (activeMode == NoiseReduction::Speex || selected == NoiseReduction::Speex) speech.clear();
        if (deep && (activeMode == NoiseReduction::DeepFilter || selected == NoiseReduction::DeepFilter)) deep->clear();
        activeMode = selected;
    }
    if (!configured || previous[LowCut] != values[LowCut] || previous[HighCut] != values[HighCut]
        || previous[LowCutSteep] != values[LowCutSteep] || previous[HighCutSteep] != values[HighCutSteep]) {
        const auto bandwidth = bandwidthFilters(values, rate);
        targets[0] = bandwidth[0]; targets[1] = bandwidth[1];
        targets[4] = bandwidth[2]; targets[5] = bandwidth[3];
    }
    if (!configured || previous[NotchHz] != values[NotchHz]) targets[2].configure(2, values[NotchHz], rate);
    if (!configured || previous[ApfHz] != values[ApfHz]) targets[3].configure(3, values[ApfHz], rate);
    previous = values;
    float pre = std::pow(10.0f, (values[PreGain] - (values[Attenuate] >= 0.5f ? 10 : 0)) / 20);
    float post = std::pow(10.0f, values[PostGain] / 20);
    if (!configured) {
        filters = targets; preGain = pre; postGain = post;
        steepMix = {values[LowCutSteep], values[HighCutSteep]}; configured = true;
    }
    const float smoothing = 1 - std::exp(-1 / (rate * 0.005f));
    auto written = telemetry ? telemetry->written.load(std::memory_order_relaxed) : 0;
    for (unsigned long i = 0; i < count; ++i) {
        for (int filter = 0; filter < 6; ++filter) {
            auto& current = filters[filter];
            const auto& target = targets[filter];
            current.b0 += smoothing * (target.b0 - current.b0);
            current.b1 += smoothing * (target.b1 - current.b1);
            current.b2 += smoothing * (target.b2 - current.b2);
            current.a1 += smoothing * (target.a1 - current.a1);
            current.a2 += smoothing * (target.a2 - current.a2);
        }
        steepMix[0] += smoothing * (values[LowCutSteep] - steepMix[0]);
        steepMix[1] += smoothing * (values[HighCutSteep] - steepMix[1]);
        preGain += smoothing * (pre - preGain);
        postGain += smoothing * (post - postGain);
        const float raw = input[i];
        float value = raw;
        if (values[Bypass] < 0.5f) {
            value = std::isfinite(raw) ? std::clamp(raw, -100.0f, 100.0f) : 0;
            value *= preGain;
            if (values[BlankerOn] >= 0.5f) value = blanker.process(value, values[BlankerThreshold]);
            value = filters[0].process(value);
            const float lowSteep = filters[4].process(value);
            value += steepMix[0] * (lowSteep - value);
            value = filters[1].process(value);
            const float highSteep = filters[5].process(value);
            value += steepMix[1] * (highSteep - value);
            if (values[NotchOn] >= 0.5f) value = filters[2].process(value);
            if (values[NrMode] == 1) value = reduce(value, values[NrLevel]);
            else if (values[NrMode] == 2) value = neural.process(value);
            else if (values[NrMode] == 3) value = wiener.process(value,values[NrLevel]);
            else if (values[NrMode] == 4) value = speech.process(value,values[NrLevel]);
            else if (values[NrMode] == 5) value = deep->process(value,values[NrLevel]);
            if (values[ApfOn] >= 0.5f) value = filters[3].process(value);
            value *= postGain;
            if (useAgc) value = agc.process(value);
            value = std::clamp(value, -1.0f, 1.0f);
        }
        if (values[Mute] >= 0.5f) value = 0;
        output[i] = value;
        if (telemetry) {
            auto index = written % ringSize;
            telemetry->stamp[index].store(written * 2 + 1);
            telemetry->input[index].store(raw);
            telemetry->output[index].store(value);
            telemetry->stamp[index].store(++written * 2);
        }
    }
    if (telemetry) telemetry->written.store(written, std::memory_order_release);
}
}
