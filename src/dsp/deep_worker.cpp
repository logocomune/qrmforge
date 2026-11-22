// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#include "dsp/deep_protocol.hpp"
#include <ladspa.h>
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <dlfcn.h>
#include <unistd.h>
namespace {
bool readExact(void* data, std::size_t size) {
    auto* bytes = static_cast<char*>(data);
    while (size) {
        const auto count = read(STDIN_FILENO,bytes,size);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return false;
        bytes += count; size -= std::size_t(count);
    }
    return true;
}
bool writeExact(const void* data, std::size_t size) {
    auto* bytes = static_cast<const char*>(data);
    while (size) {
        const auto count = write(STDOUT_FILENO,bytes,size);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return false;
        bytes += count; size -= std::size_t(count);
    }
    return true;
}
}
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    auto* library = dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
    if (!library) return 3;
    using Descriptor = const LADSPA_Descriptor* (*)(unsigned long);
    auto descriptor = reinterpret_cast<Descriptor>(dlsym(library,"ladspa_descriptor"));
    const auto* plugin = descriptor ? descriptor(0) : nullptr;
    if (!plugin || plugin->UniqueID != 7843795 || plugin->PortCount != 8 || !plugin->Label ||
        std::strcmp(plugin->Label,"deep_filter_mono") != 0 || !plugin->instantiate || !plugin->connect_port || !plugin->run) return 4;
    auto instance = plugin->instantiate(plugin,48000);
    if (!instance) return 5;
    qrmforge::DeepPacket packet;
    std::array<float,480> output{};
    std::array<float,6> controls{13.5f,-10,30,20,0,0};
    plugin->connect_port(instance,0,packet.samples.data());
    plugin->connect_port(instance,1,output.data());
    for (unsigned index = 0; index < controls.size(); ++index) plugin->connect_port(instance,index+2,&controls[index]);
    const qrmforge::DeepHello hello;
    if (!writeExact(&hello,sizeof(hello))) return 6;
    while (readExact(&packet,sizeof(packet))) {
        controls[0] = std::clamp(std::isfinite(packet.attenuation) ? packet.attenuation : 13.5f,0.3f,30.0f);
        for (auto& value : packet.samples) value = std::isfinite(value) ? std::clamp(value,-1.0f,1.0f) : 0;
        plugin->run(instance,480);
        for (auto& value : output) value = std::isfinite(value) ? std::clamp(value,-1.0f,1.0f) : 0;
        if (!writeExact(output.data(),sizeof(output))) break;
    }
    // Upstream's worker has no shutdown protocol; process exit reclaims all its threads safely.
    _exit(0);
}
