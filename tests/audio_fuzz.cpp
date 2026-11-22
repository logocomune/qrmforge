// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#include "audio/processor.hpp"
#include "audio/levels.hpp"
#include "audio/routing.hpp"
#include "test_support.hpp"
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <random>
#include <string>
using namespace qrmforge;
int main(int argc, char** argv) {
    const auto seconds = argc == 2 ? std::min(parseNumber(argv[1]).value_or(3),std::uint64_t(60)) : 3;
    const auto deadline = std::chrono::steady_clock::now()+std::chrono::seconds(seconds);
    std::mt19937 random(907302);
    RmsAverager rmsAverage;
    std::vector<std::uint32_t> audioSeeds;
    std::ifstream seeds(std::string(QRMFORGE_FUZZ_CORPUS)+"/audio-bits.txt");
    std::uint32_t bits;
    while (seeds >> std::hex >> bits) audioSeeds.push_back(bits);
    require(!audioSeeds.empty(),"Permanent audio seed corpus is available");
    std::ifstream numericSeeds(std::string(QRMFORGE_FUZZ_CORPUS)+"/numeric-properties.txt");
    require(numericSeeds.good(),"Permanent numeric seed corpus is available");
    std::string seed;
    while (std::getline(numericSeeds,seed)) {
        const auto result = parseNumber(seed);
        if (result) require(parseNumber(std::to_string(*result)) == result,"Seeded numeric parser round-trip");
    }
    AudioProcessor processor; processor.enable(true);
    std::array<float,1024> input{}, output{};
    std::uint64_t cases = 0;
    do {
        std::string property(random()%40,' ');
        for (auto& character : property) character = char(random()%256);
        const auto parsed = parseNumber(property);
        if (parsed) require(parseNumber(std::to_string(*parsed)) == parsed,"Numeric parser round-trip");
        Parameters parameters;
        for (auto& parameter : parameters) { const std::uint32_t bits = random(); std::memcpy(&parameter,&bits,sizeof(bits)); }
        if (cases%2 == 0) parameters[NrMode] = float((cases/2)%noiseReductionCount);
        if (cases%3 == 0) {
            parameters[AgcOn] = 1; parameters[Bypass] = parameters[Mute] = 0;
        }
        const float rates[]{8000,16000,32000,44100,48000,96000};
        const auto rate = rates[random()%6];
        const auto normalized = sanitize(parameters,rate);
        require(sanitize(normalized,rate) == normalized,"Sanitization is idempotent");
        processor.publish(parameters);
        for (auto& sample : input) { const std::uint32_t bits = random(); std::memcpy(&sample,&bits,sizeof(bits)); }
        for (std::size_t index = 0; index < audioSeeds.size(); ++index) std::memcpy(&input[index],&audioSeeds[index],sizeof(float));
        const auto levels = measureLevels(input);
        require(!std::isnan(levels.rmsDbfs) && !std::isnan(levels.peakDbfs),"Arbitrary float bits never produce NaN levels");
        require(levels.rmsDbfs <= levels.peakDbfs+1e-9,"Arbitrary windows preserve RMS/peak ordering");
        const auto displayedRms = rmsAverage.update(levels.rmsDbfs,double(random()%200+1)/1000);
        require(!std::isnan(displayedRms) && displayedRms != std::numeric_limits<double>::infinity(),
                "RMS averaging remains valid through arbitrary audio windows");
        const auto count = random()%1025;
        processor.process(input.data(),output.data(),count,rate);
        processor.service();
        processor.process(input.data(),output.data(),count,rate);
        for (std::size_t index = 0; index < count; ++index) {
            if (normalized[Bypass] >= 0.5f && normalized[Mute] < 0.5f)
                require(std::memcmp(&input[index],&output[index],sizeof(float)) == 0,"Bypass preserves arbitrary input bits");
            else {
                require(std::isfinite(output[index]),"Processed arbitrary audio produces finite output");
                if (normalized[AgcOn] >= 0.5f)
                    require(std::abs(output[index]) <= 0.950001f,"AGC keeps arbitrary audio below the hard limiter");
            }
        }
        AudioGraph graph;
        for (std::uint32_t index = 1; index < 7; ++index) {
            graph.nodes.push_back({index,random()%10,property,property,index%2 ? "Stream/Output/Audio" : "Audio/Sink"});
            graph.ports.push_back({index+20,index,random()%10,bool(index%2),property});
            graph.links.push_back({index+40,std::uint32_t(random()%30),std::uint32_t(random()%30),bool(random()%2),bool(random()%2)});
        }
        const auto plan = planRoute(graph,{random()%10,random()%10,3});
        if (plan) {
            require(!plan.connections.empty(),"Accepted route has explicit ports");
            for (const auto& connection : plan.connections) require(connection.input != connection.output,"A port cannot link to itself");
        }
        AudioFrame frame; processor.readFrame(frame);
        ++cases;
    } while (std::chrono::steady_clock::now() < deadline);
    std::cout << "Audio/routing fuzz cases: " << cases << '\n';
}
