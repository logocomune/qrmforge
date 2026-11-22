# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 Alessandro Lucaferro
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory() as temporary:
    prefix = Path(temporary)
    source = Path(sys.argv[1])
    executable = prefix / "nested/bin/deep_shutdown_tests"
    helper = prefix / "helpers/qrmforge/qrmforge-deepfilter-worker"
    backend = prefix / "lib64/qrmforge/libdeep_filter_ladspa.so"
    for path in (executable, helper, backend):
        path.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, executable)
    shutil.copy2(source, helper)
    backend.touch()
    subprocess.run([executable, "--installed"], check=True,
                   env={**os.environ, "QRMFORGE_TEST_INSTALLED": "1"})
