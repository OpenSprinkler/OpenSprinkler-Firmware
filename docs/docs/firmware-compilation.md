# Firmware Compilation

The firmware compilation instructions below are for OpenSprinkler **v3 and v4** only.
<br>For RPi and Linux-based OpenSprinkler (OSPi), follow the [OSPi installation instructions](https://openthings.freshdesk.com/support/solutions/articles/5000631599-installing-and-updating-the-unified-firmware-on-ospi).

## Environment Setup

1. Clone the firmware repository with required submodules, then enter the source directory:

        git clone --recurse-submodules https://github.com/OpenSprinkler/OpenSprinkler-Firmware.git
        cd OpenSprinkler-Firmware

2. Install the latest LTS version of Node.js from [https://nodejs.org/](https://nodejs.org/) if you don't already have it.
3. In the source directory, run `npm install html-minifier-terser`.
4. Install Visual Studio Code (VS Code) from [https://code.visualstudio.com/](https://code.visualstudio.com/), if you don't already have it.
5. Launch VS Code and install the **PlatformIO** extension.

---

## Building the Firmware

1. In VS Code, click `File -> Open Folder` and select the `OpenSprinkler-Firmware` folder.
2. PlatformIO will recognize the `platformio.ini` file in that folder, which contains all the libraries and settings needed to compile the firmware.
3. Select the PlatformIO build environment for your hardware:

    | Hardware | PlatformIO environment | OTA firmware artifact |
    |:---------|:-----------------------|:----------------------|
    | OpenSprinkler v3 (ESP8266) | `os3x_esp8266` | `.pio/build/os3x_esp8266/firmware.bin` |
    | OpenSprinkler v4 (ESP32-C6-N8) | `os4_esp32c6_n8` | `.pio/build/os4_esp32c6_n8/firmware.bin32n8` |

4. Click the **PlatformIO: Build** button (with the checkmark icon ✓) in the blue status bar at the bottom of the screen to build the firmware.

Alternatively, you can build from a terminal in the source directory:

```sh
# OpenSprinkler v3
pio run -e os3x_esp8266

# OpenSprinkler v4 (ESP32-C6-N8)
pio run -e os4_esp32c6_n8
```

For OpenSprinkler v4, use `firmware.bin32n8` for a normal firmware upload. PlatformIO also produces `firmware.factory.bin`, which includes the complete flash layout and is intended for factory or USB-C flashing rather than OTA upload.

---
