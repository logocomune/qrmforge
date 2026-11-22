// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#include "dsp/deep_paths.hpp"
#include "dsp/deep_filter.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <limits>
#include <string>
#include <thread>

#ifdef QRMFORGE_ENABLE_DEEPFILTER
// Narrow internal interface implemented by the isolated native-backend bridge.
extern "C" {
struct DFState;
DFState* df_create(const char* path, float attenuation, const std::atomic<bool>* stopping);
std::size_t df_get_frame_length(DFState* state);
void df_set_atten_lim(DFState* state, float attenuation);
float df_process_frame(DFState* state, float* input, float* output);
void df_free(DFState* state);
}
#endif
namespace qrmforge {
struct DeepFilter::Implementation {
    struct Frame {
        std::array<float,frameSize> samples{};
        std::uint32_t epoch = 0;
        std::uint64_t sequence = 0;
        float strength = 4.5f;
    };
    template<std::size_t capacity> struct Queue {
        std::array<Frame,capacity> frames{};
        std::atomic<unsigned> head{0}, tail{0};
        bool push(const Frame& frame) noexcept {
            const auto position = head.load(std::memory_order_relaxed);
            const auto next = (position+1)%capacity;
            if (next == tail.load(std::memory_order_acquire)) return false;
            frames[position] = frame; head.store(next,std::memory_order_release); return true;
        }
        bool pop(Frame& frame) noexcept {
            const auto position = tail.load(std::memory_order_relaxed);
            if (position == head.load(std::memory_order_acquire)) return false;
            frame = frames[position]; tail.store((position+1)%capacity,std::memory_order_release); return true;
        }
    };
    Queue<8> inputs, outputs;
    std::atomic<bool> valid{false}, starting{true}, stopping{false};
    std::atomic<std::uint32_t> epoch{0}, dropped{0};
    Frame input, output, waiting;
    bool hasWaiting = false;
    std::size_t position = 0;
    std::uint64_t sequence = 0;
    std::thread worker;
#ifdef QRMFORGE_ENABLE_DEEPFILTER
    DFState* model = nullptr;
    std::string path;
    Implementation() {
        path = deepResourcePath(
#ifdef QRMFORGE_DEEPFILTER_INSTALLED_BACKEND
            QRMFORGE_DEEPFILTER_INSTALLED_BACKEND,
#else
            "",
#endif
            "../lib/qrmforge/libdeep_filter_ladspa.so", QRMFORGE_DEEPFILTER_BACKEND);
        worker = std::thread([this] { run(); });
    }
    ~Implementation() {
        stopping = true;
        if (worker.joinable()) worker.join();
        if (model) df_free(model);
    }
    void run() noexcept {
        model = df_create(path.c_str(),13.5f,&stopping);
        valid = model && df_get_frame_length(model) == frameSize;
        starting = false;
        if (!valid.load()) return;
        std::uint32_t seen = std::numeric_limits<std::uint32_t>::max();
        Frame frame;
        while (!stopping.load()) {
            if (!inputs.pop(frame)) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); continue; }
            if (frame.epoch != epoch.load()) continue;
            if (seen != frame.epoch) {
                if (seen != std::numeric_limits<std::uint32_t>::max()) {
                    df_free(model); model = df_create(path.c_str(),13.5f,&stopping);
                    if (!model || df_get_frame_length(model) != frameSize) { valid = false; break; }
                }
                seen = frame.epoch;
            }
            df_set_atten_lim(model,frame.strength*3);
            Frame result; result.epoch = frame.epoch; result.sequence = frame.sequence;
            df_process_frame(model,frame.samples.data(),result.samples.data());
            if (df_get_frame_length(model) != frameSize) { valid = false; df_free(model); model = nullptr; break; }
            for (auto& value : result.samples) value = std::isfinite(value) ? std::clamp(value,-1.0f,1.0f) : 0;
            if (frame.epoch == epoch.load() && !outputs.push(result)) ++dropped;
        }
    }
#endif
};
DeepFilter::DeepFilter(float sampleRate) noexcept {
#ifdef QRMFORGE_ENABLE_DEEPFILTER
    if (sampleRate != 48000) return;
    try { implementation = std::make_unique<Implementation>(); }
    catch (...) { implementation.reset(); }
#else
    (void)sampleRate;
#endif
}
DeepFilter::~DeepFilter() = default;
bool DeepFilter::starting() const noexcept {
    return implementation && implementation->starting.load();
}
bool DeepFilter::supported(float sampleRate) noexcept {
#ifdef QRMFORGE_ENABLE_DEEPFILTER
    return sampleRate == 48000;
#else
    (void)sampleRate; return false;
#endif
}
bool DeepFilter::available() const noexcept { return implementation && implementation->valid.load(); }
std::uint32_t DeepFilter::droppedFrames() const noexcept { return implementation ? implementation->dropped.load() : 0; }
void DeepFilter::clear() noexcept {
    if (!implementation) return;
    auto& state = *implementation;
    ++state.epoch; state.position = 0; state.sequence = 0; state.hasWaiting = false;
    state.input.samples.fill(0); state.output.samples.fill(0);
}
float DeepFilter::process(float sample, float strength) noexcept {
    if (!available()) return 0;
    auto& state = *implementation;
    const float result = state.output.samples[state.position];
    state.input.samples[state.position] = std::isfinite(sample) ? std::clamp(sample,-1.0f,1.0f) : 0;
    if (++state.position != frameSize) return result;
    state.position = 0;
    state.input.epoch = state.epoch.load(); state.input.sequence = state.sequence++;
    state.input.strength = std::clamp(std::isfinite(strength) ? strength : 4.5f,0.1f,10.0f);
    if (!state.inputs.push(state.input)) ++state.dropped;
    state.output.samples.fill(0);
    if (state.sequence < delayFrames) return result;
    const auto expected = state.sequence-delayFrames;
    // Fixed queue capacity bounds work even when the worker produces stale frames.
    for (unsigned attempt = 0; attempt < 8; ++attempt) {
        if (!state.hasWaiting && !state.outputs.pop(state.waiting)) break;
        state.hasWaiting = false;
        if (state.waiting.epoch != state.input.epoch || state.waiting.sequence < expected) continue;
        if (state.waiting.sequence == expected) state.output = state.waiting;
        else state.hasWaiting = true;
        return result;
    }
    ++state.dropped;
    return result;
}
}
