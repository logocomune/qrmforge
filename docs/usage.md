# QRMForge usage and maintenance

## Audio workflow

1. Start the external RTP receiver or play the source application so its PipeWire node and ports exist.
2. Start `qrmforge` or portable `AppRun`.
3. Select the source under INPUT, for example `rtx-hf-receiver`.
4. Select headphones or the desired output under OUTPUT.
5. Press Apply audio and wait for the established-link confirmation.
6. Disable BYPASS to enable processing. MUTE overrides bypass.

No input/output is selected automatically. The application creates an audio filter node for its lifetime but opens no microphone by default. The actual-route labels are derived from observed links rather than selections.

Inputs include physical `Audio/Source` nodes and application/network `Stream/Output/Audio` nodes. The input channel preference is MONO, then FL, then the first audio port. Stereo inputs are not mixed. A MONO sink receives one link; FL/FR sinks receive the same processed mono signal on both channels. This is a mono processor, not independent stereo processing or radio demodulation.

Apply creates explicit links, including for network receiver streams without session-manager autoconnect. Repeating Apply on the running selection creates no duplicate links. Changing selections and applying stops the previous route before starting the new one.

Direct application-stream links to sinks are saved and detached to prevent parallel dry playback. Stop, orderly close and a failed Apply attempt try to restore those links using their original port serials. Physical-source routes to other applications are preserved. Restoration is not possible if the original ports have disappeared. The status text reports incomplete restoration.

Created processing links do not linger after the client disappears. An abrupt process kill removes the standalone node and its processing links, but cannot execute restoration of previously detached direct routes. Restart the source application's playback or reconnect it if needed after a crash.

Stop audio releases processing links and retains controls in the running application. Close stops audio and exits. There is no background daemon or tray mode. Controls do not persist across launches in this version.

If a device disappears, a confirmed link changes or PipeWire disconnects, processing stops with a specific status. After reconnecting devices, select them and press Apply. Use Reconnect after a PipeWire restart; it recreates the audio client and preserves the application's requested control values. No automatic replacement input or output is chosen.

## PRE/POST audio level meters

Each monitor includes a vertical RMS bar on its right edge, a horizontal
sample-peak marker and numeric RMS/PEAK readings below it. Both use the same
-60 to 0 dBFS scale, with 0 at the top. The meter occupies a separate strip,
leaving the spectrum and waterfall their full available height. PRE measures raw input before pre-gain and all DSP;
POST measures the actual output after processing, limiter and mute, also used by
the recorder. Bypass makes PRE and POST agree; mute silences POST while PRE
continues. A magenta peak marker/readout means the sampled audio reached or
exceeded 0 dBFS. It indicates digital full scale, not proof of waveform clipping.
Numeric readings retain values outside the bar range; silent sample peak reads
`-inf`, while the RMS average decays toward silence.
`--` means no current level window. Stop/reconnect clears both meters, and readings
expire after two seconds without a successfully received telemetry window.

Levels are calculated on the GUI thread from the existing 2048-sample telemetry
windows (about 42.7 ms at 48 kHz), without changing the audio callback. RMS is
unweighted `20 log10(sqrt(mean(sample^2)))`, referenced to sample amplitude 1;
a full-scale sine therefore reads about -3.01 dBFS RMS and 0 dBFS sample peak.
Non-finite samples contribute zero; finite over-range samples are not clamped for
metering. The RMS bar and numeric readout use a 300 ms exponential average of signal power,
with elapsed-time compensation for variable GUI refresh intervals. Sample peak
remains immediate for each window, without a peak-hold feature. Silence decays
the RMS average; Stop, reconnect and stale-data expiry reset its history.
The GUI samples recent windows rather than measuring every audio block, so peaks
between snapshots can be missed. These are audio meters, not RF S-meters, LUFS,
true-peak measurements or calibrated receiver signal-strength readings.

## DSP controls

### Desktop layout

