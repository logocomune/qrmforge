// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#include "dsp/agc.hpp"
#include "dsp/core.hpp"
#include "test_support.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <random>
#include <vector>
using namespace qrmforge;
namespace {
void testLevelControl() {
    for (float rate : {1000.0f, 8000.0f, 44100.0f, 48000.0f, 96000.0f, 384000.0f}) {
        AudioAgc attack(rate), recovery(rate);
        require(std::abs(attack.process(2) - 0.95f) < 1e-6f, "First transient receives immediate headroom protection");
        float attacked = 0, recovered = 0;
        const auto attackSamples = unsigned(std::round(rate * 0.005f));
        const auto recoverySamples = unsigned(std::round(rate * 0.5f));
        for (unsigned i = 0; i < attackSamples; ++i) attacked = attack.process(2);
        for (unsigned i = 0; i < recoverySamples; ++i) recovered = recovery.process(0.1f);
        const float expectedAttack = 0.5f + 0.45f * std::exp(-float(attackSamples) / (rate * 0.005f));
        const float expectedRecovery = 0.1f * (4 - 3 * std::exp(-float(recoverySamples) / (rate * 0.5f)));
        require(std::abs(attacked - expectedAttack) < 0.002f, "Attenuation time constant is 5 ms across sample rates");
        require(std::abs(recovered - expectedRecovery) < 0.002f, "Recovery time constant is 500 ms across sample rates");
        for (float level : {0.01f, 0.25f, 0.5f, 2.0f, 1000.0f}) {
            AudioAgc agc(rate);
            float output = 0;
            for (unsigned i = 0; i < unsigned(rate * 5); ++i) {
                output = agc.process(level);
                require(std::isfinite(output) && output >= 0 && output <= 0.950001f,
                        "AGC reserves headroom even at startup and high input levels");
            }
            const float expected = std::min(level * 4, 0.5f);
            require(std::abs(output - expected) < 0.003f,
                    "AGC settles near target with bounded weak-signal boost at every rate");
        }
    }
    AudioAgc agc(48000);
    for (int i = 0; i < 48000; ++i) require(agc.process(0) == 0, "Silence remains exact silence");
    require(agc.process(0.0001f) == 0.0001f, "Sub-floor input does not raise initial gain");
    for (int i = 0; i < 48000; ++i) agc.process(2);
    float previous = 0;
    for (int i = 0; i < 240000; ++i) {
        const float output = agc.process(0.1f);
        require(output >= previous - 1e-6f, "Gain recovers monotonically after a loud signal");
        previous = output;
    }
    require(previous > 0.39f && previous <= 0.400001f, "Slow release restores bounded gain");
    agc.clear();
    AudioAgc fresh(48000);
    for (float sample : {0.1f, -0.2f, 4.0f, 0.0f, 0.01f})
        require(agc.process(sample) == fresh.process(sample), "Clear restores fresh AGC state");
}
void testProperties() {
    std::mt19937 random(12751);
    std::uniform_real_distribution<float> signal(-100, 100);
    AudioAgc positive(48000), negative(48000);
    for (int i = 0; i < 100000; ++i) {
        const float input = signal(random);
        const float output = positive.process(input);
        require(output == -negative.process(-input), "AGC is symmetric under polarity reversal");
        require(std::abs(output) <= 0.950001f && std::abs(output) <= 4 * std::abs(input),
                "Random audio preserves peak and maximum-gain bounds");
    }
    for (float rate : {0.0f, -1.0f, 999.0f, 384001.0f,
                       std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        AudioAgc invalid(rate), reference(48000);
        for (int i = 0; i < 1000; ++i) {
            const float sample = signal(random);
            require(invalid.process(sample) == reference.process(sample), "Invalid rates use safe 48 kHz coefficients");
        }
    }
    AudioAgc invalid(48000), reference(48000);
    for (float sample : {std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
                         std::numeric_limits<float>::quiet_NaN()})
        require(invalid.process(sample) == 0, "Invalid samples produce silence without poisoning state");
    for (float sample : {std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(),
                         std::numeric_limits<float>::denorm_min(), 0.0f, 0.1f}) {
        const float output = invalid.process(sample);
        require(output == reference.process(sample) && std::isfinite(output) && std::abs(output) <= 0.950001f,
                "Extreme float samples stay bounded and invalid samples leave state intact");
    }
}
void testParameters() {
    require(defaults[AgcOn] == 0, "AGC starts disabled");
    require(HighCutSteep == 16 && AgcOn == 17, "Existing parameter IDs remain stable");
    for (float value : {-10.0f, 0.0f, 0.49f, 0.5f, 1.0f, 100.0f,
                        std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        auto parameters = defaults;
        parameters[AgcOn] = value;
        const auto sanitized = sanitize(parameters, 48000);
        const float expected = std::isfinite(value) && value >= 0.5f ? 1 : 0;
        require(sanitized[AgcOn] == expected && sanitize(sanitized, 48000) == sanitized,
                "AGC parameter is binary, finite and idempotent");
    }
}
void testCoreIntegration() {
    std::mt19937 random(2197);
    std::uniform_real_distribution<float> noise(-0.005f, 0.005f);
    std::vector<float> input(12000), expected(input.size()), output(input.size());
    for (unsigned i = 0; i < input.size(); ++i)
        input[i] = noise(random) + 0.03f * std::sin(6.28318530718f * 700 * i / 48000);
    for (float mode : {0.0f, 1.0f, 3.0f}) {
        Core core(48000), dry(48000);
        AudioAgc reference(48000);
        auto values = defaults;
        values[Bypass] = 0; values[NrMode] = mode; values[PostGain] = 6;
        values[BlankerOn] = values[NotchOn] = values[ApfOn] = 1;
        const float outputGain = std::pow(10.0f, values[PostGain] / 20);
        for (int phase = 0; phase < 8; ++phase) {
            values[AgcOn] = phase == 1 ? 0 : 1;
            values[Bypass] = phase == 3 ? 1 : 0;
            values[Mute] = phase == 5 ? 1 : 0;
            const bool active = values[AgcOn] && !values[Bypass] && !values[Mute];
            if (!active) reference.clear();
            auto dryValues = values; dryValues[PostGain] = dryValues[AgcOn] = 0;
            dry.process(input.data(), expected.data(), input.size(), dryValues);
            core.process(input.data(), output.data(), input.size(), values);
            for (unsigned i = 0; i < input.size(); ++i) {
                float sample = expected[i];
                if (!values[Bypass]) {
                    sample *= outputGain;
                    if (active) sample = reference.process(sample);
                    sample = std::clamp(sample, -1.0f, 1.0f);
                }
                require(std::abs(output[i] - sample) < 1e-6f,
                        "Core AGC follows APF/output gain and resets on disable, bypass and mute");
            }
        }
    }
    Core core(48000);
    auto values = defaults; values[AgcOn] = 1;
    float raw[]{-2, -0.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()};
    float result[4]{};
    core.process(raw, result, 4, values);
    require(std::memcmp(raw, result, sizeof(raw)) == 0, "Bypass preserves arbitrary input bits with AGC selected");
    values[Mute] = 1;
    core.process(raw, result, 4, values);
    for (float sample : result) require(sample == 0, "Mute overrides bypass and AGC");
    values[Bypass] = values[Mute] = 0; values[PostGain] = 20;
    std::fill(input.begin(), input.end(), 0);
    input[0] = 100;
    core.process(input.data(), output.data(), input.size(), values);
    for (float sample : output)
        require(std::isfinite(sample) && std::abs(sample) <= 0.950001f, "Core AGC catches high post-gain transients");
    Core contiguous(48000), segmented(48000);
    contiguous.process(input.data(), expected.data(), input.size(), values);
    for (unsigned position = 0; position < input.size();) {
        const unsigned count = std::min(unsigned(random()%257+1), unsigned(input.size())-position);
        segmented.process(input.data()+position, output.data()+position, count, values);
        position += count;
    }
    require(output == expected, "AGC output does not depend on audio block boundaries");
}
}
int main() { testLevelControl(); testProperties(); testParameters(); testCoreIntegration(); }
