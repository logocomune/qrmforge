# SPDX-License-Identifier: GPL-3.0-only
import importlib.util
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('portable', Path(__file__).resolve().parents[1] / 'packaging/build_portable.py')
portable = importlib.util.module_from_spec(spec)
spec.loader.exec_module(portable)


class LicenseTests(unittest.TestCase):
    def test_deepfilter_inventory_matches_backend_and_notice_bundle(self):
        root = Path(__file__).resolve().parents[1] / 'third_party/deepfilter'
        inventory = json.loads((root / 'INVENTORY.json').read_text())
        backend = root / 'libdeep_filter_ladspa-0.5.6-x86_64-linux-gnu.so'
        self.assertEqual(hashlib.sha256(backend.read_bytes()).hexdigest(), inventory['backend_sha256'])
        self.assertEqual(hashlib.sha256((root / 'Cargo.lock').read_bytes()).hexdigest(), inventory['lockfile_sha256'])
        notices = (root / 'THIRD-PARTY-NOTICES.txt').read_bytes()
        self.assertEqual(hashlib.sha256(notices).hexdigest(), inventory['notice_bundle_sha256'])
        observed = set(match.decode() for match in re.findall(
            rb'registry/src/[^/\s\x00]+/([a-zA-Z0-9_-]+-[0-9][a-zA-Z0-9.+_-]*)/',
            backend.read_bytes()))
        identities = set()
        recorded_observed = set()
        for package in inventory['packages']:
            identity = package['name'] + '-' + package['version']
            self.assertNotIn(identity, identities)
            identities.add(identity)
            if package['evidence'] == 'binary-path':
                recorded_observed.add(identity)
            self.assertTrue(package['license'], identity)
            self.assertTrue(package['notice_files'] or package.get('supplemental_notice_files'), identity)
            self.assertTrue(('===== ' + identity + ' =====').encode() in notices, identity)
        self.assertIn('rustfft-6.1.0', identities)
        self.assertIn('realfft-3.3.0', identities)
        self.assertTrue(observed, 'Backend contains versioned crate source paths')
        self.assertEqual(observed, recorded_observed)

    def test_deepfilter_notices_are_embedded_offline(self):
        root = Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory() as temporary:
            script = Path(temporary) / 'notices.cmake'
            script.write_text(
                f'set(PROJECT_SOURCE_DIR "{root.as_posix()}")\n'
                f'set(PROJECT_BINARY_DIR "{Path(temporary).as_posix()}")\n'
                f'include("{(root / "cmake/Notices.cmake").as_posix()}")\n')
            subprocess.run(['cmake', '-P', str(script)], check=True, capture_output=True)
            embedded = (Path(temporary) / 'qrmforge_notices.txt').read_bytes()
            notices = (root / 'third_party/deepfilter/THIRD-PARTY-NOTICES.txt').read_bytes()
            self.assertTrue(notices.replace(b'\r\n', b'\n') in embedded.replace(b'\r\n', b'\n'),
                            'Complete dependency notice text reaches the offline About resource')
            self.assertTrue(b'Copyright (c) 2015 The RustFFT Developers' in embedded)

    def test_copy_complete_common_license_directory(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / 'common'
            source.mkdir()
            for name in ('GPL-3', 'GPL-2', 'LGPL-2.1', 'CC0-1.0'):
                (source / name).write_text(name + ' full text')
            destination = root / 'licenses'
            destination.mkdir()
            portable.copy_common_licenses(source, destination)
            for item in source.iterdir():
                self.assertEqual((destination / 'common' / item.name).read_bytes(), item.read_bytes())

    def test_missing_license_directory_fails(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaises(FileNotFoundError):
                portable.copy_common_licenses(Path(temporary) / 'absent', Path(temporary))
