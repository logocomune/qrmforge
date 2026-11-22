// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#pragma once
#include <array>
#include <atomic>
#include <cstdint>

namespace qrmforge {
enum Parameter { Bypass, PreGain, LowCut, HighCut, NotchOn, NotchHz,
    ApfOn, ApfHz, NrMode, NrLevel, PostGain, BlankerOn, BlankerThreshold,
    Attenuate, Mute, LowCutSteep, HighCutSteep, AgcOn, ParameterCount };
using Parameters = std::array<float, ParameterCount>;
inline constexpr Parameters defaults{1,0,50,4500,0,1200,0,700,0,4.5f,3,0,6,0,0,0,0,0};
constexpr std::size_t ringSize = 8192;
constexpr std::size_t fftSize = 2048;
static_assert(std::atomic<float>::is_always_lock_free);
static_assert(std::atomic<std::uint32_t>::is_always_lock_free);

// Atomic samples and stamps let the GUI reject blocks overwritten during a read.
struct Telemetry {
    std::atomic<std::uint32_t> written{0};
    std::atomic<std::uint32_t> stamp[ringSize]{};
    std::atomic<float> input[ringSize]{}, output[ringSize]{};
};
inline bool readSample(const Telemetry& state, std::uint32_t position,
                       float& input, float& output) noexcept {
    const auto index = position % ringSize;
    const auto expected = (position + 1) * 2;
    if (state.stamp[index].load() != expected) return false;
    input = state.input[index].load(); output = state.output[index].load();
    return state.stamp[index].load() == expected;
}

// One writer per mailbox; readers make a single bounded attempt per audio block.
class ParameterMailbox {
public:
    ParameterMailbox() noexcept { publish(defaults); }
    void publish(const Parameters& values) noexcept {
        sequence.fetch_add(1);
        for (std::size_t index = 0; index < values.size(); ++index) data[index].store(values[index]);
        sequence.fetch_add(1);
    }
    bool read(Parameters& values) const noexcept {
        const auto before = sequence.load();
        if (before & 1) return false;
        Parameters copy{};
        for (std::size_t index = 0; index < copy.size(); ++index) copy[index] = data[index].load();
        if (sequence.load() != before) return false;
        values = copy; return true;
    }
private:
    std::atomic<std::uint32_t> sequence{0};
    std::array<std::atomic<float>, ParameterCount> data{};
};
struct AudioFrame {
    std::array<float, fftSize> input{}, output{};
    float sampleRate = 48000;
    std::uint32_t written = 0;
};
}
