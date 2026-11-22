// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#pragma once
#include "dsp/core.hpp"
#include <algorithm>
#include <cmath>
#include <complex>
namespace qrmforge::gui {
class FilterResponse {
public:
    FilterResponse(Parameters parameters, float sampleRate) noexcept {
        rate = std::isfinite(sampleRate) && sampleRate > 0 ? sampleRate : 48000;
        parameters = sanitize(parameters, rate);
        filters = bandwidthFilters(parameters, rate);
    }
    float decibels(float frequency) const noexcept {
        if (!std::isfinite(frequency)) return -60;
        const double angle = -6.283185307179586 * std::clamp(frequency, 0.0f, rate/2) / rate;
        const auto delay = std::polar(1.0, angle);
        double magnitude = 1;
        for (const auto& filter : filters) {
            const auto numerator = double(filter.b0) + double(filter.b1)*delay + double(filter.b2)*delay*delay;
            const auto denominator = 1.0 + double(filter.a1)*delay + double(filter.a2)*delay*delay;
            magnitude *= std::abs(numerator / denominator);
        }
        return float(std::clamp(20*std::log10(std::max(magnitude, 1e-12)), -60.0, 0.0));
    }
private:
    float rate;
    std::array<Biquad, 4> filters;
};
}
