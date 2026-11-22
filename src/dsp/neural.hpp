// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#pragma once
#include <array>
#include <cstddef>
namespace qrmforge {
// RNNoise consumes 480 mono samples at 48 kHz in signed-16-bit PCM scale.
class NeuralStream {
public:
    using Processor = void (*)(void*, float*, const float*) noexcept;
    NeuralStream(void* context, Processor processor) noexcept : context(context), processor(processor) {}
    float process(float sample) noexcept;
    void clear() noexcept;
private:
    void* context;
    Processor processor;
    std::array<float, 480> input{};
    std::array<float, 480> output{};
    std::size_t position = 0;
};
class Neural {
public:
    explicit Neural(float sampleRate) noexcept;
    ~Neural();
    Neural(const Neural&) = delete;
    Neural& operator=(const Neural&) = delete;
    bool available() const noexcept { return state != nullptr; }
    float process(float sample) noexcept;
    void clear() noexcept { stream.clear(); }
private:
    void* state = nullptr;
    static void processFrame(void* context, float* output, const float* input) noexcept;
    NeuralStream stream;
};
}
