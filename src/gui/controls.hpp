// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#pragma once
#include "dsp/core.hpp"
#include <optional>
namespace qrmforge::gui {
inline std::optional<Parameters> bandPreset(Parameters current, int selection) noexcept {
    if (selection >= 1 && selection <= 3) {
        auto band = preset(selection-1);
        current[LowCut] = band[LowCut]; current[HighCut] = band[HighCut]; current[ApfOn] = 0;
    } else if (selection == 4) {
        current[LowCut] = 400; current[HighCut] = 2000; current[ApfOn] = 0;
    } else if (selection == 5) {
        current[LowCut] = 575; current[HighCut] = 825; current[ApfHz] = 700;
        current[ApfOn] = 1; current[NrMode] = 1;
    } else return {};
    return current;
}
}
