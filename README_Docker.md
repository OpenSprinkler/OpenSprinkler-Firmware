# Running OpenSprinkler via Docker

## Pulling a prebuilt image

Every push to `master` and every published release is built and pushed
automatically by `.github/workflows/build-ci.yml` as a multi-arch manifest
covering **linux/amd64, linux/arm64 and linux/arm/v7**, so the same tag works on
an x86-64 server, a 64-bit Raspberry Pi OS install, and a 32-bit one. Docker
selects the right architecture for the host.

```sh
docker pull ghcr.io/rbhr/opensprinkler-firmware:master     # GitHub Container Registry
docker pull rbhr/opensprinkler:master                      # Docker Hub (mirror)
```

| Tag | Published on |
|---|---|
| `master` | every push to `master` |
| `<git tag>` | every published release |
| `release` | every non-prerelease release |
| `latest` | most recent non-prerelease release |

## Running with Docker Compose (recommended)

```sh
mkdir -p ./data          # create it yourself so it is not owned by dockerd
docker compose up -d
```

The web UI is then on <http://localhost:8080>.

`docker-compose.yaml` on its own grants **no** hardware access, so it runs
anywhere — an x86-64 box, a laptop, a Pi. On a Raspberry Pi driving real zones,
enable the hardware overlay by uncommenting one line in `.env`:

```sh
COMPOSE_FILE=docker-compose.yaml:docker-compose.pi.yaml
```

then use `docker compose up -d` as normal.

The overlay is a separate file on purpose: a `devices:` entry naming a node that
does not exist makes `docker compose up` fail outright, so listing
`/dev/gpiochip0` in the base file would break every non-Pi host.

> **Do not pass `-f docker-compose.yaml -f docker-compose.pi.yaml` by hand
> instead.** It works once, but both files describe the same container, so the
> next bare `docker compose up -d` or `docker compose pull && docker compose up
> -d` recomputes the config from the base file alone and **recreates the
> container without GPIO/I2C**. Nothing reports it: a failed `lgGpiochipOpen()`
> only trips a `DEBUG_PRINTLN`, which compiles to nothing in release builds, so
> the UI stays up, the healthcheck stays green, programs appear to run — and no
> valve ever opens. Setting `COMPOSE_FILE` in `.env` makes the overlay sticky.

### Hardware access

The OSPI build touches exactly two device classes, so it does **not** need
`--privileged` or `-v /dev:/dev`:

| Device | Used for |
|---|---|
| `/dev/gpiochip0` (`gpiochip4` on Pi 5) | station and sensor pins via lgpio |
| `/dev/i2c-1` | zone/sensor expanders, RTC, ADS1115 |

lgpio is a character-device (ioctl) library, so it needs neither `/dev/mem` nor
`CAP_SYS_RAWIO`. Avoiding privileged mode also keeps `CAP_SYS_BOOT` away from a
process that calls `reboot(RB_AUTOBOOT)`.

I2C must be enabled on the host first (`dtparam=i2c_arm=on`; `./build.sh` does
this for you). If it is not, the container fails to start with a missing-device
error rather than silently coming up with no sensors.

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

`sopts.dat` holds the MQTT, SMTP and IFTTT credentials, so treat the directory
as sensitive (`chmod 700 ./data`). There is no separate IFTTT key file.

The container runs as root, so bind-mounted files are created root-owned on the
host. See the commented `user:` / `group_add:` block in `docker-compose.pi.yaml`
to run as your own uid instead.

> **Note:** the Dockerfile declares `VOLUME /data`. If you run the image with no
> volume at all, Docker silently creates an *anonymous* volume — config then
> survives `docker stop`/`start`, but is discarded the moment the container is
> recreated, which is every image upgrade. Always mount something.

## Running with plain `docker run`

```sh
mkdir -p ~/opensprinkler
docker run -d \
  --name opensprinkler \
  --publish 8080:8080 \
  --restart unless-stopped \
  --volume ~/opensprinkler:/data \
  --device /dev/gpiochip0 \
  --device /dev/i2c-1 \
  ghcr.io/rbhr/opensprinkler-firmware:master
```

Drop both `--device` flags on a host without sprinkler hardware.

## Building the image yourself

Submodules must be checked out first — `.dockerignore` excludes `.git`, so the
build cannot fetch them itself:

```sh
git submodule update --init --recursive
docker build -t opensprinkler .
```

Notes:

* Requires a Docker version supporting multi-stage builds (>= 17.06).
* The image builds the hardware-enabled `OSPI` target by default. For the
  simulation target, pass `--build-arg BUILD_VERSION=DEMO`.
* Multi-arch builds use `docker buildx` with QEMU, as CI does.

## Publishing (maintainers)

CI pushes to GHCR using the built-in `GITHUB_TOKEN` with no setup. Publishing
runs automatically on every push to `master` and on every published release, and
can also be triggered on demand:

```sh
gh workflow run build-ci.yml --ref master
```

An on-demand run tags exactly as a branch push does (`:master`); it never moves
`:latest` or writes the `:release` tag.

**One-time step after the first successful publish:** a container package
created by `GITHUB_TOKEN` is **private** by default, and `packages: write` does
not make it anonymously readable. Until you flip it, every `docker pull` and
`docker compose up` above fails with `denied`/`unauthorized`. Set it at
*Packages → opensprinkler-firmware → Package settings → Change visibility →
Public*.

The Docker Hub mirror is optional and stays switched off until both of these
exist in **Settings → Secrets and variables → Actions**:

| Kind | Name | Value |
|---|---|---|
| Variable | `DOCKERHUB_USERNAME` | your Docker Hub account, e.g. `rbhr` |
| Secret | `DOCKERHUB_TOKEN` | a Docker Hub **Personal Access Token** (not your password) |
| Variable *(optional)* | `DOCKERHUB_REPOSITORY` | overrides the default `<username>/opensprinkler` |

The namespace is a *variable*, not a secret, deliberately: Actions masks secret
values everywhere, so a secret-derived namespace would render every tag as
`docker.io/***/opensprinkler` and produce a broken image reference.

With neither configured — including on forks, which receive no secrets — the
workflow publishes to GHCR only and logs a notice instead of failing.
