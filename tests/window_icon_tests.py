# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 Alessandro Lucaferro

"""Verify the native window advertises an embedded taskbar icon on X11."""
import os
import re
import shutil
import subprocess
import sys
import time

ICON_SIZE = 256
ICON_CARDINALS = ICON_SIZE * ICON_SIZE + 2
# xprop may count 32-bit X11 values as native longs on 64-bit hosts.
ICON_READ_BYTES = str(8 * (ICON_CARDINALS + 1))


def output(*arguments):
    return subprocess.check_output(arguments, text=True, stderr=subprocess.DEVNULL)


def main():
    if not os.environ.get("DISPLAY") or not all(shutil.which(tool) for tool in ("xprop", "xwininfo")):
        return 77
    try:
        output("xwininfo", "-root", "-tree")
    except subprocess.CalledProcessError:
        return 77
    process = subprocess.Popen(sys.argv[1:], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        deadline = time.monotonic() + 5
        missing = "No native window found for the launched process"
        while time.monotonic() < deadline:
            for window in re.findall(r'(0x[0-9a-f]+) "QRMForge"', output("xwininfo", "-root", "-tree")):
                owner = output("xprop", "-id", window, "_NET_WM_PID")
                if owner.strip().endswith("= " + str(process.pid)):
                    icon = output("xprop", "-id", window, "-len", ICON_READ_BYTES,
                                  "-f", "_NET_WM_ICON", "32c", "_NET_WM_ICON")
                    if "= " not in icon:
                        missing = "Native window has no _NET_WM_ICON"
                        continue
                    pixels = [int(value) for value in re.findall(r'\d+', icon.split("=", 1)[1])]
                    assert pixels[:2] == [ICON_SIZE, ICON_SIZE], "Unexpected native icon dimensions"
                    assert len(pixels) == ICON_CARDINALS, f"Unexpected native icon length: {len(pixels)} CARDINALs"
                    assert any(value >> 24 for value in pixels[2:]), "Native icon is transparent"
                    return 0
            time.sleep(0.1)
        raise AssertionError(missing)
    finally:
        process.terminate()
        process.wait(timeout=5)


if __name__ == "__main__":
    sys.exit(main())
