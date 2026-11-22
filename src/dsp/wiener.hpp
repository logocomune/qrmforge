// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#pragma once
#include <array>
#include <complex>
#include <cstddef>
namespace qrmforge {
class Wiener {
public:
    static constexpr std::size_t size = 512, hop = size/2;
    Wiener() noexcept;
    float process(float sample, float strength) noexcept;
    void clear() noexcept;
private:
    using Spectrum = std::array<std::complex<float>,size>;
    Spectrum spectrum{};
    std::array<std::complex<float>,hop> roots{};
    std::array<unsigned,size> reversed{};
    std::array<float,size> input{}, output{}, window{};
    std::array<float,hop+1> noise{}, gains{};
    std::size_t cursor = 0, pending = 0;
    bool initialized = false;
    void transform() noexcept;
    void frame(float strength) noexcept;
};
}
