// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Alessandro Lucaferro

#pragma once
#include <cstdlib>
#include <iostream>
inline void require(bool value, const char* message) {
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
