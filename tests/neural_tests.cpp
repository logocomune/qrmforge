// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#include "dsp/core.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <random>
#include <vector>
using namespace qrmforge;
void require(bool condition, const char* message) {
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}
struct IdentityFrame {
    unsigned calls = 0;
    static void process(void* context, float* output, const float* input) noexcept {
        ++static_cast<IdentityFrame*>(context)->calls;
        std::copy(input, input+480, output);
    }
};
void testFraming() {
    IdentityFrame backend;
    NeuralStream stream(&backend, IdentityFrame::process);
    std::mt19937 random(731);
    std::vector<float> input(9600), output;
    for (auto& sample : input) sample = float(int(random()%2001)-1000)/1000;
    for (auto sample : input) output.push_back(stream.process(sample));
    require(backend.calls == 20, "One inference per 480 samples");
    for (int i = 0; i < 480; ++i) require(output[i] == 0, "Initial frame is silent");
    for (std::size_t i = 480; i < output.size(); ++i)
        require(output[i] == input[i-480], "Framing and PCM scale preserve signal with identity test backend");
    stream.clear();
    require(stream.process(0.25f) == 0, "Reset clears old output and cursor");
    for (int i = 1; i < 480; ++i) stream.process(0.25f);
    require(stream.process(0.25f) == 0.25f, "Reset starts a new complete frame");
    stream.clear();
    for (int i = 0; i < 480; ++i) stream.process(std::numeric_limits<float>::quiet_NaN());
    require(stream.process(0) == 0, "Non-finite input is replaced with silence");
    stream.clear();
    for (int i = 0; i < 480; ++i) stream.process(100);
    require(stream.process(0) == 1, "Input PCM scaling is bounded");
}
void invalidFrame(void*, float* output, const float*) noexcept {
    std::fill(output, output+480, std::numeric_limits<float>::infinity());
    output[1] = 65536;
}
void testInvalidBackendOutput() {
    NeuralStream stream(nullptr, invalidFrame);
    for (int i = 0; i < 480; ++i) stream.process(0);
    require(stream.process(0) == 0, "Non-finite backend result sanitized");
    require(stream.process(0) == 1, "Backend result limited after scaling");
}
void testCoreModes() {
    Core core(48000), otherRate(44100);
    auto parameters = defaults; parameters[Bypass] = 0;
    parameters[NrMode] = 2;
    require(!otherRate.neuralAvailable() && otherRate.effectiveParameters(parameters)[NrMode] == 0,
            "Non-48-kHz neural mode unavailable");
#ifdef QRMFORGE_ENABLE_RNNOISE
    require(core.neuralAvailable(), "Real RNNoise library creates default model");
#else
    require(!core.neuralAvailable() && core.effectiveParameters(parameters)[NrMode] == 0,
            "Build without RNNoise reports mode off");
    Neural unavailable(48000);
    require(unavailable.process(0.4f) == 0.4f, "Unavailable adapter passes input");
#endif
    if (core.neuralAvailable()) {
        Core contiguous(48000), partitioned(48000);
        std::vector<float> signal(4096), whole(4096), chunks(4096);
        for (std::size_t i = 0; i < signal.size(); ++i)
            signal[i] = 0.1f * std::sin(float(i) * 6.28318530718f * 700 / 48000);
        contiguous.process(signal.data(), whole.data(), signal.size(), parameters);
        for (std::size_t offset = 0; offset < signal.size(); offset += 64)
            partitioned.process(signal.data()+offset, chunks.data()+offset, 64, parameters);
        for (std::size_t i = 0; i < signal.size(); ++i) {
            require(std::isfinite(whole[i]), "Inference output finite");
            require(whole[i] == chunks[i], "Neural frame processing independent of PipeWire quantum");
        }
    }
    parameters[ApfOn] = 1;
    require(core.effectiveParameters(parameters)[NrMode] == 0, "Speech model disabled for APF/CW");
    parameters[ApfOn] = 0;
    float input[128]{}, output[128]{};
    for (int i = 0; i < 20; ++i) core.process(input, output, 128, parameters);
    for (auto sample : output) require(std::isfinite(sample) && sample == 0, "Neural-mode silence remains silent");
    parameters[Bypass] = 1;
    std::fill(input, input+128, 2.0f);
    core.process(input, output, 128, parameters);
    require(std::memcmp(input, output, sizeof(input)) == 0, "Full bypass ignores neural path");
    parameters[Bypass] = 0; parameters[NrMode] = 0;
    core.process(input, output, 128, parameters);
    parameters[NrMode] = 2;
    core.process(input, output, 128, parameters);
    for (auto sample : output) require(std::isfinite(sample), "Mode switching remains finite");
}
#ifdef QRMFORGE_MOCK_RNNOISE
extern "C" void qrmforge_mock_failure(int mode);
void testInitializationFailure() {
    qrmforge_mock_failure(1);
    Neural incompatible(48000);
    require(!incompatible.available(), "Unexpected RNNoise frame size rejected");
    qrmforge_mock_failure(2);
    Neural allocationFailure(48000);
    require(!allocationFailure.available(), "RNNoise model creation failure reported");
    qrmforge_mock_failure(0);
}
#endif
#if defined(QRMFORGE_ENABLE_RNNOISE) && !defined(QRMFORGE_MOCK_RNNOISE)
void testRealNoiseSuppression() {
    Neural neural(48000);
    require(neural.available(), "Default embedded neural model initialized");
    std::mt19937 random(907302);
    double inputEnergy = 0, outputEnergy = 0;
    for (int i = 0; i < 48000; ++i) {
        float sample = float(int(random()%20001)-10000) / 200000;
        float result = neural.process(sample);
        require(std::isfinite(result), "Real inference remains finite");
        if (i >= 24000) {
            inputEnergy += sample*sample;
            outputEnergy += result*result;
        }
    }
    require(outputEnergy < inputEnergy * 0.5, "Real model attenuates stationary synthetic noise");
}
#endif
int main() {
    testFraming(); testInvalidBackendOutput(); testCoreModes();
#if defined(QRMFORGE_ENABLE_RNNOISE) && !defined(QRMFORGE_MOCK_RNNOISE)
    testRealNoiseSuppression();
#endif
#ifdef QRMFORGE_MOCK_RNNOISE
    testInitializationFailure();
#endif
}
