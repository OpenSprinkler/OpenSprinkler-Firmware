# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

The OpenSprinkler **unified** firmware: one C++ source tree that compiles for three very
different targets, selected entirely by preprocessor macros:

| Macro | Target | Notes |
|---|---|---|
| `ESP8266` | OpenSprinkler v3.x hardware | Arduino framework, LittleFS, OLED, I2C IO expanders |
| `OSPI` | OpenSprinkler Pi / Linux | POSIX `FILE*`, lgpio, native `main()` loop |
| `DEMO` | Simulation / CI | Fake pins, no hardware libs; `OS_HW_VERSION 255` |

Almost every platform difference funnels through `defines.h` (pin maps, hardware version,
buffer sizes, `USE_DISPLAY`) and `utils.h` (the `os_file_type` file abstraction).
When adding code, prefer extending those two seams over sprinkling new `#if defined(...)`
blocks through logic files.

## Build & verify

There is **no automated test suite**. Verification == the three builds CI runs
(`.github/workflows/build-ci.yml`). Always build the target(s) your change touches.

```bash
git submodule update --init --recursive     # required; external/ holds submodules

# ESP8266 firmware (needs PlatformIO + Node)
npm install html-minifier-terser
pio run --environment os3x_esp8266           # output: .pio/build/os3x_esp8266/firmware.bin

# Linux/OSPi build (Debian-ish; apt-installs deps, needs root)
sudo ./build.sh                              # -d adds -DENABLE_DEBUG -DSERIAL_DEBUG
sudo ./build.sh -s demo                      # simulation build, non-interactive, no HW libs

# Makefile build (what the Dockerfile uses)
make VERSION=OSPI                            # or VERSION=DEMO
make clean

docker build -t opensprinkler .              # --build-arg BUILD_VERSION=DEMO for sim
```

Gotchas:

- `platformio.ini`'s `linux` and `demo` environments exist **only** for editor syntax
  highlighting — they do not produce working binaries. Use `build.sh` / `make`.
- The Makefile links `-li2c -llgpio` and compiles `i2cd.cpp` regardless of `VERSION`, so
  `make VERSION=DEMO` still needs the Linux I2C/GPIO dev packages. `./build.sh -s demo`
  is the genuinely hardware-free path (it drops those sources and libs) and is what CI uses.
- `build.sh` re-syncs and checks out submodules at pinned revisions on every run, so it will
  discard local edits inside `external/`.
- The build is not incremental-friendly across targets: object files carry no target suffix,
  so `make clean` between `VERSION=` changes.

Running the Linux build: `./OpenSprinkler -d <datadir>` — all `.dat` files, `logs/`, and NVM
live there; web UI on port 88 on OSPi/DEMO (v3 stays on 80); existing installs keep
whatever `iopts.dat` already holds. `-d` defaults to the current directory, which is why
`startOpenSprinkler.sh` (used by `OpenSprinkler.service`) passes nothing and runs from the
checkout. That is load-bearing: `OpenSprinkler::update_dev()` implements in-app firmware
update by shelling out to `cd $(get_data_dir()) && ./updater.sh`, so on OSPi **the data dir
must be the git checkout**. The Docker image sets `-d /data` and therefore cannot self-update.

## Syncing with upstream

