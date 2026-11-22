// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#pragma once
#include "audio/pipewire_engine.hpp"
#include "gui/device_picker.hpp"
#include "gui/about_button.hpp"
#include "gui/about_dialog.hpp"
#include "gui/theme.hpp"
#include "gui/bandwidth_knob.hpp"
#include "gui/spectrum_view.hpp"

namespace qrmforge::gui {
class MainComponent : public juce::Component, private juce::Timer {
public:
    MainComponent();
    ~MainComponent() override;
    void paint(juce::Graphics& graphics) override;
    void resized() override;
private:
    friend bool testStandaloneControls();
    juce::TooltipWindow tooltips{this,700};
    PipeWireEngine engine;
    AudioState state = AudioState::Disconnected;
    DevicePicker routeInput{"INPUT"}, routeOutput{"OUTPUT"};
    juce::TextButton applyRouting{"Apply audio"}, refreshAudio{"Reconnect"}, stopAudio{"Stop audio"};
    juce::TextButton recordAudio{"REC"};
    juce::Label recordingStatus;
    AboutButton aboutButton;
    juce::Label aboutVersion, aboutLicense;
    juce::TextEditor thirdPartyNotices;
    juce::HyperlinkButton aboutRepository{"github.com/logocomune/qrmforge", juce::URL("https://github.com/logocomune/qrmforge/")};
    std::unique_ptr<AboutDialog> aboutDialog;
    std::unique_ptr<juce::FileChooser> recordingChooser;
    juce::Label actualRoutes, status, modeTitle, presetTitle, controlsTitle, routing;
    Parameters values = defaults;
    juce::ComboBox modes, presets;
    std::array<juce::TextButton,2> slopes;
    juce::Rectangle<int> routePanel, lifecyclePanel;
    std::array<juce::Rectangle<int>,3> controlPanels;
    std::array<BandwidthKnob,8> knobs;
    std::array<juce::Label,8> knobLabels;
    std::array<juce::ToggleButton,7> toggles;
    Spectrum input{"RECEIVER / PRE-DSP",theme::cyan};
    Spectrum output{"MONITOR / POST-DSP",theme::cyan};
    double lastEdit = -1000, lastAudio = 0, lastLevelFrame = 0;
    std::uint32_t lastWritten = 0;
    bool syncing = false;
    float displayedRate = 0;
    std::uint32_t displayedModes = 0;
    static std::unique_ptr<juce::ModalComponentManager::Callback> aboutDismissal(juce::Component&);
    void showAbout();
    void updateRouting();
    void edit(Parameter parameter, float value);
    void applyPreset();
    void synchronize();
    void toggleRecording();
    void updateRecording();
    void timerCallback() override;
};
}
