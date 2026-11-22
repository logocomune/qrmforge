// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#include "dsp/wiener.hpp"
#include <algorithm>
#include <cmath>
namespace qrmforge {
Wiener::Wiener() noexcept {
    for (unsigned index = 0; index < size; ++index) {
        unsigned bits = index, reverse = 0;
        for (unsigned bit = 0; bit < 9; ++bit) { reverse = (reverse << 1) | (bits & 1); bits >>= 1; }
        reversed[index] = reverse;
        window[index] = std::sqrt(0.5f-0.5f*std::cos(6.283185307179586f*float(index)/float(size)));
    }
    for (unsigned index = 0; index < hop; ++index)
        roots[index] = std::polar(1.0f,-6.283185307179586f*float(index)/float(size));
    clear();
}
void Wiener::clear() noexcept {
    input.fill(0); output.fill(0); noise.fill(0); gains.fill(1);
    cursor = pending = 0; initialized = false;
}
void Wiener::transform() noexcept {
    for (unsigned index = 0; index < size; ++index)
        if (index < reversed[index]) std::swap(spectrum[index],spectrum[reversed[index]]);
    for (unsigned width = 2; width <= size; width *= 2) {
        for (unsigned start = 0; start < size; start += width) {
            for (unsigned index = 0; index < width/2; ++index) {
                const auto even = spectrum[start+index];
                const auto odd = spectrum[start+index+width/2]*roots[index*size/width];
                spectrum[start+index] = even+odd; spectrum[start+index+width/2] = even-odd;
            }
        }
    }
}
void Wiener::frame(float strength) noexcept {
    for (std::size_t index = 0; index < size; ++index) spectrum[index] = input[(cursor+index)%size]*window[index];
    transform();
    const float amount = std::clamp(std::isfinite(strength) ? strength/10 : 0.45f,0.0f,1.0f);
    std::array<float,hop+1> power{};
    for (std::size_t bin = 0; bin <= hop; ++bin) power[bin] = std::norm(spectrum[bin]);
    for (std::size_t bin = 0; bin <= hop; ++bin) {
        const float current = power[bin];
        if (!initialized) noise[bin] = current;
        // A slow upward estimate avoids treating newly arriving energy as immediate noise.
        noise[bin] += (current < noise[bin] ? 0.1f : 0.002f)*(current-noise[bin]);
        const float signal = std::max(0.0f,current-noise[bin]);
        const float target = 1-amount*(1-std::max(0.08f,signal/(current+1e-12f)));
        gains[bin] += 0.2f*(target-gains[bin]);
        spectrum[bin] *= gains[bin];
        if (bin > 0 && bin < hop) spectrum[size-bin] *= gains[bin];
    }
    initialized = true;
    for (auto& value : spectrum) value = std::conj(value);
    transform();
    for (std::size_t index = 0; index < size; ++index)
        output[(cursor+index)%size] += spectrum[index].real()/float(size)*window[index];
}
float Wiener::process(float sample, float strength) noexcept {
    const float result = output[cursor]; output[cursor] = 0;
    input[cursor] = std::isfinite(sample) ? std::clamp(sample,-100.0f,100.0f) : 0;
    cursor = (cursor+1)%size;
    if (++pending == hop) { pending = 0; frame(strength); }
    return std::isfinite(result) ? result : 0;
}
}
