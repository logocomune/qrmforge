#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 Alessandro Lucaferro

"""Bundle a locally built executable; packaging never downloads or installs tools."""
import argparse
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess

SYSTEM_LIBRARIES = re.compile(r"^(libc\.|libm\.|libdl\.|libpthread\.|librt\.|libutil\.|libresolv\.|libnss_|ld-linux)")
LAUNCHER = """#!/bin/sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
export LD_LIBRARY_PATH="$HERE/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export PIPEWIRE_MODULE_DIR="$HERE/usr/lib/pipewire-0.3"
export SPA_PLUGIN_DIR="$HERE/usr/lib/spa-0.2"
export PIPEWIRE_CONFIG_DIR="$HERE/usr/share/pipewire"
export PIPEWIRE_CONFIG_NAME=client.conf
exec "$HERE/usr/bin/qrmforge" "$@"
"""
CLIENT_CONFIG = """
context.properties = { mem.warn-mlock = false }
context.spa-libs = {
    audio.convert.* = audioconvert/libspa-audioconvert
    support.* = support/libspa-support
}
context.modules = [
    { name = libpipewire-module-protocol-native }
    { name = libpipewire-module-client-node }
]
"""


def output(arguments):
    return subprocess.check_output(arguments, text=True)


def dependencies(path):
    result = output(["ldd", str(path)])
    if "not found" in result:
        raise RuntimeError(f"Unresolved dependencies for {path}: {result}")
    return [Path(match) for match in re.findall(r"=> (/\S+)", result)]


def copy_common_licenses(source, destination):
    shutil.copytree(source, destination / "common", symlinks=False)


