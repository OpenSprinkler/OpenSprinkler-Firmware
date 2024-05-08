# OpenSprinkler firmware with support added for ESP32.

Use at least 6.3.0 from framework-espressi32, since 6.2.0 contains F() macro bug! Use 'pio pkg update' from PIO Core Cli to upgrade!

Work is based on JaCharer's work (https://github.com/JaCharer/OpenSprinkler-Firmware-ESP32port) but was done from existing OpenSprinkler firmware.

**As of now - 2023.05.14 - NOTHING IS TESTED, but code compiles under PlatformIO(VSCode).**
**As of now - 2023.06.05 - Code is tested by some, runs on Wokwi simulator, but had to apply workaround after factory reset.**

** Before the merge of original master - 2024.05.06 - the 2.1.9 was working as reported by forum users, however it seemed that one of the sensors were faulty.
** As of now - 2023.05.08 - the OpenSprinkler master's branch has been merged, so firmware version is 2.2.0 (3), code compiles, my test device boots up nicely, connects to wifi nut no other functionality is tested yet

** Don't forget to change esp32.h according to your settings! It's set for my setup with ShiftRegister output and SH1106 LCD (instead of the default SSD1306) **

Hopefully I can test it in the coming days/weeks on my HW and remove this comment :)

HW is AC only, so no support, for DC/LATCH. Wired ethernet is also not supported now, maybe in the future.

LCD can be set also to SH1106 (1.3"), instead of SSD1306 (0.96").

Multiple outputs are supported: GPIO, ShiftRegister and the standard PCF8574/PCA9555A

OpenThingsLibrary is my fork, since there are some ESP32 related bugs. Will change when PR accepted.

Final comment. This is an experimental software in alpha stage, please be very careful connecting any external devices as errors may damage your device. You can use it, but it's at your own risk !

Know limitation
1. only AC configuration for now. 
2. ESP32 has many spare gpio pins to use (you can define them in PIN_FREE_LIST. However a way as UI is written prevents us from using it. Free GPIO pins are hard coded into javascript UI and available for PI a AVI version only. 
3. you need to be very careful when choosing GPIO pins for stations, buttons or sensors as some ESP32 may be 1 or 0 during startup or reboot, may not have a pullup resistors or transmit PWM signal... please refere to this article https://randomnerdtutorials.com/esp32-pinout-reference-gpios/ 
4. current measurement needs a special device (measurement resistor + amplifier) end even then analog read for ESP32 is up to 3,3v where 1V for ESP8266
5. you may have a problem with a relay board as in most cases 5V is needed. However you may shortcut the led diode and this allowed to drive optoisolators correctly with 3,3V signal however to make a relay coils trigel you need power board with 5V JVCC pin !!!
6. SPIFFS partion formating is not tested and may not work. If not... compile and upload any ESP32 SPIFF exampel skech... 

============================================
==== OpenSprinkler AVR/RPI/BBB Firmware ====
============================================

This is a unified OpenSprinkler firmware for Arduino, and Linux-based OpenSprinklers such as OpenSprinkler Pi.

Compilation instructions for OS (Arduino-based OpenSprinkler) 2.3 and 3.x:
* Install VS Code
* Launch VS Code, search and install the platformio extension.
* Download and unzip the OpenSprinkler firmware repository, open the folder in VS Code, at the bottom of the screen, click PlatformIO:Build. The firmware repository contains platformio.ini which has all the information needed for PlatformIO to build the firmware.
Additional details:
https://openthings.freshdesk.com/support/solutions/articles/5000165132

For OSPi/OSBO or other Linux-based OpenSprinkler:
https://openthings.freshdesk.com/support/solutions/articles/5000631599

============================================
Questions and comments:
http://www.opensprinkler.com
============================================
