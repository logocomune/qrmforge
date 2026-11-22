// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#include "audio/pipewire_engine.hpp"
#include <pipewire/pipewire.h>
#include <pipewire/filter.h>
#include <pipewire/permission.h>
#include <spa/param/audio/format-utils.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <map>
#include <mutex>
#include <unistd.h>

namespace qrmforge {
namespace {
std::string text(const char* value) { return value ? std::string(value,strnlen(value,512)) : std::string{}; }
std::uint64_t number(const spa_dict* properties, const char* key) {
    return properties ? parseNumber(text(spa_dict_lookup(properties,key))).value_or(0) : 0;
}
std::uint32_t identifier(const spa_dict* properties, const char* key) {
    const auto value = number(properties,key);
    return value < PW_ID_ANY ? std::uint32_t(value) : PW_ID_ANY;
}
}
struct PipeWireEngine::Implementation {
    AudioProcessor audio;
    enum class Kind { Node, Port, Link };
    struct Binding {
        Kind kind = Kind::Node;
        std::uint32_t id = 0;
        void* proxy = nullptr;
        pw_properties* properties = nullptr;
        spa_hook listener{};
        AudioNode node;
        AudioPort port;
        AudioLink link;
        ~Binding() {
            if (proxy) { spa_hook_remove(&listener); pw_proxy_destroy(static_cast<pw_proxy*>(proxy)); }
            if (properties) pw_properties_free(properties);
        }
        void update() {
            const auto* props = &properties->dict;
            if (kind == Kind::Node) {
                node = {id,number(props,PW_KEY_OBJECT_SERIAL),text(spa_dict_lookup(props,PW_KEY_NODE_NAME)),
                    text(spa_dict_lookup(props,PW_KEY_NODE_DESCRIPTION)),text(spa_dict_lookup(props,PW_KEY_MEDIA_CLASS))};
            } else if (kind == Kind::Port) {
                port.id = id; port.serial = number(props,PW_KEY_OBJECT_SERIAL);
                port.node = identifier(props,PW_KEY_NODE_ID);
                port.channel = text(spa_dict_lookup(props,PW_KEY_AUDIO_CHANNEL));
            }
        }
    };
    struct OwnedLink {
        pw_proxy* proxy = nullptr;
        spa_hook listener{};
        ~OwnedLink() {
            if (proxy) { spa_hook_remove(&listener); pw_proxy_destroy(proxy); }
        }
    };
    pw_thread_loop* loop = nullptr;
    pw_context* context = nullptr;
    pw_core* core = nullptr;
    pw_registry* registry = nullptr;
    pw_filter* filter = nullptr;
    void *input = nullptr, *output = nullptr;
    spa_hook coreListener{}, registryListener{}, filterListener{};
    std::map<std::uint32_t,std::unique_ptr<Binding>> bindings;
    std::vector<std::unique_ptr<OwnedLink>> links, restoredLinks;
    std::vector<SavedConnection> saved;
    AudioState state = AudioState::Disconnected;
    std::string message = "Connecting to PipeWire";
    RoutePlan plan;
    std::uint64_t source = 0, sink = 0;
    int initialSequence = -1, applySequence = -1, waitSequence = -1;
    bool connected = false, acknowledged = false, waited = false, startedLoop = false, failed = false;
    std::chrono::steady_clock::time_point started;