The desktop follows the Stitch "QRMForge Desktop - Restyled Main Window" reference:
blue-black surfaces, thin slate borders, a cyan primary action and compact rotary
controls. The header carries the project icon, audio status and About. A separate
action ribbon groups recording, bypass, mute, Stop audio and Reconnect; the routing
panel places explicit input/output selectors above observed route descriptions.
Gain, bandwidth, CW and NR controls use cyan, manual notch uses magenta and the
noise-blanker threshold uses amber. Active switches retain clear state colours;
hover, pressed, disabled and keyboard-focus states remain distinguishable.
Spectra, waterfalls, measured peak values and filter/notch overlays remain driven
by the existing telemetry. The restyle does not change DSP, defaults or routing.

The About button with a vector-drawn circled information icon in the top-right header opens a dialog showing the application version, the GPL-3.0-only project license, a note about separate third-party licenses and a clickable source repository link (`https://github.com/logocomune/qrmforge/`). The repository address appears once, as a clickable link that opens in the system browser. Close, Escape or Enter ends the modal session and hides the dialog; About can then reopen it.

The workstation starts at 1280 × 940 pixels and supports windows from 920 × 900 to 1800 × 1400. At widths of 1120 pixels or more, the input and output spectrum/waterfall panels appear side by side; narrower windows stack them vertically. Both monitors use cyan accents and retain explicit RECEIVER / PRE-DSP and MONITOR / POST-DSP headings. Short spectrum panels show fewer dB labels to keep the scale readable; the grid and measured values are unchanged.

The top bar keeps REC, recording status, BYPASS, MUTE, Stop audio and Reconnect visible. Device selections and Apply audio occupy a separate routing panel; the actual-route labels still report observed connections. The audio status and bottom routing message describe connection/processing health independently of effect settings.

The processing banner reports BYPASS / RAW INPUT, DSP PROCESSING ENABLED or OUTPUT MUTED, with mute taking precedence. This banner describes the requested processing settings, not whether audio is connected or arriving. Effect switches retain their settings during bypass. Gain, filter/manual-CW and noise controls are grouped below the monitors. The 12/24 dB/oct buttons beneath LOW CUT and HIGH CUT change the existing independent slopes; knob double-click remains available. NR/backend restrictions remain available in the NR selector tooltip, with unavailable modes disabled.

### Parameters

- BYPASS copies input to output without filtering, gain, attenuation or clipping. MUTE still produces silence.
- PRE-GAIN and OUTPUT GAIN control the processing chain. ATT applies -10 dB together with pre-gain, before the blanker and filters.
- AGC toggles automatic output level control after OUTPUT GAIN. It starts off; enable it while processing to level signal changes and reduce output overloads.
- LOW CUT and HIGH CUT have independent 12/24 dB/octave slopes. Click the slope button below each knob or double-click the knob body to change slope; drag changes frequency. Double-click the numeric value to edit it.
- NOISE BLANKER and NB THRESHOLD suppress impulsive interference.
- NOTCH enables the filter at the selected notch frequency.
- CW APF enables manual CW frequency enhancement. There is no automatic CW tracking.
- Noise reduction modes are OFF, NLMS, RNNoise, Wiener, SpeexDSP and DeepFilterNet. Missing or incompatible backends are disabled in the menu. See the backend table below.
- Band presets are AM 100-4500 Hz, FM 300-3000 Hz, SSB 300-2700 Hz, SSB DX 400-2000 Hz and CW 700 Hz / 250 Hz. Presets retain the chosen cut slopes. Manual edits clear the preset selection.

Startup defaults: BYPASS on, MUTE off, AGC off, NR OFF, low cut 50 Hz, high cut 4500 Hz, both slopes 12 dB/octave, pre-gain 0 dB and output gain 3 dB. The configured output gain is inactive while bypassed.

The GUI remains editable when audio is stopped. Effective values are normalized to the actual processing sample rate. The backend follows the graph's rate; it does not force a system-wide rate. A rate change temporarily outputs silence while DSP and backend state are prepared on the control thread. An unsupported mode becomes OFF; restore it manually when available again. Buffer processing supports variable block sizes without allocating sample buffers in the callback.

