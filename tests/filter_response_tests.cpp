// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#include "gui/filter_response.hpp"
#include <cstdlib>
#include <iostream>
#include <random>
#include <limits>
using namespace qrmforge;
using namespace qrmforge::gui;
void require(bool condition) {
    if (!condition) { std::cerr << "Filter response assertion failed\n"; std::exit(1); }
}
int main() {
    FilterResponse response(defaults, 48000);
    require(response.decibels(0) == -60);
    require(response.decibels(24000) == -60);
    require(std::abs(response.decibels(50) + 3.01f) < 0.1f);
    require(std::abs(response.decibels(4500) + 3.01f) < 0.1f);
    require(response.decibels(1000) > -0.2f);
    require(response.decibels(100) < response.decibels(200));
    require(response.decibels(10000) < response.decibels(5000));
    require(response.decibels(std::numeric_limits<float>::quiet_NaN()) == -60);
    for (float invalidRate : {0.0f, -1.0f, std::numeric_limits<float>::infinity(),
                              std::numeric_limits<float>::quiet_NaN()}) {
        FilterResponse fallback(defaults, invalidRate);
        require(fallback.decibels(1000) == response.decibels(1000));
    }
    require(response.decibels(-100) == response.decibels(0));
    require(response.decibels(48000) == response.decibels(24000));
    auto changed = defaults;
    changed[LowCut] = 700; changed[HighCut] = 1200;
    FilterResponse narrow(changed, 48000);
    require(narrow.decibels(300) < response.decibels(300));
    require(narrow.decibels(2600) < response.decibels(2600));
    auto invalid = defaults;
    invalid[LowCut] = std::numeric_limits<float>::quiet_NaN();
    invalid[HighCut] = std::numeric_limits<float>::infinity();
    FilterResponse normalized(invalid, 48000);
    require(normalized.decibels(1000) == response.decibels(1000));
    for (float rate : {1000.0f, 8000.0f, 48000.0f, 192000.0f}) {
        for (int combination = 0; combination < 4; ++combination) {
            auto parameters = defaults; parameters[Bypass] = 0;
            parameters[LowCutSteep] = combination % 2;
            parameters[HighCutSteep] = combination / 2;
            parameters = sanitize(parameters, rate);
            FilterResponse curve(parameters, rate);
            for (float frequency : {rate*0.01f, rate*0.1f, rate*0.3f, rate*0.45f}) {
                double warped = std::tan(3.141592653589793*frequency/rate);
                double low = std::tan(3.141592653589793*parameters[LowCut]/rate)/warped;
                double high = warped/std::tan(3.141592653589793*parameters[HighCut]/rate);
                double expected = -10*std::log10(1+std::pow(low, parameters[LowCutSteep] ? 8 : 4))
                                  -10*std::log10(1+std::pow(high, parameters[HighCutSteep] ? 8 : 4));
                require(std::abs(curve.decibels(frequency)-std::clamp(expected, -60.0, 0.0)) < 0.08);
            }
        }
    }
    std::mt19937 random(421);
    std::uniform_real_distribution<float> unit(0, 1);
    for (int trial = 0; trial < 300; ++trial) {
        float rate = 8000 + unit(random)*88000;
        auto parameters = defaults; parameters[Bypass] = 0;
        parameters[LowCut] = 50 + unit(random)*1450;
        parameters[HighCut] = 400 + unit(random)*4100;
        parameters[LowCutSteep] = trial % 2;
        parameters[HighCutSteep] = (trial / 2) % 2;
        parameters = sanitize(parameters, rate);
        FilterResponse curve(parameters, rate);
        float frequency = 100 + unit(random)*(rate*0.4f-100);
        parameters[NrMode] = 0; parameters[PostGain] = 0;
        Core core(rate);
        std::array<float, 12000> input{}, output{};
        for (int sample = 0; sample < 12000; ++sample)
            input[sample] = 0.05f*std::sin(6.283185307179586*frequency*sample/rate);
        core.process(input.data(), output.data(), input.size(), parameters);
        double energy = 0, reference = 0;
        for (int sample = 0; sample < 12000; ++sample) {
            if (sample >= 6000) {
                energy += output[sample]*output[sample]; reference += input[sample]*input[sample];
            }
        }
        float measured = std::clamp(float(10*std::log10(energy/reference)), -60.0f, 0.0f);
        require(std::isfinite(curve.decibels(frequency)));
        require(std::abs(curve.decibels(frequency)-measured) < 0.12f);
    }
}
