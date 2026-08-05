# Repository Guidelines

## Project Structure & Module Organization

This is the OpenSprinkler unified firmware for ESP8266/Arduino and Linux/Raspberry Pi (`OSPI`). Core C++ sources live at the repository root. Key files include `main.cpp`, `OpenSprinkler.cpp/.h`, `program.cpp/.h`, `opensprinkler_server.cpp/.h`, and `utils.cpp/.h`. Sensor implementations live in `sensors/`. Web UI sources are in `html/` and compile into generated `htmls.h`. Documentation is under `docs/`, examples under `examples/`, and vendored/submodule code under `external/`.

## Build, Test, and Development Commands

- `make`: build the Linux/OSPI `OpenSprinkler` binary.
- `make VERSION=DEMO`: build the demo/simulation target.
- `make clean`: remove compiled objects and the binary.
- `sudo bash build.sh -s`: install Linux dependencies, initialize submodules, and build OSPI without the service prompt.
- `sudo bash build.sh -s demo`: build the CI demo target.
- `npm install && node compress_htmls.mjs`: regenerate `htmls.h` from `html/*.html`.
- `pio run -e os3x_esp8266`: build ESP8266 firmware with PlatformIO.
- `docker build -t opensprinkler .`: build the container image.

## Coding Style & Naming Conventions

Use C++14-compatible code. Follow the style in the file being edited: local indentation, brace placement, and compact embedded-friendly patterns. Keep platform-specific logic behind `#if defined(ESP8266)` or Linux-side `OSPI`/`DEMO` guards. Prefer wrappers in `utils.h`, `gpio.h`, and `i2cd.h` over direct platform calls. Use lowercase snake_case for free functions and files, PascalCase for classes, and existing `IOPT_*`, `SOPT_*`, and `STN_TYPE_*` constants.

## Testing Guidelines

There is no formal main-repo unit test suite; `npm test` is a placeholder. Validate changes by building affected targets: `make` or `make VERSION=DEMO` for Linux work, and `pio run -e os3x_esp8266` for firmware-sensitive changes. For API, scheduling, persistence, or sensor changes, run a manual demo/device regression and confirm generated files change only when intended.

## Commit & Pull Request Guidelines

Recent commits use concise, imperative summaries such as `add missing sensors folder` or `support multiple formats of /jsl output`. Keep subjects specific and behavior-focused. Pull requests should name target platform(s), list build commands run, mention data-file or API compatibility impact, and include screenshots only for `html/` UI or documentation visual changes.

## Agent-Specific Instructions

Do not edit generated artifacts such as `htmls.h` directly when the source is in `html/`; regenerate them. Treat `external/`, `.pio/`, `node_modules/`, build outputs, `.dat` files, and `logs/` as dependency or runtime state unless explicitly targeted.

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
