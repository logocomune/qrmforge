# Bundled dependencies

## SpeexDSP preprocessing ABI and system runtime

- Publisher/source: Xiph.Org, https://github.com/xiph/speexdsp/tree/SpeexDSP-1.2.1.
- Reference header: `speexdsp/speex_preprocess.h`, copied from the selected upstream tag with its BSD-style license intact. It is an ABI/provenance reference; the small local declarations use only the required native preprocessing functions.
- Verified local runtime: Ubuntu `libspeexdsp1` version `1.2.1-1ubuntu3`, `/usr/lib/x86_64-linux-gnu/libspeexdsp.so.1.5.2`.
- Observed local SHA-256: `ef4e0d1c80fb4fcf832bb98be7df08b44faaed84ee2c003070fa1b1023a1c5fd` (records the tested file, not an independently signed upstream checksum).
- Builds link the installed runtime when available; portable packages copy it and available distribution copyright notices. No host installation or network download occurs during build or playback.
- Reviewed `preprocess.c` confirms VAD defaults to disabled; its deprecated control emits a warning even for disabling and is therefore not called. AGC is explicitly disabled and denoising enabled.

## DeepFilterNet v0.5.6 native backend and embedded model

- Publisher/source: Hendrik Schröter / Rikorose, https://github.com/Rikorose/DeepFilterNet/tree/v0.5.6.
- Official release asset: https://github.com/Rikorose/DeepFilterNet/releases/download/v0.5.6/libdeep_filter_ladspa-0.5.6-x86_64-unknown-linux-gnu.so.
- Local artifact: `deepfilter/libdeep_filter_ladspa-0.5.6-x86_64-linux-gnu.so`, copied unmodified; 52,711,472 bytes, x86-64 Linux ELF, Build ID `7f6101ff3adeb2ebe4bbd6c232e6831fb5db45e3`.
- SHA-256: `2ca3205c2911d389604a826a240e745597d50252b5cab81c8248252b335e2236`. This records the official downloaded asset and detects changes; it was not independently matched against a signed release manifest.
- Retrieved through the browser download tool after the GitHub text reader could not fetch the binary and shell DNS access failed. No application source or audio was uploaded.
- Licensing: upstream code and official model weights are dual MIT/Apache-2.0. The maintainer explicitly clarified the weights in commit https://github.com/Rikorose/DeepFilterNet/commit/7b14b89. QRMForge uses the MIT option and retains `deepfilter/LICENSE-MIT`.
- Dependency notice review (2026-11-18): `deepfilter/INVENTORY.json` records 238 registry packages in a conservative backend dependency closure, including 50 whose versioned registry paths occur in the pinned ELF. Each downloaded crate archive was SHA-256 verified against the lockfile at upstream revision `978576aa8400552a4ce9730838c635aa30db5e61`; its original `Cargo.lock` is retained alongside the inventory. Optional, build/development and non-Linux packages are intentionally overincluded, not asserted to be linked into the Linux backend.
- `deepfilter/THIRD-PARTY-NOTICES.txt` retains the available license, copyright and NOTICE files, including nested vendored-component notices such as jemalloc's. All alternatives and additional conditions in the published license declarations are preserved; the project's MIT choice for DeepFilterNet does not relicense its dependencies.
- RealFFT 3.3.0 and crunchy 0.2.2 declare MIT but omit the license text from both their published archives and recorded source revisions. The bundle supplements those declarations with later upstream MIT texts at pinned revisions, preserving their attribution. Windows import crates omit texts too; companion winapi 0.3.9 texts are supplied with their provenance. These supplements are explicitly labelled, not presented as files from the original archives.
- Rust toolchain copyright and MIT/Apache texts are retained from revision `5680fa18feaa87f3ff04063800aec256c3d4b4be`, observed in the ELF's source paths. The broad toolchain notice may also describe compiler/build-only components.
- No incompatible license was identified in the selected crate declarations. This is not a publisher-certified binary SBOM: stripped paths, optional features and the lack of a reproducible build comparison prevent proving every linked object's identity. The AppImage runtime and system-library source/relinking obligations are outside this backend inventory. See [notice maintenance](../docs/usage.md#deepfilternet-dependency-notice-maintenance).
- The LADSPA ABI header is retained with its LGPL-2.1-or-later notice and full text in `deepfilter/LADSPA-LGPL-2.1`. It is a private compatibility interface, not an installation requirement.
- Reviewed upstream `ladspa/src/lib.rs`: processing can wait and its worker has no shutdown mechanism. The backend therefore runs in a private helper process, never in the PipeWire callback. The helper avoids optional D-Bus activation and is terminated/reaped on release.
- Real native inference, frame transport, bounded callback queues and arbitrary packet float patterns were exercised. Listening quality and hardware PipeWire behavior still require target-machine checks. Scanner execution remains blocked by Docker access; integrity and functional tests do not certify absence of vulnerabilities or constitute a full transitive dependency license audit.
- Do not update the binary or digest automatically. Select a release explicitly, review source/interface and licensing changes, inspect dependencies and rerun native, protocol, timing and packaging tests.

## WavPack 5.6.0

- Publisher/source: David Bryant, https://github.com/dbry/WavPack/tree/5.6.0.
- Reference header and BSD license are included under `wavpack/`.
- Tested runtime: Ubuntu `libwavpack1` version `5.6.0-1build1`; linked from the system and copied into portable packages with available copyright notices.
- The recorder uses lossless Fast mode, 32-bit IEEE float samples and the native sample-count/header update API. No hybrid, pre-quantization or float-normalization decoding flag is used.
- Bit-exact round trips, special float patterns, compression, ring wraparound, sample-rate changes, file protection and simulated disk-write failures are covered by local tests.

## RNNoise 0.2

- Publisher: Xiph.Org; upstream release by Jean-Marc Valin.
- Source: https://github.com/xiph/rnnoise/releases/download/v0.2/rnnoise-0.2.tar.gz
- Archive: `rnnoise/rnnoise-0.2.tar.gz`, supplied by the user.
- SHA-256: `90fce4b00b9ff24c08dbfe31b82ffd43bae383d85c5535676d28b0a2b11c0d37`.
- License: BSD-style license in `rnnoise/COPYING`; included alongside the standalone application.

The digest was calculated from the supplied archive and is pinned for reproducible builds and corruption detection. It was not independently matched against a signed upstream checksum manifest. Archive paths and member types were inspected before extraction. The release contains the default model weights, so no separate model download is required.

`cmake/RNNoise.cmake` verifies the archive hash, extracts it beneath the selected build directory and compiles the unmodified generic library source list from upstream `Makefile.am`. The model is linked statically into the standalone application. No upstream setup scripts, package installers, training tools or network downloads run during the QRMForge build. The build uses portable compiler defaults; upstream may warn that newer SIMD instruction sets could improve performance. Target CPU acceleration remains a separate optimization.

To update, select an upstream release explicitly, inspect its archive and license, review source/API/build-list changes, replace the archive and pinned hash, update this record and run tests and fuzzing with real inference. Do not replace an archive without updating its hash. Clean the build directory when changing dependency versions.

Security scanners could not run in the current environment because Docker socket access is denied. Source inspection, local integrity pinning and tests do not constitute a complete dependency vulnerability audit.
