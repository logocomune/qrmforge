# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 Alessandro Lucaferro

BUILD_DIR ?= build
JOBS ?= 2
GUI ?= ON
RNNOISE ?= ON
CMAKE_FLAGS ?=
PACKAGE_DIR ?= dist
.PHONY: build clean test run fuzz appimage appimage-check
build:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release -DQRMFORGE_BUILD_GUI=$(GUI) -DQRMFORGE_ENABLE_RNNOISE=$(RNNOISE) $(CMAKE_FLAGS)
	cmake --build $(BUILD_DIR) --parallel $(JOBS)
clean:
	@if [ -f "$(BUILD_DIR)/CMakeCache.txt" ]; then cmake --build "$(BUILD_DIR)" --target clean; fi
test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure
run: build
	$(BUILD_DIR)/qrmforge_artefacts/Release/qrmforge
fuzz: build
	$(BUILD_DIR)/audio_fuzz 10
appimage-check:
	@test "$(GUI)" = ON || { echo 'AppImage requires GUI=ON'; exit 1; }
appimage: appimage-check
	$(MAKE) build
	python3 packaging/build_appimage.py --binary "$(BUILD_DIR)/qrmforge_artefacts/Release/qrmforge" --output "$(PACKAGE_DIR)" --cache "$(BUILD_DIR)/appimage-tools"
