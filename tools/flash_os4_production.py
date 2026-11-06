#!/usr/bin/env python3
"""Flash only the pinned production OS4 N8 release, never a working-tree build."""

import argparse
import glob
import hashlib
import ipaddress
import json
import os
from pathlib import Path
import platform
import shlex
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request
import uuid


TOOLS = Path(__file__).resolve().parent
VERSION = "2.2.1(6)"
APP_URL = (
    "https://firmware.opensprinkler.com/v1/releases/2.2.1-6/"
    "opensprinkler-2.2.1-6-esp32c6.bin32n8"
)
APP_SIZE = 1719472
APP_SHA256 = "103c56103e317ca3851c86372855a86657f2062e084525083994d70ed42f52e5"
BOOT_SIZE = 0x10000
BOOT_SHA256 = "168ef8880c9a3f704cdcfa7cbe73f21ce3c2c9f7cb32479a655edb059eb65c73"
FACTORY_SHA256 = "95f9c59c6396fb5cceb9fb701c7eb2f472b0bd0af7420675d70ca41a3d707575"
BOOT_PATH = TOOLS / "production" / "os4-2.2.1-6-boot.bin"
FACTORY_NAME = "os4-2.2.1-6.factory.bin"
TARGET = "os4-esp32c6-n8"


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        raise ValueError("Controller redirected the request; refusing to forward credentials or firmware.")


def device_json(opener, url, data=None, headers=None, timeout=5):
    request = urllib.request.Request(url, data=data, headers=headers or {})
    try:
        with opener.open(request, timeout=timeout) as response:
            raw = response.read(65537)
    except urllib.error.HTTPError as error:
        # Do not expose the password hash from the request URL in error messages.
        raise ValueError(f"Controller returned HTTP {error.code}.") from None
    except (OSError, urllib.error.URLError):
        raise ValueError("Controller connection failed or timed out.") from None
    if len(raw) > 65536:
        raise ValueError("Controller response is too large.")
    try:
        result = json.loads(raw)
    except (ValueError, UnicodeError):
        raise ValueError("Controller returned invalid JSON.") from None
    if not isinstance(result, dict):
        raise ValueError("Controller returned an unexpected response.")
    return result


def ota_password_hash():
    value = os.environ.get("OS_PASSWORD_HASH")
    if value is None:
        return hashlib.md5(b"opendoor").hexdigest()
    if value and (len(value) != 32 or any(c not in "0123456789abcdefABCDEF" for c in value)):
        raise ValueError("OS_PASSWORD_HASH must be a 32-character MD5 hex digest (or empty for Ignore Password).")
    return value.lower()


def flash_ota(ip, artifact, dry_run=False):
    factory = read_verified(artifact, BOOT_SIZE + APP_SIZE, FACTORY_SHA256)
    app = factory[BOOT_SIZE:]
    verify(app, APP_SIZE, APP_SHA256, "OTA application")
    base = f"http://{ip}:8080/update"
    print(f"OTA: {base}; application only ({APP_SIZE} bytes), SHA-256 {APP_SHA256}", flush=True)
    print("Settings/logs are preserved. Use W5500, not CH390D, for wired recovery.", flush=True)
    if dry_run:
        print("Dry run: no controller contact, upload or reboot.")
        return 0

    # LAN operations must not go through environment-configured HTTP proxies.
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}), NoRedirect())
    info = device_json(opener, base + "/info")
    if info.get("target") != TARGET or info.get("extension") != ".bin32n8":
        raise ValueError("Not an OS4 ESP32-C6 N8 update server; no firmware uploaded.")
    print(f"Controller reports version {info.get('current_version')}, build {info.get('current_build')}", flush=True)
    query = urllib.parse.urlencode({"pw": ota_password_hash(), "size": APP_SIZE, "sha256": APP_SHA256})
    boundary = "os4-" + uuid.uuid4().hex
    body = (
        f"--{boundary}\r\nContent-Disposition: form-data; name=\"file\"; "
        'filename="os_221_rev6.bin32n8"\r\nContent-Type: application/octet-stream\r\n\r\n'
    ).encode("ascii") + app + f"\r\n--{boundary}--\r\n".encode("ascii")
    print(f"Uploading pinned {VERSION}; keep power connected...", flush=True)
    try:
        result = device_json(opener, base + "?" + query, data=body,
                             headers={"Content-Type": f"multipart/form-data; boundary={boundary}"}, timeout=300)
    except ValueError as error:
        raise ValueError(f"{error} Upload outcome is uncertain; check the LCD/API before retrying. No automatic retry.") from None
    if result.get("result") == 2:
        raise ValueError("Authentication failed. Default password is opendoor; set OS_PASSWORD_HASH for a different password.")
    if result.get("result") != 1:
        raise ValueError("Controller rejected the firmware upload; check its update page for details.")

    print("Upload accepted. Waiting for reboot and verifying running firmware...", flush=True)
    deadline = time.monotonic() + 120
    while time.monotonic() < deadline:
        time.sleep(3)
        try:
            info = device_json(opener, base + "/info")
        except ValueError:
            continue
        if (info.get("target") == TARGET and info.get("current_version") == 221
                and info.get("current_build") == 6):
            print(f"Verified: {ip} is running {VERSION}.", flush=True)
            return 0
    raise ValueError("Upload accepted, but running 2.2.1(6) was not confirmed. Check the LCD and DHCP address; do not assume failure or reflash blindly.")


