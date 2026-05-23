# Sonoff 4CH Pro R3 Support

## Overview

This document describes the Sonoff 4CH Pro R3 support added to the OpenSprinkler firmware.

## Hardware

The Sonoff 4CH Pro R3 is an ESP8285-based 4-channel relay board. Unlike genuine OpenSprinkler hardware, it has **no I2C IO expanders** — relays are driven directly by GPIO pins.

### Pin Mapping

| Function | GPIO | Notes |
|----------|------|-------|
| Relay 1 | GPIO12 | Active HIGH |
| Relay 2 | GPIO5  | Active HIGH. **Also ESP8266 I2C SCL** |
| Relay 3 | GPIO4  | Active HIGH. **Also ESP8266 I2C SDA** |
| Relay 4 | GPIO15 | Active HIGH |
| LED     | GPIO13 | Inverted (LOW = ON) |
| Button 1| GPIO0  | **Holding low at boot enters flash mode** |
| Button 2| GPIO9  | Active LOW |
| Button 3| GPIO10 | Active LOW |
| Button 4| GPIO14 | Active LOW. **SPI CLK on boot, must reclaim** |
| Sensor 1| GPIO16 | INPUT_PULLUP (rain/flow sensor) |

### Button Wiring

Each button has its **own independent GPIO** (not a matrix). They are active LOW with internal pullups:
- Button 1: GPIO0 (pulldown to GND when pressed)
- Button 2: GPIO9 (pulldown to GND when pressed)
- Button 3: GPIO10 (pulldown to GND when pressed)
- Button 4: GPIO14 (pulldown to GND when pressed)

### Software Button Swap

Button 1 and Button 4 are swapped in software:
- `PIN_BUTTON_1 = GPIO14` (physical Button 4)
- `PIN_BUTTON_4 = GPIO0` (physical Button 1)

This avoids GPIO0 being held low during boot (which would enter flash mode).

---

## Build Configuration

### PlatformIO

Added `sonoff_4ch_pro_r3` environment to `platformio.ini`:

```ini
[env:sonoff_4ch_pro_r3]
platform = espressif8266
board = esp8285
framework = arduino
build_flags =
    -DSONOFF_4CH_PRO_R3
    ${common.build_flags}
```

The `SONOFF_4CH_PRO_R3` build flag isolates all Sonoff-specific changes.

---

## Firmware Changes

### 1. Pin Definitions (`defines.cpp`, `defines.h`)

- Added `PIN_BUTTON_4` definition in `defines.h`
- Sonoff pins are statically assigned in `defines.cpp` under `#ifdef SONOFF_4CH_PRO_R3`
- `PIN_RELAY_5 = 255` (no external relay)
- Default OS3.x pins remain at 255 for hardware auto-detection on genuine boards

### 2. I2C Remapping (`OpenSprinkler.cpp`)

**Problem:** The ESP8266 `Wire.begin()` defaults to GPIO4 (SDA) and GPIO5 (SCL) — the same pins used by Relay 2 and Relay 3. Every I2C transaction leaves these pins in `INPUT_PULLUP` mode, which cannot drive relay coils.

**Fix:** Remap I2C to unused pins:

```cpp
#if defined(SONOFF_4CH_PRO_R3)
    Wire.begin(2, 13);  // SDA=GPIO2 (unused), SCL=GPIO13 (LED, harmless to flicker)
#else
    Wire.begin();       // default I2C pins
#endif
```

This ensures any I2C activity (SSD1306 display updates, etc.) never touches the relay pins.

### 3. Per-Loop Pin Reclamation (`main.cpp`)

**Problem:** Even with I2C remapped, other code may temporarily reconfigure GPIO4/GPIO5. The `apply_all_station_bits()` only runs once per second, leaving a window where relays could drop out.

**Fix:** At the end of `do_loop()` (runs hundreds of times per second), force all relay pins back to `OUTPUT` and re-drive them to their current station bit state:

```cpp
#if defined(SONOFF_4CH_PRO_R3)
for(unsigned char sid=0; sid<os.nstations && sid<4; sid++) {
    // ... read station bit state ...
    pinMode(pin, OUTPUT);
    digitalWrite(pin, val);
}
#endif
```

### 4. Direct GPIO Relay Driving (`OpenSprinkler.cpp`)

In `apply_all_station_bits()`, added direct GPIO driving for Sonoff:

```cpp
#if defined(SONOFF_4CH_PRO_R3)
for(unsigned char sid=0; sid<nstations && sid<4; sid++) {
    // read station bit, reclaim pin as OUTPUT, drive HIGH/LOW
}
#endif
```

In `set_station_bit()`, added per-relay `pinMode(OUTPUT)` + `digitalWrite()` to ensure relays activate immediately even if I2C has reconfigured the pins.

