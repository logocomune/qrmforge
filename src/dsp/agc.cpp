// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#include "dsp/agc.hpp"
#include <algorithm>
#include <cmath>

namespace qrmforge {
AudioAgc::AudioAgc(float sampleRate) noexcept {
    if (!std::isfinite(sampleRate) || sampleRate < 1000 || sampleRate > 384000) sampleRate = 48000;
    attack = 1 - std::exp(-1 / (sampleRate * 0.005f));
    release = 1 - std::exp(-1 / (sampleRate * 0.5f));
}
float AudioAgc::process(float sample) noexcept {
    if (!std::isfinite(sample)) return 0;
    const float magnitude = std::abs(sample);
    envelope = std::max(magnitude, envelope * (1 - release));
    // Hold gain below the noise floor instead of boosting silence indefinitely.
    if (envelope >= 0.001f) {
        const float desired = std::min(4.0f, 0.5f / envelope);
        gain += (desired < gain ? attack : release) * (desired - gain);
    }
    // Reserve transient headroom while the slower level control settles.
    if (magnitude > 0) gain = std::min(gain, 0.95f / magnitude);
    return sample * gain;
}
void AudioAgc::clear() noexcept {
    envelope = 0;
    gain = 1;
}
}
