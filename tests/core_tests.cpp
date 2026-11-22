// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#include "dsp/core.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>
#include <vector>
using namespace qrmforge;
void check(bool value, const char* message) { if (!value) { std::cerr << message << '\n'; std::exit(1); } }
void testStartupBypass() {
    check(defaults[Bypass] == 1 && defaults[Mute] == 0, "Default startup is unmuted bypass");
    check(defaults[NrMode] == 0 && defaults[LowCut] == 50 && defaults[HighCut] == 4500,
          "Startup noise reduction is off and cuts span the knob range");
    Core core(48000);
    float input[]{-2, -0.0f, 0.5f, 4}, output[4]{};
    core.process(input, output, 4, defaults);
    check(std::memcmp(input, output, sizeof(input)) == 0, "Defaults preserve raw input bits");
    auto invalid = defaults;
    invalid[Bypass] = std::numeric_limits<float>::quiet_NaN();
    check(sanitize(invalid, 48000)[Bypass] == 1, "Invalid bypass reverts to enabled default");
    invalid[NrMode] = invalid[LowCut] = std::numeric_limits<float>::quiet_NaN();
    invalid[HighCut] = std::numeric_limits<float>::infinity();
    const auto normalized = sanitize(invalid, 48000);
    check(normalized[NrMode] == 0 && normalized[LowCut] == 50 && normalized[HighCut] == 4500,
          "Invalid startup controls revert to NR off and full bandwidth");
    check(sanitize(defaults, 8000)[HighCut] == 3600, "Startup bandwidth respects sample-rate limit");
}

void testPipelineOrder() {
    std::mt19937 random(4821);
    std::uniform_real_distribution<float> noise(-0.00003f, 0.00003f);
    std::vector<float> input(8192), output(input.size());
    for (unsigned i = 0; i < input.size(); ++i)
        input[i] = noise(random) + (i % 317 == 0 ? 0.002f : 0.00002f);
    for (float gain : {-20.0f, 20.0f}) {
        for (bool steep : {false, true}) {
            auto values = defaults;
            values[Bypass] = 0; values[PreGain] = gain; values[PostGain] = 0;
            values[BlankerOn] = values[NotchOn] = values[ApfOn] = 1;
            values[LowCutSteep] = values[HighCutSteep] = steep ? 1 : 0;
            values[NrMode] = 3;
            Core core(48000);
            core.process(input.data(), output.data(), input.size(), values);
            auto filters = bandwidthFilters(values, 48000);
            Biquad notch, apf;
            notch.configure(2, values[NotchHz], 48000);
            apf.configure(3, values[ApfHz], 48000);
            NoiseBlanker blanker(48000);
            Wiener reduction;
            for (unsigned i = 0; i < input.size(); ++i) {
                float expected = input[i] * std::pow(10.0f, gain / 20);
                expected = blanker.process(expected, values[BlankerThreshold]);
                expected = filters[0].process(expected);
                if (steep) expected = filters[2].process(expected);
                expected = filters[1].process(expected);
                if (steep) expected = filters[3].process(expected);
                expected = notch.process(expected);
                expected = reduction.process(expected, values[NrLevel]);
                expected = apf.process(expected);
                expected = std::clamp(expected, -1.0f, 1.0f);
                check(std::abs(output[i] - expected) < 1e-7f,
                      "Pipeline follows pre-gain, blanker, bandwidth, notch, NR, APF order");
            }
        }
    }
}

