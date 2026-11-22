# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 Alessandro Lucaferro
"""Build an AppImage with checksum-pinned official local tools."""

import argparse
import hashlib
import os
from pathlib import Path
import platform
import subprocess
import tempfile

TOOLS = {
    'appimagetool': (
        'https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage',
        'e8c1169c4af66c3184d7ee110713d4be04befe671fa358e2c1591ee840553b03',
    ),
    'runtime': (
        'https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-x86_64',
        '617aa77314d57c8b8e4fae9c3f0b35af7b30d5529cb69e98f64f92510ed98a46',
    ),
}


def verified_tool(cache, name):
    url, digest = TOOLS[name]
    cache.mkdir(parents=True, exist_ok=True)
    target = cache / (name + '-' + digest)
    if target.is_file():
        if hashlib.sha256(target.read_bytes()).hexdigest() != digest:
            raise RuntimeError(f'Cached {name} checksum mismatch: {target}; remove it and retry')
    else:
        with tempfile.TemporaryDirectory(dir=cache) as temporary:
            downloaded = Path(temporary) / name
            subprocess.run([
                'curl', '--fail', '--location', '--proto', '=https',
                '--proto-redir', '=https', '--connect-timeout', '30',
                '--max-time', '300', '--output', str(downloaded), url,
            ], check=True)
            if hashlib.sha256(downloaded.read_bytes()).hexdigest() != digest:
                raise RuntimeError(f'Downloaded {name} checksum mismatch; refusing execution')
            downloaded.replace(target)
    target.chmod(0o755)
    return target.resolve()


def build(arguments):
    if platform.machine().lower() not in ('x86_64', 'amd64'):
        raise RuntimeError('Automatic AppImage packaging currently supports x86-64 Linux only')
    cache = Path(arguments.cache).resolve()
    tool = verified_tool(cache, 'appimagetool')
    runtime = verified_tool(cache, 'runtime')
    destination = Path(arguments.output).resolve()
    destination.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    environment['APPIMAGETOOL_RUNTIME_FILE'] = str(runtime)
    packager = Path(__file__).with_name('build_portable.py')
    with tempfile.TemporaryDirectory(prefix='.appimage-', dir=destination) as temporary:
        subprocess.run([
            'python3', str(packager), '--binary', arguments.binary,
            '--output', temporary, '--appimagetool', str(tool),
            '--tool-sha256', TOOLS['appimagetool'][1],
        ], check=True, env=environment)
        artifact = Path(temporary) / 'QRMForge.AppImage'
        artifact.replace(destination / artifact.name)
    print(f'AppImage: {destination / "QRMForge.AppImage"}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', required=True)
    parser.add_argument('--output', default='dist')
    parser.add_argument('--cache', default='build/appimage-tools')
    build(parser.parse_args())
