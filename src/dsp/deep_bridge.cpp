// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#include "dsp/deep_paths.hpp"
#include "dsp/deep_protocol.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <limits.h>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <new>
#include <poll.h>
#include <pthread.h>
#include <spawn.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
struct DFState {
    int input = -1, output = -1;
    pid_t process = -1;
    bool failed = false;
    const std::atomic<bool>* stopping = nullptr;
    qrmforge::DeepPacket packet;
};
namespace {
using Clock = std::chrono::steady_clock;

bool waitForPipe(const DFState& state, pollfd& event, Clock::time_point deadline) {
    using namespace std::chrono;
    while (!state.stopping->load()) {
        const auto remaining = ceil<milliseconds>(deadline - Clock::now());
        if (remaining <= milliseconds::zero()) return false;
        // Bound shutdown latency without closing descriptors from another thread.
        const int ready = poll(&event, 1, static_cast<int>(std::min(remaining, milliseconds(25)).count()));
        if (ready > 0) return !state.stopping->load() && (event.revents & event.events);
        if (ready < 0 && errno != EINTR) return false;
    }
    return false;
}

bool readExact(const DFState& state, void* data, std::size_t size) {
    auto* bytes = static_cast<char*>(data);
    const auto deadline = Clock::now() + std::chrono::seconds(30);
    pollfd event{state.output, POLLIN, 0};
    while (size) {
        if (!waitForPipe(state, event, deadline)) return false;
        const auto count = read(state.output, bytes, size);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return false;
        bytes += count;
        size -= std::size_t(count);
    }
    return true;
}

bool writePacket(const DFState& state) {
    // Linux reports POLLOUT when a pipe can accept an atomic packet of this size.
    static_assert(sizeof(qrmforge::DeepPacket) <= PIPE_BUF);
    sigset_t blocked;
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGPIPE);
    if (pthread_sigmask(SIG_BLOCK, &blocked, nullptr) != 0) return false;
    const auto deadline = Clock::now() + std::chrono::seconds(5);
    pollfd event{state.input, POLLOUT, 0};
    while (waitForPipe(state, event, deadline)) {
        const auto count = write(state.input, &state.packet, sizeof(state.packet));
        if (count < 0 && errno == EINTR) continue;
        return count == sizeof(state.packet);
    }
    return false;
}
std::string helperPath() {
    return qrmforge::deepResourcePath(QRMFORGE_DEEPFILTER_INSTALLED_HELPER,
                                     "../libexec/qrmforge/qrmforge-deepfilter-worker",
                                     QRMFORGE_DEEPFILTER_HELPER);
}
void release(DFState* state) noexcept {
    if (state->input >= 0) close(state->input);
    if (state->output >= 0) close(state->output);
    if (state->process > 0) {
        int status;
        pid_t result;
        do { result = waitpid(state->process,&status,WNOHANG); } while (result < 0 && errno == EINTR);
        if (result == 0) {
            kill(state->process,SIGKILL);
            while (waitpid(state->process,&status,0) < 0 && errno == EINTR) {}
        }
    }
    delete state;
}
}
extern "C" DFState* df_create(const char* path, float attenuation, const std::atomic<bool>* stopping) {
    if (stopping->load()) return nullptr;
    std::string helper;
    try { helper = helperPath(); } catch (...) { return nullptr; }
    int input[2], output[2];
    if (pipe2(input,O_CLOEXEC) < 0) return nullptr;
    if (pipe2(output,O_CLOEXEC) < 0) { close(input[0]); close(input[1]); return nullptr; }
    for (int* descriptor : {&input[0],&input[1],&output[0],&output[1]}) {
        if (*descriptor > STDERR_FILENO) continue;
        const int moved = fcntl(*descriptor,F_DUPFD_CLOEXEC,3);
        if (moved < 0) { for (int fd : {input[0],input[1],output[0],output[1]}) close(fd); return nullptr; }
        close(*descriptor); *descriptor = moved;
    }
    auto* state = new (std::nothrow) DFState;
    if (!state) { close(input[0]); close(input[1]); close(output[0]); close(output[1]); return nullptr; }
    state->stopping = stopping;
    state->input = input[1]; state->output = output[0]; state->packet.attenuation = attenuation;
    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions) != 0) {
        close(input[0]); close(output[1]); release(state); return nullptr;
    }
    bool valid = posix_spawn_file_actions_adddup2(&actions,input[0],STDIN_FILENO) == 0 &&
        posix_spawn_file_actions_adddup2(&actions,output[1],STDOUT_FILENO) == 0;
    for (int descriptor : {input[0],input[1],output[0],output[1]})
        if (posix_spawn_file_actions_addclose(&actions,descriptor) != 0) valid = false;
    char* arguments[]{const_cast<char*>(helper.c_str()),const_cast<char*>(path),nullptr};
    const int result = valid ? posix_spawn(&state->process,helper.c_str(),&actions,nullptr,arguments,environ) : EINVAL;
    posix_spawn_file_actions_destroy(&actions); close(input[0]); close(output[1]);
    if (result != 0) { state->process = -1; release(state); return nullptr; }
    qrmforge::DeepHello hello;
    if (!readExact(*state,&hello,sizeof(hello)) || hello.magic != qrmforge::deepMagic || hello.rate != 48000 || hello.frame != 480) {
        release(state); return nullptr;
    }
    return state;
}
extern "C" std::size_t df_get_frame_length(DFState* state) { return state && !state->failed ? 480 : 0; }
extern "C" void df_set_atten_lim(DFState* state, float value) { state->packet.attenuation = value; }
extern "C" float df_process_frame(DFState* state, float* input, float* output) {
    std::memcpy(state->packet.samples.data(),input,480*sizeof(float));
    if (state->failed || !writePacket(*state) || !readExact(*state,output,480*sizeof(float))) {
        state->failed = true; std::fill_n(output,480,0); return 0;
    }
    return 0;
}
extern "C" void df_free(DFState* state) { if (state) release(state); }
