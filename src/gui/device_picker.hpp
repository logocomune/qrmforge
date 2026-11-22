// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#pragma once
#include "audio/routing.hpp"
#include "gui/theme.hpp"
#include <algorithm>
#include <juce_gui_basics/juce_gui_basics.h>
namespace qrmforge::gui {
class DevicePicker final : public juce::Component {
public:
    explicit DevicePicker(const juce::String& title, const juce::String& empty = {}) : emptyLabel(empty) {
        label.setText(title,juce::dontSendNotification);
        label.setFont(juce::Font(12.0f,juce::Font::bold)); label.setColour(juce::Label::textColourId,theme::muted);
        addAndMakeVisible(label); addAndMakeVisible(combo);
        combo.setTextWhenNothingSelected("Choose device");
        if (!emptyLabel.isEmpty()) { combo.addItem(emptyLabel,1); combo.setSelectedId(1,juce::dontSendNotification); }
        combo.onChange = [this] {
            const int index = combo.getSelectedId()-2;
            selected = index >= 0 && std::size_t(index) < devices.size() ? devices[std::size_t(index)].serial : 0;
        };
    }
    void setDevices(std::vector<AudioNode> current) {
        const bool same = current.size() == devices.size() && std::equal(current.begin(),current.end(),devices.begin(),
            [](const auto& left,const auto& right) { return left.serial == right.serial && left.name == right.name && left.description == right.description; });
        if (!same) {
            devices = std::move(current); combo.clear(juce::dontSendNotification); if (!emptyLabel.isEmpty()) combo.addItem(emptyLabel,1);
            for (std::size_t index = 0; index < devices.size(); ++index) {
                const auto& device = devices[index];
                auto title = audioNodeLabel(device);
                combo.addItem(juce::String::fromUTF8(title.c_str()),int(index)+2);
            }
        }
        select(selected);
    }
    void select(std::uint64_t serial) {
        selected = serial;
        int item = serial == 0 && !emptyLabel.isEmpty() ? 1 : 0;
        for (std::size_t index = 0; index < devices.size(); ++index) if (devices[index].serial == serial) item = int(index)+2;
        combo.setTextWhenNothingSelected(serial ? "Selected device unavailable" : "Choose device");
        combo.setSelectedId(item,juce::dontSendNotification);
        auto tooltip = combo.getText();
        if (item > 1) {
            const auto& device = devices[std::size_t(item-2)];
            tooltip += "\n"+juce::String::fromUTF8(device.name.c_str())+"\nObject serial: "+juce::String(device.serial);
        }
        combo.setTooltip(tooltip);
    }
    std::uint64_t selection() const noexcept { return selected; }
    void resized() override {
        auto bounds = getLocalBounds(); label.setBounds(bounds.removeFromLeft(55)); combo.setBounds(bounds.reduced(2));
    }
private:
    juce::Label label;
    juce::ComboBox combo;
    std::vector<AudioNode> devices;
    std::uint64_t selected = 0;
    juce::String emptyLabel;
};
}
