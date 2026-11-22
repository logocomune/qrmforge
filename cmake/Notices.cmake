# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 Alessandro Lucaferro

set(notice_files
  "${PROJECT_SOURCE_DIR}/third_party/NOTICES.txt"
  "${PROJECT_SOURCE_DIR}/LICENSE"
  "${PROJECT_SOURCE_DIR}/third_party/rnnoise/COPYING"
  "${PROJECT_SOURCE_DIR}/third_party/deepfilter/LICENSE-MIT"
  "${PROJECT_SOURCE_DIR}/third_party/deepfilter/THIRD-PARTY-NOTICES.txt"
  "${PROJECT_SOURCE_DIR}/third_party/deepfilter/LADSPA-LGPL-2.1"
  "${PROJECT_SOURCE_DIR}/third_party/deepfilter/ladspa.h"
  "${PROJECT_SOURCE_DIR}/third_party/speexdsp/speex_preprocess.h"
  "${PROJECT_SOURCE_DIR}/third_party/wavpack/LICENSE")
file(GLOB distribution_notices
  "/usr/share/doc/juce-modules-source*/copyright"
  "/usr/share/doc/libpipewire-0.3-0*/copyright"
  "/usr/share/doc/libspeexdsp1/copyright"
  "/usr/share/doc/libwavpack1/copyright"
  "/usr/share/common-licenses/*")
list(APPEND notice_files ${distribution_notices})
set(notice_text "")
foreach(notice IN LISTS notice_files)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${notice}")
  file(READ "${notice}" content)
  file(RELATIVE_PATH notice_label "${PROJECT_SOURCE_DIR}" "${notice}")
  if(notice MATCHES "^/usr/share/")
    string(REPLACE "/usr/share/" "distribution/" notice_label "${notice}")
  endif()
  string(APPEND notice_text "\n\n===== ${notice_label} =====\n\n${content}")
endforeach()
set(qrmforge_notices "${PROJECT_BINARY_DIR}/qrmforge_notices.txt")
file(WRITE "${qrmforge_notices}" "${notice_text}")
