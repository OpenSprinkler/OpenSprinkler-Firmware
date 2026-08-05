/*
 * EMail Sender Arduino, esp8266, stm32 and esp32 library to send email
 *
 * AUTHOR:  Renzo Mischianti
 * VERSION: 3.0.14
 *
 * https://www.mischianti.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Renzo Mischianti www.mischianti.org All right reserved.
 *
 * You may copy, alter and reuse this code in any way you like, but please leave
 * reference to www.mischianti.org in your comments if you redistribute this code.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef EMailSenderKey_h
#define EMailSenderKey_h

// Uncomment if you use esp8266 core <= 2.4.2
//#define ARDUINO_ESP8266_RELEASE_2_4_2

// If you want disable attachments and save memory comment this define
//#define ENABLE_ATTACHMENTS

// Uncomment to enable printing out nice debug messages.
//#define EMAIL_SENDER_DEBUG

// Define where debug output will be printed.
#define DEBUG_PRINTER Serial

#define STORAGE_NONE (0)
// INTERNAL STORAGE
#define STORAGE_SPIFFS (1)
#define STORAGE_LITTLEFS (2)
#define STORAGE_FFAT (3)
#define STORAGE_SPIFM  (5) 	// Libraries Adafruit_SPIFlash and SdFat-Adafruit-Fork
// EXTERNAL STORAGE
#define STORAGE_SD (4)
#define STORAGE_SDFAT2 (6) 					// Library SdFat version >= 2.0.2
#define STORAGE_SDFAT_RP2040_ESP8266 (7) 	// Library ESP8266SdFat on Raspberry Pi Pico

#define NETWORK_ESP8266_ASYNC (0)
#define NETWORK_ESP8266 (1)
#define NETWORK_ESP8266_242 (6)
#define NETWORK_W5100 (2)
#define NETWORK_ETHERNET		(2)		// Standard Arduino Ethernet library
#define NETWORK_ENC28J60 (3)
#define NETWORK_ESP32 (4)
#define NETWORK_RP2040_WIFI (4)
#define NETWORK_ESP32_ETH (5)
#define NETWORK_WiFiNINA (7)
#define NETWORK_ETHERNET_LARGE (8)
#define NETWORK_ETHERNET_ENC (9)
#define NETWORK_ETHERNET_STM (10)
#define NETWORK_UIPETHERNET (11)
#define NETWORK_ETHERNET_2 (12)
#define NETWORK_ETHERNET_GENERIC	(13)	// Ethernet generic
#define NETWORK_MBED_WIFI	(14)	// Arduino GIGA R1 WiFi

// if you want force disable SSL if present uncomment this define
// #define FORCE_DISABLE_SSL

// If you want add a wrapper to emulate SSL over Client like EthernetClient
// #define SSLCLIENT_WRAPPER

// esp8266 microcontrollers configuration
#ifndef DEFAULT_EMAIL_NETWORK_TYPE_ESP8266
	#define DEFAULT_EMAIL_NETWORK_TYPE_ESP8266 	NETWORK_ESP8266
	#define DEFAULT_INTERNAL_ESP8266_STORAGE STORAGE_LITTLEFS
	#define DEFAULT_EXTERNAL_ESP8266_STORAGE STORAGE_NONE
#endif
// esp32 microcontrollers configuration
#ifndef DEFAULT_EMAIL_NETWORK_TYPE_ESP32
	#define DEFAULT_EMAIL_NETWORK_TYPE_ESP32 	NETWORK_ESP32
	#define DEFAULT_INTERNAL_ESP32_STORAGE STORAGE_SPIFFS
	#define DEFAULT_EXTERNAL_ESP32_STORAGE STORAGE_SD
#endif
// stm32 microcontrollers configuration
#ifndef DEFAULT_EMAIL_NETWORK_TYPE_STM32
	#define DEFAULT_EMAIL_NETWORK_TYPE_STM32 	NETWORK_W5100
	#define DEFAULT_INTERNAL_STM32_STORAGE STORAGE_NONE
	#define DEFAULT_EXTERNAL_STM32_STORAGE STORAGE_SDFAT2
#endif
// Arduino microcontrollers configuration
#ifndef DEFAULT_EMAIL_NETWORK_TYPE_ARDUINO
	#define DEFAULT_EMAIL_NETWORK_TYPE_ARDUINO 	NETWORK_W5100
	#define DEFAULT_INTERNAL_ARDUINO_STORAGE STORAGE_NONE
	#define DEFAULT_EXTERNAL_ARDUINO_STORAGE STORAGE_SD
#endif
// Arduino SAMD microcontrollers configuration
#ifndef DEFAULT_EMAIL_NETWORK_TYPE_ARDUINO_SAMD
	#define DEFAULT_EMAIL_NETWORK_TYPE_SAMD NETWORK_WiFiNINA
	#define DEFAULT_INTERNAL_ARDUINO_SAMD_STORAGE STORAGE_NONE
	#define DEFAULT_EXTERNAL_ARDUINO_SAMD_STORAGE STORAGE_SD
#endif
// Raspberry Pi Pico (rp2040) configuration
#ifndef DEFAULT_EMAIL_NETWORK_TYPE_RP2040
    #define DEFAULT_EMAIL_NETWORK_TYPE_RP2040 NETWORK_RP2040_WIFI
    #define DEFAULT_INTERNAL_ARDUINO_RP2040_STORAGE STORAGE_LITTLEFS
    #define DEFAULT_EXTERNAL_ARDUINO_RP2040_STORAGE STORAGE_NONE
#endif
// Arduino MBED microcontrollers configuration LIKE Arduino GIGA
#ifndef DEFAULT_EMAIL_NETWORK_TYPE_ARDUINO_MBED
	#define DEFAULT_EMAIL_NETWORK_TYPE_MBED NETWORK_MBED_WIFI
	#define DEFAULT_INTERNAL_ARDUINO_MBED_STORAGE STORAGE_NONE
	#define DEFAULT_EXTERNAL_ARDUINO_MBED_STORAGE STORAGE_SD
#endif

#ifdef SSLCLIENT_WRAPPER
	// Generate the trust_anchors.h with this online generator
	// https://openslab-osu.github.io/bearssl-certificate-utility/
	/**
	 *  For Ethernet w5x00 card you must follow the step given
	 *  from this link https://github.com/OPEnSLab-OSU/SSLClient#sslclient-with-ethernet
	 *  you can modify Ethernet library or use directly this modified one
	 *  https://github.com/OPEnSLab-OSU/EthernetLarge
	 *
	 *  For enc28j60 use EthernetENC available from library manager or
	 *  https://github.com/jandrassy/EthernetENC
	 */
	#define ANALOG_PIN A7
	#include <SSLClient.h>
	#include "trust_anchors.h"

	#define PUT_OUTSIDE_SCOPE_CLIENT_DECLARATION
#endif

// This two value can take a number 1..20 or you can specify 'a'
// but with auto you can lost specified response error
#define DEFAULT_EHLO_RESPONSE_COUNT 6
#define DEFAULT_CONNECTION_RESPONSE_COUNT 0

#define SD_CS_PIN SS
#define SPIFM_CS_PIN SS

//#define STORAGE_INTERNAL_FORCE_DISABLE
//#define STORAGE_EXTERNAL_FORCE_DISABLE

#endif
