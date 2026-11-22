# SPDX-License-Identifier: GPL-3.0-only
"""Offline tests for pinned packaging tool acquisition."""
import hashlib
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('build_appimage', Path(__file__).resolve().parents[1] / 'packaging/build_appimage.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ToolTests(unittest.TestCase):
    def test_download_and_cache(self):
        payload = b'verified tool'
        digest = hashlib.sha256(payload).hexdigest()
        def download(command, **kwargs):
            Path(command[command.index('--output') + 1]).write_bytes(payload)
        with tempfile.TemporaryDirectory() as directory:
            with patch.dict(module.TOOLS, {'test': ('https://example.org/tool', digest)}):
                with patch.object(module.subprocess, 'run', side_effect=download) as run:
                    tool = module.verified_tool(Path(directory), 'test')
                    self.assertEqual(tool.read_bytes(), payload)
                    self.assertTrue(tool.stat().st_mode & 0o111)
                    self.assertEqual(module.verified_tool(Path(directory), 'test'), tool)
                    self.assertEqual(run.call_count, 1)
                tool.write_bytes(b'corrupted')
                with self.assertRaisesRegex(RuntimeError, 'Cached.*checksum mismatch'):
                    module.verified_tool(Path(directory), 'test')

    def test_failed_download_never_cached(self):
        for payload in (b'', b'wrong', bytes(range(256))):
            with self.subTest(payload=payload), tempfile.TemporaryDirectory() as directory:
                def download(command, **kwargs):
                    Path(command[command.index('--output') + 1]).write_bytes(payload)
                with patch.dict(module.TOOLS, {'test': ('https://example.org/tool', '0' * 64)}):
                    with patch.object(module.subprocess, 'run', side_effect=download):
                        with self.assertRaisesRegex(RuntimeError, 'Downloaded.*checksum mismatch'):
                            module.verified_tool(Path(directory), 'test')
                self.assertEqual(list(Path(directory).iterdir()), [])

    def test_network_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            with patch.object(module.subprocess, 'run', side_effect=subprocess.CalledProcessError(6, 'curl')):
                with self.assertRaises(subprocess.CalledProcessError):
                    module.verified_tool(Path(directory), 'runtime')
            self.assertEqual(list(Path(directory).iterdir()), [])

    def test_build_and_preserve_previous_artifact_on_failure(self):
        from types import SimpleNamespace
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / 'dist'
            arguments = SimpleNamespace(cache=directory, output=str(destination), binary='/trusted/binary')
            def package(command, **kwargs):
                self.assertEqual(kwargs['env']['APPIMAGETOOL_RUNTIME_FILE'], '/verified/runtime')
                stage = Path(command[command.index('--output') + 1])
                (stage / 'QRMForge.AppImage').write_bytes(b'new image')
            with patch.object(module.platform, 'machine', return_value='x86_64'):
                with patch.object(module, 'verified_tool', side_effect=[Path('/verified/tool'), Path('/verified/runtime')]):
                    with patch.object(module.subprocess, 'run', side_effect=package):
                        module.build(arguments)
                artifact = destination / 'QRMForge.AppImage'
                self.assertEqual(artifact.read_bytes(), b'new image')
                with patch.object(module, 'verified_tool', return_value=Path('/verified/tool')):
                    with patch.object(module.subprocess, 'run', side_effect=subprocess.CalledProcessError(1, 'package')):
                        with self.assertRaises(subprocess.CalledProcessError):
                            module.build(arguments)
                self.assertEqual(artifact.read_bytes(), b'new image')
                self.assertEqual(list(destination.iterdir()), [artifact])

    def test_unsupported_architecture(self):
        with patch.object(module.platform, 'machine', return_value='aarch64'):
            with self.assertRaisesRegex(RuntimeError, 'x86-64'):
                module.build(None)


if __name__ == '__main__':
    unittest.main()
