// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#pragma once
#include <cstdint>
// SpeexDSP 1.2.1 preprocessing ABI; the upstream header and license are in third_party.
extern "C" {
struct SpeexPreprocessState_;
using SpeexPreprocessState = SpeexPreprocessState_;
SpeexPreprocessState* speex_preprocess_state_init(int frameSize, int sampleRate);
void speex_preprocess_state_destroy(SpeexPreprocessState* state);
int speex_preprocess_run(SpeexPreprocessState* state, std::int16_t* samples);
int speex_preprocess_ctl(SpeexPreprocessState* state, int request, void* value);
}
