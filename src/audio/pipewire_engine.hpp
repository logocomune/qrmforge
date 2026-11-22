// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#pragma once
#include "audio/processor.hpp"
#include "audio/routing.hpp"
#include <memory>

namespace qrmforge {
enum class AudioState { Disconnected, Ready, Starting, Running, Error };
struct EngineSnapshot {
    AudioGraph graph;
    AudioState state = AudioState::Disconnected;
    std::string message;
    std::string actualInput = "Not connected", actualOutput = "Not connected";
};
class PipeWireEngine {
public:
    PipeWireEngine();
    ~PipeWireEngine();
    PipeWireEngine(const PipeWireEngine&) = delete;
    PipeWireEngine& operator=(const PipeWireEngine&) = delete;
    EngineSnapshot snapshot();
    bool apply(std::uint64_t source, std::uint64_t sink);
    void stop();
    void reconnect();
    AudioProcessor& processor() noexcept;
private:
    struct Implementation;
    std::unique_ptr<Implementation> implementation;
};
}