void testCore() {
    auto values = defaults;
    values[Bypass] = 1;
    float input[]{-2, -0.0f, 0.5f, 4};
    float output[4]{};
    Core core(48000);
    Telemetry telemetry;
    core.process(input, output, 4, values, &telemetry);
    check(std::memcmp(input, output, sizeof(input)) == 0, "Bypass must preserve input bits");
    check(telemetry.input[0] == -2 && telemetry.output[3] == 4 && telemetry.written == 4,
          "Raw telemetry and bypass output");
    float raw = 0, filtered = 0;
    check(readSample(telemetry, 0, raw, filtered) && raw == -2 && filtered == -2, "Coherent telemetry sample");
    telemetry.stamp[0] = 1;
    check(!readSample(telemetry, 0, raw, filtered), "Overwritten telemetry rejected");
    core.process(input, input, 4, values);
    check(input[3] == 4, "In-place processing");
    core.process(input, output, 0, values);
    check(preset(0)[HighCut] == 4500 && preset(0)[LowCut] == 100, "AM preset");
    check(preset(1)[HighCut] == 3000 && preset(2)[HighCut] == 2700, "FM/SSB presets");
    check(preset(99) == preset(2), "Unknown preset falls back to SSB");
    auto bad = defaults;
    bad[PreGain] = std::numeric_limits<float>::quiet_NaN();
    bad[HighCut] = 1; bad[LowCut] = 99999;
    auto valid = sanitize(bad, 1000);
    check(valid[PreGain] == 0 && valid[LowCut] <= valid[HighCut] && valid[HighCut] <= 450,
          "Parameter validation");
    std::mt19937 random(907302);
    for (int iteration = 0; iteration < 2000; ++iteration) {
        Parameters candidate;
        for (auto& value : candidate) {
            auto bits = std::uint32_t(random());
            std::memcpy(&value, &bits, sizeof(value));
        }
        auto normalized = sanitize(candidate, 48000);
        check(sanitize(normalized, 48000) == normalized, "Normalization idempotence");
        for (auto value : normalized) check(std::isfinite(value), "Finite parameters");
    }
    std::vector<float> signal(48000), result(48000);
    for (std::size_t i = 0; i < signal.size(); ++i)
        signal[i] = 0.05f * std::sin(float(i) * 6.28318530718f * 1000 / 48000);
    values = defaults; values[Bypass] = 0; values[NrMode] = 0; values[PostGain] = 0;
    Core pass(48000);
    pass.process(signal.data(), result.data(), signal.size(), values, &telemetry);
    double energy = 0;
    for (std::size_t i = 24000; i < result.size(); ++i) energy += result[i]*result[i];
    check(energy > 20 && energy < 32, "Passband response");
    values[NotchOn] = 1; values[NotchHz] = 1000;
    Core notch(48000);
    notch.process(signal.data(), result.data(), signal.size(), values);
    energy = 0;
    for (std::size_t i = 24000; i < result.size(); ++i) energy += result[i]*result[i];
    check(energy < 0.01, "Notch attenuation");
    values[NotchOn] = 0; values[ApfOn] = 1; values[ApfHz] = 1000;
    Core peak(48000);
    peak.process(signal.data(), result.data(), signal.size(), values);
    energy = 0;
    for (std::size_t i = 24000; i < result.size(); ++i) energy += result[i]*result[i];
    check(energy > 200, "CW peak gain");
    values[ApfOn] = 0; values[NrMode] = 1;
    Core nr(48000);
    nr.process(signal.data(), result.data(), signal.size(), values);
    energy = 0;
    for (std::size_t i = 24000; i < result.size(); ++i) {
        check(std::isfinite(result[i]), "NLMS finite output");
        energy += result[i]*result[i];
    }
    check(energy > 1, "NLMS learns correlated signal");
    check(telemetry.written == signal.size()+4, "Telemetry ring wrap");
    check(!readSample(telemetry, 0, raw, filtered), "Expired telemetry rejected");
    auto latest = telemetry.written.load() - 1;
    check(readSample(telemetry, latest, raw, filtered), "Latest telemetry available");
    // Check frequency rejection independently from the notch.
    for (float frequency : {50.0f, 15000.0f}) {
        for (std::size_t i = 0; i < signal.size(); ++i)
            signal[i] = 0.05f * std::sin(float(i) * 6.28318530718f * frequency / 48000);
        Core reject(48000);
        values = defaults; values[LowCut] = 300; values[HighCut] = 2600; values[Bypass] = 0; values[NrMode] = 0; values[PostGain] = 0;
        reject.process(signal.data(), result.data(), signal.size(), values);
        energy = 0;
        for (std::size_t i = 24000; i < result.size(); ++i) energy += result[i]*result[i];
        check(energy < 0.1, "Out-of-band attenuation");
    }
    std::fill(signal.begin(), signal.end(), 0.0f);
    values[NrMode] = 1;
    Core silence(48000);
    silence.process(signal.data(), result.data(), signal.size(), values);
    for (auto sample : result) check(sample == 0, "Silent NLMS remains silent");
    for (std::size_t i = 0; i < signal.size(); ++i)
        signal[i] = 0.5f * std::sin(float(i) * 6.28318530718f * 1000 / 48000);
    values[NrMode] = 0; values[PreGain] = 20; values[PostGain] = 20;
    Core limiter(48000);
    limiter.process(signal.data(), result.data(), signal.size(), values);
    bool clipped = false;
    for (auto sample : result) {
        check(std::isfinite(sample) && std::abs(sample) <= 1, "Output bounded");
        clipped = clipped || std::abs(sample) == 1;
    }
    check(clipped, "Limiter exercised");
    values[PreGain] = -20; values[PostGain] = -20; values[LowCut] = 500; values[HighCut] = 2000;
    limiter.process(signal.data(), result.data(), signal.size(), values);
    check(std::abs(result.back()) < 0.01, "Changed gain and frequencies settle");
    values[Bypass] = 1;
    limiter.process(signal.data(), result.data(), signal.size(), values);
    check(std::memcmp(signal.data(), result.data(), signal.size()*sizeof(float)) == 0,
          "Bypass remains exact after parameter changes");
}

