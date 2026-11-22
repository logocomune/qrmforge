// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#include "audio/processor.hpp"
#include "test_support.hpp"
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <thread>
using namespace qrmforge;
int main() {
    if (!DeepFilter::supported(48000)) { std::cout << "SKIP: native DeepFilter backend not built\n"; return 77; }
    AudioProcessor processor; processor.enable(true);
    require(modeAvailable(processor.availableModes(),NoiseReduction::DeepFilter),"Native DeepFilter can be selected before model preparation");
    require(processor.prepareNoiseReduction(NoiseReduction::DeepFilter),"Real official backend loads its model");
    require(processor.prepareNoiseReduction(NoiseReduction::DeepFilter),"Native preparation is idempotent");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (processor.deepFilterStarting() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    require(!processor.deepFilterStarting() && modeAvailable(processor.availableModes(),NoiseReduction::DeepFilter),
            "Native model finishes startup successfully");
    auto parameters = defaults; parameters[Bypass] = 0; parameters[NrMode] = 5; parameters[PostGain] = 0;
    processor.publish(parameters);
    std::array<float,480> input{}, output{};
    double energy = 0;
    for (unsigned frame = 0; frame < 100; ++frame) {
        for (unsigned index = 0; index < input.size(); ++index) {
            const auto sample = frame*480+index;
            input[index] = 0.12f*std::sin(6.28318530718f*700*sample/48000);
        }
        processor.process(input.data(),output.data(),output.size(),48000);
        for (auto value : output) { require(std::isfinite(value) && std::abs(value) <= 1,"Real model returns bounded audio"); energy += value*value; }
        std::this_thread::sleep_for(std::chrono::milliseconds(11));
    }
    require(processor.parameters()[NrMode] == 5 && energy > 0.00001,"Real DeepFilter runs through the integrated DSP rather than a placeholder");
    require(processor.nrDroppedFrames() < 50,"Native worker sustains paced frame delivery");
    parameters[ApfOn] = 1; processor.publish(parameters);
    processor.process(input.data(),output.data(),output.size(),48000);
    require(processor.parameters()[NrMode] == 0,"CW mode disables native voice inference");
    parameters[ApfOn] = 0; parameters[Bypass] = 1; processor.publish(parameters);
    processor.process(input.data(),output.data(),output.size(),48000);
    require(std::memcmp(input.data(),output.data(),sizeof(input)) == 0,"Bypass stays bit-identical with the real native backend");
    parameters[Mute] = 1; processor.publish(parameters);
    processor.process(input.data(),output.data(),output.size(),48000);
    for (auto value : output) require(value == 0,"Mute overrides the real native backend and bypass");
    processor.enable(false);
    std::cout << "Real DeepFilter output energy: " << energy << "; dropped transport frames: " << processor.nrDroppedFrames() << '\n';
}
