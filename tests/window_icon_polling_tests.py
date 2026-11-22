# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 Alessandro Lucaferro

import subprocess
import unittest
from unittest.mock import patch

import window_icon_tests


class WindowIconPollingTests(unittest.TestCase):
    def test_waits_for_icon_after_window_appears(self):
        icon = "_NET_WM_ICON(CARDINAL) = 256, 256, " + ", ".join(["4278190080"] * (256 * 256))
        self.check_icon_polling([0, 0, 1], ["_NET_WM_ICON: not found.", icon])

    def test_missing_icon_still_fails_at_deadline(self):
        with self.assertRaisesRegex(AssertionError, "Native window has no _NET_WM_ICON"):
            self.check_icon_polling([0, 0, 6], ["_NET_WM_ICON: not found."])

    def check_icon_polling(self, clock, icons):
        windows = '0x123 "QRMForge"'
        owner = "_NET_WM_PID(CARDINAL) = 123"
        responses = [windows]
        for icon in icons:
            responses.extend([windows, owner, icon])

        with (
            patch.dict(window_icon_tests.os.environ, {"DISPLAY": ":99"}),
            patch.object(window_icon_tests.shutil, "which", return_value="/usr/bin/tool"),
            patch.object(window_icon_tests.sys, "argv", ["window_icon_tests.py", "qrmforge"]),
            patch.object(window_icon_tests.subprocess, "Popen", spec=subprocess.Popen) as launch,
            patch.object(window_icon_tests, "output", side_effect=responses) as read_property,
            patch.object(window_icon_tests.time, "monotonic", side_effect=clock),
            patch.object(window_icon_tests.time, "sleep"),
        ):
            launch.return_value.pid = 123
            self.assertEqual(window_icon_tests.main(), 0)
            arguments = read_property.call_args.args
            self.assertIn("-len", arguments)
            self.assertGreaterEqual(int(arguments[arguments.index("-len") + 1]), 8 * (256 * 256 + 2))


if __name__ == "__main__":
    unittest.main()
