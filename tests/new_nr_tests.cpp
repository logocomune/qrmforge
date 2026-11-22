// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#include "dsp/core.hpp"
#include "dsp/nr_modes.hpp"
#include "dsp/wiener.hpp"
#include "dsp/speex.hpp"
#include "dsp/deep_filter.hpp"
#include "test_support.hpp"
#include <cmath>
#include <limits>
#include <random>
#include <vector>
using namespace qrmforge;
int main() {
    for (unsigned mode = 0; mode < noiseReductionCount; ++mode) {
        auto values = defaults; values[NrMode] = float(mode);
        require(sanitize(values,48000)[NrMode] == mode,"New NR IDs survive sanitization without changing existing IDs");
    }
    require(unsigned(NoiseReduction::Nlms) == 1 && unsigned(NoiseReduction::Rnnoise) == 2,"Previous noise reduction IDs remain stable");
    require(voiceOnly(NoiseReduction::Speex) && voiceOnly(NoiseReduction::DeepFilter) && !voiceOnly(NoiseReduction::Wiener),"Voice restrictions distinguish spectral NR");
    require(!modeAvailable(0,NoiseReduction::DeepFilter) && !modeAvailable(UINT32_MAX,NoiseReduction(99)),"Absent and invalid backends are not selectable");
    Core dispatch(48000);
    for (unsigned mode = 0; mode < noiseReductionCount; ++mode) {
        auto values = defaults; values[NrMode] = float(mode); values[ApfOn] = 1;
        const auto effective = dispatch.effectiveParameters(values);
        require(effective[NrMode] == (voiceOnly(NoiseReduction(mode)) ? 0 : mode),"CW blocks voice-only modes while retaining OFF, NLMS and Wiener");
        std::array<float,4> input{-2,-0.0f,0.25f,4}, output{};
        values[ApfOn] = 0;
        dispatch.process(input.data(),output.data(),input.size(),values);
        require(input == output,"Every NR selection preserves full bypass");
        values[Mute] = 1;
        dispatch.process(input.data(),output.data(),input.size(),values);
        for (auto sample : output) require(sample == 0,"Mute overrides bypass for every NR selection");
    }
    Wiener unity, filtered;
    std::mt19937 random(907302);
    std::uniform_real_distribution<float> noise(-0.2f,0.2f);
    std::vector<float> signal(48000);
    for (auto& value : signal) value = noise(random);
    double original = 0, reduced = 0;
    for (std::size_t index = 0; index < signal.size(); ++index) {
        const auto copied = unity.process(signal[index],0);
        const auto denoised = filtered.process(signal[index],10);
        require(std::isfinite(copied) && std::isfinite(denoised),"Spectral processing remains finite");
        if (index >= Wiener::size) require(std::abs(copied-signal[index-Wiener::size]) < 0.00001f,"Overlap-add reconstructs unity gain with exact declared delay");
        if (index > 24000) { original += signal[index-Wiener::size]*signal[index-Wiener::size]; reduced += denoised*denoised; }
    }
    require(reduced < original*0.85 && reduced > 0,"Wiener reduces stationary noise without replacing output with silence");
    filtered.clear();
    for (std::size_t index = 0; index < Wiener::size*2; ++index) require(filtered.process(0,10) == 0,"Clearing spectral state removes old audio");
    for (const float value : {std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()})
        for (int index = 0; index < 1024; ++index) require(std::isfinite(filtered.process(value,value)),"Nonfinite spectral samples and strength are bounded");
    for (float rate : {8000.0f,16000.0f,44100.0f,48000.0f}) {
        SpeexDenoiser speech(rate);
#ifdef QRMFORGE_ENABLE_SPEEXDSP
        require(speech.available() && speech.frameSize() == std::size_t(rate)/50,"Real SpeexDSP initializes its 20 ms frame");
        double energy = 0;
        for (int index = 0; index < int(rate); ++index) {
            const auto output = speech.process(0.1f*std::sin(6.28318530718f*1000*index/rate),4.5f);
            require(std::isfinite(output) && std::abs(output) <= 1,"Speex output remains bounded");
            if (index < int(speech.frameSize())) require(output == 0,"Speex framing starts with silence");
            energy += output*output;
        }
        require(energy > 0,"Real Speex backend processes nonzero audio");
        speech.clear(); require(speech.process(0,4.5f) == 0,"Speex clear removes buffered output");
#else
        require(!speech.available() && speech.process(1,4.5f) == 0,"Disabled Speex is explicitly unavailable");
#endif
    }
    for (float rate : {0.0f,1000.0f,8000.5f,96000.0f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        SpeexDenoiser speech(rate); require(!speech.available(),"Unsupported Speex sample rate refused");
    }
    DeepFilter unsupported(44100);
    require(!unsupported.available() && unsupported.process(1,4.5f) == 0,"DeepFilter is unavailable outside 48 kHz");
    unsupported.clear(); require(unsupported.droppedFrames() == 0,"Unavailable DeepFilter has no worker backlog");
    for (unsigned mode : {3u,4u}) {
        Core contiguous(48000), segmented(48000);
        if (!modeAvailable(contiguous.availableModes(),NoiseReduction(mode))) continue;
        auto values = defaults; values[Bypass] = 0; values[NrMode] = float(mode);
        std::vector<float> expected(signal.size()), actual(signal.size());
        contiguous.process(signal.data(),expected.data(),signal.size(),values);
        std::size_t position = 0;
        while (position < signal.size()) {
            const auto length = std::min(std::size_t(random()%1024+1),signal.size()-position);
            segmented.process(signal.data()+position,actual.data()+position,length,values);
            position += length;
        }
        require(actual == expected,"NR frame adaptation is independent of PipeWire block boundaries");
        values[NrMode] = 0;
        segmented.process(signal.data(),actual.data(),17,values);
        values[NrMode] = float(mode);
        std::array<float,512> silence{};
        segmented.process(silence.data(),actual.data(),silence.size(),values);
        for (std::size_t index = 0; index < std::min(std::size_t(512),mode == 3 ? Wiener::hop : std::size_t(960)); ++index)
            require(actual[index] == 0,"Re-entering a framed mode does not replay its old output buffer");
    }
}
