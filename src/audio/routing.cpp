// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#include "audio/routing.hpp"
#include <algorithm>
#include <charconv>
#include <set>

namespace qrmforge {
std::optional<std::uint64_t> parseNumber(std::string_view text) noexcept {
    if (text.empty() || text.size() > 20) return {};
    std::uint64_t result = 0;
    const auto parsed = std::from_chars(text.data(),text.data()+text.size(),result);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data()+text.size()) return {};
    return result;
}
std::string audioNodeLabel(const AudioNode& node) {
    return node.description.empty() || node.description == node.name ? node.name : node.description+" ["+node.name+"]";
}
namespace {
bool standalone(const AudioNode& node) noexcept { return node.name.compare(0,9,"qrmforge_") == 0; }
const AudioPort* portById(const AudioGraph& graph, std::uint32_t id) {
    const auto found = std::find_if(graph.ports.begin(),graph.ports.end(),[id](const auto& port) { return port.id == id; });
    return found == graph.ports.end() ? nullptr : &*found;
}
std::vector<const AudioPort*> portsFor(const AudioGraph& graph, std::uint32_t node, bool output) {
    std::vector<const AudioPort*> ports;
    for (const auto& port : graph.ports)
        if (port.node == node && port.output == output && port.serial) ports.push_back(&port);
    std::sort(ports.begin(),ports.end(),[](const auto* left,const auto* right) {
        const auto rank = [](const auto* port) { return port->channel == "MONO" ? 0 : port->channel == "FL" ? 1 : 2; };
        return rank(left) != rank(right) ? rank(left) < rank(right) : left->id < right->id;
    });
    return ports;
}
bool feedback(const AudioGraph& graph, std::uint32_t source, std::uint32_t sink) {
    std::set<std::uint32_t> visited{sink};
    std::vector<std::uint32_t> queue{sink};
    for (std::size_t index = 0; index < queue.size(); ++index) {
        if (queue[index] == source) return true;
        for (const auto& link : graph.links) {
            const auto* output = portById(graph,link.output);
            const auto* input = portById(graph,link.input);
            if (output && input && output->node == queue[index] && visited.insert(input->node).second) queue.push_back(input->node);
        }
    }
    return false;
}
}
bool selectableInput(const AudioNode& node) noexcept {
    return !standalone(node) && node.serial && (node.mediaClass == "Audio/Source" || node.mediaClass == "Stream/Output/Audio");
}
bool selectableOutput(const AudioNode& node) noexcept { return !standalone(node) && node.serial && node.mediaClass == "Audio/Sink"; }
RoutePlan planRoute(const AudioGraph& graph, RouteRequest request) {
    const AudioNode *source = nullptr, *sink = nullptr;
    unsigned sources = 0, sinks = 0;
    for (const auto& node : graph.nodes) {
        if (node.serial == request.source && selectableInput(node)) { source = &node; ++sources; }
        if (node.serial == request.sink && selectableOutput(node)) { sink = &node; ++sinks; }
    }
    RoutePlan plan;
    if (sources != 1 || sinks != 1 || !request.processor) { plan.error = "Select an available input and output"; return plan; }
    if (source->id == request.processor || sink->id == request.processor || feedback(graph,source->id,sink->id)) {
        plan.error = "Refusing an audio feedback route"; return plan;
    }
    const auto input = portsFor(graph,source->id,true);
    const auto capture = portsFor(graph,request.processor,false);
    const auto playback = portsFor(graph,request.processor,true);
    const auto output = portsFor(graph,sink->id,false);
    if (input.empty() || capture.empty()) { plan.error = "Input audio ports unavailable"; return plan; }
    if (playback.empty() || output.empty()) { plan.error = "Output audio ports unavailable"; return plan; }
    plan.connections.push_back({input.front()->id,capture.front()->id});
    if (output.front()->channel == "MONO") plan.connections.push_back({playback.front()->id,output.front()->id});
    else {
        for (const auto* port : output) if (port->channel == "FL" || port->channel == "FR") plan.connections.push_back({playback.front()->id,port->id});
        if (plan.connections.size() == 1) plan.connections.push_back({playback.front()->id,output.front()->id});
    }
    for (const auto& connection : plan.connections)
        plan.identities.push_back({portById(graph,connection.output)->serial,portById(graph,connection.input)->serial});
    if (source->mediaClass == "Stream/Output/Audio") {
        for (const auto& link : graph.links) {
            const auto* from = portById(graph,link.output);
            const auto* to = portById(graph,link.input);
            if (!from || !to || from->node != source->id || to->node == request.processor) continue;
            const auto destination = std::find_if(graph.nodes.begin(),graph.nodes.end(),[to](const auto& node) {
                return node.id == to->node && node.mediaClass == "Audio/Sink";
            });
            if (destination == graph.nodes.end()) continue;
            if (!link.removable || !from->serial || !to->serial) { plan.error = "Cannot detach the input stream's direct output"; return plan; }
            plan.detach.push_back(link.id); plan.restore.push_back({from->serial,to->serial});
        }
    }
    return plan;
}
bool routeConfirmed(const AudioGraph& graph, const RoutePlan& plan) {
    if (!plan || plan.identities.size() != plan.connections.size()) return false;
    for (std::size_t index = 0; index < plan.connections.size(); ++index) {
        const auto current = restoreConnection(graph,plan.identities[index]);
        if (!current || !(*current == plan.connections[index])) return false;
    }
    for (const auto& expected : plan.connections)
        if (std::none_of(graph.links.begin(),graph.links.end(),[&](const auto& link) {
            return link.established && link.output == expected.output && link.input == expected.input;
        })) return false;
    for (const auto& saved : plan.restore) {
        const auto direct = restoreConnection(graph,saved);
        if (direct && std::any_of(graph.links.begin(),graph.links.end(),[&](const auto& link) {
            return link.output == direct->output && link.input == direct->input;
        })) return false;
    }
    for (const auto& link : graph.links) {
        const bool touchesInput = link.input == plan.connections.front().input;
        const bool touchesOutput = plan.connections.size() > 1 && link.output == plan.connections[1].output;
        if ((touchesInput || touchesOutput) && std::none_of(plan.connections.begin(),plan.connections.end(),[&](const auto& expected) {
            return link.output == expected.output && link.input == expected.input;
        })) return false;
    }
    const auto* sourcePort = portById(graph,plan.connections.front().output);
    if (sourcePort) {
        const auto source = std::find_if(graph.nodes.begin(),graph.nodes.end(),[&](const auto& node) { return node.id == sourcePort->node; });
        if (source != graph.nodes.end() && source->mediaClass == "Stream/Output/Audio") {
            for (const auto& link : graph.links) {
                const auto* output = portById(graph,link.output);
                const auto* input = portById(graph,link.input);
                if (!output || !input || output->node != source->id || input->id == plan.connections.front().input) continue;
                if (std::any_of(graph.nodes.begin(),graph.nodes.end(),[&](const auto& node) { return node.id == input->node && node.mediaClass == "Audio/Sink"; })) return false;
            }
        }
    }
    return true;
}
std::optional<Connection> restoreConnection(const AudioGraph& graph, SavedConnection saved) {
    const AudioPort *output = nullptr, *input = nullptr;
    unsigned outputs = 0, inputs = 0;
    for (const auto& port : graph.ports) {
        if (port.serial == saved.output && port.output) { output = &port; ++outputs; }
        if (port.serial == saved.input && !port.output) { input = &port; ++inputs; }
    }
    if (outputs != 1 || inputs != 1) return {};
    return Connection{output->id,input->id};
}
}
