// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
namespace qrmforge::gui {
class BandwidthKnob : public juce::Slider {
public:
    std::function<void()> onSlopeChange;
    void mouseDoubleClick(const juce::MouseEvent& event) override {
        if (!isEnabled()) return;
        if (onSlopeChange && event.eventComponent == this && event.mods.isLeftButtonDown()) onSlopeChange();
        else juce::Slider::mouseDoubleClick(event);
    }
};
}
