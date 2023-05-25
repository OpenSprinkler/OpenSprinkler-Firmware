# OpenSprinkler firmware with support added for ESP32.

Work is based on JaCharer's work (https://github.com/JaCharer/OpenSprinkler-Firmware-ESP32port) but was done from existing OpenSprinkler firmware.

**As of now - 2023.05.14 - NOTHING IS TESTED, but code compiles under PlatformIO(VSCode).**

** Don't forget to change esp32.h according to your settings! It's set for my setup with ShiftRegister output and SH1106 LCD (instead of the default SSD1306) **

Hopefully I can test it in the coming days/weeks on my HW and remove this comment :)

HW is AC only, so no support, for DC/LATCH. Wired ethernet is also not supported now, maybe in the future.

LCD can be set also to SH1106 (1.3"), instead of SSD1306 (0.96").

Multiple outputs are supported: GPIO, ShiftRegister and the standard PCF8574/PCA9555A

OpenThings lib is not 100%, the following 2 functions must be added to Esp32LocalServer.cpp (don't forget to modify the .h as well):

void Esp32LocalClient::flush() {
	client.flush();
}

void Esp32LocalClient::stop() {
	client.stop();
}

<sub>Will open a PR, once have some time to debug/verify everything</sub>

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

For OS (Arduino-based OpenSprinkler) 2.x:
https://openthings.freshdesk.com/support/solutions/articles/5000165132-how-to-compile-opensprinkler-firmware

For OSPi/OSBO or other Linux-based OpenSprinkler:
https://openthings.freshdesk.com/support/solutions/articles/5000631599-installing-and-updating-the-unified-firmware

============================================
Questions and comments:
http://www.opensprinkler.com
============================================
