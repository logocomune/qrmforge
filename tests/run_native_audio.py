#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 Alessandro Lucaferro

"""Run native audio checks against a temporary PipeWire server, never the desktop."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

CONFIG = """
context.properties = {
    core.daemon = true
    core.name = pipewire-0
    default.clock.rate = 48000
    default.clock.quantum = 128
    default.clock.min-quantum = 64
    default.clock.max-quantum = 2048
    mem.warn-mlock = false
}
context.spa-libs = { support.* = support/libspa-support }
context.modules = [
    { name = libpipewire-module-protocol-native }
    { name = libpipewire-module-client-node }
    { name = libpipewire-module-access }
    { name = libpipewire-module-spa-node-factory }
    { name = libpipewire-module-link-factory }
]
context.objects = [
    { factory = spa-node-factory args = {
        factory.name = support.node.driver
        node.name = Test-Driver
        node.group = pipewire.dummy
        priority.driver = 20000
    } }
]
"""


def main():
    if not shutil.which("pipewire"):
        print("SKIP: pipewire executable unavailable")
        return 77
    with tempfile.TemporaryDirectory(prefix="qrmforge-audio-test-") as directory:
        root = Path(directory)
        config = root / "server.conf"
        config.write_text(CONFIG)
        environment = os.environ.copy()
        environment.update(XDG_RUNTIME_DIR=directory, PIPEWIRE_RUNTIME_DIR=directory,
                           XDG_CONFIG_HOME=str(root / "config"), PIPEWIRE_REMOTE="pipewire-0")
        environment.pop("PIPEWIRE_CONFIG_DIR", None)
        environment.pop("PIPEWIRE_CONFIG_NAME", None)
        with (root / "server.log").open("w+") as log:
            server = subprocess.Popen(["pipewire", "-c", str(config)], env=environment,
                                      stdout=log, stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic() + 5
                while time.monotonic() < deadline and server.poll() is None:
                    if (root / "pipewire-0").exists():
                        return subprocess.run([sys.argv[1]], env=environment, timeout=60).returncode
                    time.sleep(0.02)
                log.seek(0)
                output = log.read()
                if "Operation not permitted" in output or "Permission denied" in output:
                    print("SKIP: sandbox denies isolated PipeWire socket/runtime access")
                    return 77
                print(output)
                return 1
            finally:
                server.terminate()
                try:
                    server.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    server.kill()
                    server.wait()


if __name__ == "__main__":
    sys.exit(main())
