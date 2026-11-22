// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#pragma once
#include "dsp/core.hpp"
#include "audio/recorder.hpp"
#include <memory>

namespace qrmforge {
class AudioProcessor {
public:
    AudioProcessor();
    ~AudioProcessor();
    AudioProcessor(const AudioProcessor&) = delete;
    AudioProcessor& operator=(const AudioProcessor&) = delete;
    void publish(const Parameters& values) noexcept { requested.publish(values); }
    Parameters parameters() const noexcept;
    Parameters requestedParameters() const noexcept;
    bool readFrame(AudioFrame& frame) const noexcept;
    void process(const float* input, float* output, std::uint32_t count, float rate) noexcept;
    bool service();
    bool prepareNoiseReduction(NoiseReduction mode) noexcept;
    void enable(bool value) noexcept;
    float sampleRate() const noexcept { return configuredRate.load(); }
    // Control-thread queries. Only the control thread replaces the DSP core.
    std::uint32_t availableModes() const noexcept;
    bool deepFilterStarting() const noexcept { return core->deepFilterStarting(); }
    bool neuralAvailable() const noexcept { return modeAvailable(modes.load(),NoiseReduction::Rnnoise); }
    std::uint32_t nrDroppedFrames() const noexcept { return dropped.load(); }
    std::uint32_t frames() const noexcept { return telemetry.written.load(); }
    std::uint32_t invalidBlocks() const noexcept { return invalid.load(); }
    std::uint32_t quantum() const noexcept { return blockSize.load(); }
    Recorder& recorder() noexcept { return recording; }
private:
    std::unique_ptr<Core> core;
    Recorder recording;
    ParameterMailbox requested, effective;
    Parameters current = defaults;
    Telemetry telemetry;
    std::atomic<bool> enabled{false};
    bool deepAttempted = false;
    std::atomic<std::uint32_t> modes{0}, dropped{0};
    std::atomic<std::uint32_t> readers{0}, invalid{0}, blockSize{0};
    std::atomic<float> configuredRate{48000}, desiredRate{48000};
    void suspend() noexcept;
    void updateModes() noexcept;
};
}
