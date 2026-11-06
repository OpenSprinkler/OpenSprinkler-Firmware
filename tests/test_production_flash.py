"""Production pin and no-device tests; never invoke a real flash command."""

import contextlib
import hashlib
import importlib.util
import io
import os
from pathlib import Path
import tempfile
import unittest
import urllib.parse
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("production_flash", ROOT / "tools/flash_os4_production.py")
flash = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(flash)


class ProductionFlashTests(unittest.TestCase):
    def setUp(self):
        self.stack = contextlib.ExitStack()
        self.addCleanup(self.stack.close)
        self.directory = Path(self.stack.enter_context(tempfile.TemporaryDirectory()))
        self.stack.enter_context(mock.patch.dict(os.environ, {
            "OS4_FIRMWARE": "", "OS4_FIRMWARE_CACHE": str(self.directory),
            "ESPTOOL": "/bin/true",
        }))
        self.boot = flash.BOOT_PATH.read_bytes()
        self.app = b"test release application" * 100
        self.factory = self.boot + self.app
        self.stack.enter_context(mock.patch.object(flash, "APP_SIZE", len(self.app)))
        self.stack.enter_context(mock.patch.object(flash, "APP_SHA256", hashlib.sha256(self.app).hexdigest()))
        self.stack.enter_context(mock.patch.object(flash, "FACTORY_SHA256", hashlib.sha256(self.factory).hexdigest()))
        self.download = self.stack.enter_context(mock.patch.object(flash, "download_application", return_value=self.app))
        self.run = self.stack.enter_context(mock.patch.object(flash.subprocess, "run"))
        self.run.return_value.returncode = 0
        self.stack.enter_context(contextlib.redirect_stdout(io.StringIO()))

    def test_bundled_boot_is_pinned_c6_and_has_no_application(self):
        flash.verify(self.boot, 65536, flash.BOOT_SHA256, "boot")
        self.assertEqual(self.boot[0], 0xE9)
        self.assertEqual(int.from_bytes(self.boot[12:14], "little"), 13)
        self.assertEqual(self.boot[0x8000:0x8002], b"\xaa\x50")

    def test_preparation_is_cached_and_offline(self):
        self.assertEqual(flash.main(["--prepare-only"]), 0)
        self.assertEqual((self.directory / flash.FACTORY_NAME).read_bytes(), self.factory)
        self.download.side_effect = AssertionError("must not download cached firmware")
        self.assertEqual(flash.main(["--prepare-only"]), 0)
        self.download.assert_called_once()
        self.run.assert_not_called()

    def test_corrupt_cache_never_erases_or_falls_back(self):
        (self.directory / flash.FACTORY_NAME).write_bytes(b"development firmware")
        with self.assertRaisesRegex(ValueError, "mismatch"):
            flash.main(["--erase-all", "/dev/test"])
        self.run.assert_not_called()
        self.download.assert_not_called()

    def test_development_override_rejected(self):
        path = self.directory / "firmware.factory.bin"
        path.write_bytes(b"2.2.1(7)" + b"\xff" * 100)
        with mock.patch.dict(os.environ, {"OS4_FIRMWARE": str(path)}):
            with self.assertRaisesRegex(ValueError, "mismatch"):
                flash.main(["--erase-all", "/dev/test"])
        self.run.assert_not_called()
        self.download.assert_not_called()

    def test_pinned_override_is_allowed_offline(self):
        path = self.directory / "offline.bin"
        path.write_bytes(self.factory)
        with mock.patch.dict(os.environ, {"OS4_FIRMWARE": str(path)}):
            self.assertEqual(flash.main(["--prepare-only"]), 0)
        self.download.assert_not_called()
        self.run.assert_not_called()

    def test_wrong_download_cannot_create_cache_or_erase(self):
        self.download.return_value = b"wrong firmware"
        with self.assertRaisesRegex(ValueError, "mismatch"):
            flash.main(["--erase-all", "/dev/test"])
        self.assertFalse((self.directory / flash.FACTORY_NAME).exists())
        self.run.assert_not_called()

    def test_damaged_boot_cannot_download_or_flash(self):
        path = self.directory / "bad-boot.bin"
        path.write_bytes(b"bad boot")
        with mock.patch.object(flash, "BOOT_PATH", path):
            with self.assertRaisesRegex(ValueError, "mismatch"):
                flash.main(["/dev/test"])
        self.download.assert_not_called()
        self.run.assert_not_called()

    def test_dry_run_including_erase_has_no_device_access(self):
        self.assertEqual(flash.main(["--erase-all", "--dry-run", "/dev/test"]), 0)
        self.run.assert_not_called()

    def test_erase_and_no_erase_commands(self):
        for erase in (False, True):
            self.assertEqual(flash.main((["--erase-all"] if erase else []) + ["/dev/test"]), 0)
            command = self.run.call_args.args[0]
            self.assertEqual(command[1:8], ["--chip", "esp32c6", "--port", "/dev/test", "--baud", "460800", "write-flash"])
            self.assertEqual("--erase-all" in command, erase)
            self.assertEqual(command[-3:], ["--no-compress", "0x0", str(self.directory / flash.FACTORY_NAME)])

    def test_esptool_failure_is_propagated(self):
        self.run.return_value.returncode = 2
        self.assertEqual(flash.main(["/dev/test"]), 2)

    def test_missing_esptool_fails_before_device_access(self):
        with mock.patch.dict(os.environ, {"ESPTOOL": str(self.directory / "missing")}):
            with self.assertRaisesRegex(ValueError, "not executable"):
                flash.main(["/dev/test"])
        self.run.assert_not_called()

    def test_macos_requires_exactly_one_device(self):
        with mock.patch.object(flash.platform, "system", return_value="Darwin"):
            with mock.patch.object(flash.glob, "glob", return_value=[]):
                with self.assertRaisesRegex(ValueError, "exactly one"):
                    flash.select_port(None)
            with mock.patch.object(flash.glob, "glob", side_effect=[["/dev/cu.usbmodem1"], []]):
                self.assertEqual(flash.select_port(None), "/dev/cu.usbmodem1")
            with mock.patch.object(flash.glob, "glob", side_effect=[["/dev/cu.usbmodem1"], ["/dev/cu.usbserial2"]]):
                with self.assertRaisesRegex(ValueError, "exactly one"):
                    flash.select_port(None)
            self.assertEqual(flash.select_port("/dev/explicit"), "/dev/explicit")

    def ota_info(self, build=7):
        return {"target": flash.TARGET, "extension": ".bin32n8", "current_version": 221, "current_build": build}

    def test_ota_uploads_only_pinned_application_and_checks_version(self):
        for target in (["192.168.1.50"], ["--ip", "192.168.1.50"]):
            with mock.patch.object(flash, "device_json", side_effect=[self.ota_info(), {"result": 1}, self.ota_info(), self.ota_info(6)]) as request, \
                    mock.patch.object(flash, "ota_password_hash", return_value="a" * 32), \
                    mock.patch.object(flash.time, "sleep"), \
                    mock.patch.dict(os.environ, {"ESPTOOL": "/not/installed"}):
                self.assertEqual(flash.main(target), 0)
            upload = request.call_args_list[1]
            query = urllib.parse.parse_qs(urllib.parse.urlsplit(upload.args[1]).query)
            self.assertEqual(query, {"pw": ["a" * 32], "size": [str(len(self.app))], "sha256": [flash.APP_SHA256]})
            body = upload.kwargs["data"]
            self.assertIn(b'name="file"; filename="os_221_rev6.bin32n8"', body)
            self.assertEqual(body.split(b"\r\n\r\n", 1)[1].rsplit(b"\r\n--", 1)[0], self.app)
            self.assertEqual(request.call_count, 4)
        self.run.assert_not_called()

    def test_ota_dry_run_never_contacts_device_or_prompts(self):
        with mock.patch.object(flash, "device_json") as request, mock.patch.object(flash, "ota_password_hash") as password:
            self.assertEqual(flash.main(["--dry-run", "192.168.1.50"]), 0)
        request.assert_not_called()
        password.assert_not_called()
        self.run.assert_not_called()

    def test_ota_invalid_arguments_do_not_prepare_or_flash(self):
        for args in (["--erase-all", "192.168.1.50"], ["--ip", "192.168.1.50", "/dev/test"], ["192.168.1.999"]):
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                flash.main(args)
        self.download.assert_not_called()
        self.run.assert_not_called()

    def test_ota_wrong_target_never_uploads_or_prompts(self):
        with mock.patch.object(flash, "device_json", return_value={"target": "os3-esp8266"}) as request, \
                mock.patch.object(flash, "ota_password_hash") as password:
            with self.assertRaisesRegex(ValueError, "Not an OS4"):
                flash.main(["192.168.1.50"])
        self.assertEqual(request.call_count, 1)
        password.assert_not_called()

    def test_ota_corrupt_image_never_contacts_controller(self):
        (self.directory / flash.FACTORY_NAME).write_bytes(b"wrong image")
        with mock.patch.object(flash, "device_json") as request:
            with self.assertRaisesRegex(ValueError, "mismatch"):
                flash.main(["192.168.1.50"])
        request.assert_not_called()

    def test_ota_rejection_or_timeout_never_retries_upload(self):
        for result, message in (({"result": 2}, "Authentication failed"),
                                ({"result": 17}, "rejected"),
                                (ValueError("timeout"), "outcome is uncertain")):
            with mock.patch.object(flash, "device_json", side_effect=[self.ota_info(), result]) as request, \
                    mock.patch.object(flash, "ota_password_hash", return_value=""):
                with self.assertRaisesRegex(ValueError, message):
                    flash.main(["192.168.1.50"])
            self.assertEqual(request.call_count, 2)

    def test_ota_unconfirmed_boot_is_not_success(self):
        with mock.patch.object(flash, "device_json", side_effect=[self.ota_info(), {"result": 1}]), \
                mock.patch.object(flash, "ota_password_hash", return_value=""), \
                mock.patch.object(flash.time, "monotonic", side_effect=[0, 121]):
            with self.assertRaisesRegex(ValueError, "not confirmed"):
                flash.main(["192.168.1.50"])

    def test_device_json_limits_and_does_not_expose_password(self):
        opener = mock.Mock()
        response = mock.MagicMock()
        opener.open.return_value = response
        for raw in (b"x" * 65537, b"not json", b"[]"):
            response.__enter__.return_value.read.return_value = raw
            with self.assertRaises(ValueError):
                flash.device_json(opener, "http://192.168.1.50:8080/update")
        secret_url = "http://192.168.1.50:8080/update?pw=secret"
        opener.open.side_effect = flash.urllib.error.HTTPError(secret_url, 401, "secret", {}, None)
        with self.assertRaises(ValueError) as error:
            flash.device_json(opener, secret_url)
        self.assertNotIn("secret", str(error.exception))
        with self.assertRaisesRegex(ValueError, "redirected"):
            flash.NoRedirect().redirect_request(None, None, 302, "", {}, secret_url)

    def test_password_hash_input(self):
        with mock.patch.dict(os.environ):
            os.environ.pop("OS_PASSWORD_HASH", None)
            with mock.patch("builtins.input", side_effect=AssertionError("must not prompt")):
                self.assertEqual(flash.ota_password_hash(), hashlib.md5(b"opendoor").hexdigest())
        with mock.patch.dict(os.environ, {"OS_PASSWORD_HASH": ""}):
            self.assertEqual(flash.ota_password_hash(), "")
        with mock.patch.dict(os.environ, {"OS_PASSWORD_HASH": "A" * 32}):
            self.assertEqual(flash.ota_password_hash(), "a" * 32)
        with mock.patch.dict(os.environ, {"OS_PASSWORD_HASH": "invalid"}):
            with self.assertRaisesRegex(ValueError, "MD5"):
                flash.ota_password_hash()


class ReleasePinTests(unittest.TestCase):
    def test_pins_are_fixed_not_working_tree_version(self):
        self.assertEqual(flash.VERSION, "2.2.1(6)")
        self.assertEqual(flash.APP_SIZE, 1719472)
        self.assertEqual(flash.APP_SHA256, "103c56103e317ca3851c86372855a86657f2062e084525083994d70ed42f52e5")
        self.assertEqual(flash.FACTORY_SHA256, "95f9c59c6396fb5cceb9fb701c7eb2f472b0bd0af7420675d70ca41a3d707575")

    def test_download_checks_exact_hash(self):
        response = mock.MagicMock()
        response.__enter__.return_value.read.return_value = b"bad remote response"
        with mock.patch.object(flash.urllib.request, "urlopen", return_value=response):
            with contextlib.redirect_stdout(io.StringIO()):
                with self.assertRaisesRegex(ValueError, "mismatch"):
                    flash.download_application()


if __name__ == "__main__":
    unittest.main()