This is a fork of [OpenSprinkler/OpenSprinkler-Firmware](https://github.com/OpenSprinkler/OpenSprinkler-Firmware)
that adds Docker packaging. The firmware itself is upstream's, so pulling their changes in is a
routine operation rather than a special event:

```bash
git fetch upstream
git merge upstream/master
git submodule update --init --recursive   # upstream moves the pins under external/
make clean && make VERSION=OSPI           # or ./build.sh -s demo for the hardware-free path
```

`upstream`'s push URL is deliberately set to `DISABLED`; this fork never pushes there.

**Expect conflicts in these files.** The fork is not purely additive — it edits upstream-owned
files as well as adding its own:

| File | Why it diverges |
|---|---|
| `defines.h` | `OS_FW_MINOR` bumped 5 → 6; OSPi default HTTP port is 88, not 8080 |
| `OpenSprinkler.cpp` | `reboot()` exits instead of spinning when the syscall is denied |
| `Makefile` | builds `smtp.c` with the project flags instead of make's built-in rule |
| `Dockerfile` | the fork's own multi-arch build |
| `.github/workflows/build-ci.yml` | publishes to GHCR and Docker Hub |
| `README.md`, `docs/docs/2.2.1/221_5_manual.md` | document the fork and the port change |

Files the fork adds outright — `docker-compose.yaml`, `docker-compose.pi.yaml`, `.env`,
`CLAUDE.md` — never conflict.

**`OS_FW_MINOR` is a standing collision.** This fork reports 2.2.1(6) while being based on
upstream's `221(5)` tag — the fork point *is* that tag, exactly. When upstream ships their own
2.2.1(6) the line conflicts, and the two builds then claim the same version while differing.
Resolve it deliberately; do not take either side automatically.

`build.sh` re-syncs and checks out `external/` at pinned revisions on every run, so it discards
local edits inside the submodules. Fork changes do not belong there.

### Releasing

`build-ci.yml` publishes `:master` on every push to master, but `:release` and `:latest` fire
only on a published GitHub release. Anything pinning this image — notably the
[Home Assistant add-on](https://github.com/rbhr/ha-app-OpenSprinkler-Server) — needs a real tag:

```bash
git tag v2.2.1.5-ospi.1 && git push origin v2.2.1.5-ospi.1
gh release create v2.2.1.5-ospi.1
```

The scheme is `v<upstream firmware version>-ospi.<packaging revision>`. Upstream's own tags
(`221(5)`) cannot be reused: parentheses are not legal in a Docker tag, and `metadata-action`'s
`type=ref,event=tag` uses the git tag verbatim as the image tag.

## Generated files — do not hand-edit

`htmls.h` (tracked in git) is generated from `html/*.html` by `compress_htmls.mjs`
(minify + gzip → PROGMEM byte arrays). PlatformIO runs it automatically via
`run_prebuild.py`. Edit the HTML in `html/`, then rebuild; if you build outside PlatformIO,
re-run `node compress_htmls.mjs` yourself.

## `.gitignore` traps

The ignore list is aggressive and pattern-based, not path-based:

```
*.sh   *.json   *.dat   *.o   *.bak   logs/**   OpenSprinkler
```

with only `!build.sh`, `!startOpenSprinkler.sh`, `!updater.sh`, `!package.json` rescued.
**Any new shell script or JSON file you add will be silently untracked** — add a `!`
negation to `.gitignore` (preferred) or `git add -f`. `package-lock.json` is likewise not
tracked. Always `git status --ignored` or `git check-ignore -v <file>` before assuming a
new file was committed.

`testmode.h` is also ignored: it is an optional local header, pulled in via
`#if __has_include("testmode.h")` in `OpenSprinkler.cpp`, carrying private production-test
WiFi credentials (template: `testmode.example.h`). Its presence forces WiFi STA mode via
`OpenSprinkler::wifi_testmode`.

## Architecture

**Control loop.** `main.cpp` is the scheduler and owns `do_setup()` / `do_loop()`.
On ESP8266 those are called from Arduino `setup()`/`loop()` (`mainArduino.ino`); on Linux
from `main()` at the bottom of `main.cpp`. `do_loop()` polls flow/current/sensors on
millisecond timers, services the network, then does per-second scheduling:
match programs → enqueue → `schedule_all_stations()` → `turn_on/off_station()` →
`os.apply_all_station_bits()`.

**`OpenSprinkler` (`OpenSprinkler.h/.cpp`)** is an all-static "god object" (`os`) holding
device state: `iopts[]` (integer options), `sopts[]` (string options), `station_bits[]`,
`nvdata`, `status`, sensor state, display, MQTT, OTC config. Persistence is per-concern flat
files (`iopts.dat`, `sopts.dat`, `stns.dat`, `prog.dat`, `nvcon.dat`, `sens.dat`, …), all
named in `defines.h`.

**Storage/wire formats are append-only.** The `IOPT_*` and `SOPT_*` enums are indices into
on-disk arrays and into the `/jo` and `/co` API — never reorder or delete an entry. Retired
options keep their slot and are marked `IOPT_FLAG_RETIRED` (see the `IOPT_*_RETIRED` names
and the parallel flash-resident metadata table `iopt_defs[NUM_IOPTS]`). The same rule
applies to sensor records, station data, and the X-macro unit lists.

**Programs & queue (`program.h/.cpp`).** `ProgramStruct` packs schedule types (weekday /
interval / monthly / single-run) and start-time encodings (fixed, sunrise/sunset ±offset)
into bitfields — read the comments in `program.h` before touching them. `ProgramData` owns
the runtime queue (`RuntimeQueueStruct`) and pause state.

**Stations.** `STN_TYPE_*` covers standard solenoid, RF, remote-IP, remote-OTC, GPIO, HTTP,
and HTTPS. Non-standard types stow their config in a fixed `STATION_SPECIAL_DATA_SIZE` blob
derived from `TMP_BUFFER_SIZE`, so new special-station structs must fit that budget.

**Web/API (`opensprinkler_server.cpp`).** Handlers are registered by two parallel arrays
near the bottom: `uris[]` (`"cv"`, `"jc"`, `"jp"`, …) and `urls[]` (handler functions).
**The two must stay index-aligned**, including inside the `#if defined(ESP8266)` /
`ENABLE_DEBUG` blocks. A new endpoint means an entry in both, plus a doc update in
`docs/docs/2.2.1/221_5_api.md`. Requests arrive through the OpenThings Framework
(`external/OpenThings-Framework-Firmware-Library`, `OTF_PARAMS_DEF` / `OTF_PARAMS` macros);
JSON responses are streamed with `BufferFiller` (`bfiller.h`) into a fixed `tmp_buffer`, so
watch `TMP_BUFFER_SIZE` when adding fields.

**Sensor subsystem (`sensors/`).** Newer than the rest of the tree and the only place with
real class polymorphism. `Sensor` is the abstract base; subclasses are `ADS1115Sensor`,
`AggregateSensor`, `WeatherSensor`, `SystemInternalSensor`, `OnboardDigitalSensor`, held in
a `SensorUnion` so they can be statically allocated — objects returned by `Sensor::get()` /
`Sensor::parse()` are static, never `delete` them. Records serialize as
`type, common_len, subclass_len, common payload, subclass payload`, deliberately append-only
so old `sens.dat` files stay readable. Units, unit groups, and aggregate actions are X-macro
lists (`SENSOR_UNIT_LIST` etc.) — append at the end. `SensorAdjustment` applies
piecewise-linear sensor curves to program watering durations.

Note the two unrelated "sensor" concepts: the legacy onboard `SENSOR1..4` GPIO inputs
(rain/flow/soil/program-switch, configured via `IOPT_SENSOR*_TYPE`, SN3/SN4 are OS 3.4-only)
versus the newer external sensor-expander records in `sens.dat` (`MAX_SENSORS` = 64).

**Weather (`weather.cpp`).** Periodically fetches from `weather.opensprinkler.com`
(overridable via `SOPT_WEATHERURL`) and applies watering-level scaling, sunrise/sunset,
timezone, external IP, and auto rain-delay — the `WEATHER_UPDATE_*` flags. Program durations
are the product of this weather percentage and any sensor adjustment
(`water_time_scale()` in `utils.cpp`).

**Notifications.** `notifier.cpp` fans a `NotifQueue` of `NOTIFY_*` events out to MQTT
(`mqtt.cpp`), IFTTT/web hooks, and email (`EMailSender.*` on ESP8266, `smtp.c/h` on Linux).

**Remote access.** OTC (OpenThings Cloud, `cloud.openthings.io`) provides cloud connectivity
and backs `STN_TYPE_REMOTE_OTC` stations; config lives in `SOPT_OTC_OPTS` / `OTCConfig`.

**Vendored code.** `ArduinoJson.hpp`, `smtp.c`, `EMailSender.*`, `RCSwitch.*`, `TimeLib.*`,
`I2CRTC.*`, `font.h`, `images.h` are third-party — avoid restyling them. `external/` are git
submodules; changes belong upstream. (`external/influxdb-cpp` is declared in `.gitmodules`
but is not referenced by any build file or source — it is checked out and unused.)

## Conventions

- C++14, **tabs** for indentation, GPLv3 header on new files.
- `unsigned char` is used pervasively instead of `uint8_t` in the older files; match the
  surrounding file rather than normalizing.
- `time_os_t` (`types.h`) is the firmware's time type — use it, not `time_t`.
- Bump `OS_FW_MINOR` / `OS_FW_VERSION` in `defines.h` when the API changes. Changing
  `OS_FW_VERSION` triggers an automatic factory reset on devices upgrading to that build.
- User-facing docs live in `docs/` (MkDocs → GitHub Pages). Recent history consistently
  updates the versioned manual and API reference under `docs/docs/2.2.1/` in the same commit
  as behavior/API changes; `docs/mkdocs.yml` nav must list any new page.
