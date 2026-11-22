// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#pragma once
#include "model/parameters.hpp"
#include "gui/level_meter.hpp"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
namespace qrmforge::gui {
class Spectrum : public juce::Component {
public:
    explicit Spectrum(juce::String title, juce::Colour colour);
    void push(const std::array<float, fftSize>& samples, float sampleRate);
    void clear();
    void clearLevels() { meter.clear(); }
    AudioLevels levels() const noexcept { return meter.levels(); }
    bool hasLevels() const noexcept { return meter.hasData(); }
    void resized() override;
    void setBand(const Parameters& parameters, float sampleRate);
    void paint(juce::Graphics& graphics) override;
private:
    juce::String title;
    juce::Colour accent;
    juce::dsp::FFT fft{11};
    juce::dsp::WindowingFunction<float> window{fftSize, juce::dsp::WindowingFunction<float>::hann};
    std::array<float, fftSize*2> transform{};
    std::array<float, fftSize/2+1> spectrum{};
    juce::Image waterfall{juce::Image::RGB, 900, 100, true};
    LevelMeter meter;
    float rate = 48000, low = 300, high = 2600;
    bool hasData = false;
    bool bypassed = false;
    Parameters bandParameters = defaults;
};
}
