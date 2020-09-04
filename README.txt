============================================
====== AriloSprinkler ESP32 Firmware =======
============================================

This is a fork of the OpenSprinkler firmware targetting the ESP32. The ESP32 code is based on the fork from J.Charer.

Changes to Opensprinkler standard firmware:
- ESP32 port from J.Charer merged with modifications to use the port expander again and pin adaptions to my setup
- Adaptions to current measurement (SW but HW as well, as the ESP32 can not read under 0.2V). The SW adaptions are "Work in Progress"
- Long range LoRa transceiver driver (SX1262) to allow remote control of the AriloSprinkler system. The driver is "Work in Progress".

A HW fork will be created soon to share the board designs. Currently 2 versions have been created, one based on the TTGO V2.1 1.6 board and one for the ESP32-WROOM module as well as a LoRa E22-900M30S-SX1262 transceiver.

For questions send a message to:
gmail email: arijav2020
