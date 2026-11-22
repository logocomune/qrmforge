// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#include "dsp/neural.hpp"
#include <algorithm>
#include <cmath>
#ifdef QRMFORGE_ENABLE_RNNOISE
#include <rnnoise.h>
#endif
namespace qrmforge {
float NeuralStream::process(float sample) noexcept {
    const float result = output[position] / 32768.0f;
    input[position] = (std::isfinite(sample) ? std::clamp(sample, -1.0f, 1.0f) : 0) * 32768;
    if (++position == input.size()) {
        processor(context, output.data(), input.data());
        position = 0;
    }
    return std::isfinite(result) ? std::clamp(result, -1.0f, 1.0f) : 0;
}
void NeuralStream::clear() noexcept {
    input.fill(0);
    output.fill(0);
    position = 0;
}
Neural::Neural(float sampleRate) noexcept : stream(this, processFrame) {
#ifdef QRMFORGE_ENABLE_RNNOISE
    if (sampleRate == 48000 && rnnoise_get_frame_size() == 480)
        state = rnnoise_create(nullptr);
#else
    (void)sampleRate;
#endif
}
Neural::~Neural() {
#ifdef QRMFORGE_ENABLE_RNNOISE
    if (state) rnnoise_destroy(static_cast<DenoiseState*>(state));
#endif
}
void Neural::processFrame(void* context, float* output, const float* input) noexcept {
#ifdef QRMFORGE_ENABLE_RNNOISE
    auto& neural = *static_cast<Neural*>(context);
    rnnoise_process_frame(static_cast<DenoiseState*>(neural.state), output, input);
#else
    (void)context;
    (void)input;
    std::fill(output, output + 480, 0);
#endif
}
float Neural::process(float sample) noexcept {
    return available() ? stream.process(sample) : sample;
}
}
