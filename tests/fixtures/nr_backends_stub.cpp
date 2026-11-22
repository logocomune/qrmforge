// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#include "dsp/speex_abi.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <new>
#include <thread>

// Deterministic ABI fixtures; these are not denoising implementations or models.
namespace {
std::atomic<int> speexFailure{0}, deepFailure{0};
std::atomic<bool> holdDeep{false}, processing{false};
std::atomic<float> attenuation{0};
}
extern "C" void nr_mock_speex_failure(int value) { speexFailure = value; }
extern "C" void nr_mock_deep_failure(int value) { deepFailure = value; }
extern "C" void nr_mock_hold_deep(bool value) { holdDeep = value; }
extern "C" bool nr_mock_processing() { return processing.load(); }
extern "C" float nr_mock_attenuation() { return attenuation.load(); }
struct SpeexPreprocessState_ { int size; };
extern "C" SpeexPreprocessState* speex_preprocess_state_init(int length, int) {
    return speexFailure == 1 ? nullptr : new (std::nothrow) SpeexPreprocessState_{length};
}
extern "C" void speex_preprocess_state_destroy(SpeexPreprocessState* state) { delete state; }
extern "C" int speex_preprocess_ctl(SpeexPreprocessState*, int, void*) { return speexFailure == 2 ? -1 : 0; }
extern "C" int speex_preprocess_run(SpeexPreprocessState* state, std::int16_t* samples) {
    for (int index = 0; index < state->size; ++index) samples[index] /= 2;
    return 1;
}
struct DFState {};
extern "C" DFState* df_create(const char*, float, const std::atomic<bool>*) { return deepFailure == 1 ? nullptr : new (std::nothrow) DFState; }
extern "C" std::size_t df_get_frame_length(DFState*) { return deepFailure == 2 ? 256 : 480; }
extern "C" void df_set_atten_lim(DFState*, float value) { attenuation = value; }
extern "C" float df_process_frame(DFState*, float* input, float* output) {
    processing = true;
    while (holdDeep.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    for (std::size_t index = 0; index < 480; ++index) output[index] = input[index]*0.5f;
    processing = false;
    return 0;
}
extern "C" void df_free(DFState* state) { delete state; }
