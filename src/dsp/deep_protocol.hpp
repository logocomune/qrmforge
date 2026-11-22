// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#pragma once
#include <array>
#include <cstdint>
namespace qrmforge {
inline constexpr std::uint32_t deepMagic = 0x51444633;
struct DeepHello { std::uint32_t magic = deepMagic, rate = 48000, frame = 480; };
struct DeepPacket { float attenuation = 13.5f; std::array<float,480> samples{}; };
static_assert(sizeof(DeepPacket) == 481*sizeof(float));
}
