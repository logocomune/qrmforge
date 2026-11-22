// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#include "dsp/speex.hpp"
#include "dsp/speex_abi.hpp"
#include <algorithm>
#include <cmath>
namespace qrmforge {
namespace {
constexpr int setDenoise = 0, setGainControl = 2, setNoiseSuppression = 18;
}
SpeexDenoiser::SpeexDenoiser(float rate) noexcept {
#ifdef QRMFORGE_ENABLE_SPEEXDSP
    if (!std::isfinite(rate) || rate < 8000 || rate > 48000 || std::floor(rate) != rate) return;
    length = std::size_t(rate)/50;
    auto* handle = speex_preprocess_state_init(int(length),int(rate));
    if (!handle) return;
    int enabled = 1, disabled = 0;
    // VAD defaults to off; its deprecated control emits a warning even when disabling it.
    if (speex_preprocess_ctl(handle,setDenoise,&enabled) != 0 || speex_preprocess_ctl(handle,setGainControl,&disabled) != 0) {
        speex_preprocess_state_destroy(handle); return;
    }
    state = handle;
#else
    (void)rate;
#endif
}
SpeexDenoiser::~SpeexDenoiser() {
#ifdef QRMFORGE_ENABLE_SPEEXDSP
    if (state) speex_preprocess_state_destroy(static_cast<SpeexPreprocessState*>(state));
#endif
}
void SpeexDenoiser::clear() noexcept { input.fill(0); output.fill(0); position = 0; }
float SpeexDenoiser::process(float sample, float strength) noexcept {
#ifdef QRMFORGE_ENABLE_SPEEXDSP
    if (!available()) return 0;
    const float result = output[position];
    input[position] = std::int16_t(std::lrint((std::isfinite(sample) ? std::clamp(sample,-1.0f,1.0f) : 0)*32767));
    if (++position == length) {
        position = 0;
        int requested = -int(std::round(std::clamp(std::isfinite(strength) ? strength : 4.5f,0.1f,10.0f)*3));
        auto* handle = static_cast<SpeexPreprocessState*>(state);
        if (requested != attenuation && speex_preprocess_ctl(handle,setNoiseSuppression,&requested) != 0) {
            failed = true; output.fill(0); return 0;
        }
        attenuation = requested;
        speex_preprocess_run(handle,input.data());
        for (std::size_t index = 0; index < length; ++index) output[index] = float(input[index])/32768;
    }
    return result;
#else
    (void)sample; (void)strength;
    return 0;
#endif
}
}
