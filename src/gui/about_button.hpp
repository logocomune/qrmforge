// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "gui/theme.hpp"

namespace qrmforge::gui {
class AboutButton final : public juce::TextButton {
public:
    AboutButton() : juce::TextButton("About") {}

    void paintButton(juce::Graphics& graphics, bool highlighted, bool down) override {
        getLookAndFeel().drawButtonBackground(graphics, *this,
            findColour(juce::TextButton::buttonColourId), highlighted, down);
        graphics.setColour(theme::cyan);
        const auto centre = juce::Point<float>(18.0f, float(getHeight()) * 0.5f);
        graphics.drawEllipse(centre.x - 7.0f, centre.y - 7.0f, 14.0f, 14.0f, 1.4f);
        graphics.fillEllipse(centre.x - 0.9f, centre.y - 4.2f, 1.8f, 1.8f);
        graphics.fillRoundedRectangle(centre.x - 0.8f, centre.y - 0.8f, 1.6f, 5.0f, 0.6f);
        graphics.setColour(theme::text);
        graphics.setFont(14.0f);
        graphics.drawText(getButtonText(), getLocalBounds().withTrimmedLeft(32).withTrimmedRight(8),
            juce::Justification::centredLeft);
    }
};
}
