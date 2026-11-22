#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 Alessandro Lucaferro

"""Exercise the isolated native worker with seeded arbitrary float packet bytes."""
import math
import random
import struct
import subprocess
import sys

worker, backend = sys.argv[1:]
assert subprocess.run([worker], capture_output=True).returncode == 2
assert subprocess.run([worker, "/nonexistent/qrmforge-backend.so"], capture_output=True).returncode != 0
randomizer = random.Random(907302)
frames = 200
payload = randomizer.randbytes(frames * 481 * 4) + b"truncated packet"
result = subprocess.run([worker, backend], input=payload, capture_output=True, timeout=30)
assert result.returncode == 0, result.stderr.decode(errors="replace")
assert struct.unpack("=III", result.stdout[:12]) == (0x51444633, 48000, 480)
assert len(result.stdout) == 12 + frames * 480 * 4
samples = struct.unpack("=" + str(frames * 480) + "f", result.stdout[12:])
assert all(math.isfinite(value) and abs(value) <= 1 for value in samples)
print(f"Native worker packet fuzz: {frames} frames; truncated input handled cleanly")
