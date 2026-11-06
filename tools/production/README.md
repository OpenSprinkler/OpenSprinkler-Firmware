# Pinned OS4 N8 Production Flashing

Both `../flash-os4-n8.sh` and `../flash-os4-n8-factory-erase.sh` are production
tools pinned to **2.2.1(6)**. They never build firmware, inspect the current
`defines.h`, follow a latest-release pointer, or use `.pio` artifacts.

## Usage

From the firmware repository on Linux or macOS (Python 3.8+ and esptool 5.x):

```sh
# Download and verify once; no USB access. Subsequent flashes can be offline.
tools/flash-os4-n8.sh --prepare-only

# Check the command and artifact without flashing.
tools/flash-os4-n8.sh --dry-run /dev/ttyACM0

# Brand-new, previously unprogrammed controllers only; no full-chip erase.
tools/flash-os4-n8.sh /dev/ttyACM0

# USB full reinitialization for previously programmed controllers.
# This deletes ALL settings, logs and both application slots.
tools/flash-os4-n8-factory-erase.sh /dev/ttyACM0

# Enclosed controllers: OTA downgrade, preserving settings and logs.
tools/flash-os4-n8.sh 192.168.1.50
# Equivalent explicit form:
tools/flash-os4-n8.sh --ip 192.168.1.50
# Verify the image and show the OTA operation without contacting the controller:
tools/flash-os4-n8.sh --dry-run 192.168.1.50
```

On macOS, omit the port to auto-select exactly one `/dev/cu.usbmodem*` or
`/dev/cu.usbserial*` device. Multiple devices require an explicit port. On Linux,
omitting the port selects `/dev/ttyACM0`. The erase wrapper also supports
`--prepare-only` and `--dry-run`; neither mode opens a device or erases anything.

The default esptool executable remains
`~/.local/share/opensprinkler-esptool/bin/esptool`. `ESPTOOL` may select another
complete executable path. The scripts no longer depend on PlatformIO.

`OS4_FIRMWARE` may point to an offline copy of the **exact pinned factory image**,
not an application-only `.bin32n8`. Its size and hash must match; this variable
cannot bypass the release pin. A stale development-image override fails closed.

The default cache is `tools/.firmware-cache/`; `OS4_FIRMWARE_CACHE` can relocate
it. A corrupt cached image is rejected before USB access. Remove that cache file
and run `--prepare-only` to download again. Failed downloads are never activated.
When moving to another workstation, copy both wrappers, `flash_os4_production.py`
and this `production/` directory. Include the prepared cache for offline use.

## Artifact Provenance

The application is the published release artifact, not a rebuild:

- URL: <https://firmware.opensprinkler.com/v1/releases/2.2.1-6/opensprinkler-2.2.1-6-esp32c6.bin32n8>
- Size: 1,719,472 bytes.
- SHA-256: `103c56103e317ca3851c86372855a86657f2062e084525083994d70ed42f52e5`.
- Pin checked against the release descriptor at
  <https://firmware.opensprinkler.com/v1/releases/2.2.1-6/release.json>.

`os4-2.2.1-6-boot.bin` contains only the first 65,536 bytes of a factory image
built on 2026-10-12 from release tag `221(6)`, commit
`097114c26fd7d26044a84bc4ef74d079f5c2c0a5`, in an isolated checkout. It includes:

| Offset | Content |
|--------|---------|
| `0x0000` | ESP32-C6 bootloader |
| `0x8000` | Partition table generated from that commit's `os4_n8.csv` |
| `0xe000` | Arduino `boot_app0.bin` OTA initialization data |
| Other bytes below `0x10000` | Erased (`0xff`) padding |

