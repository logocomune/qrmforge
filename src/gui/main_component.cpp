// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#include "gui/main_component.hpp"
#include "dsp/core.hpp"
#include "gui/controls.hpp"
#include "qrmforge_assets.hpp"
namespace qrmforge::gui {
namespace {
constexpr Parameter knobParameters[]{PreGain, LowCut, HighCut, NotchHz, ApfHz, NrLevel, PostGain, BlankerThreshold};
constexpr Parameter toggleParameters[]{Bypass, Attenuate, BlankerOn, NotchOn, ApfOn, AgcOn, Mute};
constexpr const char* knobNames[]{"PRE-GAIN", "LOW CUT", "HIGH CUT", "NOTCH", "CW FREQUENCY", "NR STRENGTH", "OUTPUT GAIN", "NB THRESHOLD"};
constexpr const char* toggleNames[]{"BYPASS", "ATT -10 dB", "NOISE BLANKER", "NOTCH", "CW APF", "AGC", "MUTE"};
constexpr double minimum[]{-20,50,400,200,300,0.1,-20,2};
constexpr double maximum[]{20,1500,4500,3500,1200,10,20,20};
double seconds() { return juce::Time::getMillisecondCounterHiRes()/1000; }
void label(juce::Label& component, const juce::String& text) {
    component.setText(text, juce::dontSendNotification);
    component.setColour(juce::Label::textColourId, theme::muted);
    component.setFont(juce::Font(13.0f,juce::Font::bold));
}
}
MainComponent::MainComponent() {
    setSize(1280, 940);
    for (auto* component : std::initializer_list<juce::Component*>{&modes,&presets,&status,
         &modeTitle,&presetTitle,&controlsTitle,&routing,&input,&output,&routeInput,&routeOutput,
         &applyRouting,&refreshAudio,&stopAudio,&actualRoutes,&recordAudio,&recordingStatus,&aboutButton}) addAndMakeVisible(component);
    label(aboutVersion, "Version " + juce::JUCEApplication::getInstance()->getApplicationVersion());
    label(aboutLicense, "GPL-3.0-only\nThird-party components retain their own licenses.");
    thirdPartyNotices.setName("Third-party software & licenses");
    thirdPartyNotices.setMultiLine(true);
    thirdPartyNotices.setFont(juce::Font(13.0f));
    thirdPartyNotices.setBorder(juce::BorderSize<int>(12));
    thirdPartyNotices.setIndents(2,4);
    thirdPartyNotices.setColour(juce::TextEditor::backgroundColourId,juce::Colour(0xff0b121c));
    thirdPartyNotices.setColour(juce::TextEditor::textColourId,juce::Colour(0xffc4d0df));
    thirdPartyNotices.setColour(juce::TextEditor::outlineColourId,juce::Colour(0xff29394d));
    thirdPartyNotices.setColour(juce::TextEditor::focusedOutlineColourId,juce::Colour(0xff56c9e8));
    thirdPartyNotices.setColour(juce::TextEditor::highlightColourId,juce::Colour(0xff24465a));
    thirdPartyNotices.setColour(juce::ScrollBar::thumbColourId,juce::Colour(0xff48627c));
    thirdPartyNotices.setReadOnly(true);
    thirdPartyNotices.setScrollbarsShown(true);
    thirdPartyNotices.setText(juce::String::fromUTF8(QRMForgeAssets::qrmforge_notices_txt,
        QRMForgeAssets::qrmforge_notices_txtSize), false);
    aboutRepository.setName({});
    aboutButton.onClick = [this] { showAbout(); };
    aboutButton.setTooltip("About QRMForge and its source repository.");
    label(modeTitle, "NOISE REDUCTION"); label(presetTitle, "BAND PRESET");
    label(controlsTitle, "PROCESSING BYPASSED"); label(status, "Ready / no audio selected");
    controlsTitle.setJustificationType(juce::Justification::centredRight);
    controlsTitle.setFont(juce::Font(12,juce::Font::bold));
    label(routing, "Select input and output, then Apply audio. Closing stops processing.");
    label(actualRoutes, "Actual input: Not connected\nActual output: Not connected");
    label(recordingStatus,"WavPack / float32 lossless / processed output");
    for (auto* information : {&status,&recordingStatus,&actualRoutes,&routing})
        information->setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(),12.0f,juce::Font::plain));
    applyRouting.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff00d2ff));
    applyRouting.setColour(juce::TextButton::textColourOffId,juce::Colour(0xff071016));
    for (std::size_t index = 0; index < slopes.size(); ++index) {
        slopes[index].onClick = [this,index] {
            const auto parameter = index == 0 ? LowCutSteep : HighCutSteep;
            if (!syncing) edit(parameter,values[parameter] >= 0.5f ? 0 : 1);
        };
        slopes[index].setTooltip("Switch cutoff slope between 12 and 24 dB/octave.");
        addAndMakeVisible(slopes[index]);
    }
    recordAudio.onClick = [this] { toggleRecording(); };
    recordAudio.setTooltip("Record processed output as fast, lossless 32-bit float WavPack. Bypass and mute are included.");
    applyRouting.onClick = [this] { engine.apply(routeInput.selection(),routeOutput.selection()); updateRouting(); };
    stopAudio.onClick = [this] { engine.stop(); input.clear(); output.clear(); updateRouting(); };
    refreshAudio.onClick = [this] { engine.reconnect(); engine.processor().publish(values); input.clear(); output.clear(); updateRouting(); };
    modes.addItemList({"OFF", "NLMS / voice + CW", "RNNoise / voice", "Wiener / spectral", "SpeexDSP / voice", "DeepFilterNet / voice"}, 1);
    presets.addItemList({"AM 100-4500", "FM 300-3000", "SSB 300-2700", "SSB DX 400-2000", "CW 700 / 250 Hz"}, 1);
    presets.setTextWhenNothingSelected("Choose preset");
    modes.onChange = [this] { if (!syncing) edit(NrMode, float(modes.getSelectedId()-1)); };
    presets.onChange = [this] { if (!syncing) applyPreset(); };
    for (std::size_t index = 0; index < knobs.size(); ++index) {
        auto& knob = knobs[index];
        knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 90, 22);
        knob.setRange(minimum[index], maximum[index], index == 0 || index == 5 || index == 6 || index == 7 ? 0.1 : 1);
        const auto accent = index == 3 ? 0xffff2d78 : index == 7 ? 0xfff59e0b : 0xff00d2ff;
        knob.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(accent));
        knob.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff29374b));
        knob.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        knob.setTextValueSuffix(index == 0 || index == 6 ? " dB" : (index >= 1 && index <= 4 ? " Hz" : ""));
        knob.onValueChange = [this, index] {
            if (!syncing) edit(knobParameters[index], float(knobs[index].getValue()));
        };
        if (index == 1 || index == 2) {
            knob.onSlopeChange = slopes[index-1].onClick;
            knob.setTooltip("Drag to change cutoff. Double-click the knob to switch 12/24 dB/oct. Double-click the Hz text to edit frequency.");
        }
        label(knobLabels[index], knobNames[index]);
        knobLabels[index].setJustificationType(juce::Justification::centred);
        addAndMakeVisible(knob); addAndMakeVisible(knobLabels[index]);
    }
    for (std::size_t index = 0; index < toggles.size(); ++index) {
        toggles[index].setButtonText(toggleNames[index]);
        toggles[index].setColour(juce::ToggleButton::tickColourId,juce::Colour(index == 2 ? 0xfff59e0b : index == 3 || index == 6 ? 0xffff2d78 : index == 5 ? 0xff23c99a : 0xff00d2ff));
        if (toggleParameters[index] == AgcOn)
            toggles[index].setTooltip("Automatic output level after output gain: fast attack, slow release, up to +12 dB boost. Inactive during bypass or mute.");
        toggles[index].onClick = [this,index] {
            if (!syncing) edit(toggleParameters[index], toggles[index].getToggleState() ? 1.0f : 0.0f);
        };
        addAndMakeVisible(toggles[index]);
    }
    engine.processor().publish(values);
    synchronize(); updateRouting(); updateRecording(); startTimerHz(30);
}
MainComponent::~MainComponent() { stopTimer(); engine.stop(); }
void MainComponent::toggleRecording() {
    auto& recorder = engine.processor().recorder();
    if (recorder.snapshot().state == RecordingState::Recording) { recorder.stop(); updateRecording(); return; }
    if (state != AudioState::Running) return;
    const auto filename = "QRMForge-"+juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S")+".wv";
    recordingChooser = std::make_unique<juce::FileChooser>("Save processed audio as WavPack",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(filename),"*.wv");
    auto safe = juce::Component::SafePointer<MainComponent>(this);
    recordingChooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles,
        [safe](const juce::FileChooser& chooser) {
            if (!safe || chooser.getResult() == juce::File{}) return;
            if (safe->engine.snapshot().state != AudioState::Running) {
                safe->recordingStatus.setText("Start audio before recording",juce::dontSendNotification); return;
            }
            const auto file = chooser.getResult().withFileExtension(".wv");
            safe->engine.processor().recorder().start({file.getFullPathName().toStdString(),safe->engine.processor().sampleRate()});
            safe->updateRecording();
        });
}
void MainComponent::updateRecording() {
    const auto snapshot = engine.processor().recorder().snapshot();
    const bool active = snapshot.state == RecordingState::Recording;
    const bool saving = snapshot.state == RecordingState::Saving;
    recordAudio.setButtonText(active ? "Stop REC" : saving ? "Saving..." : "REC");
    recordAudio.setEnabled(!saving && (active || (state == AudioState::Running && engine.processor().frames() > 0)));
    recordAudio.setColour(juce::TextButton::buttonColourId,juce::Colour(active ? 0xffa52b4c : 0xff26364a));
    juce::String text;
    if (snapshot.state == RecordingState::Error) text = recordingErrorText(snapshot.error);
    else if (active || saving || snapshot.state == RecordingState::Finished) {
        const auto seconds = snapshot.sampleRate ? snapshot.samples/snapshot.sampleRate : 0;
        text = (active ? "REC " : saving ? "Saving " : snapshot.samples ? "Saved " : "No audio captured ");
        text += juce::String(seconds/60)+":"+juce::String(seconds%60).paddedLeft('0',2)+" / "+juce::File(snapshot.path).getFileName();
    } else text = "WavPack / float32 lossless / processed output";
    recordingStatus.setText(text,juce::dontSendNotification); recordingStatus.setTooltip(snapshot.path);
}
void MainComponent::updateRouting() {
    auto snapshot = engine.snapshot();
    std::vector<AudioNode> sources, sinks;
    for (const auto& node : snapshot.graph.nodes) {
        if (selectableInput(node)) sources.push_back(node);
        if (selectableOutput(node)) sinks.push_back(node);
    }
    const auto sort = [](const auto& left,const auto& right) { return left.serial < right.serial; };
    std::sort(sources.begin(),sources.end(),sort); std::sort(sinks.begin(),sinks.end(),sort);
    routeInput.setDevices(std::move(sources)); routeOutput.setDevices(std::move(sinks));
    actualRoutes.setText("Actual input: "+juce::String(snapshot.actualInput)+"\nActual output: "+juce::String(snapshot.actualOutput),juce::dontSendNotification);
    state = snapshot.state;
    routing.setText(juce::String(snapshot.message),juce::dontSendNotification);
    stopAudio.setEnabled(state == AudioState::Starting || state == AudioState::Running || state == AudioState::Error);
    applyRouting.setEnabled(state != AudioState::Starting && state != AudioState::Disconnected);
}
void MainComponent::edit(Parameter parameter, float value) {
    auto updated = values; updated[parameter] = value;
    updated = sanitize(updated, engine.processor().sampleRate());
    const auto mode = NoiseReduction(unsigned(updated[NrMode]));
    if (mode == NoiseReduction::DeepFilter && updated[ApfOn] < 0.5f) engine.processor().prepareNoiseReduction(mode);
    if (!modeAvailable(engine.processor().availableModes(),mode) || (voiceOnly(mode) && updated[ApfOn] >= 0.5f)) updated[NrMode] = 0;
    engine.processor().publish(updated); values = updated; lastEdit = seconds();
    presets.setSelectedId(0, juce::dontSendNotification);
    synchronize();
}
void MainComponent::applyPreset() {
    const auto updated = bandPreset(values, presets.getSelectedId());
    if (updated) { engine.processor().publish(*updated); values = *updated; lastEdit = seconds(); synchronize(); }
}
void MainComponent::synchronize() {
    syncing = true;
    displayedRate = engine.processor().sampleRate(); displayedModes = engine.processor().availableModes();
    for (std::size_t index = 0; index < knobs.size(); ++index)
        knobs[index].setValue(values[knobParameters[index]], juce::dontSendNotification);
    for (std::size_t index = 0; index < toggles.size(); ++index)
        toggles[index].setToggleState(values[toggleParameters[index]] >= 0.5f, juce::dontSendNotification);
    modes.setSelectedId(int(values[NrMode])+1, juce::dontSendNotification);
    for (unsigned index = 0; index < noiseReductionCount; ++index) {
        const auto mode = NoiseReduction(index);
        modes.setItemEnabled(int(index)+1,modeAvailable(displayedModes,mode) && !(voiceOnly(mode) && values[ApfOn] >= 0.5f));
    }
    modes.setTooltip("Voice modes are disabled during CW APF. RNNoise and DeepFilterNet require 48 kHz. SpeexDSP supports 8-48 kHz. DeepFilterNet loads its bundled local model on first selection.");
    knobs[5].setTooltip("NR strength: NLMS adaptation, Wiener suppression amount, SpeexDSP/DeepFilterNet attenuation limit (strength x 3 dB). RNNoise uses its fixed model.");
    for (int index : {1, 2}) {
        const auto slope = values[index == 1 ? LowCutSteep : HighCutSteep] >= 0.5f ? 24 : 12;
        slopes[std::size_t(index-1)].setButtonText(juce::String(slope)+" dB/oct");
    }
    controlsTitle.setText(values[Mute] >= 0.5f ? "OUTPUT MUTED" : values[Bypass] >= 0.5f ? "BYPASS / RAW INPUT" : "DSP PROCESSING ENABLED",juce::dontSendNotification);
    controlsTitle.setColour(juce::Label::textColourId,juce::Colour(values[Mute] >= 0.5f ? 0xffff2d78 : values[Bypass] >= 0.5f ? 0xff00d2ff : 0xff23c99a));
    controlsTitle.setTooltip("Effect switches retain their settings during bypass. Mute overrides bypass. Audio connection state is shown above.");
    input.setBand(values, engine.processor().sampleRate()); output.setBand(values, engine.processor().sampleRate());
    syncing = false;
}
void MainComponent::timerCallback() {
    updateRouting();
    updateRecording();
    auto& processor = engine.processor();
    const auto previous = values;
    const auto written = processor.frames();
    if (written != lastWritten) {
        lastWritten = written; lastAudio = seconds();
        AudioFrame frame;
        if (processor.readFrame(frame)) {
            input.push(frame.input,frame.sampleRate);
            output.push(frame.output,frame.sampleRate);
            lastLevelFrame = seconds();
        }
        if (seconds()-lastEdit > 0.2) values = processor.parameters();
    }
    const auto requested = processor.requestedParameters();
    const bool deepRequested = requested[NrMode] == float(NoiseReduction::DeepFilter) && requested[ApfOn] < 0.5f;
    const bool deepSelectable = modeAvailable(processor.availableModes(), NoiseReduction::DeepFilter);
    if (deepRequested && (deepSelectable || values[NrMode] == float(NoiseReduction::DeepFilter)))
        values[NrMode] = deepSelectable ? requested[NrMode] : float(NoiseReduction::Off);
    if (seconds()-lastLevelFrame > 2) { input.clearLevels(); output.clearLevels(); }
    if (previous != values || displayedRate != processor.sampleRate() || displayedModes != processor.availableModes()) synchronize();
    juce::String message = state == AudioState::Running ? "DSP running" : state == AudioState::Starting ? "Connecting audio" : "Audio stopped";
    if (state == AudioState::Running && seconds()-lastAudio > 2) message = "No audio updates: check source playback";
    message += " / "+juce::String(processor.sampleRate(),0)+" Hz / "+juce::String(processor.quantum())+" frames";
    if (deepRequested && processor.deepFilterStarting()) message += " / Loading DeepFilterNet";
    else if (deepRequested && !deepSelectable && processor.sampleRate() == 48000) message += " / DeepFilterNet unavailable";
    else if (values[NrMode] >= 2) message += " / buffered NR";
    if (values[NrMode] == 5 && processor.nrDroppedFrames()) message += " / NR dropped frames: "+juce::String(processor.nrDroppedFrames());
    if (processor.invalidBlocks()) message += " / invalid blocks: "+juce::String(processor.invalidBlocks());
    status.setText(message,juce::dontSendNotification);
}
std::unique_ptr<juce::ModalComponentManager::Callback> MainComponent::aboutDismissal(juce::Component& component) {
    auto dialog = juce::Component::SafePointer<juce::Component>(&component);
    return std::unique_ptr<juce::ModalComponentManager::Callback>(juce::ModalCallbackFunction::create([dialog](int) {
        if (dialog) dialog->setVisible(false);
    }));
}
void MainComponent::showAbout() {
    if (aboutDialog && aboutDialog->isCurrentlyModal()) {
        aboutDialog->toFront(true);
        return;
    }
    aboutVersion.setJustificationType(juce::Justification::centred);
    aboutLicense.setJustificationType(juce::Justification::centred);
    aboutRepository.setColour(juce::HyperlinkButton::textColourId,juce::Colour(0xff00f2fe));
    aboutDialog = std::make_unique<AboutDialog>(AboutContent{aboutVersion,aboutLicense,aboutRepository,thirdPartyNotices});
    addAndMakeVisible(*aboutDialog);
    aboutDialog->setBounds(getLocalBounds());
    aboutDialog->enterModalState(true, aboutDismissal(*aboutDialog).release(), false);
}
void MainComponent::paint(juce::Graphics& graphics) {
    graphics.fillAll(theme::background);
    const auto icon = juce::ImageCache::getFromMemory(QRMForgeAssets::qrmforge_png,QRMForgeAssets::qrmforge_pngSize);
    graphics.drawImageWithin(icon,16,22,34,34,juce::RectanglePlacement::centred);
    graphics.setColour(juce::Colours::white);
    graphics.setFont(juce::Font(25.0f,juce::Font::bold));
    graphics.drawText("QRM",60,21,60,36,juce::Justification::centredLeft);
    graphics.setColour(theme::cyan);
    graphics.drawText("Forge",122,21,90,36,juce::Justification::centredLeft);
    const auto panel = [&](juce::Rectangle<int> bounds) {
        graphics.setColour(theme::surface);
        graphics.fillRoundedRectangle(bounds.toFloat(),12.0f);
        graphics.setColour(theme::border);
        graphics.drawRoundedRectangle(bounds.toFloat().reduced(0.5f),12.0f,1.0f);
    };
    panel(lifecyclePanel);
    panel(routePanel);
    constexpr const char* titles[]{"GAIN", "FILTERS / MANUAL CW", "NOISE REDUCTION"};
    for (std::size_t index = 0; index < controlPanels.size(); ++index) {
        panel(controlPanels[index]);
        graphics.setColour(theme::muted);
        graphics.setFont(juce::Font(11.0f,juce::Font::bold));
        graphics.drawText(titles[index],controlPanels[index].reduced(12,6).removeFromTop(18),juce::Justification::centredLeft);
    }
}
void MainComponent::resized() {
    if (aboutDialog) aboutDialog->setBounds(getLocalBounds());
    auto area = getLocalBounds().reduced(16);
    auto heading = area.removeFromTop(48);
    aboutButton.setBounds(heading.removeFromRight(96));
    heading.removeFromRight(12);
    heading.removeFromLeft(214); status.setBounds(heading);
    area.removeFromTop(8);
    lifecyclePanel = area.removeFromTop(48);
    auto lifecycle = lifecyclePanel.reduced(10,8);
    refreshAudio.setBounds(lifecycle.removeFromRight(108).reduced(3,0));
    stopAudio.setBounds(lifecycle.removeFromRight(108).reduced(3,0));
    toggles[6].setBounds(lifecycle.removeFromRight(90).reduced(3,0));
    toggles[0].setBounds(lifecycle.removeFromRight(108).reduced(3,0));
    recordAudio.setBounds(lifecycle.removeFromLeft(90));
    recordingStatus.setBounds(lifecycle.reduced(10,0));
    area.removeFromTop(10);
    routePanel = area.removeFromTop(84);
    auto route = routePanel.reduced(12,10);
    auto routeRow = route.removeFromTop(36);
    applyRouting.setBounds(routeRow.removeFromRight(132).reduced(4,0));
    routeInput.setBounds(routeRow.removeFromLeft(routeRow.getWidth()/2));
    routeOutput.setBounds(routeRow);
    actualRoutes.setBounds(route.removeFromBottom(24));
    area.removeFromTop(10);
    auto selectors = area.removeFromTop(52);
    auto noise = selectors.removeFromLeft(int(float(selectors.getWidth())*0.22f));
    auto preset = selectors.removeFromLeft(int(float(getWidth()-32)*0.25f));
    modeTitle.setBounds(noise.removeFromTop(20)); modes.setBounds(noise.reduced(0,1).withTrimmedRight(12));
    presetTitle.setBounds(preset.removeFromTop(20)); presets.setBounds(preset.reduced(0,1).withTrimmedRight(12));
    controlsTitle.setBounds(selectors);
    area.removeFromTop(12);
    routing.setBounds(area.removeFromBottom(22));
    area.removeFromBottom(8);
    auto switches = area.removeFromBottom(40);
    const int switchWidth = switches.getWidth()/5;
    for (int index = 1; index <= 5; ++index) toggles[std::size_t(index)].setBounds(switches.removeFromLeft(switchWidth).reduced(4,0));
    area.removeFromBottom(10);
    auto controls = area.removeFromBottom(190);
    const int unit = (controls.getWidth()-24)/8;
    controlPanels[0] = controls.removeFromLeft(unit*2);
    controls.removeFromLeft(12);
    controlPanels[1] = controls.removeFromLeft(unit*4);
    controls.removeFromLeft(12);
    controlPanels[2] = controls;
    constexpr std::array<std::array<int,4>,3> groups{{{{0,6,-1,-1}},{{1,2,3,4}},{{5,7,-1,-1}}}};
    for (std::size_t group = 0; group < groups.size(); ++group) {
        auto row = controlPanels[group].reduced(8,8).withTrimmedTop(22);
        const int count = group == 1 ? 4 : 2;
        const int width = row.getWidth()/count;
        for (int position = 0; position < count; ++position) {
            const auto index = std::size_t(groups[group][std::size_t(position)]);
            auto bounds = row.removeFromLeft(width).reduced(2,0);
            knobLabels[index].setBounds(bounds.removeFromTop(18));
            auto slope = bounds.removeFromBottom(22);
            knobs[index].setBounds(bounds);
            if (index == 1 || index == 2) slopes[index-1].setBounds(slope.reduced(4,0));
        }
    }
    area.removeFromBottom(12);
    if (getWidth() >= 1120) {
        input.setBounds(area.removeFromLeft((area.getWidth()-12)/2));
        area.removeFromLeft(12);
    } else {
        input.setBounds(area.removeFromTop((area.getHeight()-12)/2));
        area.removeFromTop(12);
    }
    output.setBounds(area);
}
}
