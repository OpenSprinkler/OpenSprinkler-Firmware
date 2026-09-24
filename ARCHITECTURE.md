# Architecture Migration

Firmware 2.2.1(6) will restructure the existing implementation incrementally. The current HTTP API, persistent `.dat` formats, and behavior on ESP8266, OSPI, and Demo remain compatibility boundaries. ESP32 support should enter through the new platform and board boundaries rather than add conditionals throughout existing code. Each migration step must leave supported targets buildable and should be independently reviewable.

## Target Layout

Use shallow, responsibility-based directories. Do not create a directory until it has a meaningful group of files to contain.

- `core/`: controller state, scheduling, programs, stations, and options
- `api/`: route registration, handlers, and response formatting
- `services/`: MQTT, OTC, weather, notifications, email, and other integrations
- `platform/`: ESP8266, ESP32, and Linux operating-system adapters
- `drivers/`: reusable hardware drivers such as ADC, GPIO expanders, display, RTC, and RF
- `boards/`: board capabilities, hardware detection, and pin assignments
- `storage/`: persistent data and logging
- `util/`: small platform-independent helpers
- `sensors/`: the existing expanded-sensor domain

Directories should normally contain files directly; deeper nesting is reserved for a group that becomes large enough to justify it.

## Dependency Direction

`core/`, `sensors/`, and API contracts must not depend on a concrete board or operating system. They use narrow interfaces supplied by `platform/`, `drivers/`, and `storage/`. Board profiles compose pins and capabilities without embedding controller behavior. Platform implementations are selected by the build rather than scattered conditionals where practical.

Embedded paths should continue to favor static storage, explicit ownership, and bounded buffers. New abstractions must not introduce routine heap allocation on ESP8266.

## Current Boundaries

- `boards/` owns immutable board profiles and hardware detection.
- `platform/` owns Linux/Arduino GPIO, Linux I2C, and Linux clock adapters.
- `drivers/` owns ADC, RTC, I/O expander, RF, display, and USB-PD hardware access.
- `core/` owns program data and runtime scheduling policy.
- `api/` owns HTTP parsing, route registration, server implementation, and domain handlers.
- `services/` owns MQTT, weather, notifications, email, and network provisioning integrations.
- `storage/` owns cross-platform file access and sprinkler-log persistence.
- `sensors/` remains the expanded-sensor domain.

Internal includes now reference their owning subdirectory directly; the temporary root forwarding headers have been removed. New includes should use repository-root-relative paths. `main.cpp` remains the platform entrypoint and polling orchestrator. `OpenSprinkler` remains the compatibility state facade; splitting that state requires a separate behavioral redesign and is not part of this file migration.

## Completed Migration

1. Established Demo API contract checks and board, hardware-detection, and storage tests.
2. Centralized board capabilities, pin assignments, and hardware detection.
3. Extracted platform adapters and reusable hardware drivers.
4. Centralized HTTP infrastructure and route registration, and established domain handler boundaries.
5. Moved program data and runtime scheduling out of the root entrypoint.
6. Isolated file access and sprinkler logging without changing persistent formats.

## Follow-up Work

Future changes can split the remaining controller state, options, and station storage from `OpenSprinkler` behind explicit interfaces. That work should be driven by ESP32 requirements and accompanied by focused state-transition and persistence tests; it should not be mixed into mechanical file moves.

Avoid reintroducing compatibility wrappers, broad renames, or unrelated cleanup in future migration commits.