void testCutSlopes() {
    std::vector<float> input(24000), output(24000), expected(24000);
    for (std::size_t index = 0; index < input.size(); ++index)
        input[index] = 0.05f*std::sin(6.283185307179586*800*index/48000);
    auto parameters = defaults; parameters[Bypass] = 0;
    parameters[NrMode] = 0; parameters[PostGain] = 0;
    Core changing(48000);
    changing.process(input.data(), output.data(), input.size(), parameters);
    Biquad originalLow, originalHigh;
    originalLow.configure(0, parameters[LowCut], 48000);
    originalHigh.configure(1, parameters[HighCut], 48000);
    for (std::size_t index = 0; index < input.size(); ++index)
        check(std::abs(output[index]-originalHigh.process(originalLow.process(input[index]))) < 1e-7f,
              "Default 12 dB retains original filter output");
    for (int selection : {0, 1, 3, 2, 0, 3, 0}) {
        parameters[LowCutSteep] = selection % 2;
        parameters[HighCutSteep] = selection / 2;
        parameters[LowCut] = selection % 2 ? 700 : 300;
        parameters[HighCut] = selection / 2 ? 1200 : 2600;
        changing.process(input.data(), output.data(), input.size(), parameters);
        Core reference(48000);
        reference.process(input.data(), expected.data(), input.size(), parameters);
        for (std::size_t index = 0; index < output.size(); ++index) {
            check(std::isfinite(output[index]) && std::abs(output[index]) < 0.15f, "Slope transitions remain bounded");
            if (index > 20000) check(std::abs(output[index]-expected[index]) < 0.0001f, "Slope transitions settle");
        }
    }
    for (auto slope : {LowCutSteep, HighCutSteep}) {
        for (float raw : {-10.0f, 0.0f, 0.49f, 0.5f, 0.9f, 20.0f,
                           std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
            parameters[slope] = raw;
            float expectedValue = std::isfinite(raw) && raw >= 0.5f ? 1 : 0;
            check(sanitize(parameters, 48000)[slope] == expectedValue, "Slope selector normalization");
        }
    }
    parameters = defaults; parameters[LowCutSteep] = parameters[HighCutSteep] = 1;
    parameters[Bypass] = 1;
    changing.process(input.data(), output.data(), input.size(), parameters);
    check(std::memcmp(input.data(), output.data(), input.size()*sizeof(float)) == 0, "Steep filters preserve full bypass");
    parameters[Mute] = 1;
    changing.process(input.data(), output.data(), input.size(), parameters);
    for (auto sample : output) check(sample == 0, "Steep filters preserve mute priority");
}

void testAdditionalControls() {
    NoiseBlanker blanker(48000);
    for (int i = 0; i < 5000; ++i)
        check(blanker.process(0.01f, 6) == 0.01f, "Blanker preserves baseline");
    check(blanker.process(1, 6) == 0.01f, "Positive impulse replaced");
    check(blanker.process(-1, 6) == 0.01f, "Negative impulse replaced");
    check(blanker.process(0.01f, 6) == 0.01f, "Blanker recovers after impulse");
    bool admitted = false;
    for (int i = 0; i < 30; ++i) admitted = admitted || blanker.process(1, 6) == 1;
    check(admitted, "Maximum blanking duration bounded");
    NoiseBlanker voice(48000);
    for (int i = 0; i < 48000; ++i) {
        float sample = 0.1f * std::sin(float(i) * 6.28318530718f * 1000 / 48000);
        check(voice.process(sample, 6) == sample, "Blanker preserves steady sine wave");
    }
    NoiseBlanker startup(1000);
    check(startup.process(1, 6) == 1, "Warmup does not suppress startup");
    auto parameters = defaults; parameters[Bypass] = 0;
    parameters[NrMode] = 0; parameters[PostGain] = 0;
    std::vector<float> input(4800), reference(4800), attenuated(4800);
    for (std::size_t i = 0; i < input.size(); ++i)
        input[i] = 0.05f * std::sin(float(i) * 6.28318530718f * 1000 / 48000);
    Core normal(48000), attenuator(48000);
    normal.process(input.data(), reference.data(), input.size(), parameters);
    parameters[Attenuate] = 1;
    attenuator.process(input.data(), attenuated.data(), input.size(), parameters);
    // Gain now precedes recursive float filters, so rounding differs between paths.
    for (std::size_t i = 0; i < input.size(); ++i)
        check(std::abs(attenuated[i] - reference[i]*std::pow(10.0f, -0.5f)) < 1e-5f,
              "ATT applies exactly minus ten decibels");
    parameters[Mute] = 1;
    Telemetry telemetry;
    attenuator.process(input.data(), attenuated.data(), input.size(), parameters, &telemetry);
    for (std::size_t i = 0; i < input.size(); ++i)
        check(attenuated[i] == 0 && telemetry.output[i] == 0 && telemetry.input[i] == input[i],
              "Mute silences output and retains raw telemetry");
    parameters[Bypass] = 1;
    attenuator.process(input.data(), attenuated.data(), input.size(), parameters);
    for (auto sample : attenuated) check(sample == 0, "Mute overrides bypass");
    parameters[Mute] = 0;
    attenuator.process(input.data(), attenuated.data(), input.size(), parameters);
    check(std::memcmp(input.data(), attenuated.data(), input.size()*sizeof(float)) == 0,
          "Unmuted bypass ignores attenuation");
    parameters[Bypass] = 0; parameters[BlankerOn] = 1;
    attenuator.process(input.data(), attenuated.data(), input.size(), parameters);
    for (auto sample : attenuated) check(std::isfinite(sample), "Blanker integrated output finite");
    std::fill(input.begin(), input.end(), 0.01f);
    input[2400] = 1;
    Core withBlanker(48000), withoutBlanker(48000);
    parameters = defaults; parameters[Bypass] = 0; parameters[NrMode] = 0; parameters[PostGain] = 0;
    withoutBlanker.process(input.data(), reference.data(), input.size(), parameters);
    parameters[BlankerOn] = 1;
    withBlanker.process(input.data(), attenuated.data(), input.size(), parameters);
    float originalPeak = 0, reducedPeak = 0;
    for (std::size_t i = 2400; i < input.size(); ++i) {
        originalPeak = std::max(originalPeak, std::abs(reference[i]));
        reducedPeak = std::max(reducedPeak, std::abs(attenuated[i]));
    }
    check(originalPeak > 0.01f && reducedPeak < originalPeak * 0.1f,
          "Blanker suppresses impulse before filter ringing");
}
int main() { testPipelineOrder(); testStartupBypass(); testCore(); testCutSlopes(); testAdditionalControls(); }
