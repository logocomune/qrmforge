// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#include "audio/pipewire_engine.hpp"
#include "test_support.hpp"
#include <pipewire/pipewire.h>
#include <pipewire/filter.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>
using namespace qrmforge;
namespace {
template<typename Predicate> void waitFor(Predicate predicate, const char* message) {
    const auto end = std::chrono::steady_clock::now()+std::chrono::seconds(8);
    while (!predicate() && std::chrono::steady_clock::now() < end) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    require(predicate(),message);
}
class AudioFixture {
public:
    pw_thread_loop* loop = nullptr;
    pw_filter *source = nullptr, *sink = nullptr;
    void *sourcePort = nullptr, *sinkPort = nullptr;
    std::atomic<float> peak{0};
    std::atomic<std::uint32_t> blocks{0};
    pw_proxy* direct = nullptr;
    spa_hook directListener{};
    AudioFixture() {
        pw_init(nullptr,nullptr);
        loop = pw_thread_loop_new("standalone-audio-test",nullptr);
        require(loop,"Create fixture loop");
        static const pw_filter_events sourceEvents = [] {
            pw_filter_events events{}; events.version = PW_VERSION_FILTER_EVENTS;
            events.process = [](void* data, spa_io_position* position) {
                auto& self = *static_cast<AudioFixture*>(data);
                auto* output = static_cast<float*>(pw_filter_get_dsp_buffer(self.sourcePort,position->clock.duration));
                if (output) std::fill_n(output,position->clock.duration,0.25f);
            }; return events;
        }();
        static const pw_filter_events sinkEvents = [] {
            pw_filter_events events{}; events.version = PW_VERSION_FILTER_EVENTS;
            events.process = [](void* data, spa_io_position* position) {
                auto& self = *static_cast<AudioFixture*>(data);
                auto* input = static_cast<float*>(pw_filter_get_dsp_buffer(self.sinkPort,position->clock.duration));
                if (!input) return;
                float peak = 0;
                for (std::uint32_t index = 0; index < position->clock.duration; ++index) peak = std::max(peak,std::abs(input[index]));
                self.peak = peak; ++self.blocks;
            }; return events;
        }();
        source = pw_filter_new_simple(pw_thread_loop_get_loop(loop),"test-network-receiver",
            pw_properties_new(PW_KEY_NODE_NAME,"test-network-receiver",PW_KEY_MEDIA_CLASS,"Stream/Output/Audio",
                PW_KEY_MEDIA_TYPE,"Audio",PW_KEY_MEDIA_CATEGORY,"Playback",PW_KEY_NODE_AUTOCONNECT,"false",
                "node.always-process","true",nullptr),&sourceEvents,this);
        sink = pw_filter_new_simple(pw_thread_loop_get_loop(loop),"test-output",
            pw_properties_new(PW_KEY_NODE_NAME,"test-output",PW_KEY_MEDIA_CLASS,"Audio/Sink",
                PW_KEY_MEDIA_TYPE,"Audio",PW_KEY_MEDIA_CATEGORY,"Capture",PW_KEY_NODE_AUTOCONNECT,"false",
                "node.always-process","true",nullptr),&sinkEvents,this);
        require(source && sink,"Create isolated source and sink filters");
        sourcePort = pw_filter_add_port(source,PW_DIRECTION_OUTPUT,PW_FILTER_PORT_FLAG_MAP_BUFFERS,1,
            pw_properties_new(PW_KEY_PORT_NAME,"receive_MONO",PW_KEY_AUDIO_CHANNEL,"MONO",PW_KEY_FORMAT_DSP,"32 bit float mono audio",nullptr),nullptr,0);
        sinkPort = pw_filter_add_port(sink,PW_DIRECTION_INPUT,PW_FILTER_PORT_FLAG_MAP_BUFFERS,1,
            pw_properties_new(PW_KEY_PORT_NAME,"playback_MONO",PW_KEY_AUDIO_CHANNEL,"MONO",PW_KEY_FORMAT_DSP,"32 bit float mono audio",nullptr),nullptr,0);
        require(sourcePort && sinkPort,"Create DSP fixture ports");
        require(pw_filter_connect(source,PW_FILTER_FLAG_RT_PROCESS,nullptr,0) >= 0 &&
                pw_filter_connect(sink,PW_FILTER_FLAG_RT_PROCESS,nullptr,0) >= 0,"Connect fixture filters");
        require(pw_thread_loop_start(loop) >= 0,"Start fixture loop");
    }
    ~AudioFixture() {
        pw_thread_loop_stop(loop);
        if (direct) { spa_hook_remove(&directListener); pw_proxy_destroy(direct); }
        if (source) pw_filter_destroy(source);
        pw_filter_destroy(sink); pw_thread_loop_destroy(loop);
    }
    void removeSource() {
        pw_thread_loop_lock(loop); pw_filter_destroy(source); source = nullptr; pw_thread_loop_unlock(loop);
    }
    void connectDirect(Connection connection) {
        pw_thread_loop_lock(loop);
        auto* core = pw_filter_get_core(source);
        const auto outputNode = std::to_string(pw_filter_get_node_id(source)), inputNode = std::to_string(pw_filter_get_node_id(sink));
        const auto from = std::to_string(connection.output), to = std::to_string(connection.input);
        auto* properties = pw_properties_new(PW_KEY_LINK_OUTPUT_NODE,outputNode.c_str(),PW_KEY_LINK_OUTPUT_PORT,from.c_str(),
            PW_KEY_LINK_INPUT_NODE,inputNode.c_str(),PW_KEY_LINK_INPUT_PORT,to.c_str(),PW_KEY_OBJECT_LINGER,"false",nullptr);
        direct = static_cast<pw_proxy*>(pw_core_create_object(core,"link-factory",PW_TYPE_INTERFACE_Link,PW_VERSION_LINK,&properties->dict,0));
        pw_properties_free(properties);
        require(direct,"Create initial direct receiver output");
        static const pw_proxy_events events = [] {
            pw_proxy_events result{}; result.version = PW_VERSION_PROXY_EVENTS;
            result.removed = [](void* data) {
                auto& self = *static_cast<AudioFixture*>(data);
                spa_hook_remove(&self.directListener); pw_proxy_destroy(self.direct); self.direct = nullptr;
            }; return result;
        }();
        pw_proxy_add_listener(direct,&directListener,&events,this);
        pw_thread_loop_unlock(loop);
    }
};
}
int main() {
    AudioFixture fixture;
    PipeWireEngine engine;
    std::uint64_t source = 0, sink = 0;
    waitFor([&] {
        const auto current = engine.snapshot();
        for (const auto& node : current.graph.nodes) {
            if (node.name == "test-network-receiver") source = node.serial;
            if (node.name == "test-output") sink = node.serial;
        }
        return source && sink && current.graph.ports.size() >= 4 && current.state == AudioState::Ready;
    },"Discover source, sink and standalone ports on real PipeWire");
    Connection direct;
    const auto initial = engine.snapshot().graph;
    for (const auto& port : initial.ports) {
        for (const auto& node : initial.nodes) {
            if (node.id != port.node) continue;
            if (node.serial == source && port.output) direct.output = port.id;
            if (node.serial == sink && !port.output) direct.input = port.id;
        }
    }
    fixture.connectDirect(direct);
    waitFor([&] { return engine.snapshot().graph.links.size() == 1; },"Observe initial direct receiver output");
    require(engine.apply(source,sink),"Apply explicit network route");
    waitFor([&] { return engine.snapshot().state == AudioState::Running; },"Confirm established network links");
    waitFor([&] { engine.snapshot(); return std::abs(fixture.peak.load()-0.25f) < 0.00001f; },"Bypass audio reaches the selected sink");
    const auto links = engine.snapshot().graph.links.size();
    require(engine.apply(source,sink),"Repeat Apply succeeds");
    require(engine.snapshot().graph.links.size() == links,"Repeat Apply creates no duplicate links");
    auto values = defaults; values[Mute] = 1; engine.processor().publish(values);
    waitFor([&] { return engine.processor().parameters()[Mute] == 1 && fixture.peak.load() == 0; },"GUI-style parameter update mutes real output");
    AudioFrame frame;
    waitFor([&] { return engine.processor().readFrame(frame) && frame.input[0] == 0.25f && frame.output[0] == 0; },"Raw pre-DSP and muted post-DSP telemetry");
    engine.stop();
    require(engine.snapshot().state == AudioState::Ready,"Stop releases routing and returns Ready");
    waitFor([&] {
        const auto current = engine.snapshot().graph;
        return current.links.size() == 1 && current.links[0].output == direct.output && current.links[0].input == direct.input && current.links[0].established;
    },"Stop removes application links and restores the original direct output");
    engine.processor().publish(defaults);
    require(engine.apply(source,sink),"Restart audio");
    waitFor([&] { return engine.snapshot().state == AudioState::Running; },"Restart confirms real links");
    fixture.removeSource();
    waitFor([&] { return engine.snapshot().state == AudioState::Error; },"Hot-unplug stops processing with an error");
    engine.stop(); engine.reconnect();
    waitFor([&] { return engine.snapshot().state == AudioState::Ready; },"Explicit reconnect reopens PipeWire");
}