### 5. Physical Buttons (`main.cpp`)

**Problem:** The original `button_read()` function is designed for genuine OS3.x hardware with a button matrix and LCD UI state machine. It does not work with independent GPIO buttons.

**Fix:** In `ui_state_machine()`, bypass `button_read()` entirely for Sonoff:

```cpp
#if defined(SONOFF_4CH_PRO_R3)
pinMode(14, INPUT_PULLUP);  // reclaim from SPI
unsigned char btn_state = 0;
if (digitalRead(0) == 0)       btn_state = 1;
else if (digitalRead(9) == 0)  btn_state = 2;
else if (digitalRead(10) == 0) btn_state = 3;
else if (digitalRead(14) == 0) btn_state = 4;
```

**Debounce:** Only act on a **new press** (transition from 0 to a button number) with a 200ms debounce timer to prevent mechanical bounce from toggling the relay twice.

**Queue Management:** Button presses use the **same runtime queue** as the web UI:
- **Turn ON:** `pd.enqueue()` + `schedule_all_stations()` + `set_station_bit(sid, 1)`. Duration is 64800 seconds (18 hours, max `uint16_t`).
- **Turn OFF:** Set `deque_time = now` + `turn_off_running_station_immediate()` + `set_station_bit(sid, 0)`.

This ensures the main loop's queue-based station management works correctly and doesn't clear the relay.

### 6. Station Count

`nboards = 1` (8 stations: 4 physical relays + 4 placeholders). Set explicitly in `begin()` for Sonoff, since there are no I2C expanders to auto-detect.

### 7. I2CRTC Skip (`I2CRTC.cpp`)

Skipped `Wire.begin()` in the `I2CRTC` constructor and skipped `RTC.detect()` in `OpenSprinkler::begin()` for Sonoff. No RTC chip exists on the Sonoff board.

### 8. Debug Cleanup

Removed all `Serial.print()` debug statements from:
- `set_station_bit()` (relay on/off logging)
- `button_read()` (GPIO state dumps)
- `ui_state_machine()` (button press logging)
- Boot relay self-test (4 relay clicks at startup)

---

## Root Cause Analysis

### Why Relays 2 and 3 Initially Failed

The ESP8266 `Wire` library uses **open-drain signaling**:
- To drive HIGH: sets pin to `INPUT_PULLUP` (weak ~40kΩ pullup)
- To drive LOW: sets pin to `OUTPUT LOW`

After every I2C transaction, GPIO4 (SDA) and GPIO5 (SCL) are left in `INPUT_PULLUP` mode. The `lcd_print_time()` function updates the SSD1306 OLED over I2C every second. Even though the Sonoff has no OLED, the firmware still calls these routines.

With `INPUT_PULLUP`, the relay board's transistor driver doesn't get enough base current to stay saturated. Relay 2 (GPIO5) and Relay 3 (GPIO4) drop out after each I2C transaction. Then `apply_all_station_bits()` (once per second) would reclaim them as OUTPUT and turn them back on — causing rapid blinking.

Buttons 1 and 4 (GPIO12/GPIO15) were never affected because they're not I2C pins.

### Why the Reference Fork Works

The reference fork stripped out all I2C code entirely. Without `Wire.begin()`, GPIO4/GPIO5 never get reconfigured to INPUT_PULLUP, so `digitalWrite()` works correctly.

---

## Verification Steps

1. Build: `pio run -e sonoff_4ch_pro_r3`
2. Flash: `pio run -e sonoff_4ch_pro_r3 --target upload`
3. Web UI → Manual Run → test each station 1-4
4. Press each physical button to toggle its relay
5. Verify relays stay on until toggled off (not just 60 seconds)
6. Both `sonoff_4ch_pro_r3` and `os3x_esp8266` builds should succeed

---

## Important Notes

- **I2C remapping is critical:** Without `Wire.begin(2, 13)`, I2C display updates will corrupt GPIO4/GPIO5 and cause Relay 2/3 to blink or fail.
- **Per-loop reclamation is a safety net:** It ensures relay pins are never left in INPUT mode for more than a few milliseconds.
- **Button 1 (GPIO0):** Holding this button during boot enters flash mode. It is mapped to `PIN_BUTTON_4` in software so the physical Button 1 acts as logical Button 4.
- **Button 4 (GPIO14):** `SPI.begin()` during `start_ether()` reconfigures GPIO14 as SPI CLK (OUTPUT LOW). It must be reclaimed as `INPUT_PULLUP` before reading buttons.
- **Rain sensor:** `PIN_SENSOR1 = GPIO16` is configured as `INPUT_PULLUP`. With no sensor connected, it reads HIGH ("no rain"), so scheduled programs run normally.
