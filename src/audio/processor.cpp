// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#include "audio/processor.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

namespace qrmforge {
AudioProcessor::AudioProcessor() : core(std::make_unique<Core>(48000)) {
    updateModes();
}
AudioProcessor::~AudioProcessor() { recording.stop(); suspend(); }
void AudioProcessor::suspend() noexcept {
    enabled.store(false);
    // Control-thread only: callbacks never wait for a GUI update or DSP replacement.
    while (readers.load() != 0) std::this_thread::yield();
}
void AudioProcessor::enable(bool value) noexcept {
    if (value) enabled.store(true);
    else { recording.stop(); suspend(); }
}
void AudioProcessor::updateModes() noexcept {
    modes.store(core->availableModes() & ~modeBit(NoiseReduction::DeepFilter));
}
std::uint32_t AudioProcessor::availableModes() const noexcept {
    auto available = modes.load();
    if (deepAttempted ? core->deepFilterSelectable() : DeepFilter::supported(configuredRate.load()))
        available |= modeBit(NoiseReduction::DeepFilter);
    return available;
}
bool AudioProcessor::prepareNoiseReduction(NoiseReduction mode) noexcept {
    if (mode != NoiseReduction::DeepFilter) return modeAvailable(availableModes(),mode);
    if (deepAttempted) return modeAvailable(availableModes(),mode);
    const bool running = enabled.load(); suspend(); deepAttempted = true;
    const bool accepted = core->prepareDeepFilter(); updateModes();
    if (running) enabled = true;
    return accepted;
}
Parameters AudioProcessor::parameters() const noexcept {
    Parameters result = requestedParameters();
    effective.read(result);
    return result;
}
Parameters AudioProcessor::requestedParameters() const noexcept {
    Parameters result = defaults;
    requested.read(result);
    return result;
}
bool AudioProcessor::readFrame(AudioFrame& frame) const noexcept {
    const auto end = telemetry.written.load(std::memory_order_acquire);
    if (end < fftSize) return false;
    frame.sampleRate = sampleRate(); frame.written = end;
    for (std::uint32_t index = 0; index < fftSize; ++index)
        if (!readSample(telemetry,end-fftSize+index,frame.input[index],frame.output[index])) return false;
    return true;
}
void AudioProcessor::process(const float* input, float* output, std::uint32_t count, float rate) noexcept {
    if (!output || count == 0) return;
    blockSize.store(count);
    readers.fetch_add(1);
    if (!enabled.load() || !input || !std::isfinite(rate) || rate < 1000 || rate > 384000 || rate != configuredRate.load()) {
        if (std::isfinite(rate) && rate >= 1000 && rate <= 384000) desiredRate.store(rate);
        if (!input || !std::isfinite(rate) || rate < 1000 || rate > 384000) invalid.fetch_add(1);
        std::fill_n(output,count,0.0f);
    } else {
        requested.read(current);
        const auto values = core->effectiveParameters(current);
        effective.publish(values);
        core->process(input,output,count,values,&telemetry);
        updateModes(); dropped.store(core->droppedFrames());
    }
    recording.push({output,count,rate});
    readers.fetch_sub(1);
}
bool AudioProcessor::service() {
    const auto rate = desiredRate.load();
    if (rate == configuredRate.load()) return false;
    const bool running = enabled.load();
    suspend();
    core = std::make_unique<Core>(rate);
    configuredRate.store(rate); deepAttempted = false;
    if (requestedParameters()[NrMode] == 5 && DeepFilter::supported(rate)) {
        deepAttempted = true; core->prepareDeepFilter();
    }
    updateModes();
    if (running) enabled.store(true);
    return true;
}
}