def verify(data, size, digest, label):
    if len(data) != size or hashlib.sha256(data).hexdigest() != digest:
        raise ValueError(
            f"{label}: size/SHA-256 mismatch. Refusing anything except pinned "
            f"OS4 N8 {VERSION}. No device operation was started."
        )


def read_verified(path, size, digest):
    # Bound reads even if an override mistakenly points at a device or huge file.
    if not path.is_file():
        raise ValueError(f"Firmware file not found: {path}")
    with path.open("rb") as stream:
        data = stream.read(size + 1)
    verify(data, size, digest, str(path))
    return data


def download_application():
    print(f"Downloading released {VERSION} application: {APP_URL}", flush=True)
    with urllib.request.urlopen(APP_URL, timeout=45) as response:
        data = response.read(APP_SIZE + 1)
    verify(data, APP_SIZE, APP_SHA256, "Downloaded application")
    return data


def prepare_image():
    override = os.environ.get("OS4_FIRMWARE")
    if override:
        path = Path(override).expanduser().resolve()
        read_verified(path, BOOT_SIZE + APP_SIZE, FACTORY_SHA256)
        return path

    cache = Path(os.environ.get("OS4_FIRMWARE_CACHE", str(TOOLS / ".firmware-cache")))
    cache = cache.expanduser().resolve()
    path = cache / FACTORY_NAME
    if path.exists():
        read_verified(path, BOOT_SIZE + APP_SIZE, FACTORY_SHA256)
        return path

    boot = read_verified(BOOT_PATH, BOOT_SIZE, BOOT_SHA256)
    data = boot + download_application()
    verify(data, BOOT_SIZE + APP_SIZE, FACTORY_SHA256, "Assembled factory image")
    cache.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=cache, prefix=".os4-", delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(data)
        temporary.replace(path)
    finally:
        if temporary is not None and temporary.exists():
            temporary.unlink()
    return path


def select_port(explicit):
    if explicit:
        return explicit
    if platform.system() != "Darwin":
        return "/dev/ttyACM0"
    ports = sorted(set(glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/cu.usbserial*")))
    if len(ports) != 1:
        raise ValueError(
            "Connect exactly one OS4, or pass its port explicitly. "
            f"Detected ports: {', '.join(ports) or 'none'}"
        )
    return ports[0]


def main(argv=None):
    parser = argparse.ArgumentParser(description=f"Production OS4 N8 flasher, PINNED to {VERSION}.")
    parser.add_argument("port", nargs="?", help="USB port or IPv4 address for OTA; default: USB auto-selection")
    parser.add_argument("--ip", help="controller IPv4 address for OTA on port 8080 (preserves settings)")
    parser.add_argument("--erase-all", action="store_true", help="erase all flash, including settings and logs")
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument("--prepare-only", action="store_true", help="download/verify/cache the image without accessing a controller")
    modes.add_argument("--dry-run", action="store_true", help="prepare/verify and show operations without accessing a controller")
    args = parser.parse_args(argv)
    if args.ip and args.port:
        parser.error("Use either a positional USB/IP target or --ip, not both.")
    ip = args.ip
    if not ip and args.port:
        try:
            ip = str(ipaddress.IPv4Address(args.port))
        except ipaddress.AddressValueError:
            if not args.port.startswith("/dev/"):
                parser.error("Target must be a /dev/ USB port or a valid IPv4 address.")
    if ip:
        ip = str(ipaddress.IPv4Address(ip))
        if args.erase_all:
            parser.error("OTA cannot erase all flash. Use flash-os4-n8.sh without --erase-all for OTA.")

    print(f"PRODUCTION: OpenSprinkler v4 N8 firmware {VERSION} (pinned)", flush=True)
    artifact = prepare_image()
    print(f"Verified factory image: {artifact}\nSHA-256: {FACTORY_SHA256}", flush=True)
    if args.prepare_only:
        return 0
    if ip:
        return flash_ota(ip, artifact, args.dry_run)

    port = select_port(args.port)
    esptool = Path(os.environ.get(
        "ESPTOOL", str(Path.home() / ".local/share/opensprinkler-esptool/bin/esptool")
    )).expanduser().resolve()
    if not esptool.is_file() or not os.access(esptool, os.X_OK):
        raise ValueError(f"esptool not executable: {esptool}. Set ESPTOOL to its complete executable path.")
    command = [str(esptool), "--chip", "esp32c6", "--port", port, "--baud", "460800", "write-flash"]
    if args.erase_all:
        command.append("--erase-all")
        print("ERASE ALL: existing settings, logs and both application slots will be removed.", flush=True)
    else:
        print("NO ERASE ALL: intended for previously unprogrammed units only.", flush=True)
    command.extend(["--no-compress", "0x0", str(artifact)])
    print(shlex.join(command), flush=True)
    if args.dry_run:
        return 0
    # Verify once more immediately before esptool. Neither erase path ever builds.
    read_verified(artifact, BOOT_SIZE + APP_SIZE, FACTORY_SHA256)
    return subprocess.run(command, check=False).returncode


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)
