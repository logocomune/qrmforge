// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "qrmforge_assets.hpp"

namespace qrmforge::gui {
struct AboutContent {
    juce::Label& version;
    juce::Label& license;
    juce::HyperlinkButton& repository;
    juce::TextEditor& notices;
};

class AboutDialog final : public juce::Component {
public:
    explicit AboutDialog(AboutContent content) : content(content) {
        setName("About QRMForge");
        setWantsKeyboardFocus(true);
        for (auto* child : std::initializer_list<juce::Component*>{
                 &content.version, &content.license, &content.repository, &content.notices,
                 &closeButton, &dismissButton}) addAndMakeVisible(child);
        closeButton.onClick = dismissButton.onClick = [this] { dismiss(); };
        dismissButton.setTooltip("Close About");
        dismissButton.setTitle("Close About");
        content.version.setColour(juce::Label::textColourId,juce::Colour(0xff00f2fe));
        content.version.setFont(juce::Font(13.0f));
        content.notices.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(),12.0f,juce::Font::plain));
    }

    void dismiss() {
        exitModalState(0);
        setVisible(false);
    }

    bool keyPressed(const juce::KeyPress& key) override {
        if (key == juce::KeyPress::escapeKey || key == juce::KeyPress::returnKey) {
            dismiss();
            return true;
        }
        return false;
    }

    void resized() override {
        panel = juce::Rectangle<int>(juce::jmin(720,getWidth()-32),juce::jmin(610,getHeight()-32))
                    .withCentre(getLocalBounds().getCentre());
        auto contentArea = panel.reduced(28,0);
        dismissButton.setBounds(panel.getRight()-46,panel.getY()+10,32,32);
        const int top = panel.getY();
        content.version.setBounds(panel.getCentreX()-80,top+160,160,28);
        content.license.setBounds(contentArea.getX(),top+198,contentArea.getWidth(),42);
        content.repository.setBounds(contentArea.getX(),top+250,contentArea.getWidth(),28);
        content.notices.setBounds(contentArea.getX(),top+322,contentArea.getWidth(),panel.getHeight()-406);
        closeButton.setBounds(panel.getCentreX()-48,panel.getBottom()-48,96,32);
    }

    void paint(juce::Graphics& graphics) override {
        graphics.fillAll(juce::Colour(0xd9070a0e));
        const auto bounds = panel.toFloat();
        graphics.setColour(juce::Colour(0xff0b0f17));
        graphics.fillRoundedRectangle(bounds,12.0f);
        graphics.saveState();
        graphics.reduceClipRegion(panel);
        graphics.setColour(juce::Colour(0xff101622));
        graphics.fillRect(panel.withHeight(52));
        graphics.fillRect(panel.withTop(panel.getBottom()-64));
        graphics.setColour(juce::Colour(0xff233045));
        graphics.drawHorizontalLine(panel.getY()+52,float(panel.getX()),float(panel.getRight()));
        graphics.drawHorizontalLine(panel.getBottom()-64,float(panel.getX()),float(panel.getRight()));
        graphics.restoreState();
        graphics.setColour(juce::Colour(0xff006c78));
        graphics.drawRoundedRectangle(bounds.reduced(0.5f),12.0f,1.0f);
        graphics.setColour(juce::Colour(0xff00f2fe));
        graphics.fillEllipse(float(panel.getX()+25),float(panel.getY()+22),9.0f,9.0f);
        graphics.setColour(juce::Colour(0xffbdc8d8));
        graphics.setFont(juce::Font(12.0f,juce::Font::bold));
        graphics.drawText("ABOUT QRMFORGE",panel.getX()+44,panel.getY()+12,240,28,juce::Justification::centredLeft);
        const int brandX = panel.getCentreX()-84;
        const int brandY = panel.getY()+82;
        const auto icon = juce::ImageCache::getFromMemory(QRMForgeAssets::qrmforge_png,QRMForgeAssets::qrmforge_pngSize);
        graphics.drawImageWithin(icon,brandX-48,brandY+2,36,36,juce::RectanglePlacement::centred);
        graphics.setFont(juce::Font(30.0f,juce::Font::bold));
        graphics.setColour(juce::Colours::white);
        graphics.drawText("QRM",brandX,brandY,76,40,juce::Justification::centredLeft);
        graphics.setColour(juce::Colour(0xff00f2fe));
        graphics.drawText("Forge",brandX+76,brandY,140,40,juce::Justification::centredLeft);
        graphics.setFont(14.0f);
        graphics.setColour(juce::Colour(0xffbdc8d8));
        graphics.drawText("Audio DSP for amateur radio and HF broadcast listening.",
            panel.getX()+20,panel.getY()+128,panel.getWidth()-40,24,juce::Justification::centred);
        graphics.setColour(juce::Colour(0xff161e2e));
        graphics.fillRoundedRectangle(content.version.getBounds().toFloat(),14.0f);
        graphics.setColour(juce::Colour(0xff006c78));
        graphics.drawRoundedRectangle(content.version.getBounds().toFloat(),14.0f,1.0f);
        graphics.setFont(juce::Font(11.0f,juce::Font::bold));
        graphics.setColour(juce::Colour(0xff94a3b8));
        graphics.drawText("THIRD-PARTY SOFTWARE & LICENSES",panel.getX()+28,panel.getY()+294,panel.getWidth()-140,24,juce::Justification::centredLeft);
        graphics.setFont(10.0f);
        graphics.drawText("SCROLLABLE",panel.getRight()-104,panel.getY()+294,76,24,juce::Justification::centredRight);
    }

    juce::TextButton closeButton{"Close"}, dismissButton{"X"};
    juce::Rectangle<int> panel;
private:
    AboutContent content;
};
}
