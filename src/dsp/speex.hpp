// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
namespace qrmforge {
class SpeexDenoiser {
public:
    explicit SpeexDenoiser(float sampleRate) noexcept;
    ~SpeexDenoiser();
    SpeexDenoiser(const SpeexDenoiser&) = delete;
    SpeexDenoiser& operator=(const SpeexDenoiser&) = delete;
    bool available() const noexcept { return state && !failed; }
    std::size_t frameSize() const noexcept { return length; }
    float process(float sample, float strength) noexcept;
    void clear() noexcept;
private:
    void* state = nullptr;
    std::size_t length = 0, position = 0;
    std::array<std::int16_t,960> input{};
    std::array<float,960> output{};
    int attenuation = 0;
    bool failed = false;
};
}
