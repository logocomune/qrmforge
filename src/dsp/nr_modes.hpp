// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#pragma once
#include <cstdint>
namespace qrmforge {
enum class NoiseReduction : unsigned { Off = 0, Nlms = 1, Rnnoise = 2, Wiener = 3, Speex = 4, DeepFilter = 5 };
constexpr unsigned noiseReductionCount = 6;
constexpr std::uint32_t modeBit(NoiseReduction mode) noexcept { return 1u << unsigned(mode); }
constexpr bool voiceOnly(NoiseReduction mode) noexcept {
    return mode == NoiseReduction::Rnnoise || mode == NoiseReduction::Speex || mode == NoiseReduction::DeepFilter;
}
inline bool modeAvailable(std::uint32_t available, NoiseReduction mode) noexcept {
    return unsigned(mode) < noiseReductionCount && (available & modeBit(mode)) != 0;
}
}
