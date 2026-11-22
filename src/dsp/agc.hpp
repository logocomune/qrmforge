// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#pragma once

namespace qrmforge {
class AudioAgc {
public:
    explicit AudioAgc(float sampleRate) noexcept;
    float process(float sample) noexcept;
    void clear() noexcept;
private:
    float attack;
    float release;
    float envelope = 0;
    float gain = 1;
};
}