Processing order: input → pre-gain/ATT → noise blanker → HPF → LPF → manual notch → selected noise reduction → CW APF → output gain → optional AGC → hard limiter (±1) → mute → output. Disabled optional stages are skipped. HPF/LPF remain Butterworth IIR filters with independent 12/24 dB/octave slopes. There is no auto-notch. APF disables voice-only noise reduction modes; compatible NR modes run before APF. Pre-gain also affects the blanker's absolute minimum detection floor. Bypass preserves raw input, with mute still applied; input telemetry remains before all processing.

### Automatic gain control

Press **AGC** to enable or disable the standalone output AGC. It works with voice and CW independently of the selected NR mode. Its peak detector has immediate attack and 500 ms decay; gain moves toward a 0.5 peak target (approximately -6 dBFS) with a 5 ms attenuation time constant and 500 ms recovery time constant. Boost is capped at 4x (approximately +12 dB). Gain holds when the detector falls below 0.001 (-60 dBFS), limiting amplification during silence. These are time constants, not guaranteed settling times.

A sample-level gain cap reserves peaks below 0.95 (approximately -0.45 dBFS), including sudden transients. This has no look-ahead delay and is not a soft limiter; rapid gain reduction can still be audible. The existing ±1 hard limiter remains the final safety bound. AGC cannot repair distortion already present at the input or introduced inside an NR backend.

OUTPUT GAIN remains the multiplier before AGC. With AGC on, automatic leveling can counteract manual gain changes; use the final system/headphone volume for listening loudness. With AGC off, OUTPUT GAIN behaves as before, including hard clipping if its output exceeds ±1. Start near 0 dB rather than maximum.

Disabling AGC, bypassing or muting resets its detector and gain. Re-enabling starts from unity gain; sample-rate changes recreate it with the correct timing. Bypass skips AGC entirely, and mute always silences output. Recording includes the actual AGC-processed output. Processing uses fixed scalar state without allocation or additional buffering.

## Noise reduction backends

Existing mode IDs remain OFF=0, NLMS=1 and RNNoise=2. New modes are appended as Wiener=3, SpeexDSP=4 and DeepFilterNet=5. Select one mode using NOISE REDUCTION; use NR STRENGTH to adjust the selected algorithm.

| Mode | Availability | NR STRENGTH | Buffering |
| --- | --- | --- | --- |
| OFF | Always | Unused | No NR buffering |
| NLMS | Always | Existing adaptive prediction level | No added NR frame |
| RNNoise | Bundled model, 48 kHz | Fixed model, knob unused | 480-sample frame adapter |
| Wiener | Built in, no extra library | Suppression amount, 0.1-10 | 512-sample FFT, 50% overlap |
| SpeexDSP | Native runtime, integer rates 8-48 kHz | Maximum suppression approximately strength x 3 dB | 20 ms frame adapter |
| DeepFilterNet | Bundled official v0.5.6 backend/model, x86-64 Linux, 48 kHz | Attenuation limit strength x 3 dB | Three 480-sample transport frames plus model delay |

Wiener estimates a per-frequency noise floor and applies smoothed Wiener gains using overlap-add reconstruction. It does not recognize speech or CW. Stationary useful signals can also be attenuated, particularly at strong settings; compare with bypass and NLMS on real receiver recordings. Its identity-gain reconstruction delay is 512 samples; complete application latency also includes other filters and PipeWire buffering.

SpeexDSP enables denoising and leaves automatic gain control and VAD disabled. It converts framed audio to signed 16-bit PCM for the native preprocessing API. Builds find `libspeexdsp.so.1` without requiring development headers; the ABI reference is included under `third_party/speexdsp/`. Disable it explicitly with `-DQRMFORGE_ENABLE_SPEEXDSP=OFF`; a missing runtime also produces a build warning and a disabled menu item.

All three voice backends (RNNoise, SpeexDSP and DeepFilterNet) are disabled while CW APF is enabled, both in the GUI and DSP validation. OFF, NLMS and Wiener remain available. Bypass still copies the original samples bit-for-bit and mute still takes priority. Switching framed modes clears their buffered output.

