// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#pragma once
// ABI-only test fixture. Product targets must use the installed RNNoise header.
#ifdef __cplusplus
extern "C" {
#endif
typedef struct DenoiseState DenoiseState;
typedef struct RNNModel RNNModel;
int rnnoise_get_frame_size(void);
DenoiseState* rnnoise_create(RNNModel* model);
void rnnoise_destroy(DenoiseState* state);
float rnnoise_process_frame(DenoiseState* state, float* output, const float* input);
#ifdef __cplusplus
}
#endif
