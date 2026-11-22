// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#include "audio/processor.hpp"
#include "gui/controls.hpp"
#include "test_support.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <random>
#include <thread>
using namespace qrmforge;
int main() {
    ParameterMailbox mailbox;
    Parameters values{};
    require(mailbox.read(values) && values == defaults,"Initial coherent defaults");
    std::atomic<bool> done{false};
    std::thread writer([&] {
        for (int iteration = 0; iteration < 100000; ++iteration) { Parameters update; update.fill(float(iteration)); mailbox.publish(update); }
        done = true;
    });
    Parameters initial; initial.fill(-1); mailbox.read(values);
    while (!done) if (mailbox.read(initial)) {
        if (initial == defaults) continue;
        require(std::all_of(initial.begin(),initial.end(),[&](float value) { return value == initial[0]; }),"Concurrent snapshots never tear");
    }
    writer.join();
    AudioProcessor processor;
    require(processor.prepareNoiseReduction(NoiseReduction::Nlms),"Unframed modes require no model preparation");
    require(!processor.prepareNoiseReduction(NoiseReduction(99)),"Unknown modes cannot be prepared");
#ifndef QRMFORGE_ENABLE_DEEPFILTER
    require(!processor.prepareNoiseReduction(NoiseReduction::DeepFilter),"Missing native backend cannot be prepared");
#endif
    std::array<float,256> input{}, output{};
    input.fill(0.2f); output.fill(1);
    processor.process(input.data(),output.data(),output.size(),48000);
    require(std::all_of(output.begin(),output.end(),[](float value) { return value == 0; }),"Stopped engine is silent");
    require(!processor.service(),"Unchanged sample rate requires no rebuild");
    processor.enable(true);
    processor.process(input.data(),output.data(),output.size(),48000);
    require(input == output && processor.quantum() == 256,"Bypass reproduces input at actual quantum");
    AudioFrame frame;
    require(!processor.readFrame(frame),"Incomplete telemetry frame rejected");
    for (int index = 0; index < 40; ++index) processor.process(input.data(),output.data(),output.size(),48000);
    require(processor.readFrame(frame) && frame.input[0] == 0.2f && frame.output[0] == 0.2f,"Latest telemetry survives ring overwrite");
    values = defaults; values[Mute] = 1; processor.publish(values);
    require(processor.requestedParameters() == values,"Requested settings are retained independently of audio callbacks");
    processor.process(input.data(),output.data(),output.size(),48000);
    require(output[0] == 0 && processor.parameters()[Mute] == 1,"Mute overrides bypass and reports effective controls");
    processor.process(nullptr,output.data(),output.size(),48000);
    require(output[0] == 0 && processor.invalidBlocks() == 1,"Missing input produces silence and diagnostic");
    processor.process(input.data(),nullptr,input.size(),48000);
    processor.process(input.data(),output.data(),0,48000);
    for (float rate : {0.0f,-1.0f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN(),1000000.0f}) {
        output.fill(1); processor.process(input.data(),output.data(),output.size(),rate);
        require(output[0] == 0,"Malformed graph rate is silent");
    }
    output.fill(1); processor.process(input.data(),output.data(),output.size(),44100);
    require(output[0] == 0 && processor.service() && processor.sampleRate() == 44100,"Rate changes suspend processing until control-thread rebuild");
    require(!processor.neuralAvailable(),"Voice backend unavailable outside 48 kHz");
    values = defaults; values[Bypass] = 0; values[NrMode] = 2; processor.publish(values);
    processor.process(input.data(),output.data(),output.size(),44100);
    require(processor.parameters()[NrMode] == 0,"Unsupported neural selection normalizes to OFF");
    processor.enable(false);
    processor.process(input.data(),output.data(),output.size(),32000);
    require(processor.service(),"Stopped processor can prepare a new rate");
    processor.process(input.data(),output.data(),output.size(),32000);
    require(output[0] == 0,"Rate service does not activate a stopped processor");
    processor.enable(true);
    values = defaults; values[Bypass] = 0; values[ApfOn] = 1; values[NrMode] = 2;
    processor.publish(values); processor.process(input.data(),output.data(),output.size(),48000); processor.service();
    processor.process(input.data(),output.data(),output.size(),48000);
    require(processor.parameters()[NrMode] == 0,"CW APF excludes voice inference");
    const auto cw = qrmforge::gui::bandPreset(defaults,5);
    require(cw && (*cw)[ApfOn] == 1 && (*cw)[ApfHz] == 700,"CW preset remains manual");
    require(!qrmforge::gui::bandPreset(defaults,0),"Invalid preset rejected");
    for (int preset = 1; preset <= 4; ++preset) require(bool(qrmforge::gui::bandPreset(defaults,preset)),"All audio presets available");
    processor.enable(false);
    std::atomic<bool> finish{false};
    std::thread audio([&] { while (!finish) processor.process(input.data(),output.data(),output.size(),48000); });
    for (int index = 0; index < 200; ++index) { processor.enable(true); processor.enable(false); }
    finish = true; audio.join();
    processor.process(input.data(),output.data(),output.size(),48000);
    require(output[0] == 0,"Control-thread suspension leaves subsequent callbacks silent");
}
