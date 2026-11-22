// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#include "rnnoise.h"
#include <algorithm>
#include <new>
// Identity transform for adapter tests, not a denoising model.
struct DenoiseState {};
namespace { int failureMode = 0; }
extern "C" void qrmforge_mock_failure(int mode) { failureMode = mode; }
extern "C" int rnnoise_get_frame_size() { return failureMode == 1 ? 240 : 480; }
extern "C" DenoiseState* rnnoise_create(RNNModel*) {
    return failureMode == 2 ? nullptr : new (std::nothrow) DenoiseState;
}
extern "C" void rnnoise_destroy(DenoiseState* state) { delete state; }
extern "C" float rnnoise_process_frame(DenoiseState*, float* output, const float* input) {
    std::copy(input, input+480, output);
    return 0;
}
