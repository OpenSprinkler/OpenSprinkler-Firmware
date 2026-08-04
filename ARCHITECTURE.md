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

## Migration Order

1. Establish Demo API contract checks and keep all current platform builds green.
2. Centralize board capabilities and pin assignments.
3. Extract platform and hardware access behind narrow interfaces.
4. Split API handlers by domain without changing routes or JSON.
5. Move scheduling, station, program, and option logic out of the `OpenSprinkler` object.
6. Isolate persistence and add explicit migration tests before changing any data format.

Use temporary compatibility wrappers when needed, then remove them once all callers use the new boundary. Avoid broad renames or unrelated cleanup in migration commits.