### DeepFilterNet backend

On x86-64 Linux, the official v0.5.6 native backend with its embedded DFN3 model is bundled and enabled by default. No Rust/Python toolchain, model download or user-supplied path is required. Disable it explicitly with `-DQRMFORGE_ENABLE_DEEPFILTER=OFF`. Other architectures do not load this x86-64 binary.

Inference uses bounded queues and a private local helper process. The audio callback does not wait for model inference. Missed transport frames produce silence and increment the NR dropped-frame counter. Mode changes reject packets from previous generations and reset the native context on the worker. CPU, startup time and complete latency depend on the target machine.

The upstream native backend exposes a LADSPA interface, used internally by the helper only. Nothing is installed into the system LADSPA directories or PipeWire configuration. The helper does not activate upstream's optional D-Bus control server. Process isolation is necessary because the upstream run function can wait internally and its processing thread has no shutdown protocol; QRMForge terminates and reaps its own helper when releasing the context.

DeepFilter teardown cancels pending frame I/O and restart handshakes before
joining its inference thread. The bridge checks cancellation at most every
25 ms while polling a pipe, then terminates and reaps the helper if needed.
On first selection at 48 kHz, DeepFilter loads on its inference worker.
The status line reports loading or failure. During loading, the other DSP
stages continue with noise reduction off. On success, DeepFilter activates
automatically if it is still selected and CW APF is off. Switching modes
during loading takes effect immediately.

Installed builds look for the DeepFilterNet library and helper in the
CMake directories `CMAKE_INSTALL_LIBDIR` and `CMAKE_INSTALL_LIBEXECDIR`.
Relative install directories are resolved from the installed executable
using `CMAKE_INSTALL_BINDIR`, so prefix overrides also work with layouts
such as `lib64`. Absolute install destinations remain fixed. The portable
layout and build-tree paths are used as fallbacks.

