// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#include <cstdlib>
#include "gui/main_component.hpp"
#include "gui/look_and_feel.hpp"
#include "qrmforge_assets.hpp"
#if QRMFORGE_WIDGET_TESTS
#include "tests/gui_widget_tests.hpp"
#endif
namespace {
juce::Image applicationIcon() {
    return juce::ImageCache::getFromMemory(QRMForgeAssets::qrmforge_png,QRMForgeAssets::qrmforge_pngSize);
}
class Application final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "QRMForge"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }
    void initialise(const juce::String& commandLine) override {
        look.setColour(juce::ResizableWindow::backgroundColourId, qrmforge::gui::theme::background);
        look.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff56c9e8));
        juce::LookAndFeel::setDefaultLookAndFeel(&look);
#if QRMFORGE_WIDGET_TESTS
        if (commandLine == "--test-widgets") {
            setApplicationReturnValue(std::getenv("DISPLAY") ? (qrmforge::gui::testNotchMarker() && qrmforge::gui::testBandwidthKnob() && qrmforge::gui::testStandaloneControls() ? 0 : 1) : 77);
            quit(); return;
        }
        if (commandLine == "--test-window-icon") {
            const auto icon = applicationIcon();
            setApplicationReturnValue(icon.getWidth() == 256 && icon.getHeight() == 256 ? 0 : 1);
            quit(); return;
        }
#endif
        if (commandLine.startsWith("--render=")) {
            qrmforge::gui::MainComponent component;
            auto image = component.createComponentSnapshot(component.getLocalBounds());
            juce::FileOutputStream stream(juce::File(commandLine.substring(9).unquoted()));
            juce::PNGImageFormat format;
            setApplicationReturnValue(stream.openedOk() && stream.setPosition(0) && stream.truncate().wasOk() && format.writeImageToStream(image, stream) ? 0 : 1);
            quit();
        } else window = std::make_unique<Window>();
    }
    void shutdown() override { window.reset(); juce::LookAndFeel::setDefaultLookAndFeel(nullptr); }
private:
    class Window final : public juce::DocumentWindow {
    public:
        Window() : DocumentWindow("QRMForge", juce::Colour(0xff101620), allButtons) {
            setUsingNativeTitleBar(true);
            setContentOwned(new qrmforge::gui::MainComponent(), true);
            setResizable(true, false);
            setResizeLimits(920, 900, 1800, 1400);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
            const auto icon = applicationIcon();
            setIcon(icon);
            if (auto* peer = getPeer()) peer->setIcon(icon);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };
    qrmforge::gui::StudioLookAndFeel look;
    std::unique_ptr<Window> window;
};
}
START_JUCE_APPLICATION(Application)