def package(arguments):
    root = Path(__file__).resolve().parent.parent
    executable = Path(arguments.binary).resolve()
    directory = Path(arguments.output).resolve()
    directory.mkdir(parents=True, exist_ok=True)
    appdir = directory / "QRMForge.AppDir"
    if appdir.exists():
        raise RuntimeError(f"Output already exists: {appdir}; select a fresh output directory")
    binary = appdir / "usr/bin"
    libraries = appdir / "usr/lib"
    licenses = appdir / "usr/share/licenses"
    binary.mkdir(parents=True)
    libraries.mkdir(parents=True)
    licenses.mkdir(parents=True)
    shutil.copy2(root / "LICENSE", licenses / "QRMForge-LICENSE")
    shutil.copy2(executable, binary / executable.name)
    shutil.copy2(root / "packaging/qrmforge.desktop", appdir)
    shutil.copy2(root / "packaging/qrmforge.svg", appdir / "qrmforge.svg")
    shutil.copy2(root / "packaging/qrmforge.png", appdir / "qrmforge.png")
    shutil.copy2(root / "packaging/qrmforge.png", appdir / ".DirIcon")
    shutil.copytree(root / "third_party", licenses / "dependencies", ignore=shutil.ignore_patterns("*.tar.gz", "*.so", "*.so.*"))
    copy_common_licenses(Path("/usr/share/common-licenses"), licenses)
    sources = [executable]
    resources = executable.parents[2] / "qrmforge-deepfilter-resource.txt"
    if resources.exists():
        worker_name, backend_name, digest = resources.read_text().splitlines()
        worker, backend = Path(worker_name), Path(backend_name)
        if hashlib.sha256(backend.read_bytes()).hexdigest() != digest:
            raise RuntimeError("Bundled DeepFilter backend checksum mismatch")
        worker_target = appdir / "usr/libexec/qrmforge/qrmforge-deepfilter-worker"
        backend_target = libraries / "qrmforge/libdeep_filter_ladspa.so"
        worker_target.parent.mkdir(parents=True)
        backend_target.parent.mkdir(parents=True)
        shutil.copy2(worker,worker_target)
        shutil.copy2(backend,backend_target)
        sources.extend([worker,backend])
    module_directory = Path(output(["pkg-config", "--variable=moduledir", "libpipewire-0.3"]).strip())
    spa_directory = Path(output(["pkg-config", "--variable=plugindir", "libspa-0.2"]).strip())
    for name in ("libpipewire-module-protocol-native.so", "libpipewire-module-client-node.so"):
        destination = libraries / "pipewire-0.3" / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(module_directory / name, destination)
        sources.append(module_directory / name)
    for name in ("support/libspa-support.so", "support/libspa-dbus.so", "audioconvert/libspa-audioconvert.so"):
        destination = libraries / "spa-0.2" / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(spa_directory / name, destination)
        sources.append(spa_directory / name)
    # JUCE opens these desktop libraries at runtime rather than linking them.
    cache = output(["ldconfig", "-p"])
    for name in ("libX11.so.6", "libXext.so.6", "libXinerama.so.1", "libXrandr.so.2", "libXcursor.so.1", "libXrender.so.1", "libXfixes.so.3", "libfontconfig.so.1"):
        match = re.search(r"\s"+re.escape(name)+r"\s[^\n]*=> (/\S+)", cache)
        if not match:
            raise RuntimeError(f"Missing desktop runtime library: {name}")
        path = Path(match.group(1))
        shutil.copy2(path, libraries / name)
        sources.append(path)
    pending = list(sources)
    bundled = set()
    while pending:
        path = pending.pop()
        for dependency in dependencies(path):
            if SYSTEM_LIBRARIES.match(dependency.name) or dependency.name in bundled:
                continue
            bundled.add(dependency.name)
            shutil.copy2(dependency, libraries / dependency.name)
            pending.append(dependency)
            sources.append(dependency)
    config = appdir / "usr/share/pipewire/client.conf"
    config.parent.mkdir(parents=True)
    config.write_text(CLIENT_CONFIG)
    launcher = appdir / "AppRun"
    launcher.write_text(LAUNCHER)
    launcher.chmod(0o755)
    packages = {"juce-modules-source", "juce-modules-source-data"}
    for source in sources:
        for path in (source, source.resolve()):
            result = subprocess.run(["dpkg-query", "-S", str(path)], capture_output=True, text=True)
            if result.returncode == 0:
                packages.add(result.stdout.split(": ", 1)[0])
                break
    for name in packages:
        copyright_file = Path("/usr/share/doc") / name.split(":", 1)[0] / "copyright"
        if copyright_file.exists():
            shutil.copy2(copyright_file, licenses / (name.replace(":", "-")+"-copyright"))
    (licenses / "PACKAGE-VERSIONS.txt").write_text(output([
        "dpkg-query", "-W", "-f=${binary:Package}\t${Version}\n", *sorted(packages)]))
    (licenses / "README.txt").write_text(
        "QRMForge project license: QRMForge-LICENSE\n"
        "Direct dependency credits and notices: dependencies/\n"
        "Distribution copyright notices: *-copyright\n"
        "Full common license texts: common/ (including GPL, LGPL and CC0)\n"
        "Package versions: PACKAGE-VERSIONS.txt\n"
        "Notices referring to /usr/share/common-licenses can be read in common/.\n"
        "Static dependencies of prebuilt backends and runtime require separate review.\n"
        "Source and build instructions: https://github.com/logocomune/qrmforge\n")
    manifest = []
    for path in sorted(appdir.rglob("*")):
        if path.is_file():
            manifest.append(f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.relative_to(appdir)}")
    (directory / "SHA256SUMS-AppDir").write_text("\n".join(manifest)+"\n")
    archive = shutil.make_archive(str(directory / "QRMForge-linux"), "gztar", directory, appdir.name)
    print(f"Portable directory: {appdir}\nArchive: {archive}")
    if arguments.appimagetool:
        tool = Path(arguments.appimagetool).resolve()
        if not arguments.tool_sha256 or hashlib.sha256(tool.read_bytes()).hexdigest() != arguments.tool_sha256:
            raise RuntimeError("An explicitly verified appimagetool SHA-256 is required")
        environment = os.environ.copy()
        environment["APPIMAGE_EXTRACT_AND_RUN"] = "1"
        subprocess.run([str(tool), "--no-appstream", str(appdir), str(directory / "QRMForge.AppImage")], check=True, env=environment)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--appimagetool")
    parser.add_argument("--tool-sha256")
    package(parser.parse_args())
