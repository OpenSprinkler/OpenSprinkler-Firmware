# Repository Guidelines

## Project Structure & Module Organization

This is the OpenSprinkler unified firmware for ESP8266/Arduino and Linux/Raspberry Pi (`OSPI`). Root files such as `main.cpp`, `OpenSprinkler.cpp/.h`, and `utils.cpp/.h` provide entrypoint, controller-facade, and shared utility code. Domain code is organized into `core/`, `api/`, `services/`, `sensors/`, and `storage/`; hardware boundaries live in `boards/`, `drivers/`, and `platform/`. Tests are under `tests/`. Web UI sources in `html/` generate `htmls.h`, documentation is under `docs/`, and dependencies/submodules are under `external/`. See `ARCHITECTURE.md` for dependency boundaries.

## Build, Test, and Development Commands

- `make`: build the Linux/OSPI `OpenSprinkler` binary.
- `make VERSION=DEMO`: build the demo/simulation target.
- `make clean`: remove compiled objects and the binary.
- `sudo bash build.sh -s`: install Linux dependencies, initialize submodules, and build OSPI without the service prompt.
- `sudo bash build.sh -s demo`: build the CI demo target.
- `make test-api`: run focused C++ tests, build Demo, and verify core HTTP contracts.
- `make test-board-profiles`, `make test-hardware-detection`, `make test-storage-files`: run focused native tests.
- `npm ci && node compress_htmls.mjs`: regenerate `htmls.h` reproducibly from `html/*.html`.
- `pio run -e os3x_esp8266`: build ESP8266 firmware with PlatformIO.
- `docker build -t opensprinkler .`: build the container image.

## Coding Style & Naming Conventions

Use C++14-compatible code and follow local indentation and brace style. Include project headers by repository-root-relative path, for example `core/program.h` or `platform/gpio.h`. New platform-specific behavior belongs behind `platform/`, `boards/`, or `drivers/` interfaces; prefer `platform/gpio.h`, `platform/i2c.h`, `platform/clock.h`, `storage/files.h`, and board-profile APIs over direct OS or hardware calls. Use lowercase snake_case for free functions and files, PascalCase for classes, and existing `IOPT_*`, `SOPT_*`, and `STN_TYPE_*` constants.

## Testing Guidelines

Focused native tests cover board profiles, hardware detection, storage, and API contracts. Run the relevant focused target during development and `make test-api` for cross-domain changes; note that it cleans and rebuilds Demo. Build affected platforms with `make`, `make VERSION=DEMO`, and/or `pio run -e os3x_esp8266`. Hardware-sensitive scheduling, sensor, GPIO, and networking changes still require device regression testing.

## Commit & Pull Request Guidelines

Recent commits use concise, imperative summaries such as `add missing sensors folder` or `support multiple formats of /jsl output`. Keep subjects specific and behavior-focused. Pull requests should name target platform(s), list build commands run, mention data-file or API compatibility impact, and include screenshots only for `html/` UI or documentation visual changes.

## Agent-Specific Instructions

Do not reintroduce root forwarding headers; include the owning subdirectory directly. When adding a source directory, register it in both the Makefile source lists and PlatformIO `build_src_filter`. Do not edit generated artifacts such as `htmls.h` directly when the source is in `html/`; regenerate them. Treat `external/`, `.pio/`, `node_modules/`, build outputs, `.dat` files, and `logs/` as dependency or runtime state unless explicitly targeted.

## Cross-Review Protocol (Claude second opinion)

Claude Code acts as a skeptical second reviewer. For any **non-trivial** change (skip typo/format/one-liner fixes), consult it at two checkpoints:

1. **Before implementing** — pipe your proposed plan for a critique, and address it (or justify skipping) before writing code:
   ```
   echo "<your plan>" | ./tools/claude-review.sh plan
   ```
2. **After implementing** — before declaring the work done, get a diff review and resolve or explicitly wave off each finding:
   ```
   ./tools/claude-review.sh diff        # or: staged / range <A..B>
   ```

Surface Claude's feedback to the user **verbatim, grouped by severity**, and state what you changed in response to each point (or why you disagree). The human makes the final call to proceed at each checkpoint — do not treat Claude's review as either an automatic approval or an automatic blocker.

The reviewer runs read-only (it verifies against the codebase but never edits). Its persona lives in `tools/claude-reviewer-prompt.md`; keep it in sync if review priorities change.
