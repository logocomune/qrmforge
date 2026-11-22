// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#pragma once
#include "gui/main_component.hpp"
#include "gui/bandwidth_knob.hpp"
#include <limits>
#include <random>
namespace qrmforge::gui {
inline bool testNotchMarker() {
    for (const auto accent : {juce::Colour(0xff56c9e8),juce::Colour(0xffedb86f)}) {
        Spectrum spectrum("TEST",accent);
        spectrum.setSize(360,220);
        auto values = defaults; values[Bypass] = 0; values[NotchHz] = 1200;
        const auto render = [&] {
            spectrum.setBand(values,48000);
            return spectrum.createComponentSnapshot(spectrum.getLocalBounds());
        };
        const auto marker = [](const juce::Image& image, int x) {
            int intensity = 0;
            for (int offset = -2; offset <= 2; ++offset) {
                const auto pixel = image.getPixelAt(x+offset,150);
                if (pixel.getRed() > pixel.getGreen()+20 && pixel.getBlue() > pixel.getGreen()+8)
                    intensity = juce::jmax(intensity,int(pixel.getRed()));
            }
            return intensity;
        };
        const auto meterBounds = spectrum.getChildComponent(0)->getBounds();
        if (!spectrum.getLocalBounds().contains(meterBounds) || meterBounds.getWidth() >= meterBounds.getHeight()) return false;
        const int plotWidth = meterBounds.getX()-10-16;
        const int firstX = 16+int(1200.0f/4500*float(plotWidth-1));
        const int secondX = 16+int(2400.0f/4500*float(plotWidth-1));
        if (marker(render(),firstX)) return false;
        values[NotchOn] = 1;
        const int active = marker(render(),firstX);
        if (!active) return false;
        values[NotchHz] = 2400;
        auto moved = render();
        if (marker(moved,firstX) || !marker(moved,secondX)) return false;
        values[Bypass] = 1;
        const int bypass = marker(render(),secondX);
        if (!bypass || bypass >= active) return false;
        for (const float frequency : {-1.0f,6000.0f,std::numeric_limits<float>::quiet_NaN()}) {
            values[NotchHz] = frequency;
            auto hidden = render();
            for (int x = 18; x < 16+plotWidth-2; ++x) if (marker(hidden,x)) return false;
        }
    }
    Spectrum live("LIVE",theme::cyan);
    live.setSize(480,300);
    std::array<float,fftSize> samples{};
    for (std::size_t index = 0; index < samples.size(); ++index)
        samples[index] = 0.5f*std::sin(float(index)*2.0f*juce::MathConstants<float>::pi*700.0f/48000.0f);
    live.push(samples,48000);
    if (live.createComponentSnapshot(live.getLocalBounds()).isNull()) return false;
    live.clear();
    if (live.createComponentSnapshot(live.getLocalBounds()).isNull()) return false;
    return true;
}
inline bool testStandaloneControls() {
    {
        LevelMeter meter;
        for (const auto size : std::vector<juce::Point<int>>{{0,0},{55,120},{80,63},{80,120}}) {
            meter.setSize(size.x,size.y);
            juce::Image image(juce::Image::RGB,80,120,true);
            juce::Graphics graphics(image);
            meter.paint(graphics);
            for (const auto levels : {AudioLevels{},AudioLevels{-80,-70},AudioLevels{-20,-6},AudioLevels{0,0},AudioLevels{6,6}}) {
                meter.update(levels);
                meter.paint(graphics);
                if (!meter.hasData() || meter.levels().rmsDbfs != levels.rmsDbfs) return false;
            }
            meter.clear();
            if (meter.hasData() || !std::isinf(meter.levels().rmsDbfs)) return false;
        }
    }
    {
        MainComponent metered;
        auto& processor = metered.engine.processor();
        processor.enable(true);
        std::array<float,fftSize> samples{}, output{};
        samples.fill(0.25f);
        processor.process(samples.data(),output.data(),output.size(),48000);
        metered.timerCallback();
        if (!metered.input.hasLevels() || !metered.output.hasLevels() ||
            metered.input.levels().rmsDbfs != metered.output.levels().rmsDbfs) return false;
        auto muted = defaults;
        muted[Mute] = 1;
        processor.publish(muted);
        processor.process(samples.data(),output.data(),output.size(),48000);
        metered.timerCallback();
        if (!std::isfinite(metered.input.levels().rmsDbfs) || !std::isinf(metered.output.levels().rmsDbfs)) return false;
        metered.lastLevelFrame = juce::Time::getMillisecondCounterHiRes()/1000-3;
        metered.timerCallback();
        if (metered.input.hasLevels() || metered.output.hasLevels()) return false;
        processor.process(samples.data(),output.data(),output.size(),48000);
        metered.timerCallback();
        if (!metered.input.hasLevels()) return false;
        metered.stopAudio.onClick();
        if (metered.input.hasLevels() || metered.output.hasLevels()) return false;
    }

    juce::Component dismissalTarget;
    dismissalTarget.setVisible(true);
    auto dismissal = MainComponent::aboutDismissal(dismissalTarget);
    dismissal->modalStateFinished(0);
    if (dismissalTarget.isVisible()) return false;
    auto deletedTarget = std::make_unique<juce::Component>();
    auto lateDismissal = MainComponent::aboutDismissal(*deletedTarget);
    deletedTarget.reset();
    lateDismissal->modalStateFinished(0);
    MainComponent component;
    if (component.aboutRepository.getName().isNotEmpty()) return false;
    if (component.aboutButton.getButtonText() != "About" ||
        component.aboutRepository.getURL().toString(false) != "https://github.com/logocomune/qrmforge/" ||
        !component.aboutButton.isVisible() || !component.aboutButton.isEnabled()) return false;
    if (component.values != defaults || !component.toggles[0].getToggleState() || component.toggles[6].getToggleState()) return false;
    std::vector<juce::Point<int>> sizes{{920,900}, {1119,900}, {1120,900}, {1280,940}, {1800,1400}};
    std::mt19937 generator(610842);
    for (int trial = 0; trial < 100; ++trial)
        sizes.emplace_back(920+int(generator()%881),900+int(generator()%501));
    for (const auto size : sizes) {
        component.setSize(size.x,size.y);
        std::vector<juce::Component*> controls{&component.input,&component.output,&component.routeInput,
            &component.routeOutput,&component.applyRouting,&component.refreshAudio,&component.stopAudio,
            &component.recordAudio,&component.recordingStatus,&component.actualRoutes,&component.status,&component.aboutButton,
            &component.modes,&component.presets,&component.modeTitle,&component.presetTitle,&component.controlsTitle,&component.routing};
        for (auto& knob : component.knobs) controls.push_back(&knob);
        for (auto& title : component.knobLabels) controls.push_back(&title);
        for (auto& toggle : component.toggles) controls.push_back(&toggle);
        for (auto& slope : component.slopes) controls.push_back(&slope);
        for (std::size_t index = 0; index < controls.size(); ++index) {
            const auto bounds = controls[index]->getBounds();
            if (bounds.isEmpty() || !component.getLocalBounds().contains(bounds)) return false;
            for (std::size_t other = index+1; other < controls.size(); ++other)
                if (bounds.intersects(controls[other]->getBounds())) return false;
        }
        if (size.x >= 1120) {
            if (component.input.getY() != component.output.getY() || component.input.getRight() >= component.output.getX()) return false;
        } else if (component.input.getBottom() >= component.output.getY()) return false;
        if (component.input.getHeight() < 150 || component.output.getHeight() < 150) return false;
        for (auto* monitor : {&component.input,&component.output}) {
            const auto bounds = monitor->getChildComponent(0)->getBounds();
            if (!monitor->getLocalBounds().contains(bounds) || bounds.getX() <= monitor->getWidth()/2 ||
                bounds.getHeight() <= bounds.getWidth()) return false;
        }
        if (component.aboutButton.getY() >= component.routePanel.getY()) return false;
        for (auto* action : std::initializer_list<juce::Component*>{&component.recordAudio,&component.recordingStatus,
             &component.refreshAudio,&component.stopAudio,&component.toggles[0],&component.toggles[6]})
            if (!component.lifecyclePanel.contains(action->getBounds())) return false;
        for (auto* selector : std::initializer_list<juce::Component*>{&component.routeInput,&component.routeOutput,
             &component.applyRouting,&component.actualRoutes})
            if (!component.routePanel.contains(selector->getBounds())) return false;
    }
    const auto iconImage = component.aboutButton.createComponentSnapshot(component.aboutButton.getLocalBounds());
    if (iconImage.isNull()) return false;
    const int iconCentreY = component.aboutButton.getHeight() / 2;
    if (iconImage.getPixelAt(18, iconCentreY + 2) == iconImage.getPixelAt(21, iconCentreY + 2)) return false;
    if (component.values != defaults || component.engine.processor().requestedParameters() != defaults) return false;
    for (const int width : {920,1280}) {
        component.setSize(width,940);
        const auto image = component.createComponentSnapshot(component.getLocalBounds());
        if (image.isNull() || image.getWidth() != width || image.getHeight() != 940) return false;
    }
    for (std::size_t index = 0; index < component.knobs.size(); ++index) {
        const auto expected = index == 3 ? theme::magenta : index == 7 ? theme::amber : theme::cyan;
        if (component.knobs[index].findColour(juce::Slider::rotarySliderFillColourId) != expected) return false;
    }
    auto& action = component.refreshAudio;
    const auto renderAction = [&](bool highlighted, bool down) {
        juce::Image image(juce::Image::RGB,action.getWidth(),action.getHeight(),true);
        juce::Graphics graphics(image);
        action.getLookAndFeel().drawButtonBackground(graphics,action,
            action.findColour(juce::TextButton::buttonColourId),highlighted,down);
        return image.getPixelAt(action.getWidth()/2,action.getHeight()/2);
    };
    const auto normalAction = renderAction(false,false);
    if (normalAction == renderAction(true,false) || renderAction(true,false) == renderAction(true,true)) return false;
    action.setEnabled(false);
    if (normalAction == renderAction(false,false)) return false;
    action.setEnabled(true);
    auto& button = component.toggles[3];
    const auto renderButton = [&](bool highlighted, bool down) {
        juce::Image image(juce::Image::RGB,button.getWidth(),button.getHeight(),true);
        juce::Graphics graphics(image);
        button.getLookAndFeel().drawToggleButton(graphics,button,highlighted,down);
        return image.getPixelAt(10,5);
    };
    const auto idle = renderButton(false,false);
    const auto hovered = renderButton(true,false);
    const auto pressed = renderButton(false,true);
    if (idle == hovered || hovered == pressed || idle == pressed) return false;
    button.setToggleState(true,juce::sendNotificationSync);
    if (renderButton(false,false) == idle) return false;
    button.setToggleState(false,juce::sendNotificationSync);
    component.knobs[0].setEnabled(false);
    if (component.createComponentSnapshot(component.getLocalBounds()).isNull()) return false;
    component.knobs[0].setEnabled(true);
    for (const bool bypass : {true,false}) {
        component.toggles[0].setToggleState(bypass,juce::sendNotificationSync);
        if (component.controlsTitle.getText() != (bypass ? "BYPASS / RAW INPUT" : "DSP PROCESSING ENABLED")) return false;
        component.toggles[6].setToggleState(true,juce::sendNotificationSync);
        if (component.controlsTitle.getText() != "OUTPUT MUTED") return false;
        component.toggles[6].setToggleState(false,juce::sendNotificationSync);
        if (component.createComponentSnapshot(component.getLocalBounds()).isNull()) return false;
    }
    component.toggles[0].setToggleState(true,juce::sendNotificationSync);
    component.syncing = true;
    component.slopes[0].onClick(); component.knobs[2].onSlopeChange();
    if (component.values[LowCutSteep] != 0 || component.values[HighCutSteep] != 0) return false;
    component.syncing = false;
    for (std::size_t index = 0; index < component.slopes.size(); ++index) {
        const auto parameter = index == 0 ? LowCutSteep : HighCutSteep;
        for (const auto expected : {1.0f,0.0f}) {
            const auto before = component.values;
            component.slopes[index].onClick();
            auto wanted = before; wanted[parameter] = expected;
            if (component.values != wanted || component.engine.processor().requestedParameters() != wanted) return false;
            if (component.slopes[index].getButtonText() != (expected == 1 ? "24 dB/oct" : "12 dB/oct")) return false;
        }
    }
    auto& agc = component.toggles[5];
    if (agc.getButtonText() != "AGC" || agc.getToggleState() || component.values[AgcOn] != 0) return false;
    if (!agc.isVisible() || agc.getWidth() < 70 || agc.getHeight() == 0) return false;
    agc.setToggleState(true,juce::sendNotificationSync);
    if (component.values[AgcOn] != 1 || component.engine.processor().requestedParameters()[AgcOn] != 1) return false;
    component.synchronize();
    if (!agc.getToggleState()) return false;
    agc.setToggleState(false,juce::sendNotificationSync);
    if (component.values[AgcOn] != 0 || component.engine.processor().requestedParameters()[AgcOn] != 0) return false;
    if (component.modes.getNumItems() != int(noiseReductionCount)) return false;
    if (component.recordAudio.isEnabled()) return false;
    component.recordAudio.onClick();
    for (unsigned mode = 0; mode < noiseReductionCount; ++mode)
        if (component.modes.isItemEnabled(int(mode)+1) != modeAvailable(component.engine.processor().availableModes(),NoiseReduction(mode))) return false;
    component.modes.setSelectedId(4,juce::sendNotificationSync);
    if (component.values[NrMode] != 3) return false;
    component.knobs[5].setValue(7.5,juce::sendNotificationSync);
    if (component.engine.processor().requestedParameters()[NrLevel] != 7.5f) return false;
    if (component.modes.isItemEnabled(5)) {
        component.modes.setSelectedId(5,juce::sendNotificationSync);
        if (component.values[NrMode] != 4) return false;
    }
    if (!component.modes.isItemEnabled(6)) {
        component.edit(NrMode,5);
        if (component.values[NrMode] != 0) return false;
    } else {
        component.modes.setSelectedId(6,juce::sendNotificationSync);
        if (component.values[NrMode] != 5) return false;
    }
    component.knobs[0].setValue(6,juce::sendNotificationSync);
    if (component.values[PreGain] != 6 || component.engine.processor().requestedParameters()[PreGain] != 6) return false;
    component.knobs[1].onSlopeChange();
    component.knobs[2].onSlopeChange();
    if (component.values[LowCutSteep] != 1 || component.values[HighCutSteep] != 1) return false;
    component.presets.setSelectedId(5,juce::sendNotificationSync);
    if (component.values[ApfOn] != 1 || component.values[NrMode] != 1 || component.modes.isItemEnabled(3)) return false;
    if (component.modes.isItemEnabled(5) || component.modes.isItemEnabled(6) || !component.modes.isItemEnabled(4)) return false;
    component.presets.setSelectedId(3,juce::sendNotificationSync);
    if (component.values[ApfOn] != 0 || component.values[HighCut] != 2700 || component.values[LowCutSteep] != 1) return false;
    component.modes.setSelectedId(1,juce::sendNotificationSync);
    if (component.values[NrMode] != 0) return false;
    component.modes.setSelectedId(2,juce::dontSendNotification);
    component.timerCallback();
    if (component.modes.getSelectedId() != 2) return false;
    component.modes.onChange();
    if (component.values[NrMode] != 1) return false;
    for (auto& toggle : component.toggles) toggle.setToggleState(true,juce::sendNotificationSync);
    if (component.values[Mute] != 1 || component.values[Attenuate] != 1 || component.values[BlankerOn] != 1 || component.values[AgcOn] != 1) return false;
    component.refreshAudio.onClick();
    if (component.engine.processor().requestedParameters() != component.values) return false;
    component.state = AudioState::Running;
    auto& processor = component.engine.processor();
    processor.enable(true);
    const auto capture = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("qrmforge-recording-widget",".wv",false);
    if (!processor.recorder().start({capture.getFullPathName().toStdString(),48000})) return false;
    std::array<float,256> samples{}, output{}; samples.fill(0.25f);
    processor.process(samples.data(),output.data(),samples.size(),48000);
    component.updateRecording();
    if (component.recordAudio.getButtonText() != "Stop REC") return false;
    component.recordAudio.onClick(); processor.recorder().wait(); component.updateRecording();
    if (component.recordAudio.getButtonText() != "REC" || !component.recordingStatus.getText().startsWith("Saved")) return false;
    if (!capture.existsAsFile() || !capture.deleteFile()) return false;
    processor.enable(false);
    component.stopAudio.onClick();
    if (component.aboutVersion.getText() != "Version " + juce::JUCEApplication::getInstance()->getApplicationVersion() ||
        !component.aboutLicense.getText().contains("GPL-3.0-only") ||
        !component.aboutLicense.getText().contains("Third-party")) return false;
    if (component.thirdPartyNotices.getText().contains("/home/")) return false;
    if (component.thirdPartyNotices.getName() != "Third-party software & licenses" ||
        component.thirdPartyNotices.findColour(juce::TextEditor::backgroundColourId) != juce::Colour(0xff0b121c) ||
        component.thirdPartyNotices.getBorder() != juce::BorderSize<int>(12)) return false;
    if (!component.thirdPartyNotices.isReadOnly() || !component.thirdPartyNotices.isMultiLine()) return false;
    for (const auto* name : {"JUCE", "PipeWire", "RNNoise", "SpeexDSP", "DeepFilterNet", "WavPack", "LADSPA", "GNU GENERAL PUBLIC LICENSE"})
        if (!component.thirdPartyNotices.getText().contains(name)) return false;
    component.aboutButton.onClick();
    if (!component.aboutDialog || !component.aboutDialog->isVisible()) return false;
    auto* dialog = component.aboutDialog.get();
    component.aboutButton.onClick();
    if (component.aboutDialog.get() != dialog) return false;
    for (const auto size : std::vector<juce::Point<int>>{{920,900},{1280,940},{1800,1400}}) {
        component.setSize(size.x,size.y);
        if (!component.getLocalBounds().contains(component.aboutDialog->panel) ||
            !component.aboutDialog->panel.contains(component.thirdPartyNotices.getBounds()) ||
            !component.aboutDialog->panel.contains(component.aboutDialog->closeButton.getBounds()) ||
            component.thirdPartyNotices.getBounds().intersects(component.aboutDialog->closeButton.getBounds())) return false;
        if (component.aboutDialog->createComponentSnapshot(component.aboutDialog->getLocalBounds()).isNull()) return false;
    }
    component.aboutDialog->closeButton.onClick();
    if (component.aboutDialog->isVisible() || component.aboutDialog->isCurrentlyModal()) return false;
    component.aboutButton.onClick();
    component.aboutDialog->dismissButton.onClick();
    if (component.aboutDialog->isVisible()) return false;
    for (const auto key : {juce::KeyPress::escapeKey,juce::KeyPress::returnKey}) {
        component.aboutButton.onClick();
        if (!component.aboutDialog->keyPressed(juce::KeyPress(key)) || component.aboutDialog->isVisible()) return false;
    }
    component.aboutButton.onClick();
    if (component.aboutDialog->keyPressed(juce::KeyPress('a')) || !component.aboutDialog->isVisible()) return false;
    component.aboutDialog->dismiss();
    return component.engine.processor().requestedParameters() == component.values;
}
inline bool testBandwidthKnob() {
    BandwidthKnob knob;
    knob.setRange(50, 1500, 1); knob.setValue(300, juce::dontSendNotification);
    int slope = 12;
    knob.onSlopeChange = [&] { slope = slope == 12 ? 24 : 12; };
    const auto now = juce::Time::getCurrentTime();
    const juce::MouseEvent click(juce::Desktop::getInstance().getMainMouseSource(), {30, 30},
        juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1, 0, 0, 0, 0,
        &knob, &knob, now, {30, 30}, now, 2, false);
    knob.mouseDoubleClick(click);
    if (slope != 24 || knob.getValue() != 300) return false;
    knob.mouseDoubleClick(click);
    if (slope != 12 || knob.getValue() != 300) return false;
    juce::Label valueText;
    const juce::MouseEvent textClick(juce::Desktop::getInstance().getMainMouseSource(), {5, 5},
        juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1, 0, 0, 0, 0,
        &valueText, &valueText, now, {5, 5}, now, 2, false);
    knob.mouseDoubleClick(textClick);
    if (slope != 12 || knob.getValue() != 300) return false;
    const juce::MouseEvent rightClick(juce::Desktop::getInstance().getMainMouseSource(), {30, 30},
        juce::ModifierKeys(juce::ModifierKeys::rightButtonModifier), 1, 0, 0, 0, 0,
        &knob, &knob, now, {30, 30}, now, 2, false);
    knob.mouseDoubleClick(rightClick);
    if (slope != 12) return false;
    knob.setEnabled(false); knob.mouseDoubleClick(click);
    if (slope != 12) return false;
    knob.setEnabled(true); knob.onSlopeChange = {};
    knob.mouseDoubleClick(click);
    return knob.getValue() == 300;
}
}