The release binary is pinned by SHA-256 at configure time. Portable packaging verifies it again and includes the helper, backend and required libraries. At runtime they are resolved relative to the executable; installed packages do not need the source checkout. Code and official model weights are dual-licensed MIT/Apache-2.0, as [explicitly clarified by the maintainer](https://github.com/Rikorose/DeepFilterNet/commit/7b14b89); QRMForge retains the MIT notice and upstream attribution.

## Recording

Start audio and wait for processing to begin. Press **REC**, choose a new filename, and save. The extension is `.wv`. **Stop REC** requests finalization; **Saving...** remains visible until buffered audio and the final sample-count header are written. The row shows duration and filename. No existing file or symlink is overwritten.

Recording captures the actual mono post-DSP output: bypass records bypassed samples and mute records silence. The format is WavPack lossless, IEEE float32, Fast mode. There is no conversion to integer PCM or lossy/hybrid mode. The encoded stream is bit-exact when decoded, including float bit patterns; the chosen DSP effects have already modified the audio before this tap.

Compression and file access run on a dedicated worker. A fixed 262,144-sample ring separates it from the callback (about 5.46 seconds at 48 kHz). If it fills, recording stops with an error rather than silently dropping samples or blocking playback. Sample-rate changes, invalid recording buffers and encoder/disk errors also stop recording. Already accepted samples are finalized where possible; inspect the displayed error before relying on a partial file. Empty captures are removed only when the filename still identifies the empty file created by the recorder.

Stop audio, changing routes, reconnect and orderly application close finalize the current recording. Abrupt process termination or storage failure cannot guarantee finalization. The recorder saves DSP samples actually delivered to it; upstream transport dropouts are outside the encoder's lossless guarantee. Compression ratio varies with the audio, and arbitrary float noise may compress poorly.

## Monitoring

RECEIVER / PRE-DSP shows the original input before all processing, including while muted or bypassed. MONITOR / POST-DSP shows actual output. Both include spectrum, waterfall, peak level, cutoff markers and the bandwidth response. FFT and rendering run on the GUI thread. A bounded local telemetry ring rejects samples overwritten during reads.

When NOTCH is enabled, both panels show a pink vertical marker through the spectrum and waterfall, labeled with its frequency in Hz. The marker follows the NOTCH frequency knob, remains visible when muted and fades with a `(bypass)` label while bypassed. It disappears when NOTCH is disabled; frequencies outside the displayed band are not drawn. This is a frequency marker, not a notch response curve.

The status shows negotiated sample rate, most recent processing block size, missing/invalid block diagnostics and stalled audio updates. End-to-end latency and underruns are not comprehensively measured by this version; rate/quantum display is not a latency guarantee.

## Troubleshooting

- **PipeWire unavailable**: run in the same desktop user's session and check its runtime socket. Press Reconnect after restoring the service.
- **Input/output ports unavailable**: start source playback, wait for ports and apply again.
- **Cannot detach direct output**: the selected stream has a direct sink link the client cannot remove. Inspect its routing and permissions.
- **Routing timeout**: a requested link did not become established. Inspect `pw-link -l` and `wpctl status -n`; the application stops the partial route and attempts restoration.
- **No audio updates**: verify incoming RTP/application playback. A running graph alone does not establish that non-silent audio is arriving.
- **Audio links changed/device disappeared**: inspect the endpoints and apply explicitly again.

## Project identity

The project and window title are QRMForge. The executable and desktop entry are `qrmforge`; processing nodes use the `qrmforge_` prefix. Run `make run` from this directory. CMake options use `QRMFORGE_` names. Portable artifacts are named `QRMForge.AppDir`, `QRMForge-linux.tar.gz` and, when explicitly wrapped, `QRMForge.AppImage`.

## Packaging and licenses

With GUI builds, `make test` checks the embedded icon and verifies that the native X11 window publishes it. The native test requires a reachable X11 display, `xprop`, and `xwininfo`. It allows up to five seconds for the window and its icon property to appear. A separate polling regression test runs without an X11 display.

Run `make clean` to invoke CMake's clean target in `BUILD_DIR` (default `build`). This removes compiled artifacts and preserves the CMake configuration, downloaded tools, and packaged releases. If the directory has no CMake cache, the command succeeds without doing anything.

Run `make appimage` to compile and generate `dist/QRMForge.AppImage`. `GUI=OFF` is rejected. `BUILD_DIR`, `JOBS`, `RNNOISE` and `CMAKE_FLAGS` retain their normal meanings; `PACKAGE_DIR` selects the output directory. The automatic wrapper downloads official tool/runtime assets over HTTPS using `curl`, verifies the SHA-256 digests pinned in `packaging/build_appimage.py`, and caches them under `BUILD_DIR/appimage-tools`. A corrupt cache or changed upstream asset fails closed. To update tooling, review upstream provenance and update the pinned digests deliberately. Builds use temporary staging and replace only the final AppImage after success; existing portable AppDirs are untouched.

### AppImage releases

The checkout action is pinned to the official `actions/checkout` v6.0.2 commit (`de0fac2e4500dabe0009e67214ff5f5447ce83dd`), whose manifest uses Node.js 24. Keep `persist-credentials: false` when updating this pin.

Packaging requires `libpipewire-0.3-modules` and `libspa-0.2-modules` on Ubuntu 24.04. Development headers alone do not provide the native protocol/client-node modules and SPA support/audio-conversion plugins copied into the AppDir. The release workflow installs both runtime packages explicitly and checks the required module files before compiling.

The workflow in `.github/workflows/release.yml` runs on pushed tags matching `v*`, using Ubuntu 24.04 on x86-64. It installs build dependencies, runs `make test` and the packaging unit tests, then runs `make appimage`. The release contains `QRMForge.AppImage`, `QRMForge.AppImage.tar.gz` and `SHA256SUMS`. The compressed archive contains the executable AppImage. A job-scoped `contents: write` permission allows the GitHub CLI to create the release and upload its assets with the repository token. Re-running a release replaces assets with the same names. Push a release tag only after reviewing the corresponding source and licensing obligations below.

```sh
git tag v1.0.0
git push origin v1.0.0
```

Headless desktop/audio tests may be skipped according to their existing skip conditions; CI does not prove live audio or compatibility with older distributions.

`packaging/build_portable.py` builds an AppDir plus a compressed archive from a trusted locally compiled executable. It bundles non-glibc linked libraries, JUCE's required desktop libraries, PipeWire client modules and the required SPA libraries. A local client configuration exists only inside the package and does not change the host server configuration. Packaging inspects dependencies with `ldd`; do not use the script on an untrusted executable.

Launch `AppRun` after extracting the archive. It sets library/module paths for that process and does not install files. The package requires a compatible glibc and host PipeWire/session manager. Packaging currently targets the build host's architecture; clean-distribution compatibility must be verified before releases. AppImage wrapping requires an explicitly selected local tool and matching digest; the portable archive does not require FUSE.

The corrected local desktop AppImage is `dist-appimage-iconfix-20261115/QRMForge.AppImage`. It is executable as built. Close any earlier QRMForge instance before opening the new package. Launch it directly on a desktop with working FUSE, or use extraction mode when FUSE is unavailable:

```sh
APPIMAGE_EXTRACT_AND_RUN=1 ./dist-appimage-iconfix-20261115/QRMForge.AppImage
```

The application embeds `packaging/qrmforge.png` and assigns it to both the JUCE title bar and the native window peer. The desktop entry declares `StartupWMClass=QRMForge`, matching JUCE's X11 window class. The packaging script copies the PNG to the AppDir root and `.DirIcon` automatically. This covers the application window/taskbar separately from the AppImage file icon; desktop shells decide how these hints are displayed, and launch-menu integration still requires a desktop entry.

On GNOME, a generic gear in the running-app dock can indicate a missing desktop application association. The `.desktop` file inside an AppImage is not automatically registered in the user's application database. The local `dist-appimage-iconfix-20261115/qrmforge-launcher.desktop` uses absolute paths to this AppImage and its packaged icon, with `StartupWMClass=QRMForge`. Install that launcher as `${XDG_DATA_HOME:-$HOME/.local/share}/applications/qrmforge.desktop`, close the old instance and launch QRMForge from the applications menu. Regenerate the launcher's Exec/Icon paths if the package is moved; keep its AppDir icon available. The generic packaging desktop entry uses `Exec=qrmforge` and must not be installed unchanged unless the binary is on PATH. The current sandbox cannot write to the user's applications directory or verify the GNOME dock visually.

When changing the icon, update the SVG source and regenerate the committed PNG with `convert -background none packaging/qrmforge.svg -resize 256x256 -depth 8 packaging/qrmforge.png`, then rebuild and repackage. ImageMagick is needed only to regenerate the PNG, not to build or run the application. Supply the separately verified runtime through `APPIMAGETOOL_RUNTIME_FILE` when wrapping; see [tool provenance and validation limits](validation.md#local-appimage-build-2026-11-15).

RNNoise 0.2 is independently replicated with its model, pinned archive and BSD-style notices under `third_party/`. GUI and DSP sources are maintained within this project. JUCE and the bundled system libraries retain their upstream licensing terms. The locally supplied JUCE distribution includes GPL terms; verify applicable JUCE licensing before redistribution. The portable package includes available Debian copyright notices and GPL-3 text. No public release or Store upload is performed by the build.

Update project code and RNNoise deliberately within this project, with characterization/regression tests, local security review and a changelog entry.

### License review and recommendation

The documentation review checked `CMakeLists.txt`, the configured build cache, DSP implementations, bundled license files, installed distribution copyright records and the current AppDir notices. Direct dependencies are JUCE, PipeWire, RNNoise, SpeexDSP, DeepFilterNet and WavPack, plus the LADSPA compatibility header. The README provides brief credits and explains the implemented filters; hashes, backend/model origin and update procedures remain in [third-party provenance](../third_party/README.md).

The configured JUCE is **7.0.5**, from `/usr/lib/x86_64-linux-gnu/cmake/JUCE-7.0.5`. Its installed `juce_gui_basics` header offers GPLv3 or alternative JUCE 7 terms, and the distribution copyright record identifies the GUI code as GPL-3. This review concerns that version, not licensing terms for later JUCE releases. See [JUCE 7.0.5 licensing](https://github.com/juce-framework/JUCE/blob/7.0.5/LICENSE.md). Acknowledging JUCE does not remove its licensing conditions.

RNNoise's bundled COPYING, SpeexDSP's upstream header/runtime notices and WavPack's bundled LICENSE contain BSD-style copyright, redistribution and disclaimer requirements. DeepFilterNet's retained MIT license requires preservation of its copyright and permission notice. Its maintainer [explicitly included model weights in the dual MIT/Apache-2.0 grant](https://github.com/Rikorose/DeepFilterNet/commit/7b14b89); the MIT option is used here. The LADSPA header separately retains LGPL-2.1-or-later terms. PipeWire's [core license](https://github.com/PipeWire/pipewire/blob/1.0.5/COPYING) is MIT/Expat, but its modules and transitive dependencies must be considered individually.

**Project license: GPL-3.0-only**, adopted for QRMForge's own code. The full text is in [LICENSE](../LICENSE); project source/build files carry matching SPDX identifiers and copyright notices. This applies to project-owned code, documentation and artwork. Third-party code and models retain their own licenses. Licenses in `third_party/` remain applicable to those components. Reconsider the complete licensing basis before using a different JUCE license or release.

Before public binary distribution, confirm ownership or permission for all project code and preserve upstream license/copyright texts. For a GPL distribution, provide the corresponding source and necessary build materials through a GPL-compliant distribution method; copying only the GPL text into an AppImage is insufficient. Follow applicable LGPL source/relinking requirements for bundled LGPL components as well. The [GPLv3 text](https://www.gnu.org/licenses/gpl-3.0.en.html) and [GNU compatibility guidance](https://www.gnu.org/licenses/license-list.html) explain these obligations.

The portable packager includes dependency notices, distribution copyright files,
common license texts and the project license as
`usr/share/licenses/QRMForge-LICENSE`. CMake installs the project license under
`share/licenses/qrmforge/LICENSE`. Distribution notices may reference host paths
such as `/usr/share/common-licenses/LGPL-2.1`; portable copies are retained in
`usr/share/licenses/common`. DeepFilterNet crate notices now have the conservative
inventory below. Exact binary composition, AppImage runtime coverage and applicable
source/relinking materials still require release verification. No vulnerability
scan or public release is implied by this license review.


### Offline third-party notices

About includes a read-only scrolling notice viewer titled "Third-party software & licenses",
with a dark background, padded text, cyan focus border and muted scrollbar.
The notice viewer uses the modal panel's content width, with equal horizontal
margins and a separate heading above it. It contains credits for JUCE, PipeWire,
RNNoise, SpeexDSP, DeepFilterNet, WavPack and the LADSPA header, followed by project
and direct dependency license texts. CMake embeds the notices into the executable,
including available distribution notices for the directly linked libraries and
common license texts. Notice headings use project-relative or distribution labels,
without exposing the build user's home directory. No browser or network connection is needed to read them.

The portable packager copies all `/usr/share/common-licenses` texts into
`usr/share/licenses/common`, retains distribution copyright notices and records
resolved distribution package versions in `PACKAGE-VERSIONS.txt`. Absolute common
license references in distribution notices correspond to this bundled directory.
The AppImage release also includes `QRMForge-source.tar.gz` from the exact tagged
project revision; it includes project build materials and the bundled RNNoise
source archive, but is not a complete source archive for all bundled system
libraries, the DeepFilterNet binary or the AppImage runtime.

DeepFilterNet dependency notices are now retained in a conservative inventory as
described below. Remaining release verification: establish exact prebuilt-backend
composition through a publisher SBOM or reproducible build, review all components
in the pinned AppImage runtime, and supply the exact corresponding sources/build
materials required by the licenses of bundled GPL/LGPL components. A project
source archive, credits viewer or successful build does not establish completion
of that review.

The [AppImage runtime inventory](../third_party/appimage-runtime/README.md) records
the reviewed ELF, component notices, the upstream FUSE patch and remaining
source and relinking requirements. The portable packager copies this directory
under `usr/share/licenses/dependencies/appimage-runtime/`.

### DeepFilterNet dependency notice maintenance

The inventory hashes the exact bytes of `third_party/deepfilter/THIRD-PARTY-NOTICES.txt`. Keep Git line-ending conversion disabled for this file through `.gitattributes`. After regenerating the bundle and inventory, run `python3 -m unittest discover -s tests -p 'packaging_license_tests.py'` in a fresh checkout to verify the committed bytes.

The 2026-11-18 review retains `third_party/deepfilter/INVENTORY.json`, the upstream
`Cargo.lock` and `THIRD-PARTY-NOTICES.txt`. The inventory covers 238 registry
packages; 50 contain versioned source-path evidence in the pinned backend ELF.
Other entries are a conservative lockfile closure, including optional D-Bus,
build/development and non-Linux dependencies. They are not all claimed to ship in
the Linux backend. Registry archive digests match the lockfile at upstream
revision `978576aa8400552a4ce9730838c635aa30db5e61`.

The bundle preserves available full license/copyright/NOTICE files, including
vendored subcomponents, plus Rust toolchain notices from the revision visible in
the binary. RealFFT and crunchy published MIT declarations without separate
license texts at the selected versions; later upstream texts are supplied with
pinned source URLs. Windows import-crate texts come from the companion winapi
project archive. The inventory explicitly identifies these supplemental sources.
Do not describe them as original files from the older crate archives.

CMake embeds the bundle in the offline About viewer. Existing portable and CMake
installation rules retain the inventory, lockfile and bundle under the dependency
license directory; no Rust installation or additional application download is
needed. These are attribution/provenance materials, not a dependency upgrade or
an archive of corresponding sources.

When replacing the backend:

1. Pin the ELF digest and upstream source revision. Compare registry source paths
   in the ELF with the crate versions in the selected lockfile; absence of a path
   does not prove absence of code. Prefer a publisher SBOM or reproducible build.
2. Select roots from the backend manifests/features. `selection_roots` records the
   current selection; recursively follow lockfile dependency edges, retaining
   ambiguous version matches conservatively. Do not infer feature/target selection
   from Cargo.lock alone. The original lockfile includes unrelated workspace
   packages; it is not the binary inventory.
3. Obtain public crate archives from each recorded `archive_url`, verify their
   SHA-256 against the lockfile before reading them, and retain license, copyright
   and NOTICE files, including nested vendor directories. Do not execute build
   scripts or install crates to collect notices. Review additional terms rather
   than applying DeepFilterNet's MIT choice to every dependency.
4. Record each archive's license declaration, authors, source revision when
   available, notice paths and hashes. Resolve missing texts against upstream,
   record exact revisions and distinguish later supplements. Retain appropriate
   Rust runtime notices and any additional linked native-component notices.
5. Refresh the inventory, bundle and their SHA-256 values together. Run
   `python3 -m unittest discover -s tests -p packaging_license_tests.py`, rebuild,
   and check the actual package's `usr/share/licenses/dependencies/deepfilter/`
   files and offline About text. Preserve the separate AppImage/system-library
   release checks above. Ordinary builds and packaging use only committed files.


### About dialog design

The About overlay follows the Stitch "QRMForge Desktop - About Modal Dialog"
reference: dimmed workstation, centred dark panel with cyan outline, compact
header and top-right close action, centred project branding, version badge,
license summary and repository link, followed by a labelled scrolling notice
panel and a separate Close footer. The panel is 720 by 610 pixels where space
allows and remains within the application window when resized. Close, X, Escape
and Enter dismiss it without stopping audio. The application uses its existing
icon and actual license/version data rather than placeholder reference content.