Toolchain: the release's pinned pioarduino platform 55.03.39, Arduino-ESP32
3.3.9 / ESP-IDF 5.5.4; release submodule commits were used. The boot prefix is
assembled by that platform's factory-image generator (DIO, 80 MHz, 8 MB).
This is a reconstructed factory package, not a claim that the complete factory
image itself was published with 2.2.1(6). Boot components originate from
[Espressif Arduino-ESP32](https://github.com/espressif/arduino-esp32) and
[ESP-IDF](https://github.com/espressif/esp-idf); the ESP-IDF bootloader is under
Apache-2.0 (see the accompanying license).

- Boot prefix SHA-256: `168ef8880c9a3f704cdcfa7cbe73f21ce3c2c9f7cb32479a655edb059eb65c73`.
- Append the published application at `0x10000`; no application bytes are modified.
- Result: `os4-2.2.1-6.factory.bin`, 1,785,008 bytes.
- Factory SHA-256: `95f9c59c6396fb5cceb9fb701c7eb2f472b0bd0af7420675d70ca41a3d707575`.

All three hashes are fixed in the helper and verified before flashing. Updating
production firmware requires an intentional code/artifact review of these pins;
building a development branch cannot change production flashing.

## OTA Downgrade from Development 2.2.1(7)

OTA needs Python only, not esptool. Connect a **W5500** Ethernet module (not
CH390D, which 2.2.1(6) does not support), or use an already configured Wi-Fi
connection. Use DHCP for wired recovery: 2.2.1(6) has the v4 wired static-IP
ordering bug fixed on the development branch. Find the controller's IPv4 address
and pass it as shown above. Its update server must be reachable on port 8080.
Use a trusted local network: the controller's upload protocol is HTTP.

The script checks `/update/info` for the OS4 N8 target, automatically uses the
MD5 hash of the factory password `opendoor`, uploads the hash-verified **application only**,
then waits up to two minutes for `/update/info` to report version 221, build 6.
It never sends the factory boot/partition prefix over OTA. Keep power connected
through the upload and reboot. Settings and logs are not erased.

No password entry is needed. For units with a different password, `OS_PASSWORD_HASH`
can override the default with its 32-character MD5 digest. An empty value is only appropriate when
Ignore Password is enabled. The script does not print the hash, follow redirects,
or use environment HTTP proxies for controller requests. Upload failures are not
automatically retried: if the response is lost, check the LCD/API first. If DHCP
assigns another address after reboot, verification at the old address will fail
even though the installation may have succeeded.

`--erase-all` and the factory-erase wrapper deliberately reject IP targets. OTA
does not erase the entire chip or rewrite the partition table/bootloader.

The release and current development branch use the identical `os4_n8.csv`
partition layout. The base settings, station and program formats remain
compatible, and both report dotted version 2.2.1, so this downgrade does not
automatically factory-reset settings. Extra data is not itself a reason to erase:

- `stnx.dat` is ignored by 2.2.1(6), as is the appended hostname string slot.
- New onboard-analog sensor records are unsupported and hidden by 2.2.1(6).
- New features do not survive functionally: cycle/soak is not applied, and Analog
  port mode is not understood. Before downgrading a configured test unit, delete
  analog readings and return ports to None, and review programs that relied on
  cycle/soak. Do not treat the older firmware as preserving those behaviors.
- For newly flashed production units still at defaults, leaving these extra
  files/slots alone is fine. If a unit has been configured for development tests,
  factory-reset it **on 2.2.1(7) before downgrading** and then reconnect Ethernet.
  This also prevents old development cycle/soak settings from resurfacing in a
  later upgrade; 2.2.1(6)'s reset does not know about `stnx.dat`.

After OTA, the inactive application slot can still contain 2.2.1(7); normal
boots use the newly selected 2.2.1(6) slot. Full-chip erase is only needed when
you specifically want full reinitialization, not for this compatible downgrade.

For bench development, use the explicit PlatformIO/esptool development workflow,
not these scripts. After reworking an accidentally programmed production unit,
verify firmware 2.2.1(6) on its LCD/API and run the normal production functional
test. A successful esptool verification alone does not test the hardware.

## Validation

```sh
python3 -B -m unittest tests/test_production_flash.py
bash -n tools/flash-os4-n8.sh tools/flash-os4-n8-factory-erase.sh
```

The production package was independently merged with esptool and matched the
factory hash above. Download/cache preparation and both wrappers' dry-run paths
were tested, including rejection of the development `.pio` factory image. The
tests mock device operations; no controller was flashed during this change.
OTA tests cover application-only uploads, target checks, authentication failures,
ambiguous upload failures, post-update version checks, dry runs, and refusal to
combine OTA with full-chip erase.
