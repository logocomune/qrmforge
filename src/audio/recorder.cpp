// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#include "audio/recorder.hpp"
#include <wavpack.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace qrmforge {
static_assert(sizeof(float) == sizeof(std::int32_t) && std::numeric_limits<float>::is_iec559);
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
const char* recordingErrorText(RecordingError error) noexcept {
    switch (error) {
    case RecordingError::None: return "";
    case RecordingError::InvalidRate: return "Recording requires an integer sample rate between 1000 and 384000 Hz";
    case RecordingError::FileExists: return "File already exists: choose a new filename";
    case RecordingError::OpenFailed: return "Cannot create recording file";
    case RecordingError::CodecFailed: return "Cannot initialize or encode WavPack audio";
    case RecordingError::WriteFailed: return "Recording stopped: file write failed";
    case RecordingError::RateChanged: return "Recording stopped: audio sample rate changed";
    case RecordingError::Overflow: return "Recording stopped: encoder/disk could not keep up";
    case RecordingError::InvalidBuffer: return "Recording stopped: audio buffer unavailable";
    }
    return "Recording error";
}
struct Recorder::Implementation {
    std::array<std::int32_t,capacity> ring{};
    std::atomic<std::uint64_t> head{0}, tail{0}, written{0};
    std::atomic<unsigned> producers{0};
    std::atomic<bool> active{false}, stopping{true};
    std::atomic<RecordingState> state{RecordingState::Idle};
    std::atomic<RecordingError> error{RecordingError::None};
    std::uint32_t rate = 0;
    std::string path;
    FILE* file = nullptr;
    WavpackContext* codec = nullptr;
    std::vector<unsigned char> firstBlock;
    struct stat identity{};
    std::thread worker;
    void fail(RecordingError reason) noexcept {
        auto expected = RecordingError::None;
        error.compare_exchange_strong(expected,reason);
        active = false; stopping = true; state = RecordingState::Error;
    }
    static int writeBlock(void* opaque, void* data, std::int32_t size) noexcept {
        auto& self = *static_cast<Implementation*>(opaque);
        if (size <= 0) return 1;
        try {
            if (self.firstBlock.empty()) self.firstBlock.assign(static_cast<unsigned char*>(data),static_cast<unsigned char*>(data)+size);
        } catch (...) { self.fail(RecordingError::WriteFailed); return 0; }
        if (std::fwrite(data,1,std::size_t(size),self.file) != std::size_t(size)) { self.fail(RecordingError::WriteFailed); return 0; }
        return 1;
    }
    void close() noexcept {
        if (codec) { WavpackCloseFile(codec); codec = nullptr; }
        if (file) {
            if (std::fclose(file) != 0) fail(RecordingError::WriteFailed);
            file = nullptr;
        }
        // Remove only an empty capture that still refers to the file we created.
        if (written == 0) {
            struct stat current{};
            if (lstat(path.c_str(),&current) == 0 && current.st_dev == identity.st_dev && current.st_ino == identity.st_ino)
                unlink(path.c_str());
        }
    }
    bool open() {
        const int descriptor = ::open(path.c_str(),O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
        if (descriptor < 0) { fail(errno == EEXIST ? RecordingError::FileExists : RecordingError::OpenFailed); return false; }
        if (fstat(descriptor,&identity) < 0) { ::close(descriptor); fail(RecordingError::OpenFailed); return false; }
        file = fdopen(descriptor,"w+b");
        if (!file) { ::close(descriptor); fail(RecordingError::OpenFailed); return false; }
        codec = WavpackOpenFileOutput(writeBlock,this,nullptr);
        WavpackConfig config{};
        config.bits_per_sample = 32; config.bytes_per_sample = 4;
        config.float_norm_exp = 127; config.flags = CONFIG_FAST_FLAG;
        config.num_channels = 1; config.channel_mask = 4; config.sample_rate = rate;
        config.block_samples = 4096;
        if (!codec || !WavpackSetConfiguration64(codec,&config,-1,nullptr) || !WavpackPackInit(codec)) {
            fail(RecordingError::CodecFailed); close(); return false;
        }
        return true;
    }
    void run() noexcept {
        std::array<std::int32_t,4096> samples{};
        for (;;) {
            const auto begin = tail.load(std::memory_order_relaxed);
            const auto end = head.load(std::memory_order_acquire);
            const auto count = std::size_t(std::min<std::uint64_t>(end-begin,samples.size()));
            if (count) {
                const auto offset = std::size_t(begin%capacity), first = std::min(count,std::size_t(capacity)-offset);
                std::memcpy(samples.data(),ring.data()+offset,first*sizeof(std::int32_t));
                std::memcpy(samples.data()+first,ring.data(),(count-first)*sizeof(std::int32_t));
                tail.store(begin+count,std::memory_order_release);
                if (!WavpackPackSamples(codec,samples.data(),std::uint32_t(count))) { fail(RecordingError::CodecFailed); break; }
                written.fetch_add(count);
            } else {
                if (stopping.load() && producers.load() == 0 && tail.load() == head.load()) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        if (!WavpackFlushSamples(codec)) fail(RecordingError::WriteFailed);
        if (!firstBlock.empty()) {
            WavpackUpdateNumSamples(codec,firstBlock.data());
            if (fseeko(file,0,SEEK_SET) != 0 || std::fwrite(firstBlock.data(),1,firstBlock.size(),file) != firstBlock.size()) fail(RecordingError::WriteFailed);
        }
        close();
        if (error.load() == RecordingError::None) state = RecordingState::Finished;
    }
};
Recorder::Recorder() : implementation(std::make_unique<Implementation>()) {}
Recorder::~Recorder() { stop(); wait(); }
void Recorder::stop() noexcept {
    auto& self = *implementation;
    self.active = false;
    auto expected = RecordingState::Recording;
    self.state.compare_exchange_strong(expected,RecordingState::Saving);
    self.stopping = true;
}
void Recorder::wait() { if (implementation->worker.joinable()) implementation->worker.join(); }
bool Recorder::start(const RecordingRequest& request) {
    stop(); wait();
    auto& self = *implementation;
    while (self.producers.load() != 0) std::this_thread::yield();
    self.error = RecordingError::None; self.head = 0; self.tail = 0; self.written = 0; self.firstBlock.clear();
    self.path = request.path; self.identity = {};
    if (self.path.empty() || self.path.find('\0') != std::string::npos) { self.fail(RecordingError::OpenFailed); return false; }
    if (!std::isfinite(request.sampleRate) || request.sampleRate < 1000 || request.sampleRate > 384000 || std::floor(request.sampleRate) != request.sampleRate) {
        self.fail(RecordingError::InvalidRate); return false;
    }
    self.rate = std::uint32_t(request.sampleRate);
    if (!self.open()) return false;
    self.stopping = false; self.state = RecordingState::Recording;
    try { self.worker = std::thread([&self] { self.run(); }); }
    catch (...) { self.fail(RecordingError::OpenFailed); self.close(); return false; }
    self.active = true; return true;
}
void Recorder::push(RecordingBlock block) noexcept {
    auto& self = *implementation;
    self.producers.fetch_add(1);
    if (self.active.load() && block.count) {
        const auto head = self.head.load(std::memory_order_relaxed);
        const auto used = head-self.tail.load(std::memory_order_acquire);
        if (!block.samples) self.fail(RecordingError::InvalidBuffer);
        else if (block.sampleRate != self.rate) self.fail(RecordingError::RateChanged);
        else if (used > capacity || block.count > capacity-used) self.fail(RecordingError::Overflow);
        else {
            const auto offset = std::size_t(head%capacity), first = std::min(std::size_t(block.count),std::size_t(capacity)-offset);
            std::memcpy(self.ring.data()+offset,block.samples,first*sizeof(float));
            std::memcpy(self.ring.data(),block.samples+first,(block.count-first)*sizeof(float));
            self.head.store(head+block.count,std::memory_order_release);
        }
    }
    self.producers.fetch_sub(1);
}
RecordingSnapshot Recorder::snapshot() const {
    const auto& self = *implementation;
    return {self.state.load(),self.error.load(),self.written.load(),self.rate,self.path};
}
}
