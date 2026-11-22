// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace qrmforge {
struct AudioLevels {
    double rmsDbfs = -std::numeric_limits<double>::infinity();
    double peakDbfs = -std::numeric_limits<double>::infinity();
};

template<std::size_t Size>
AudioLevels measureLevels(const std::array<float,Size>& samples) noexcept {
    if (samples.empty()) return {};
    double squares = 0;
    double peak = 0;
    for (float sample : samples) {
        const double value = std::isfinite(sample) ? double(sample) : 0.0;
        squares += value*value;
        peak = std::max(peak,std::abs(value));
    }
    AudioLevels levels;
    if (squares > 0) levels.rmsDbfs = 10.0*std::log10(squares/double(samples.size()));
    if (peak > 0) levels.peakDbfs = 20.0*std::log10(peak);
    return levels;
}
class RmsAverager {
public:
    double update(double rmsDbfs, double elapsedSeconds) noexcept {
        if (!std::isfinite(elapsedSeconds) || elapsedSeconds <= 0 || std::isnan(rmsDbfs)) return decibels();
        const double target = std::pow(10.0,rmsDbfs/10.0);
        if (!std::isfinite(target)) return decibels();
        const double weight = -std::expm1(-elapsedSeconds/0.3);
        power += weight*(target-power);
        return decibels();
    }

    void clear() noexcept { power = 0; }
    double decibels() const noexcept {
        return power > 0 ? 10.0*std::log10(power) : -std::numeric_limits<double>::infinity();
    }
private:
    double power = 0;
};

}
