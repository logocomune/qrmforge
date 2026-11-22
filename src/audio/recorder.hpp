// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#pragma once
#include <cstdint>
#include <memory>
#include <string>
namespace qrmforge {
enum class RecordingState { Idle, Recording, Saving, Finished, Error };
enum class RecordingError { None, InvalidRate, FileExists, OpenFailed, CodecFailed, WriteFailed, RateChanged, Overflow, InvalidBuffer };
struct RecordingRequest { std::string path; float sampleRate = 48000; };
struct RecordingBlock { const float* samples = nullptr; std::uint32_t count = 0; float sampleRate = 48000; };
struct RecordingSnapshot {
    RecordingState state = RecordingState::Idle;
    RecordingError error = RecordingError::None;
    std::uint64_t samples = 0;
    std::uint32_t sampleRate = 0;
    std::string path;
};
const char* recordingErrorText(RecordingError error) noexcept;
class Recorder {
public:
    static constexpr std::uint32_t capacity = 262144;
    Recorder();
    ~Recorder();
    Recorder(const Recorder&) = delete;
    Recorder& operator=(const Recorder&) = delete;
    bool start(const RecordingRequest& request);
    void stop() noexcept;
    void wait();
    void push(RecordingBlock block) noexcept;
    RecordingSnapshot snapshot() const;
private:
    struct Implementation;
    std::unique_ptr<Implementation> implementation;
};
}
