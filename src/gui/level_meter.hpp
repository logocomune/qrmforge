// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#pragma once
#include "audio/levels.hpp"
#include "gui/theme.hpp"

namespace qrmforge::gui {
class LevelMeter final : public juce::Component {
public:
    void update(AudioLevels current) {
        const double now = juce::Time::getMillisecondCounterHiRes();
        const double elapsed = lastUpdate > 0 ? (now-lastUpdate)/1000.0 : 1.0/30.0;
        averaging.update(current.rmsDbfs,elapsed);
        lastUpdate = now;
        values = current;
        active = true;
        repaint();
    }

    void clear() {
        values = {};
        averaging.clear();
        lastUpdate = 0;
        active = false;
        repaint();
    }

    AudioLevels levels() const noexcept { return values; }
    bool hasData() const noexcept { return active; }

    void paint(juce::Graphics& graphics) override {
        if (getWidth() < 56 || getHeight() < 96) return;
        const auto format = [&](double value) {
            if (!active) return juce::String("--");
            if (!std::isfinite(value)) return juce::String("-inf");
            return juce::String(value,1);
        };
        const auto track = juce::Rectangle<float>(12,20,12,float(getHeight()-54));
        const auto heightFor = [&](double value) {
            return float(std::clamp((value+60.0)/60.0,0.0,1.0))*track.getHeight();
        };
        graphics.setFont(9.0f);
        graphics.setColour(theme::muted);
        graphics.drawText("dBFS",0,0,getWidth(),12,juce::Justification::centred);
        graphics.setColour(theme::border);
        graphics.fillRoundedRectangle(track,3.0f);
        graphics.setColour(theme::cyan);
        graphics.fillRoundedRectangle(track.withTop(track.getBottom()-heightFor(averaging.decibels())),3.0f);
        if (active && values.peakDbfs > -60) {
            graphics.setColour(values.peakDbfs >= 0 ? theme::magenta : juce::Colours::white);
            const int y = juce::jlimit(int(track.getY()),int(track.getBottom())-2,
                int(track.getBottom()-heightFor(values.peakDbfs)));
            graphics.fillRect(10,y,16,2);
        }
        graphics.setColour(theme::faint);
        for (int db : {0,-20,-40,-60}) {
            const int y = int(track.getBottom()-heightFor(db));
            graphics.drawHorizontalLine(y,28.0f,32.0f);
            graphics.drawText(juce::String(db),35,y-5,getWidth()-35,11,juce::Justification::centredLeft);
        }
        graphics.setFont(10.0f);
        graphics.setColour(theme::text);
        graphics.drawText("RMS " + format(averaging.decibels()),0,getHeight()-28,getWidth(),13,juce::Justification::centred);
        graphics.setColour(values.peakDbfs >= 0 ? theme::magenta : theme::muted);
        graphics.drawText("PEAK " + format(values.peakDbfs),0,getHeight()-14,getWidth(),13,juce::Justification::centred);
    }

private:
    AudioLevels values;
    RmsAverager averaging;
    double lastUpdate = 0;
    bool active = false;
};
}
