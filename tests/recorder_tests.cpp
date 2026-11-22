// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#include "audio/processor.hpp"
#include "test_support.hpp"
#include <wavpack.h>
#include <array>
#include <cmath>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <random>
#include <sys/resource.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>
using namespace qrmforge;
std::vector<std::int32_t> decode(const std::string& path) {
    char error[100]{};
    auto* decoder = WavpackOpenFileInput(path.c_str(),error,0,0);
    require(decoder,"Recorded file is readable WavPack");
    require((WavpackGetMode(decoder)&MODE_LOSSLESS) && (WavpackGetMode(decoder)&MODE_FLOAT),"File is lossless floating-point audio");
    require(WavpackGetNumChannels(decoder) == 1 && WavpackGetBitsPerSample(decoder) == 32,"Mono float32 format");
    require(WavpackGetSampleRate(decoder) == 48000,"Recording retains its negotiated sample rate");
    const auto length = WavpackGetNumSamples64(decoder);
    require(length >= 0 && length < 2000000,"Final header has an accurate sample count");
    std::vector<std::int32_t> result(std::size_t(length),0);
    require(WavpackUnpackSamples(decoder,result.data(),std::uint32_t(result.size())) == result.size(),"All recorded frames decode");
    require(WavpackGetNumErrors(decoder) == 0,"Recorded block checksums validate");
    WavpackCloseFile(decoder); return result;
}
int main() {
    char directory[] = "/tmp/qrmforge-recording-XXXXXX";
    require(mkdtemp(directory),"Create isolated recording directory");
    const auto root = std::filesystem::path(directory);
    const auto path = [&](const char* name) { return (root/name).string(); };
    std::vector<float> samples(50000);
    std::mt19937 random(907302);
    for (auto& sample : samples) { const std::uint32_t bits = random(); std::memcpy(&sample,&bits,4); }
    const std::uint32_t special[]{0,0x80000000,0x7fc00001,0x7f800000,0xff800000,1,0x7f7fffff};
    std::memcpy(samples.data(),special,sizeof(special));
    Recorder recorder;
    require(recorder.snapshot().state == RecordingState::Idle,"Recorder starts idle");
    recorder.push({nullptr,1,48000});
    for (float rate : {0.0f,-1.0f,48000.5f,400000.0f,std::numeric_limits<float>::quiet_NaN()})
        require(!recorder.start({path("invalid.wv"),rate}) && recorder.snapshot().error == RecordingError::InvalidRate,"Unsupported rates refused before creating a file");
    require(!recorder.start({path("missing/file.wv"),48000}) && recorder.snapshot().error == RecordingError::OpenFailed,"Missing parent directory produces a useful error");
    { std::ofstream keep(path("existing.wv")); keep << "KEEP"; }
    require(!recorder.start({path("existing.wv"),48000}) && recorder.snapshot().error == RecordingError::FileExists,"Existing files are never overwritten");
    { std::ifstream keep(path("existing.wv")); std::string text; keep >> text; require(text == "KEEP","Existing contents survive recording attempts"); }
    require(!recorder.start({path("existing.wv")+std::string("\0suffix",7),48000}) && recorder.snapshot().error == RecordingError::OpenFailed,"Embedded NUL paths are rejected before reaching the filesystem");
    require(recorder.start({path("float.wv"),48000}),"Start float recording");
    recorder.push({samples.data(),0,48000});
    recorder.push({samples.data(),std::uint32_t(samples.size()),48000});
    recorder.stop(); recorder.wait();
    require(recorder.snapshot().state == RecordingState::Finished && recorder.snapshot().samples == samples.size(),"Stop drains and finalizes accepted frames");
    const auto restored = decode(path("float.wv"));
    require(restored.size() == samples.size() && std::memcmp(restored.data(),samples.data(),samples.size()*4) == 0,"Lossless float recording preserves every bit, including NaN payloads and signed zero");
    require(recorder.start({path("empty.wv"),48000}),"Start an empty capture");
    recorder.stop(); recorder.wait();
    require(!std::filesystem::exists(path("empty.wv")),"Empty captures are removed without leaving unreadable files");
    require(recorder.start({path("null.wv"),48000}),"Start missing-buffer test");
    recorder.push({nullptr,1,48000}); recorder.wait();
    require(recorder.snapshot().error == RecordingError::InvalidBuffer,"Missing buffer stops with an explicit error");
    require(recorder.start({path("overflow.wv"),48000}),"Start overflow test");
    recorder.push({samples.data(),Recorder::capacity+1,48000}); recorder.wait();
    require(recorder.snapshot().error == RecordingError::Overflow,"Oversized block is rejected before reading it");
    require(recorder.start({path("rate.wv"),48000}),"Start rate change test");
    recorder.push({samples.data(),100,48000}); recorder.push({samples.data(),100,44100}); recorder.wait();
    require(recorder.snapshot().error == RecordingError::RateChanged && decode(path("rate.wv")).size() == 100,"Rate changes preserve a valid prefix rather than mixing clock rates");
    {
        Recorder scoped;
        require(scoped.start({path("destructor.wv"),48000}),"Start destructor flush test");
        scoped.push({samples.data(),1000,48000});
    }
    require(decode(path("destructor.wv")).size() == 1000,"Closing the recorder finalizes audio");
    std::vector<float> silence(160000,0);
    require(recorder.start({path("silence.wv"),48000}),"Start compression test");
    recorder.push({silence.data(),std::uint32_t(silence.size()),48000}); recorder.stop(); recorder.wait();
    require(std::filesystem::file_size(path("silence.wv")) < silence.size()*sizeof(float)/10,"Fast lossless encoding actually compresses compressible audio");
    require(recorder.start({path("wrap.wv"),48000}),"Start ring wrap test");
    for (unsigned block = 0; block < 300; ++block) {
        recorder.push({samples.data(),1024,48000});
        if (block%8 == 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    recorder.stop(); recorder.wait();
    const auto wrapped = decode(path("wrap.wv"));
    require(recorder.snapshot().error == RecordingError::None && wrapped.size() == 300*1024,"Bounded ring handles concurrent wraparound");
    for (unsigned block = 0; block < 300; ++block)
        require(std::memcmp(wrapped.data()+block*1024,samples.data(),1024*4) == 0,"No samples disappear or reorder at the ring boundary");
    {
        AudioProcessor processor; processor.enable(true);
        require(processor.recorder().start({path("processed.wv"),48000}),"Start DSP output capture");
        std::array<float,256> input{}, output{}; input.fill(0.25f);
        auto parameters = defaults; parameters[Mute] = 1; processor.publish(parameters);
        processor.process(input.data(),output.data(),input.size(),48000);
        processor.publish(defaults); processor.process(input.data(),output.data(),input.size(),48000);
        processor.enable(false); processor.recorder().wait();
        const auto audio = decode(path("processed.wv"));
        for (std::size_t index = 0; index < audio.size(); ++index) {
            const float expected = index < 256 ? 0 : 0.25f;
            require(std::memcmp(&audio[index],&expected,4) == 0,"Recording captures actual post-DSP mute and bypass output");
        }
    }
    const auto child = fork(); require(child >= 0,"Create isolated write-failure test");
    if (child == 0) {
        std::signal(SIGXFSZ,SIG_IGN);
        const rlimit limit{1024,1024};
        if (setrlimit(RLIMIT_FSIZE,&limit) != 0) _exit(1);
        Recorder limited;
        if (!limited.start({path("limited.wv"),48000})) _exit(1);
        limited.push({samples.data(),std::uint32_t(samples.size()),48000}); limited.stop(); limited.wait();
        _exit(limited.snapshot().error == RecordingError::WriteFailed ? 0 : 1);
    }
    int status = 0; require(waitpid(child,&status,0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0,"Disk write failures cannot appear as successful recordings");
    for (auto error : {RecordingError::InvalidRate,RecordingError::FileExists,RecordingError::OpenFailed,RecordingError::CodecFailed,
                       RecordingError::WriteFailed,RecordingError::RateChanged,RecordingError::Overflow,RecordingError::InvalidBuffer})
        require(std::strlen(recordingErrorText(error)) > 0,"Recording errors have actionable text");
    require(std::strlen(recordingErrorText(RecordingError::None)) == 0 && std::strlen(recordingErrorText(RecordingError(99))) > 0,"Error text handles idle and unknown states");
    std::filesystem::remove_all(root);
}
