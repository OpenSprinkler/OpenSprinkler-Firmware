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


************************************************************** UPDATE 01.04.2022 *************************************
This is the lwip version from OpenSprinkler branch dev/os220 

we found out that the lwip_enc28j60 had some bugs, so use better the lwip_enc28j60 from here: 
https://github.com/esp8266/Arduino/pull/8376

Also it is better to use the updated version of LitteFS from here:
https://github.com/littlefs-project/littlefs

**********************************************************************************************************************
