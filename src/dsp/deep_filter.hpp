// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
namespace qrmforge {
class DeepFilter {
public:
    static constexpr std::size_t frameSize = 480, delayFrames = 3;
    explicit DeepFilter(float sampleRate) noexcept;
    ~DeepFilter();
    DeepFilter(const DeepFilter&) = delete;
    DeepFilter& operator=(const DeepFilter&) = delete;
    bool starting() const noexcept;
    bool available() const noexcept;
    static bool supported(float sampleRate) noexcept;
    float process(float sample, float strength) noexcept;
    void clear() noexcept;
    std::uint32_t droppedFrames() const noexcept;
private:
    struct Implementation;
    std::unique_ptr<Implementation> implementation;
};
}
