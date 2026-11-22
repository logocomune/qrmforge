// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#include "dsp/core.hpp"
#include "test_support.hpp"
#include <chrono>
#include <cmath>
#include <limits>
#include <thread>
using namespace qrmforge;
extern "C" void nr_mock_speex_failure(int);
extern "C" void nr_mock_deep_failure(int);
extern "C" void nr_mock_hold_deep(bool);
extern "C" bool nr_mock_processing();
extern "C" float nr_mock_attenuation();
template <typename Predicate>
void waitForStartup(Predicate starting) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (starting() && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    require(!starting(), "DeepFilter startup completes");
}

int main() {
    for (int failure : {1,2}) {
        nr_mock_speex_failure(failure);
        SpeexDenoiser unavailable(48000);
        require(!unavailable.available(),"Speex allocation/control failures disable the backend");
    }
    nr_mock_speex_failure(0);
    SpeexDenoiser speech(48000);
    for (unsigned index = 0; index < 960; ++index) require(speech.process(0.5f,10) == 0,"Speex frame adapter waits one full frame");
    require(std::abs(speech.process(0.5f,10)-0.25f) < 0.0001f,"Speex PCM conversion and fixed-frame adapter");
    speech.clear();
    nr_mock_speex_failure(2);
    for (unsigned index = 0; index < 960; ++index) speech.process(0.5f,2);
    require(!speech.available() && speech.process(1,2) == 0,"Runtime control failure never masquerades as denoising");
    nr_mock_speex_failure(0);
    for (int failure : {1,2}) {
        nr_mock_deep_failure(failure);
        DeepFilter unavailable(48000);
        waitForStartup([&] { return unavailable.starting(); });
        require(!unavailable.available(),"DeepFilter missing state and mismatched frame ABI are refused");
    }
    nr_mock_deep_failure(0);
    DeepFilter deep(48000);
    waitForStartup([&] { return deep.starting(); });
    require(deep.available(),"Compatible C API creates the DeepFilter worker");
    nr_mock_hold_deep(true);
    for (unsigned index = 0; index < 480; ++index) deep.process(0.2f,4.5f);
    const auto deadline = std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while (!nr_mock_processing() && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    require(nr_mock_processing(),"Native inference runs on the worker");
    // Completing this loop while inference is held proves the callback never waits for it.
    for (unsigned index = 0; index < 480*30; ++index) require(std::isfinite(deep.process(0.2f,4.5f)),"Backpressure yields bounded finite output");
    require(deep.droppedFrames() > 0,"Queue saturation is reported rather than growing memory");
    deep.clear(); nr_mock_hold_deep(false);
    bool received = false;
    for (unsigned frame = 0; frame < 15; ++frame) {
        for (unsigned index = 0; index < 480; ++index) {
            const auto sample = deep.process(-0.4f,10);
            require(sample <= 0,"A generation change rejects buffered speech from the previous mode");
            received = received || sample < -0.1f;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(12));
    }
    require(received && nr_mock_attenuation() == 30,"Worker delivers processed audio with requested attenuation");
    deep.clear();
    for (unsigned index = 0; index < 480; ++index)
        require(deep.process(std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::quiet_NaN()) == 0,"Invalid samples and a cleared queue produce silence");
    Core core(48000);
    require(core.prepareDeepFilter() && core.prepareDeepFilter(),"Control-thread preparation is idempotent");
    waitForStartup([&] { return core.deepFilterStarting(); });
    require(modeAvailable(core.availableModes(),NoiseReduction::DeepFilter),"Prepared native model is reported to the UI");
    auto parameters = defaults; parameters[NrMode] = 5;
    require(core.effectiveParameters(parameters)[NrMode] == 5,"Prepared DeepFilter can be selected");
    parameters[ApfOn] = 1;
    require(core.effectiveParameters(parameters)[NrMode] == 0,"CW APF excludes the DeepFilter voice backend");
    parameters[Bypass] = 0; parameters[ApfOn] = 0;
    std::array<float,1440> input{}, output{}; input.fill(0.25f);
    core.process(input.data(),output.data(),input.size(),parameters);
    require(core.droppedFrames() < 10,"DeepFilter dispatch remains bounded within the DSP chain");
    parameters[Bypass] = 1;
    core.process(input.data(),output.data(),input.size(),parameters);
    require(input == output,"Full bypass remains bit-identical after DeepFilter selection");
    parameters[Mute] = 1;
    core.process(input.data(),output.data(),input.size(),parameters);
    for (auto sample : output) require(sample == 0,"Mute overrides bypass with DeepFilter selected");
}