    Implementation() {
        static std::once_flag initialized;
        std::call_once(initialized, [] { pw_init(nullptr,nullptr); });
        loop = pw_thread_loop_new("qrmforge",nullptr);
        if (!loop) { message = "Cannot create PipeWire loop"; return; }
        context = pw_context_new(pw_thread_loop_get_loop(loop),nullptr,0);
        if (!context) { message = "Cannot create PipeWire context"; return; }
        core = pw_context_connect(context,pw_properties_new(PW_KEY_APP_NAME,"QRMForge",nullptr),0);
        if (!core) { message = "PipeWire unavailable: check the desktop user session"; return; }
        static const pw_core_events coreEvents = [] {
            pw_core_events events{}; events.version = PW_VERSION_CORE_EVENTS;
            events.done = [](void* data, std::uint32_t id, int sequence) {
                auto& self = *static_cast<Implementation*>(data);
                if (id != PW_ID_CORE) return;
                if (sequence == self.initialSequence) { self.connected = true; self.state = AudioState::Ready; self.message = "Ready: choose input and output"; }
                if (sequence == self.applySequence) self.acknowledged = true;
                if (sequence == self.waitSequence) { self.waited = true; pw_thread_loop_signal(self.loop,false); }
            };
            events.error = [](void* data, std::uint32_t id, int, int, const char* error) {
                auto& self = *static_cast<Implementation*>(data);
                self.message = "PipeWire: "+text(error); self.failed = true;
                if (id == PW_ID_CORE) { self.connected = false; self.state = AudioState::Disconnected; }
                pw_thread_loop_signal(self.loop,false);
            }; return events;
        }();
        pw_core_add_listener(core,&coreListener,&coreEvents,this);
        registry = pw_core_get_registry(core,PW_VERSION_REGISTRY,0);
        if (!registry) { message = "Cannot read PipeWire registry"; return; }
        static const pw_registry_events registryEvents = [] {
            pw_registry_events events{}; events.version = PW_VERSION_REGISTRY_EVENTS;
            events.global = [](void* data, std::uint32_t id, std::uint32_t permissions, const char* type,
                                std::uint32_t version, const spa_dict* properties) {
                static_cast<Implementation*>(data)->add(id,permissions,type,version,properties);
            };
            events.global_remove = [](void* data, std::uint32_t id) { static_cast<Implementation*>(data)->bindings.erase(id); };
            return events;
        }();
        pw_registry_add_listener(registry,&registryListener,&registryEvents,this);
        const auto name = "qrmforge_"+std::to_string(getpid());
        filter = pw_filter_new(core,"QRMForge",pw_properties_new(
            PW_KEY_NODE_NAME,name.c_str(),PW_KEY_NODE_DESCRIPTION,"QRMForge",
            PW_KEY_MEDIA_TYPE,"Audio",PW_KEY_MEDIA_CATEGORY,"Filter",PW_KEY_MEDIA_ROLE,"DSP",
            PW_KEY_MEDIA_CLASS,"Audio/Filter",PW_KEY_NODE_AUTOCONNECT,"false",
            "node.want-driver","true",nullptr));
        if (!filter) { message = "Cannot create DSP audio node"; return; }
        static const pw_filter_events filterEvents = [] {
            pw_filter_events events{}; events.version = PW_VERSION_FILTER_EVENTS;
            events.state_changed = [](void* data, pw_filter_state, pw_filter_state state, const char* error) {
                auto& self = *static_cast<Implementation*>(data);
                if (state == PW_FILTER_STATE_ERROR) { self.failed = true; self.message = "Audio filter: "+text(error); }
            };
            events.process = [](void* data, spa_io_position* position) {
                auto& self = *static_cast<Implementation*>(data);
                if (!position || position->clock.duration == 0 || position->clock.duration > 1048576) return;
                const auto count = std::uint32_t(position->clock.duration);
                const auto rate = position->clock.rate.num ? float(position->clock.rate.denom)/float(position->clock.rate.num) : 0;
                auto* input = static_cast<float*>(pw_filter_get_dsp_buffer(self.input,count));
                auto* output = static_cast<float*>(pw_filter_get_dsp_buffer(self.output,count));
                self.audio.process(input,output,count,rate);
            }; return events;
        }();
        pw_filter_add_listener(filter,&filterListener,&filterEvents,this);
        input = pw_filter_add_port(filter,PW_DIRECTION_INPUT,PW_FILTER_PORT_FLAG_MAP_BUFFERS,1,
            pw_properties_new(PW_KEY_FORMAT_DSP,"32 bit float mono audio",PW_KEY_PORT_NAME,"input_MONO",PW_KEY_AUDIO_CHANNEL,"MONO",nullptr),nullptr,0);
        output = pw_filter_add_port(filter,PW_DIRECTION_OUTPUT,PW_FILTER_PORT_FLAG_MAP_BUFFERS,1,
            pw_properties_new(PW_KEY_FORMAT_DSP,"32 bit float mono audio",PW_KEY_PORT_NAME,"output_MONO",PW_KEY_AUDIO_CHANNEL,"MONO",nullptr),nullptr,0);
        if (!input || !output || pw_filter_connect(filter,PW_FILTER_FLAG_RT_PROCESS,nullptr,0) < 0) {
            message = "Cannot connect the DSP audio ports"; return;
        }
        initialSequence = pw_core_sync(core,PW_ID_CORE,0);
        if (initialSequence < 0 || pw_thread_loop_start(loop) < 0) { message = "Cannot start PipeWire connection"; return; }
        startedLoop = true;
    }
    ~Implementation() {
        if (loop && startedLoop) {
            pw_thread_loop_lock(loop);
            stopRoutes();
            if (filter) { spa_hook_remove(&filterListener); pw_filter_destroy(filter); filter = nullptr; }
            pw_thread_loop_unlock(loop);
            pw_thread_loop_stop(loop);
        }
        audio.enable(false);
        if (filter) { spa_hook_remove(&filterListener); pw_filter_destroy(filter); }
        links.clear(); restoredLinks.clear(); bindings.clear();
        if (registry) { spa_hook_remove(&registryListener); pw_proxy_destroy(reinterpret_cast<pw_proxy*>(registry)); }
        if (core) { spa_hook_remove(&coreListener); pw_core_disconnect(core); }
        if (context) pw_context_destroy(context);
        if (loop) pw_thread_loop_destroy(loop);
    }
    void add(std::uint32_t id, std::uint32_t permissions, const char* type,
             std::uint32_t version, const spa_dict* properties) {
        Kind kind; std::uint32_t supported;
        if (std::strcmp(type,PW_TYPE_INTERFACE_Node) == 0) { kind = Kind::Node; supported = PW_VERSION_NODE; }
        else if (std::strcmp(type,PW_TYPE_INTERFACE_Port) == 0) { kind = Kind::Port; supported = PW_VERSION_PORT; }
        else if (std::strcmp(type,PW_TYPE_INTERFACE_Link) == 0) { kind = Kind::Link; supported = PW_VERSION_LINK; }
        else return;
        auto binding = std::make_unique<Binding>(); binding->id = id; binding->kind = kind;
        binding->properties = properties ? pw_properties_new_dict(properties) : pw_properties_new(nullptr,nullptr);
        if (!binding->properties) return;
        binding->proxy = pw_registry_bind(registry,id,type,std::min(version,supported),0);
        if (!binding->proxy) return;
        binding->link.removable = (permissions & PW_PERM_X) != 0;
        binding->update(); auto* record = binding.get(); bindings[id] = std::move(binding);
        if (kind == Kind::Node) {
            static const pw_node_events events = [] {
                pw_node_events result{}; result.version = PW_VERSION_NODE_EVENTS;
                result.info = [](void* data, const pw_node_info* info) {
                    auto& item = *static_cast<Binding*>(data);
                    if (info->props) pw_properties_update(item.properties,info->props);
                    item.update();
                }; return result;
            }();
            pw_node_add_listener(static_cast<pw_node*>(record->proxy),&record->listener,&events,record);
        } else if (kind == Kind::Port) {
            static const pw_port_events events = [] {
                pw_port_events result{}; result.version = PW_VERSION_PORT_EVENTS;
                result.info = [](void* data, const pw_port_info* info) {
                    auto& item = *static_cast<Binding*>(data);
                    if (info->props) pw_properties_update(item.properties,info->props);
                    item.update(); item.port.output = info->direction == PW_DIRECTION_OUTPUT;
                }; return result;
            }();
            pw_port_add_listener(static_cast<pw_port*>(record->proxy),&record->listener,&events,record);
        } else {
            static const pw_link_events events = [] {
                pw_link_events result{}; result.version = PW_VERSION_LINK_EVENTS;
                result.info = [](void* data, const pw_link_info* info) {
                    auto& item = *static_cast<Binding*>(data);
                    item.link.id = item.id; item.link.output = info->output_port_id; item.link.input = info->input_port_id;
                    item.link.established = info->state == PW_LINK_STATE_ACTIVE || info->state == PW_LINK_STATE_PAUSED;
                }; return result;
            }();
            pw_link_add_listener(static_cast<pw_link*>(record->proxy),&record->listener,&events,record);
        }
    }
    AudioGraph graph() const {
        AudioGraph result;
        for (const auto& entry : bindings) {
            const auto& binding = *entry.second;
            if (binding.kind == Kind::Node) result.nodes.push_back(binding.node);
            else if (binding.kind == Kind::Port) result.ports.push_back(binding.port);
            else result.links.push_back(binding.link);
        }
        return result;
    }
    bool createLink(Connection connection, bool restore) {
        const auto from = std::to_string(connection.output), to = std::to_string(connection.input);
        const auto current = graph();
        const auto output = std::find_if(current.ports.begin(),current.ports.end(),[&](const auto& port) { return port.id == connection.output && port.output; });
        const auto input = std::find_if(current.ports.begin(),current.ports.end(),[&](const auto& port) { return port.id == connection.input && !port.output; });
        if (output == current.ports.end() || input == current.ports.end()) return false;
        const auto outputNode = std::to_string(output->node), inputNode = std::to_string(input->node);
        auto* properties = pw_properties_new(PW_KEY_LINK_OUTPUT_PORT,from.c_str(),PW_KEY_LINK_INPUT_PORT,to.c_str(),
            PW_KEY_LINK_OUTPUT_NODE,outputNode.c_str(),PW_KEY_LINK_INPUT_NODE,inputNode.c_str(),
            PW_KEY_OBJECT_LINGER,restore ? "true" : "false",nullptr);
        auto* proxy = static_cast<pw_proxy*>(pw_core_create_object(core,"link-factory",PW_TYPE_INTERFACE_Link,PW_VERSION_LINK,&properties->dict,0));
        pw_properties_free(properties);
        if (!proxy) return false;
        auto link = std::make_unique<OwnedLink>(); link->proxy = proxy;
        static const pw_proxy_events events = [] {
            pw_proxy_events result{}; result.version = PW_VERSION_PROXY_EVENTS;
            result.destroy = [](void* data) {
                auto& link = *static_cast<OwnedLink*>(data); spa_hook_remove(&link.listener); link.proxy = nullptr;
            };
            result.removed = [](void* data) {
                auto& link = *static_cast<OwnedLink*>(data); if (link.proxy) pw_proxy_destroy(link.proxy);
            }; return result;
        }();
        pw_proxy_add_listener(proxy,&link->listener,&events,link.get());
        (restore ? restoredLinks : links).push_back(std::move(link));
        return true;
    }
    bool roundTrip() {
        if (!connected || !startedLoop) return false;
        waited = false; waitSequence = pw_core_sync(core,PW_ID_CORE,0);
        if (waitSequence < 0) return false;
        timespec deadline{}; pw_thread_loop_get_time(loop,&deadline,1000000000);
        while (!waited && connected)
            if (pw_thread_loop_timed_wait_full(loop,&deadline) < 0) return false;
        return waited;
    }
    bool stopRoutes() {
        audio.enable(false);
        links.clear();
        bool restored = true;
        if (connected) {
            roundTrip();
            const auto current = graph();
            const auto own = filter ? pw_filter_get_node_id(filter) : PW_ID_ANY;
            for (const auto& link : current.links) {
                const bool ours = std::any_of(current.ports.begin(),current.ports.end(),[&](const auto& port) {
                    return port.node == own && (port.id == link.output || port.id == link.input);
                });
                if (ours && (!link.removable || pw_registry_destroy(registry,link.id) < 0)) restored = false;
            }
            for (const auto& item : saved) {
                const auto connection = restoreConnection(current,item);
                if (!connection) { restored = false; continue; }
                const bool exists = std::any_of(current.links.begin(),current.links.end(),[&](const auto& link) {
                    return link.output == connection->output && link.input == connection->input;
                });
                if (!exists && !createLink(*connection,true)) restored = false;
            }
            if (!saved.empty()) {
                if (!roundTrip() || !roundTrip()) restored = false;
                const auto updated = graph();
                for (const auto& item : saved) {
                    const auto connection = restoreConnection(updated,item);
                    if (!connection || std::none_of(updated.links.begin(),updated.links.end(),[&](const auto& link) {
                        return link.established && link.output == connection->output && link.input == connection->input;
                    })) restored = false;
                }
            }
        } else if (!saved.empty()) restored = false;
        saved.clear(); restoredLinks.clear(); plan = {}; source = sink = 0; applySequence = -1;
        return restored;
    }
    bool selectedEndpointsExist(const AudioGraph& current) const {
        bool input = false, output = false;
        for (const auto& node : current.nodes) {
            if (node.serial == source && selectableInput(node)) input = true;
            if (node.serial == sink && selectableOutput(node)) output = true;
        }
        return input && output;
    }
    void poll() {
        if (failed) {
            const auto reason = message;
            const bool restored = stopRoutes();
            state = connected ? AudioState::Error : AudioState::Disconnected;
            message = reason+(restored ? "" : "; previous output could not be fully restored");
            failed = false; return;
        }
        if (state != AudioState::Starting && state != AudioState::Running) return;
        const auto current = graph();
        std::string reason;
        if (!selectedEndpointsExist(current)) reason = "Selected device disappeared: apply again after choosing available devices";
        else if (state == AudioState::Starting) {
            if (acknowledged && routeConfirmed(current,plan)) {
                state = AudioState::Running; message = "Running: input and output links confirmed"; audio.enable(true);
            } else if (std::chrono::steady_clock::now()-started > std::chrono::seconds(5)) reason = "Routing timeout: input or output links not established";
        } else if (!routeConfirmed(current,plan)) reason = "Audio links changed: inspect devices and apply again";
        if (!reason.empty()) {
            const bool restored = stopRoutes(); state = AudioState::Error;
            message = reason+(restored ? "" : "; previous output could not be fully restored");
        }
    }
};
PipeWireEngine::PipeWireEngine() : implementation(std::make_unique<Implementation>()) {}
PipeWireEngine::~PipeWireEngine() = default;
AudioProcessor& PipeWireEngine::processor() noexcept { return implementation->audio; }
void PipeWireEngine::reconnect() {
    // Controls belong to the application, independent of PipeWire connection lifetime.
    const auto values = implementation->audio.requestedParameters();
    implementation.reset(); implementation = std::make_unique<Implementation>();
    implementation->audio.publish(values);
}
EngineSnapshot PipeWireEngine::snapshot() {
    auto& self = *implementation;
    self.audio.service();
    if (self.loop) pw_thread_loop_lock(self.loop);
    self.poll();
    EngineSnapshot snapshot; snapshot.graph = self.graph(); snapshot.state = self.state; snapshot.message = self.message;
    const auto own = self.filter ? pw_filter_get_node_id(self.filter) : PW_ID_ANY;
    for (const auto& link : snapshot.graph.links) {
        if (!link.established) continue;
        const AudioPort *from = nullptr, *to = nullptr;
        for (const auto& port : snapshot.graph.ports) {
            if (port.id == link.output) from = &port;
            if (port.id == link.input) to = &port;
        }
        if (!from || !to) continue;
        for (const auto& node : snapshot.graph.nodes) {
            if (to->node == own && node.id == from->node) snapshot.actualInput = audioNodeLabel(node);
            if (from->node == own && node.id == to->node) snapshot.actualOutput = audioNodeLabel(node);
        }
    }
    if (self.loop) pw_thread_loop_unlock(self.loop);
    return snapshot;
}
bool PipeWireEngine::apply(std::uint64_t source, std::uint64_t sink) {
    auto& self = *implementation;
    if (!self.loop) return false;
    pw_thread_loop_lock(self.loop);
    bool success = false;
    self.poll();
    if (!self.connected || !self.filter) self.message = "PipeWire unavailable: reconnect audio";
    else if (self.state == AudioState::Starting) self.message = "Audio routing is still being applied";
    else if (self.state == AudioState::Running && self.source == source && self.sink == sink && routeConfirmed(self.graph(),self.plan)) success = true;
    else {
        self.stopRoutes();
        self.plan = planRoute(self.graph(),{source,sink,pw_filter_get_node_id(self.filter)});
        if (!self.plan) { self.message = self.plan.error; self.state = AudioState::Error; }
        else {
            self.saved = self.plan.restore;
            bool requested = true;
            for (auto id : self.plan.detach) if (pw_registry_destroy(self.registry,id) < 0) requested = false;
            for (const auto& connection : self.plan.connections) {
                const auto current = self.graph();
                const bool exists = std::any_of(current.links.begin(),current.links.end(),[&](const auto& link) {
                    return link.established && link.output == connection.output && link.input == connection.input;
                });
                if (!exists && !self.createLink(connection,false)) requested = false;
            }
            self.applySequence = pw_core_sync(self.core,PW_ID_CORE,0);
            if (!requested || self.applySequence < 0) {
                self.stopRoutes(); self.state = AudioState::Error; self.message = "Cannot create input/output audio links";
            } else {
                self.source = source; self.sink = sink; self.acknowledged = false;
                self.started = std::chrono::steady_clock::now(); self.state = AudioState::Starting;
                self.message = "Applying audio: waiting for established links"; success = true;
            }
        }
    }
    pw_thread_loop_unlock(self.loop); return success;
}
void PipeWireEngine::stop() {
    auto& self = *implementation;
    if (!self.loop) return;
    pw_thread_loop_lock(self.loop);
    const bool restored = self.stopRoutes();
    self.state = self.connected ? AudioState::Ready : AudioState::Disconnected;
    self.message = restored ? "Stopped: audio links released" : "Stopped: previous output could not be fully restored";
    pw_thread_loop_unlock(self.loop);
}
}
