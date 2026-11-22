// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
namespace qrmforge::gui {
inline float spectrumDecibels(float magnitude, std::size_t size) noexcept {
    if (!std::isfinite(magnitude) || magnitude <= 0 || size == 0) return -100;
    return std::clamp(20 * std::log10(magnitude / (float(size)*0.25f)), -100.0f, 12.0f);
}
inline float spectrumBinDecibels(float magnitude, std::size_t bin) noexcept {
    // DC and Nyquist do not have a mirrored negative-frequency contribution.
    return spectrumDecibels(bin == 0 || bin == 1024 ? magnitude*0.5f : magnitude, 2048);
}
inline float viewFrequency(float sampleRate) noexcept {
    return std::isfinite(sampleRate) && sampleRate > 0 ? std::min(4500.0f, sampleRate*0.5f) : 4500;
}
inline std::size_t spectrumBin(float frequency, float sampleRate) noexcept {
    if (!std::isfinite(frequency) || !std::isfinite(sampleRate) || sampleRate <= 0) return 0;
    return std::size_t(std::clamp(frequency/sampleRate*2048, 0.0f, 1024.0f));
}
}
