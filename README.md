# OpenSprinkler Firmware — Docker fork

A fork of the [OpenSprinkler unified firmware](https://github.com/OpenSprinkler/OpenSprinkler-Firmware)
packaged as a Docker container, so an OpenSprinkler Pi controller (or a
hardware-free simulation of one) can be run with `docker compose up -d` instead
of a native build and a systemd unit.

The firmware itself is upstream's. This fork adds the packaging around it.

### View Full Documentation

[![MkDocs](https://img.shields.io/badge/docs%20by-MkDocs-1f77b4?style=for-the-badge&logo=readthedocs)](https://opensprinkler.github.io/OpenSprinkler-Firmware/)

The user manual and API reference are upstream's and apply unchanged — start
there for anything about programs, stations, sensors or the HTTP API:

* [Full documentation](https://opensprinkler.github.io/OpenSprinkler-Firmware/) (MkDocs)
* [User manual](https://opensprinkler.github.io/OpenSprinkler-Firmware/2.2.1/221_5_manual/)
* [API reference](https://opensprinkler.github.io/OpenSprinkler-Firmware/2.2.1/221_5_api/)
* [Upstream repository](https://github.com/OpenSprinkler/OpenSprinkler-Firmware)

## How this fork differs

| Change | Notes |
|---|---|
| Docker packaging | Multi-arch images (amd64, arm64, arm/v7) published to GHCR and Docker Hub, plus `docker-compose.yaml` and a Raspberry Pi hardware overlay |
| **OSPi default HTTP port is `88`, not `8080`** | Applies to OSPi and DEMO builds only; OpenSprinkler v3 stays on `80`. Existing installs keep whatever port their `iopts.dat` already holds — see below |
| `reboot()` handling | Exits instead of spinning when the syscall is denied, so a container restart policy can do the restart |
| `smtp.c` build fix | Built with the project's flags rather than make's built-in rule |

> **Upgrading an existing install:** the port change only affects a *fresh*
> install or a factory reset, because defaults are consulted only when
> `iopts.dat` is absent. Carrying over a `./data` from firmware 2.2.1(5) or
> earlier? It will still be on `8080` — either set `ports: "88:8080"` in
> `docker-compose.yaml` or change the HTTP Port in the UI. The container side of
> the mapping **must** match the firmware's own port or the container is
> unreachable.

## Quick start (Docker Compose)

```sh
git clone https://github.com/rbhr/OpenSprinkler-Firmware.git
cd OpenSprinkler-Firmware
mkdir -p ./data          # create it yourself so it is not owned by dockerd
docker compose up -d
```

The web UI is then on <http://localhost:88>. The default device password is
`opendoor` — change it on first use.

**On a Raspberry Pi driving real zones**, uncomment one line in `.env` to make
the GPIO/I2C overlay stick to every `docker compose` command:

```sh
COMPOSE_FILE=docker-compose.yaml:docker-compose.pi.yaml
```

Do not pass `-f docker-compose.yaml -f docker-compose.pi.yaml` by hand instead —
it works once, but the next bare `docker compose up -d` silently recreates the
container *without* hardware access, and nothing reports it. The UI stays up,
the healthcheck stays green, and no valve ever opens.
[Details](README_Docker.md#running-with-docker-compose-recommended).

## Plain `docker run`

```sh
mkdir -p ~/opensprinkler
docker run -d \
  --name opensprinkler \
  --publish 88:88 \
  --restart unless-stopped \
  --volume ~/opensprinkler:/data \
  --device /dev/gpiochip0 \
  --device /dev/i2c-1 \
  ghcr.io/rbhr/opensprinkler-firmware:master
```

Drop both `--device` flags on a host without sprinkler hardware. Never run with
no volume: the image declares `VOLUME /data`, so Docker creates an *anonymous*
volume that is discarded on every image upgrade.

## Prebuilt images

```sh
docker pull ghcr.io/rbhr/opensprinkler-firmware:master     # GitHub Container Registry
docker pull richardrundle/opensprinkler:master             # Docker Hub (mirror)
```

| Tag | Published on |
|---|---|
| `master` | every push to `master` |
| `<git tag>` | every published release |
| `release` | every non-prerelease release |
| `latest` | most recent non-prerelease release |

## Building

Submodules are required for every build — `external/` holds them, and
`.dockerignore` excludes `.git`, so the image build cannot fetch them itself:

```sh
git submodule update --init --recursive
```

**The container image:**

```sh
docker build -t opensprinkler .
docker build -t opensprinkler --build-arg BUILD_VERSION=DEMO .   # simulation target
```

**Natively on Linux/OSPi** (Debian-ish; apt-installs dependencies, needs root):

```sh
sudo ./build.sh                  # OSPi hardware build
sudo ./build.sh -d               # adds -DENABLE_DEBUG -DSERIAL_DEBUG
sudo ./build.sh -s demo          # simulation, non-interactive, no hardware libs
```

Then run it with `./OpenSprinkler -d <datadir>`.

**ESP8266 firmware for OpenSprinkler v3** (needs PlatformIO and Node):

```sh
npm install html-minifier-terser
pio run --environment os3x_esp8266
```

Gotchas worth knowing:

* `platformio.ini`'s `linux` and `demo` environments exist only for editor
  syntax highlighting — they do not produce working binaries.
* `make VERSION=DEMO` still links `-li2c -llgpio`, so it needs the Linux I2C/GPIO
  dev packages. `./build.sh -s demo` is the genuinely hardware-free path.
* Object files carry no target suffix, so `make clean` between `VERSION=` changes.
* `build.sh` re-checks-out submodules at pinned revisions on every run and will
  discard local edits inside `external/`.

## Persistent data

Everything the firmware persists lives in the directory passed via `-d`, which
the image sets to `/data`:

```
/data/iopts.dat     integer options          /data/done.dat    factory-reset marker
/data/sopts.dat     string options           /data/sens.dat    external sensor defs
/data/stns.dat      stations                 /data/senadj.dat  per-program sensor adj.
/data/prog.dat      programs                 /data/logs/       run + sensor logs
/data/nvcon.dat     non-volatile controller data
```

`sopts.dat` holds MQTT, SMTP and IFTTT credentials, so treat the directory as
sensitive (`chmod 700 ./data`). The container runs as root, so bind-mounted
files are created root-owned on the host.

Note that the Docker image cannot self-update: in-app firmware update shells out
to `updater.sh` in the data directory, which on OSPi must be the git checkout.
Upgrade by pulling a new image instead.

## More detail

[**README_Docker.md**](README_Docker.md) covers the same ground in more depth,
plus hardware access, the multi-arch build, and publishing setup for
maintainers. [CLAUDE.md](CLAUDE.md) is a map of the source tree.

## License

GPLv3, as upstream — see [LICENSE.txt](LICENSE.txt).
