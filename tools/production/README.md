# Pinned OS4 N8 Production Flashing

Both `../flash-os4-n8.sh` and `../flash-os4-n8-factory-erase.sh` are production
tools pinned to **2.2.1(7)**. They never build firmware, inspect the current
`defines.h`, follow a latest-release pointer, or use `.pio` artifacts.

## Usage

From the firmware repository on Linux or macOS (Python 3.8+ and esptool 5.x):

```sh
# Download and verify once; no USB access. Subsequent flashes can be offline.
tools/flash-os4-n8.sh --prepare-only

# Verify the artifact and show the command without flashing.
tools/flash-os4-n8.sh --dry-run /dev/ttyACM0

# Brand-new, previously unprogrammed controllers only; no full-chip erase.
tools/flash-os4-n8.sh /dev/ttyACM0

# Previously programmed controllers: deletes ALL settings, logs and both slots.
tools/flash-os4-n8-factory-erase.sh /dev/ttyACM0

# OTA installation, preserving settings and logs.
tools/flash-os4-n8.sh 192.168.1.50
tools/flash-os4-n8.sh --ip 192.168.1.50

# No controller contact or reboot.
tools/flash-os4-n8.sh --dry-run 192.168.1.50
```

On macOS, omit the port to auto-select exactly one `/dev/cu.usbmodem*` or
`/dev/cu.usbserial*` device. Multiple devices require an explicit port. On Linux,
omitting the port selects `/dev/ttyACM0`. Both wrappers support `--prepare-only`
and `--dry-run`; neither mode opens a device or erases anything.

The default esptool executable is
`~/.local/share/opensprinkler-esptool/bin/esptool`. `ESPTOOL` may select another
complete executable path. The scripts do not depend on PlatformIO.

`OS4_FIRMWARE` may point to an offline copy of the **exact pinned factory image**,
not an application-only `.bin32n8`. Its size and hash must match; this variable
cannot bypass the release pin. Older or development-image overrides fail closed.

The default cache is `tools/.firmware-cache/`; `OS4_FIRMWARE_CACHE` can relocate
it. A corrupt cached image is rejected before device access. Remove that cache
file and run `--prepare-only` to download again. Failed downloads are never
activated. An old (6) cache is ignored, not selected as a fallback.
When moving to another workstation, copy both wrappers, `flash_os4_production.py`
and this `production/` directory. Include the prepared cache for offline use.

## Artifact Provenance

The application is the published release artifact, not a rebuild:

- URL: <https://firmware.opensprinkler.com/v1/releases/2.2.1-7/opensprinkler-2.2.1-7-esp32c6.bin32n8>
- Size: 1,786,864 bytes.
- SHA-256: `d041f038aa56ad64c0728014d3fd21cb52969e951d500ec20b7d23306a1c6e00`.
- Release descriptor:
  <https://firmware.opensprinkler.com/v1/releases/2.2.1-7/release.json>.

`os4-2.2.1-7-boot.bin` is the first 65,536 bytes of the exact November 6
release build's saved factory image. It includes:

| Offset | Content |
|--------|---------|
| `0x0000` | ESP32-C6 bootloader |
| `0x8000` | Partition table generated from `os4_n8.csv` |
| `0xe000` | Arduino `boot_app0.bin` OTA initialization data |
| Other bytes below `0x10000` | Erased (`0xff`) padding |

Build source: `9795a049832fdf0c3b9d3870c4dbb40e0b8e764d`, with the owner's
local test-mode configuration. The released tag `221(7)` points to merge commit
`04de197a7feadf728a897ac9128af449c91993ef`; intervening changes were documentation
only. Toolchain: pioarduino 55.03.312-1, Arduino-ESP32 3.3.12 / ESP-IDF 5.5.5.
The factory-image generator uses DIO, 80 MHz, 8 MB.

Boot components originate from
[Espressif Arduino-ESP32](https://github.com/espressif/arduino-esp32) and
[ESP-IDF](https://github.com/espressif/esp-idf); the ESP-IDF bootloader is under
Apache-2.0 (see the accompanying license).

- Boot prefix SHA-256: `15b4a74f85942d9d700c19cd4c796fd4bc3954fdd8d726dedb10f13e1ccea526`.
- Append the published application at `0x10000`; no application bytes change.
- Result: `os4-2.2.1-7.factory.bin`, 1,852,400 bytes.
- Factory SHA-256: `31128b564d9bdfd6120df1c6e55689794785f2f2f3ceb70aa7dc92d9bf273f70`.

All three hashes are fixed in the helper and verified before flashing. Updating
production firmware requires an intentional review of these pins; building a
development branch cannot change production flashing.

## OTA Installation

OTA needs Python only, not esptool. Use a network interface supported by the
currently installed firmware: (6) supports Wi-Fi or W5500, while (7) also supports
CH390. The update server must be reachable at the supplied IPv4 address on port
8080. Use a trusted local network: the controller's upload protocol is HTTP.

The script checks `/update/info` for the OS4 N8 target, automatically uses the
MD5 hash of the factory password `opendoor`, uploads the verified **application
only**, and waits up to two minutes for version 221, build 7. It never sends
the factory boot/partition prefix over OTA. Keep power connected throughout.
Settings and logs are not erased.

For a different password, set `OS_PASSWORD_HASH` to its 32-character MD5 digest.
An empty value is only appropriate with Ignore Password enabled. The script
does not print the hash, follow redirects, or use HTTP proxies for controller
requests. Upload failures are not automatically retried: a lost response or
changed DHCP address leaves the outcome uncertain. Check the LCD/API first.

`--erase-all` and the factory-erase wrapper reject IP targets. OTA does not erase
the entire chip or rewrite the partition table/bootloader. The (6) and (7)
partition layouts match. These helpers no longer perform a downgrade to (6).

For development builds, use the explicit PlatformIO/esptool workflow instead.
After production flashing, verify 2.2.1(7) on the LCD/API and run the normal
functional test. Successful esptool verification alone does not test hardware.

## Validation

```sh
python3 -B -m unittest tests/test_production_flash.py
bash -n tools/flash-os4-n8.sh tools/flash-os4-n8-factory-erase.sh
```

The reconstructed package is checked against the exact archived factory image.
The device-free tests cover fixed pins, cache/override rejection, USB command
construction, application-only OTA, target checks, authentication failures,
ambiguous upload failures, running-version checks and dry runs. No controller
is flashed by these tests.
