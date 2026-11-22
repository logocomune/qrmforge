// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "gui/theme.hpp"
namespace qrmforge::gui {
class StudioLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    StudioLookAndFeel() {
        setColour(juce::TextButton::buttonColourId,theme::control);
        setColour(juce::TextButton::textColourOffId,theme::text);
        setColour(juce::ComboBox::backgroundColourId,theme::inset);
        setColour(juce::ComboBox::outlineColourId,theme::border);
        setColour(juce::ComboBox::textColourId,theme::text);
        setColour(juce::ComboBox::arrowColourId,theme::muted);
        setColour(juce::PopupMenu::backgroundColourId,theme::surface);
        setColour(juce::PopupMenu::highlightedBackgroundColourId,theme::control);
        setColour(juce::PopupMenu::highlightedTextColourId,theme::cyan);
    }

    void drawButtonBackground(juce::Graphics& graphics, juce::Button& button,
                              const juce::Colour& colour, bool highlighted, bool down) override {
        auto fill = colour.withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.45f);
        if (highlighted || down) fill = fill.brighter(down ? 0.15f : 0.07f);
        const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        graphics.setColour(fill);
        graphics.fillRoundedRectangle(bounds,7.0f);
        graphics.setColour(button.hasKeyboardFocus(true) ? theme::cyan : juce::Colour(0xff334155));
        graphics.drawRoundedRectangle(bounds,7.0f,1.0f);
    }

    juce::Font getTextButtonFont(juce::TextButton&, int) override {
        return juce::Font(14.0f,juce::Font::bold);
    }

    juce::Font getComboBoxFont(juce::ComboBox&) override {
        return juce::Font(15.0f);
    }

    juce::Label* createSliderTextBox(juce::Slider& slider) override {
        auto* label = juce::LookAndFeel_V4::createSliderTextBox(slider);
        label->setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(),13.0f,juce::Font::plain));
        label->setColour(juce::Label::textColourId,theme::text);
        return label;
    }
    void drawToggleButton(juce::Graphics& graphics, juce::ToggleButton& button,
                          bool highlighted, bool down) override {
        const auto accent = button.findColour(juce::ToggleButton::tickColourId);
        const bool active = button.getToggleState();
        const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        graphics.setColour(active ? accent.withAlpha(0.12f) : theme::surface);
        graphics.fillRoundedRectangle(bounds,6);
        graphics.setColour(active ? accent : theme::border);
        graphics.drawRoundedRectangle(bounds,6,button.hasKeyboardFocus(true) ? 2.0f : 1.0f);
        if (highlighted || down) {
            graphics.setColour(juce::Colours::white.withAlpha(down ? 0.10f : 0.04f));
            graphics.fillRoundedRectangle(bounds,6);
        }
        graphics.setColour(active ? accent : theme::text);
        graphics.setFont(juce::Font(12,juce::Font::bold));
        graphics.drawText(button.getButtonText(),button.getLocalBounds().reduced(6,0),juce::Justification::centred);
    }
    void drawRotarySlider(juce::Graphics& graphics, int x, int y, int width, int height,
                          float position, float start, float end, juce::Slider& slider) override {
        const float radius = juce::jmax(1.0f,juce::jmin(32.0f,float(juce::jmin(width,height))*0.5f-8));
        const auto centre = juce::Point<float>(float(x)+float(width)*0.5f,float(y)+float(height)*0.5f);
        const auto accent = slider.findColour(juce::Slider::rotarySliderFillColourId)
            .withMultipliedAlpha(slider.isEnabled() ? 1.0f : 0.45f);
        juce::Path track, level;
        track.addCentredArc(centre.x,centre.y,radius,radius,0,start,end,true);
        level.addCentredArc(centre.x,centre.y,radius,radius,0,start,start+(end-start)*position,true);
        graphics.setColour(theme::border);
        graphics.strokePath(track,juce::PathStrokeType(4,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        graphics.setColour(accent.withAlpha(0.10f));
        graphics.strokePath(level,juce::PathStrokeType(10,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        graphics.setColour(accent);
        graphics.strokePath(level,juce::PathStrokeType(4,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        const auto body = juce::Rectangle<float>(radius*1.6f,radius*1.6f).withCentre(centre);
        graphics.setGradientFill(juce::ColourGradient(juce::Colour(0xff1a2333),body.getTopLeft(),
            juce::Colour(0xff0c121e),body.getBottomLeft(),false));
        graphics.fillEllipse(body);
        graphics.setColour(juce::Colour(0xff34475d)); graphics.drawEllipse(body,1);
        const float angle = start+(end-start)*position;
        const auto tip = centre+juce::Point<float>(std::sin(angle)*radius*0.67f,-std::cos(angle)*radius*0.67f);
        const auto base = centre+juce::Point<float>(std::sin(angle)*radius*0.32f,-std::cos(angle)*radius*0.32f);
        graphics.setColour(accent); graphics.drawLine({base,tip},3);
    }
};
}
