// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#include "gui/spectrum_view.hpp"
#include "gui/spectrum.hpp"
#include "gui/filter_response.hpp"
#include "gui/theme.hpp"
namespace qrmforge::gui {
Spectrum::Spectrum(juce::String label, juce::Colour colour) : title(std::move(label)), accent(colour) {
    spectrum.fill(-100);
    addAndMakeVisible(meter);
}
void Spectrum::clear() {
    meter.clear();
    hasData = false; spectrum.fill(-100); waterfall.clear(waterfall.getBounds()); repaint();
}
void Spectrum::setBand(const Parameters& parameters, float sampleRate) {
    bandParameters = parameters;
    rate = std::isfinite(sampleRate) && sampleRate > 0 ? sampleRate : 48000;
    low = parameters[LowCut]; high = parameters[HighCut];
    bypassed = parameters[Bypass] >= 0.5f; repaint();
}
void Spectrum::push(const std::array<float, fftSize>& samples, float sampleRate) {
    rate = sampleRate;
    transform.fill(0);
    meter.update(measureLevels(samples));
    for (std::size_t index = 0; index < samples.size(); ++index) {
        float value = std::isfinite(samples[index]) ? std::clamp(samples[index], -100.0f, 100.0f) : 0;
        transform[index] = value;
    }
    window.multiplyWithWindowingTable(transform.data(), fftSize);
    fft.performFrequencyOnlyForwardTransform(transform.data());
    for (std::size_t index = 0; index < spectrum.size(); ++index)
        spectrum[index] = spectrumBinDecibels(transform[index], index);
    waterfall.moveImageSection(0, 1, 0, 0, waterfall.getWidth(), waterfall.getHeight()-1);
    for (int x = 0; x < waterfall.getWidth(); ++x) {
        auto bin = spectrumBin(float(x)*viewFrequency(rate)/float(waterfall.getWidth()-1), rate);
        float intensity = std::clamp((spectrum[bin]+100)/80, 0.0f, 1.0f);
        auto colour = theme::inset.interpolatedWith(accent, intensity);
        if (intensity > 0.8f) colour = colour.interpolatedWith(juce::Colours::white, (intensity-0.8f)*3);
        waterfall.setPixelAt(x, 0, colour);
    }
    hasData = true;
    repaint();
}
void Spectrum::resized() {
    auto area = getLocalBounds().reduced(16,10);
    area.removeFromTop(24);
    meter.setBounds(area.removeFromRight(80));
}
void Spectrum::paint(juce::Graphics& graphics) {
    graphics.setColour(theme::background);
    graphics.fillRoundedRectangle(getLocalBounds().toFloat(), 12);
    graphics.setColour(theme::border);
    graphics.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f),12,1);
    auto area = getLocalBounds().reduced(16, 10);
    auto heading = area.removeFromTop(24);
    graphics.setColour(accent); graphics.setFont(juce::Font(13, juce::Font::bold));
    graphics.drawText(title, heading, juce::Justification::centredLeft);
    graphics.setColour(theme::muted); graphics.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(),10.0f,juce::Font::plain));
    graphics.drawText(meter.hasData() ? "Live audio" : "Waiting for audio",
                      heading, juce::Justification::centredRight);
    area.removeFromRight(90);
    auto axis = area.removeFromBottom(20);
    auto plot = area.removeFromTop(juce::jmax(40, area.getHeight()/2));
    auto image = area;
    graphics.setColour(theme::inset); graphics.fillRect(plot); graphics.fillRect(image);
    graphics.setColour(theme::border.withAlpha(0.6f));
    for (int division = 1; division < 4; ++division)
        graphics.drawVerticalLine(plot.getX()+plot.getWidth()*division/4,float(plot.getY()),float(image.getBottom()));
    for (int db : {-20,-40,-60,-80}) {
        float y = float(plot.getBottom()) - (float(db)+100)/112*float(plot.getHeight());
        graphics.setColour(theme::border); graphics.drawHorizontalLine(int(y), float(plot.getX()), float(plot.getRight()));
        if (plot.getHeight() >= 80 || db == -20 || db == -60) {
            graphics.setColour(theme::faint); graphics.setFont(10);
            graphics.drawText(juce::String(db), plot.getX()+3, int(y)-11, 25, 12, juce::Justification::left);
        }
    }
    if (hasData) {
        juce::Path path;
        for (int x = 0; x < plot.getWidth(); ++x) {
            auto bin = spectrumBin(float(x)*viewFrequency(rate)/float(juce::jmax(1,plot.getWidth()-1)), rate);
            float y = float(plot.getBottom()) - (spectrum[bin]+100)/112*float(plot.getHeight());
            if (x == 0) path.startNewSubPath(float(plot.getX()), y);
            else path.lineTo(float(plot.getX()+x), y);
        }
        graphics.setColour(accent); graphics.strokePath(path, juce::PathStrokeType(1.4f));
        graphics.drawImage(waterfall, image.toFloat());
    }
    const auto frequencyX = [&](float frequency) {
        return float(plot.getX()) + std::clamp(frequency/viewFrequency(rate), 0.0f, 1.0f)*float(plot.getWidth()-1);
    };
    const float opacity = bypassed ? 0.25f : 1.0f;
    graphics.setColour(accent.withAlpha(0.08f*opacity));
    graphics.fillRect(juce::Rectangle<float>(frequencyX(low), float(plot.getY()),
                       std::max(0.0f, frequencyX(high)-frequencyX(low)), float(plot.getHeight())));
    const FilterResponse response(bandParameters, rate);
    juce::Path profile;
    for (int x = 0; x < plot.getWidth(); ++x) {
        float frequency = float(x)*viewFrequency(rate)/float(juce::jmax(1, plot.getWidth()-1));
        float y = float(plot.getY()) - response.decibels(frequency)/60*float(plot.getHeight()-1);
        if (x == 0) profile.startNewSubPath(float(plot.getX()), y);
        else profile.lineTo(float(plot.getX()+x), y);
    }
    graphics.setColour(juce::Colours::white.withAlpha(0.8f*opacity));
    graphics.strokePath(profile, juce::PathStrokeType(1.6f));
    graphics.setFont(10);
    graphics.drawText(bypassed ? "Cut response: bypass" : "Cut response: 0 dB", plot.reduced(3).removeFromTop(14), juce::Justification::centredRight);
    graphics.drawText("-60 dB", plot.reduced(3).removeFromBottom(14), juce::Justification::centredRight);
    graphics.setColour(accent.withAlpha(0.35f));
    for (float frequency : {low, high}) {
        int x = plot.getX()+int(std::clamp(frequency/viewFrequency(rate), 0.0f, 1.0f)*float(plot.getWidth()-1));
        graphics.drawVerticalLine(x, float(plot.getY()), float(image.getBottom()));
    }
    const float notch = bandParameters[NotchHz];
    if (bandParameters[NotchOn] >= 0.5f && std::isfinite(notch) && notch >= 0 && notch <= viewFrequency(rate)) {
        const float x = frequencyX(notch);
        graphics.setColour(theme::magenta.withAlpha(bypassed ? 0.35f : 1.0f));
        graphics.drawLine(x,float(plot.getY()),x,float(image.getBottom()),1.6f);
        const int labelWidth = juce::jmin(180,plot.getWidth());
        const int labelX = juce::jlimit(plot.getX(),plot.getRight()-labelWidth,int(x)+5);
        graphics.setFont(11);
        graphics.drawText("NOTCH "+juce::String(notch,0)+" Hz"+(bypassed ? " (bypass)" : ""),
                          labelX,plot.getY()+17,labelWidth,16,juce::Justification::centredLeft);
    }
    graphics.setColour(theme::muted); graphics.setFont(11);
    graphics.drawText("0 Hz", axis, juce::Justification::centredLeft);
    graphics.drawText(juce::String(low, 0)+" - "+juce::String(high, 0)+" Hz", axis, juce::Justification::centred);
    graphics.drawText(juce::String(viewFrequency(rate)/1000, 1)+" kHz", axis, juce::Justification::centredRight);
}
}
