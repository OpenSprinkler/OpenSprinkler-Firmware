# Local Changes Against CH390 0.4.1

Upstream: https://github.com/espressif/esp-eth-drivers/tree/master/ch390

The pre-existing patch in commit `f27b3ef` rejects RX lengths below the CRC size,
preventing unsigned underflow when subtracting four CRC bytes. Current local
hardening extends it as follows:

- Validate length and header flag before either reading or dropping a frame,
  including frames whose status indicates an error. Reject CRC-only frames too.
  Never move the RX pointer by an unvalidated wire length.
- Count bad ready bytes, bad headers, receive errors, local receive resets, TX
  timeouts, failed health checks, successful recoveries and failed recoveries.
  Snapshot through `esp_eth_mac_ch390_diagnostics()` under the MAC lock.
- Check TX availability before writing packet memory. A timed-out transmit must
  not advance the FIFO write pointer with an unsent packet.
- Serialize multi-register MAC/PHY operations and recovery with a recursive
  mutex, not just the individual SPI transactions. Do not hold this lock while
  delivering a received frame to lwIP.
- Check stable chip identity/MAC/RX-enable state about once a second. Three
  consecutive failed checks, or three corrupt-ready/header/TX-timeout incidents
  within ten seconds, request software reinitialization. CRC/error-status frames
  with valid framing are discarded and counted, not treated as chip failures.
  TX timeouts count toward escalation only after five seconds of observed busy
  with the physical link up and no successful TX-slot acquisition in between.
- Throttle local RX resets to one per second, preserving upstream's 1 ms settle
  delay between stopping RX and restarting/resetting the receive pointer.
  Full recovery has a 30-second
  minimum interval, doubling after failures up to five minutes. Reset the MAC in
  place and restore its address, multicast filters, receive mode and flow-control
  configuration. Retain the IDF interface, PHY object and IP configuration.
- Periodically inspect RX independently of the IRQ pin level, even if an edge
  was lost. Limit each receive batch to 16 frames before yielding. A full batch
  self-notifies the worker to continue after one RTOS tick, rather than waiting
  another second. This is not a 16-frames-per-second cap.

The shared C-compatible policy is in `include/ch390_health.h`; the ESP8266 raw
adapter uses the same thresholds. Neither path reboots the controller, changes
watering state, treats a missing cable/IP/Internet connection as a chip fault,
or attempts to power-cycle the module. The v3 adapter is polling-only and uses
no IRQ or reset pin. OS4 uses IRQ and hardware reset at initialization, but
runtime recovery currently uses software reset only. Hardware-reset escalation
is deliberately not enabled without bench validation of PHY/event handling.

The upstream CRC-underflow report is still a follow-up; this document does not
claim these changes have been submitted upstream or tested against newer SDKs.
