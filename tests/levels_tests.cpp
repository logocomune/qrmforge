// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#include "audio/levels.hpp"
#include "audio/processor.hpp"
#include "test_support.hpp"
#include <random>

using namespace qrmforge;

int main() {
    const auto silence = measureLevels(std::array<float,4>{});
    require(std::isinf(silence.rmsDbfs) && silence.rmsDbfs < 0 &&
            std::isinf(silence.peakDbfs) && silence.peakDbfs < 0,"Silence is negative infinity dBFS");
    const auto empty = measureLevels(std::array<float,0>{});
    require(empty.rmsDbfs == silence.rmsDbfs && empty.peakDbfs == silence.peakDbfs,"Empty windows are silent");
    struct Case { std::array<float,4> samples; double rms; double peak; };
    const Case cases[]{
        {{{0.5f,0.5f,0.5f,0.5f}},-6.020599913,-6.020599913},
        {{{-0.5f,0.5f,-0.5f,0.5f}},-6.020599913,-6.020599913},
        {{{1,0,0,0}},-6.020599913,0},
        {{{2,-2,2,-2}},6.020599913,6.020599913},
        {{{NAN,INFINITY,-INFINITY,1}},-6.020599913,0},
    };
    for (const auto& test : cases) {
        const auto level = measureLevels(test.samples);
        require(std::abs(level.rmsDbfs-test.rms) < 1e-7 && std::abs(level.peakDbfs-test.peak) < 1e-7,
                "Known constant, impulse, over-range and invalid-sample levels");
    }
    for (const float value : {std::numeric_limits<float>::max(),std::numeric_limits<float>::denorm_min()}) {
        const auto level = measureLevels(std::array<float,2>{value,-value});
        require(std::isfinite(level.rmsDbfs) && std::isfinite(level.peakDbfs),"Float extremes remain measurable");
    }
    std::array<float,fftSize> sine{};
    for (std::size_t index = 0; index < sine.size(); ++index)
        sine[index] = float(std::sin(2*3.141592653589793*32*double(index)/double(sine.size())));
    const auto sinusoid = measureLevels(sine);
    require(std::abs(sinusoid.rmsDbfs+3.010299957) < 1e-6 && std::abs(sinusoid.peakDbfs) < 1e-7,
            "Full-scale sine RMS is -3.01 dBFS and sample peak is 0 dBFS");
    std::mt19937 generator(20261116);
    std::uniform_real_distribution<float> amplitude(-2,2);
    for (int trial = 0; trial < 4000; ++trial) {
        std::array<float,64> samples{}, inverted{}, scaled{};
        for (std::size_t index = 0; index < samples.size(); ++index) {
            samples[index] = amplitude(generator);
            inverted[index] = -samples[index];
            scaled[index] = samples[index]*0.25f;
        }
        const auto level = measureLevels(samples);
        const auto opposite = measureLevels(inverted);
        const auto quieter = measureLevels(scaled);
        require(level.rmsDbfs <= level.peakDbfs+1e-10,"RMS never exceeds sample peak");
        require(level.rmsDbfs == opposite.rmsDbfs && level.peakDbfs == opposite.peakDbfs,"Levels ignore polarity");
        require(std::abs(quieter.rmsDbfs-level.rmsDbfs+12.041199827) < 1e-7 &&
                std::abs(quieter.peakDbfs-level.peakDbfs+12.041199827) < 1e-7,"Gain shifts both levels logarithmically");
        std::shuffle(samples.begin(),samples.end(),generator);
        const auto reordered = measureLevels(samples);
        require(std::abs(level.rmsDbfs-reordered.rmsDbfs) < 1e-10 && level.peakDbfs == reordered.peakDbfs,
                "Window levels do not depend on sample order");
    }
    RmsAverager averaging;
    require(std::isinf(averaging.decibels()),"RMS averaging starts silent");
    const double first = averaging.update(0,0.3);
    require(std::abs(first-10*std::log10(1-std::exp(-1.0))) < 1e-10,"RMS uses a 300 ms exponential power average");
    require(first < 0 && first > -3,"RMS attack follows the averaged power rather than jumping");
    const double decay = averaging.update(-INFINITY,0.3);
    require(std::abs(decay-first+10/std::log(10.0)) < 1e-10,"RMS release decays smoothly through silent windows");
    for (const double invalid : {0.0,-1.0,double(INFINITY),double(NAN)})
        require(averaging.update(0,invalid) == decay,"Invalid update intervals preserve RMS state");
    for (const double invalid : {double(NAN),double(INFINITY),10000.0})
        require(averaging.update(invalid,0.1) == decay,"Invalid power values cannot poison averaging");
    averaging.clear();
    require(std::isinf(averaging.decibels()),"Stop clears RMS averaging history");
    for (int trial = 1; trial < 100; ++trial) {
        RmsAverager split, whole;
        const double duration = double(trial)/100;
        for (int update = 0; update < 30; ++update) split.update(-12,duration/30);
        require(std::abs(split.decibels()-whole.update(-12,duration)) < 1e-10,
                "RMS averaging is independent of GUI update frequency");
    }
    AudioProcessor processor;
    processor.enable(true);
    sine.fill(0.25f);
    std::array<float,fftSize> output{};
    processor.process(sine.data(),output.data(),output.size(),48000);
    AudioFrame frame;
    require(processor.readFrame(frame),"Meter reads a complete coherent telemetry window");
    require(measureLevels(frame.input).rmsDbfs == measureLevels(frame.output).rmsDbfs,
            "PRE and POST agree under bypass");
    auto parameters = defaults;
    parameters[Mute] = 1;
    processor.publish(parameters);
    processor.process(sine.data(),output.data(),output.size(),48000);
    require(processor.readFrame(frame),"Muted telemetry remains available");
    require(std::isfinite(measureLevels(frame.input).rmsDbfs) &&
            std::isinf(measureLevels(frame.output).rmsDbfs),"Mute silences POST and preserves PRE metering");
    require(frame.output == output,"POST window is the actual output buffer");
    parameters[Mute] = 0;
    parameters[Bypass] = 0;
    parameters[PreGain] = 6;
    parameters[PostGain] = 6;
    processor.publish(parameters);
    for (std::size_t index = 0; index < sine.size(); ++index)
        sine[index] = 0.1f*float(std::sin(2*3.141592653589793*32*double(index)/double(sine.size())));
    for (int block = 0; block < 32; ++block)
        processor.process(sine.data(),output.data(),output.size(),48000);
    require(processor.readFrame(frame) && frame.input == sine && frame.output == output,
            "Processed meter windows preserve pre-gain input and final output provenance");
    require(measureLevels(frame.output).rmsDbfs > measureLevels(frame.input).rmsDbfs+10,
            "POST reflects output gain while PRE stays at receiver level");
}
