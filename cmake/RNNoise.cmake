# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 Alessandro Lucaferro

# Build the unmodified generic upstream C implementation from the local release.
set(rnnoise_archive "${PROJECT_SOURCE_DIR}/third_party/rnnoise/rnnoise-0.2.tar.gz")
set(rnnoise_hash "90fce4b00b9ff24c08dbfe31b82ffd43bae383d85c5535676d28b0a2b11c0d37")
file(SHA256 "${rnnoise_archive}" actual_hash)
if(NOT actual_hash STREQUAL rnnoise_hash)
  message(FATAL_ERROR "Bundled RNNoise archive checksum mismatch")
endif()
set(rnnoise_root "${PROJECT_BINARY_DIR}/_deps/rnnoise-${rnnoise_hash}")
if(NOT EXISTS "${rnnoise_root}/rnnoise-0.2/include/rnnoise.h")
  file(MAKE_DIRECTORY "${rnnoise_root}")
  file(ARCHIVE_EXTRACT INPUT "${rnnoise_archive}" DESTINATION "${rnnoise_root}")
endif()
set(rnnoise_source "${rnnoise_root}/rnnoise-0.2")
set(rnnoise_files denoise rnn pitch kiss_fft celt_lpc nnet nnet_default
    parse_lpcnet_weights rnnoise_data rnnoise_tables)
set(rnnoise_sources "")
foreach(source IN LISTS rnnoise_files)
  list(APPEND rnnoise_sources "${rnnoise_source}/src/${source}.c")
endforeach()
add_library(qrmforge_rnnoise STATIC ${rnnoise_sources})
set_target_properties(qrmforge_rnnoise PROPERTIES POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)
target_compile_definitions(qrmforge_rnnoise PRIVATE DISABLE_DEBUG_FLOAT=1)
target_include_directories(qrmforge_rnnoise PUBLIC "${rnnoise_source}/include" PRIVATE "${rnnoise_source}/src")
target_link_libraries(qrmforge_rnnoise PUBLIC m)
install(FILES "${PROJECT_SOURCE_DIR}/third_party/rnnoise/COPYING"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/qrmforge" RENAME RNNoise-COPYING)
