// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#include "audio/routing.hpp"
#include "test_support.hpp"
#include <algorithm>
#include <limits>
#include <random>
using namespace qrmforge;
AudioGraph fixture() {
    return {{{1,101,"rtx-hf-receiver","Radio RTP","Stream/Output/Audio"},
             {2,102,"analog-output","Headphones","Audio/Sink"},
             {3,103,"qrmforge_123","DSP","Audio/Filter"}},
            {{10,1,110,true,"MONO"},{20,2,120,false,"FL"},{21,2,121,false,"FR"},
             {30,3,130,false,"MONO"},{31,3,131,true,"MONO"}}, {}};
}
int main() {
    for (const auto text : {"","-1","1x"," 1","18446744073709551616","123456789012345678901"})
        require(!parseNumber(text),"Invalid numeric property rejected");
    require(parseNumber("18446744073709551615") == UINT64_MAX,"Full object serial range");
    require(parseNumber("0") == 0,"Zero parses without becoming a selectable identity");
    auto graph = fixture();
    require(audioNodeLabel(graph.nodes[0]) == "Radio RTP [rtx-hf-receiver]","Display includes exact node name");
    graph.nodes[0].description.clear(); require(audioNodeLabel(graph.nodes[0]) == "rtx-hf-receiver","Name fallback");
    require(!selectableInput(graph.nodes[2]) && !selectableOutput(graph.nodes[2]),"Own node excluded");
    graph.nodes[2].mediaClass = "Audio/Source"; require(!selectableInput(graph.nodes[2]),"Other standalone nodes excluded by identity prefix");
    graph = fixture();
    auto plan = planRoute(graph,{101,102,3});
    require(bool(plan) && plan.connections == std::vector<Connection>{{10,30},{31,20},{31,21}},"RTP has explicit mono input and dual mono output");
    require(!routeConfirmed(graph,plan),"Request alone is not confirmation");
    graph.links = {{50,10,30,true,true},{51,31,20,true,true},{52,31,21,true,true}};
    require(routeConfirmed(graph,plan),"Observed established links confirm route");
    graph.ports[0].serial = 999;
    require(!routeConfirmed(graph,plan),"Recycled global port ID cannot substitute another stream port");
    graph.ports[0].serial = 110;
    require(planRoute(graph,{101,102,3}).connections == plan.connections,"Repeat planning remains stable");
    graph.links[0].established = false; require(!routeConfirmed(graph,plan),"Unestablished link rejected");
    graph.links[0].established = true;
    graph.links.push_back({53,99,30,true,true}); require(!routeConfirmed(graph,plan),"Unexpected input rejected");
    graph.links.back() = {53,31,99,true,true}; require(!routeConfirmed(graph,plan),"Unexpected output rejected");
    graph = fixture(); graph.links = {{50,10,20,true,true},{51,10,21,true,true}};
    plan = planRoute(graph,{101,102,3});
    require(bool(plan) && plan.detach == std::vector<std::uint32_t>{50,51} && plan.restore.size() == 2,"Dry RTP output detached and saved");
    graph.links = {{50,10,30,true,true},{51,31,20,true,true},{52,31,21,true,true}};
    require(routeConfirmed(graph,plan),"Reused global link IDs cannot masquerade as old dry output links");
    graph.links.push_back({60,10,20,true,true});
    require(!routeConfirmed(graph,plan),"Recreated direct output prevents confirmation");
    graph.links = {{50,10,20,true,false},{51,10,21,true,true}};
    graph.links[0].removable = false; require(!planRoute(graph,{101,102,3}),"Dry output permission failure explicit");
    require(restoreConnection(graph,{110,120}) == std::optional<Connection>{{10,20}},"Restoration uses port serials");
    graph.ports[0].serial = 111; require(!restoreConnection(graph,{110,120}),"Recycled numeric port ID cannot restore old endpoint");
    graph = fixture(); graph.ports.push_back(graph.ports[0]); require(!restoreConnection(graph,{110,120}),"Duplicate port identity rejected");
    for (const auto request : {RouteRequest{0,102,3},RouteRequest{101,0,3},RouteRequest{101,102,0},RouteRequest{999,102,3},RouteRequest{101,102,1}})
        require(!planRoute(fixture(),request),"Invalid selection or feedback rejected");
    graph = fixture(); graph.nodes.push_back(graph.nodes[0]); require(!planRoute(graph,{101,102,3}),"Duplicate source serial rejected");
    graph = fixture(); graph.nodes[0].mediaClass = "Video/Source"; require(!planRoute(graph,{101,102,3}),"Non-audio input rejected");
    graph = fixture(); graph.nodes[0].mediaClass = "Audio/Source";
    graph.links = {{50,10,20,true,false}};
    require(planRoute(graph,{101,102,3}).detach.empty(),"Physical source's unrelated routes are preserved");
    graph = fixture(); graph.ports[0].channel = "FR"; graph.ports.push_back({11,1,111,true,"FL"});
    require(planRoute(graph,{101,102,3}).connections.front().output == 11,"Stereo input selects FL");
    graph.ports.back().channel = "OTHER"; require(planRoute(graph,{101,102,3}).connections.front().output == 10,"Unknown channel falls back to first port");
    graph = fixture(); graph.ports[1].channel = "MONO";
    require(planRoute(graph,{101,102,3}).connections.size() == 2,"Mono sink receives one output link");
    graph.ports[1].channel = "OTHER"; graph.ports[2].channel = "OTHER";
    require(planRoute(graph,{101,102,3}).connections.back().input == 20,"Unknown output channel fallback");
    for (std::size_t missing : {0u,1u,2u,3u}) {
        graph = fixture();
        if (missing == 0) graph.ports.erase(graph.ports.begin());
        if (missing == 1) graph.ports.erase(graph.ports.begin()+3);
        if (missing == 2) graph.ports.pop_back();
        if (missing == 3) graph.ports.erase(graph.ports.begin()+1,graph.ports.begin()+3);
        require(!planRoute(graph,{101,102,3}),"Missing input/output ports rejected");
    }
    graph = fixture(); graph.ports.push_back({11,1,111,false,"MONO"}); graph.ports.push_back({22,2,122,true,"MONO"});
    graph.links = {{50,22,11,true,true}};
    require(!planRoute(graph,{101,102,3}),"Indirect feedback path rejected");
    require(!routeConfirmed(graph,{}),"Empty plan is never confirmed");
    std::mt19937 random(907302);
    for (int iteration = 0; iteration < 2000; ++iteration) {
        graph = fixture(); std::shuffle(graph.ports.begin(),graph.ports.end(),random);
        const auto result = planRoute(graph,{101,102,3});
        require(bool(result) && result.connections == std::vector<Connection>{{10,30},{31,20},{31,21}},"Port enumeration order does not affect channel selection");
    }
}
