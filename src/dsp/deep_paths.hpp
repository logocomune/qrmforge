// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#pragma once
#include <filesystem>
#include <string>

namespace qrmforge {
inline std::string deepResourcePath(const char* installed, const char* portable, const char* fallback) {
    std::error_code error;
    const auto executable = std::filesystem::read_symlink("/proc/self/exe", error);
    if (error) return fallback;
    for (const auto* path : {installed, portable}) {
        if (!*path) continue;
        const auto candidate = executable.parent_path() / path;
        if (std::filesystem::is_regular_file(candidate, error)) return candidate.string();
    }
    return fallback;
}
}
