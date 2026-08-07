import importlib.util
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest


REPO_ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "firmware_release", REPO_ROOT / "tools" / "firmware_release.py")
firmware_release = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(firmware_release)


class FirmwareReleaseTests(unittest.TestCase):
    def image(self, target):
        data = bytearray(1024)
        data[0] = 0xE9
        data[1] = 1
        if target == "os4-esp32c6":
            data[12:14] = (0x000D).to_bytes(2, "little")
        else:
            data[4:8] = (0x40100000).to_bytes(4, "little")
        return data

    def test_reads_firmware_version(self):
        with tempfile.TemporaryDirectory() as directory:
            defines = Path(directory) / "defines.h"
            defines.write_text(
                "#define OS_FW_VERSION 221 // degree: °\n#define OS_FW_MINOR 6\n",
                encoding="utf-8")
            self.assertEqual(firmware_release.firmware_version(defines), (221, 6))
        self.assertEqual(firmware_release.dotted_version(221), "2.2.1")

    def test_catalog_url_normalization(self):
        self.assertEqual(
            firmware_release.catalog_root("https://firmware.example"),
            "https://firmware.example/v1")
        self.assertEqual(
            firmware_release.catalog_root("https://firmware.example/v1/"),
            "https://firmware.example/v1")
        self.assertEqual(
            firmware_release.origin_url("https://firmware.example/v1"),
            "https://firmware.example")

    def test_remote_destination_requires_absolute_path(self):
        self.assertEqual(
            firmware_release.remote_destination("user@example:/srv/firmware/v1/"),
            ("user@example", "/srv/firmware/v1"))
        with self.assertRaises(ValueError):
            firmware_release.remote_destination("user@example:relative/path")

    def test_rejects_malformed_catalogs_and_unsafe_release_ids(self):
        with self.assertRaises(RuntimeError):
            firmware_release.parse_catalog(b"[]", "test")
        with self.assertRaises(RuntimeError):
            firmware_release.validate_catalog_entry({
                "id": "../release",
                "targets": ["os3-esp8266"],
                "descriptor": "/v1/releases/../release/release.json",
                "signature": "/v1/releases/../release/release.sig",
            })

    def test_validates_target_specific_image_headers(self):
        with tempfile.TemporaryDirectory() as directory:
            esp8266 = Path(directory) / "firmware.bin"
            esp32c6 = Path(directory) / "firmware.bin32"
            esp8266.write_bytes(self.image("os3-esp8266"))
            esp32c6.write_bytes(self.image("os4-esp32c6"))
            self.assertEqual(
                firmware_release.validate_image(esp8266, "os3-esp8266"),
                esp8266.resolve())
            self.assertEqual(
                firmware_release.validate_image(esp32c6, "os4-esp32c6"),
                esp32c6.resolve())
            with self.assertRaises(RuntimeError):
                firmware_release.validate_image(esp8266, "os4-esp32c6")
            with self.assertRaises(RuntimeError):
                firmware_release.validate_image(esp32c6, "os3-esp8266")

    def test_embedded_public_key_shape(self):
        point = firmware_release.public_point_from_header(
            REPO_ROOT / "services" / "firmware_update_public_key.h")
        self.assertEqual(len(point), 65)
        self.assertEqual(point[0], 0x04)

    def test_prepares_independently_signed_release(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            private_key = root / "release.pem"
            public_header = root / "public_key.h"
            firmware_release.keygen(SimpleNamespace(
                private_key=private_key, public_header=public_header))
            esp8266 = root / "firmware.bin"
            esp32c6 = root / "firmware.bin32"
            esp8266.write_bytes(self.image("os3-esp8266"))
            esp32c6.write_bytes(self.image("os4-esp32c6"))
            output = root / "v1"
            release_id = firmware_release.prepare(SimpleNamespace(
                private_key=private_key,
                public_header=public_header,
                output_dir=output,
                esp8266=esp8266,
                esp32c6=esp32c6,
                version=221,
                build=6,
                release_id="2.2.1-6-test",
            ))
            self.assertEqual(release_id, "2.2.1-6-test")
            catalog = firmware_release.load_catalog(output / "manifest.json")
            self.assertEqual(catalog["schema"], 2)
            self.assertEqual(len(catalog["releases"]), 1)
            entry = catalog["releases"][0]
            firmware_release.validate_catalog_entry(entry)
            descriptor_path = firmware_release.output_path_for_url(
                output, entry["descriptor"])
            signature_path = firmware_release.output_path_for_url(
                output, entry["signature"])
            descriptor = descriptor_path.read_bytes()
            self.assertLessEqual(len(descriptor), firmware_release.MAX_DESCRIPTOR_BYTES)
            firmware_release.verify_signature(
                descriptor, signature_path.read_bytes(),
                firmware_release.public_point(private_key))
            parsed = json.loads(descriptor.decode("ascii"))
            self.assertEqual(parsed["id"], release_id)
            self.assertEqual(
                set(parsed["targets"]), {"os3-esp8266", "os4-esp32c6"})


if __name__ == "__main__":
    unittest.main()
