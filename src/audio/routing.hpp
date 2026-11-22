// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace qrmforge {
std::optional<std::uint64_t> parseNumber(std::string_view text) noexcept;
struct AudioNode {
    std::uint32_t id = 0;
    std::uint64_t serial = 0;
    std::string name, description, mediaClass;
};
std::string audioNodeLabel(const AudioNode& node);
bool selectableInput(const AudioNode& node) noexcept;
bool selectableOutput(const AudioNode& node) noexcept;
struct AudioPort {
    std::uint32_t id = 0, node = 0;
    std::uint64_t serial = 0;
    bool output = false;
    std::string channel;
};
struct AudioLink {
    std::uint32_t id = 0, output = 0, input = 0;
    bool established = false, removable = false;
};
struct AudioGraph {
    std::vector<AudioNode> nodes;
    std::vector<AudioPort> ports;
    std::vector<AudioLink> links;
};
struct Connection {
    std::uint32_t output = 0, input = 0;
    bool operator==(const Connection& other) const noexcept { return output == other.output && input == other.input; }
};
struct SavedConnection { std::uint64_t output = 0, input = 0; };
struct RoutePlan {
    std::vector<Connection> connections;
    std::vector<SavedConnection> identities;
    std::vector<std::uint32_t> detach;
    std::vector<SavedConnection> restore;
    std::string error;
    explicit operator bool() const noexcept { return error.empty() && !connections.empty(); }
};
struct RouteRequest { std::uint64_t source = 0, sink = 0; std::uint32_t processor = 0; };
RoutePlan planRoute(const AudioGraph& graph, RouteRequest request);
bool routeConfirmed(const AudioGraph& graph, const RoutePlan& plan);
std::optional<Connection> restoreConnection(const AudioGraph& graph, SavedConnection saved);
}
