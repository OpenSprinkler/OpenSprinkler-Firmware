# Project-Local Updater HTTP Servers

These copies serve only the controller's dedicated port-8080 firmware updater.
OTF's normal HTTP/cloud parser is unchanged. Do not patch the shared PlatformIO
package installation.

- `updater-esp8266`: ESP8266 Arduino core 3.1.2 ESP8266WebServer.
- `updater-esp32`: Arduino-ESP32 core 3.3.12 WebServer.

Upstream LGPL notices and license text are retained. The bounded-parser approach follows the
OpenGarage updater work; this adaptation retains OpenSprinkler's small plain
descriptor POST and query-string credentials.

Local changes: shared byte/line deadlines and byte limits, pre-body admission
after query/header parsing, a 1024-byte descriptor limit, a 3 MiB multipart cap,
70-byte boundary cap, 32-query-argument cap before argument allocation, no
multipart text fields, and safe EOF/abort handling.
Headers have a 5-second total budget and 8 KiB cap; lines are capped at 1024
bytes. Descriptor bodies have a 5-second budget. Authorized image bodies have
a 10-second idle deadline and 300-second total request deadline, matching the
page's upload timeout. All elapsed checks are wrap-safe.

Authentication must succeed before any body is read. Admission must not consume
upload tokens or change transfer state. Credentials supplied only after the file
in legacy multipart forms are rejected; reload the current updater page.
The page first calls the read-only `/update/check` endpoint so a wrong password
or busy refusal arrives before a large body can cause a browser-side TCP reset.
The upload itself still independently rechecks authorization.

Custom uploaders must include the final CRLF after the closing multipart
delimiter (`--boundary--\r\n`). Omitting that line break aborts the request
without installing the image. Standard browsers, curl, Python requests and Go
send this ending. Malformed, over-limit or URL-encoded requests may be closed
without an HTTP error response before application admission; the current page
does not send these shapes.

These remain synchronous parsers: authorized uploads pause application work.
Bounded requests are not a general LAN denial-of-service defense. Rebase these
small parser modifications and rerun the updater tests whenever upgrading cores.

Validation:
- `make test-updater-security`: production readers, both multipart parsers,
  admission callbacks and update service with host clock/flash/crypto adapters.
- `make test-updater-page`: mocked browser uploads (requires Playwright; use
  `BROWSER_PATH` for an installed Chromium and `NODE_PATH` if installed separately).
- `UPDATER_GENERATED=1 make test-updater-page` checks the compressed page too.
- Build both embedded targets, then bench-test valid manual/signed updates,
  wrong-password correction, interrupted uploads and post-update reboot.
