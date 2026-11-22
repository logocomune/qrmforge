// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#include "dsp/deep_filter.hpp"
#include "dsp/deep_protocol.hpp"
#include "test_support.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <thread>
#include <unistd.h>

using namespace qrmforge;
using namespace std::chrono_literals;

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--installed") {
        DeepFilter filter(48000);
        const auto deadline = std::chrono::steady_clock::now() + 2s;
        while (filter.starting() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(5ms);
        require(filter.available(), "Installed helper receives the installed library path");
        return 0;
    }
    if (argc == 2 && std::getenv("QRMFORGE_TEST_INSTALLED")) {
        const auto executable = std::filesystem::read_symlink("/proc/self/exe");
        if (executable.filename() != "qrmforge-deepfilter-worker" ||
            !std::filesystem::is_regular_file(argv[1])) return 1;
        const DeepHello hello;
        return std::fwrite(&hello, sizeof(hello), 1, stdout) == 1 ? 0 : 1;
    }
    const auto* marker = std::getenv("QRMFORGE_TEST_SHUTDOWN_MARKER");
    if (argc > 1) {
        std::this_thread::sleep_for(750ms);
        DeepHello hello;
        DeepPacket packet;
        if (!marker || std::fwrite(&hello, sizeof(hello), 1, stdout) != 1 ||
            std::fflush(stdout) != 0 || std::fread(&packet, sizeof(packet), 1, stdin) != 1) return 1;
        std::ofstream(marker).close();
        std::this_thread::sleep_for(3s);
        return 0;
    }

    char directory[] = "/tmp/qrmforge-shutdown-XXXXXX";
    require(mkdtemp(directory) != nullptr, "Create temporary test directory");
    const auto path = std::filesystem::path(directory) / "waiting";
    require(setenv("QRMFORGE_TEST_SHUTDOWN_MARKER", path.c_str(), 1) == 0, "Set helper marker path");
    const auto constructionStart = std::chrono::steady_clock::now();
    auto filter = std::make_unique<DeepFilter>(48000);
    require(std::chrono::steady_clock::now() - constructionStart < 500ms, "Model startup returns before helper handshake");
    const auto startupDeadline = std::chrono::steady_clock::now() + 2s;
    while (!filter->available() && std::chrono::steady_clock::now() < startupDeadline)
        std::this_thread::sleep_for(5ms);
    require(filter->available(), "Test helper completes handshake");
    for (int sample = 0; sample < 480; ++sample) filter->process(0.0f, 4.5f);
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (!std::filesystem::exists(path) && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(5ms);
    require(std::filesystem::exists(path), "Helper receives frame and stalls");
    const auto start = std::chrono::steady_clock::now();
    filter.reset();
    std::filesystem::remove_all(directory);
    require(std::chrono::steady_clock::now() - start < 1s, "Shutdown interrupts stalled inference");
}
