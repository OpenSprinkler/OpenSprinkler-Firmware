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

#ifndef EMailSender_h
#define EMailSender_h

#include "EMailSenderKey.h"

#if ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#if(NETWORK_ESP8266_242 == DEFAULT_EMAIL_NETWORK_TYPE_ESP8266)
	#define ARDUINO_ESP8266_RELEASE_2_4_2
	#define DEFAULT_EMAIL_NETWORK_TYPE_ESP8266 NETWORK_ESP8266
#endif
//
//#if(NETWORK_ESP8266_SD == DEFAULT_EMAIL_NETWORK_TYPE_ESP8266)
//	#define ESP8266_GT_2_4_2_SD_STORAGE_SELECTED
//	#define DEFAULT_EMAIL_NETWORK_TYPE_ESP8266 NETWORK_ESP8266
//#endif
#if !defined(EMAIL_NETWORK_TYPE)
// select Network type based
	#if defined(ESP8266) || defined(ESP31B)
		#define EMAIL_NETWORK_TYPE DEFAULT_EMAIL_NETWORK_TYPE_ESP8266
		#define INTERNAL_STORAGE DEFAULT_INTERNAL_ESP8266_STORAGE
		#define EXTERNAL_STORAGE DEFAULT_EXTERNAL_ESP8266_STORAGE
	#elif defined(ARDUINO_ARCH_STM32) || defined(__STM32F1__) || defined(__STM32F4__)
		#define EMAIL_NETWORK_TYPE DEFAULT_EMAIL_NETWORK_TYPE_STM32
		#define INTERNAL_STORAGE DEFAULT_INTERNAL_STM32_STORAGE
		#define EXTERNAL_STORAGE DEFAULT_EXTERNAL_STM32_STORAGE
    #elif defined(ESP32)
        #define EMAIL_NETWORK_TYPE DEFAULT_EMAIL_NETWORK_TYPE_ESP32
		#define INTERNAL_STORAGE DEFAULT_INTERNAL_ESP32_STORAGE
		#define EXTERNAL_STORAGE DEFAULT_EXTERNAL_ESP32_STORAGE
    #elif defined(ARDUINO_ARCH_RP2040)
        #define EMAIL_NETWORK_TYPE DEFAULT_EMAIL_NETWORK_TYPE_RP2040
        #define INTERNAL_STORAGE DEFAULT_INTERNAL_ARDUINO_RP2040_STORAGE
        #define EXTERNAL_STORAGE DEFAULT_EXTERNAL_ARDUINO_RP2040_STORAGE
	#elif defined(ARDUINO_ARCH_SAMD)
		#define EMAIL_NETWORK_TYPE DEFAULT_EMAIL_NETWORK_TYPE_SAMD
		#define INTERNAL_STORAGE DEFAULT_INTERNAL_ARDUINO_SAMD_STORAGE
		#define EXTERNAL_STORAGE DEFAULT_EXTERNAL_ARDUINO_SAMD_STORAGE
	#elif defined(ARDUINO_ARCH_MBED)
		#define EMAIL_NETWORK_TYPE DEFAULT_EMAIL_NETWORK_TYPE_MBED
		#define INTERNAL_STORAGE DEFAULT_INTERNAL_ARDUINO_MBED_STORAGE
		#define EXTERNAL_STORAGE DEFAULT_EXTERNAL_ARDUINO_MBED_STORAGE
	#else
		#define EMAIL_NETWORK_TYPE DEFAULT_EMAIL_NETWORK_TYPE_ARDUINO
		#define INTERNAL_STORAGE DEFAULT_INTERNAL_ARDUINO_STORAGE
		#define EXTERNAL_STORAGE DEFAULT_EXTERNAL_ARDUINO_STORAGE
	#endif
#endif

//#if defined(ESP8266) || defined(ESP31B)
//	#ifndef STORAGE_EXTERNAL_FORCE_DISABLE
//		#define STORAGE_EXTERNAL_ENABLED
//	#endif
//	#ifndef STORAGE_INTERNAL_FORCE_DISABLE
//		#define STORAGE_INTERNAL_ENABLED
//	#endif
//#elif defined(ESP32)
//	#ifndef STORAGE_EXTERNAL_FORCE_DISABLE
//		#define STORAGE_EXTERNAL_ENABLED
//	#endif
//	#ifndef STORAGE_INTERNAL_FORCE_DISABLE
//		#define STORAGE_INTERNAL_ENABLED
//	#endif
//#elif defined(ARDUINO_ARCH_STM32)
//	#ifndef STORAGE_EXTERNAL_FORCE_DISABLE
//		#define STORAGE_EXTERNAL_ENABLED
//	#endif
//#else
//	#ifndef STORAGE_EXTERNAL_FORCE_DISABLE
//		#define STORAGE_EXTERNAL_ENABLED
//	#endif
//#endif

#ifdef ENABLE_ATTACHMENTS
	#if !defined(STORAGE_EXTERNAL_FORCE_DISABLE) && (EXTERNAL_STORAGE != STORAGE_NONE)
		#define STORAGE_EXTERNAL_ENABLED
	#endif
	#if !defined(STORAGE_INTERNAL_FORCE_DISABLE) && (INTERNAL_STORAGE != STORAGE_NONE)
		#define STORAGE_INTERNAL_ENABLED
	#endif
#endif

// Includes and defined based on Network Type
#if(EMAIL_NETWORK_TYPE == NETWORK_ESP8266_ASYNC)

// Note:
//   No SSL/WSS support for client in Async mode
//   TLS lib need a sync interface!

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#elif defined(ESP32)
#include <WiFi.h>
#include <WiFiClientSecure.h>

#ifndef FORCE_DISABLE_SSL
	#define EMAIL_NETWORK_CLASS WiFiClientSecure
#else
    #define EMAIL_NETWORK_CLASS WiFiClient
#endif
//#define EMAIL_NETWORK_SSL_CLASS WiFiClientSecure
#define EMAIL_NETWORK_SERVER_CLASS WiFiServer

#elif defined(ESP31B)
#include <ESP31BWiFi.h>
#else
#error "network type ESP8266 ASYNC only possible on the ESP mcu!"
#endif
//
//#include <ESPAsyncTCP.h>
//#include <ESPAsyncTCPbuffer.h>
//#define EMAIL_NETWORK_CLASS AsyncTCPbuffer
//#define EMAIL_NETWORK_SERVER_CLASS AsyncServer

#elif(EMAIL_NETWORK_TYPE == NETWORK_ESP8266 || EMAIL_NETWORK_TYPE == NETWORK_ESP8266_242)

#if !defined(ESP8266) && !defined(ESP31B)
#error "network type ESP8266 only possible on the ESP mcu!"
#endif

#ifdef ESP8266
#include <ESP8266WiFi.h>
#else
#include <ESP31BWiFi.h>
#endif
#ifndef FORCE_DISABLE_SSL
	#define EMAIL_NETWORK_CLASS WiFiClientSecure
#else
    #define EMAIL_NETWORK_CLASS WiFiClient
#endif
//#define EMAIL_NETWORK_SSL_CLASS WiFiClientSecure
#define EMAIL_NETWORK_SERVER_CLASS WiFiServer

#elif(EMAIL_NETWORK_TYPE == NETWORK_W5100 || EMAIL_NETWORK_TYPE == NETWORK_ETHERNET_ENC)

#include <Ethernet.h>
#include <SPI.h>
#define EMAIL_NETWORK_CLASS EthernetClient
#define EMAIL_NETWORK_SERVER_CLASS EthernetServer

#elif(EMAIL_NETWORK_TYPE == NETWORK_ETHERNET_GENERIC)

#include <Ethernet_Generic.h>
#include <SPI.h>
#define EMAIL_NETWORK_CLASS EthernetClient
#define EMAIL_NETWORK_SERVER_CLASS EthernetServer

#elif(EMAIL_NETWORK_TYPE == NETWORK_ENC28J60 || EMAIL_NETWORK_TYPE == NETWORK_UIPETHERNET)

#include <UIPEthernet.h>

#define EMAIL_NETWORK_CLASS UIPClient
#define EMAIL_NETWORK_SERVER_CLASS UIPServer

//#include <UIPEthernet.h>
//UIPClient base_client;
//SSLClient client(base_client, TAs, (size_t)TAs_NUM, A5);
//
//#define EMAIL_NETWORK_CLASS SSLClient
//#define EMAIL_NETWORK_SERVER_CLASS UIPServer

#elif(EMAIL_NETWORK_TYPE == NETWORK_ESP32)

#include <WiFi.h>
#include <WiFiClientSecure.h>
#ifndef FORCE_DISABLE_SSL
	#define EMAIL_NETWORK_CLASS WiFiClientSecure
#else
    #define EMAIL_NETWORK_CLASS WiFiClient
#endif
//#define EMAIL_NETWORK_SSL_CLASS WiFiClientSecure
#define EMAIL_NETWORK_SERVER_CLASS WiFiServer

#elif(EMAIL_NETWORK_TYPE == NETWORK_ESP32_ETH)

#include <ETH.h>
#define EMAIL_NETWORK_CLASS WiFiClient
#define EMAIL_NETWORK_SERVER_CLASS WiFiServer

#elif(EMAIL_NETWORK_TYPE == NETWORK_ETHERNET_LARGE)

#include <EthernetLarge.h>
#include <SPI.h>
#define EMAIL_NETWORK_CLASS EthernetClient
#define EMAIL_NETWORK_SERVER_CLASS EthernetServer

#elif(EMAIL_NETWORK_TYPE == NETWORK_ETHERNET_2)

#include <Ethernet2.h>
#include <SPI.h>
#define EMAIL_NETWORK_CLASS EthernetClient
#define EMAIL_NETWORK_SERVER_CLASS EthernetServer

#elif(EMAIL_NETWORK_TYPE == NETWORK_ETHERNET_STM)

#include <Ethernet_STM.h>
#include <SPI.h>
#define EMAIL_NETWORK_CLASS EthernetClient
#define EMAIL_NETWORK_SERVER_CLASS EthernetServer

#elif(EMAIL_NETWORK_TYPE == NETWORK_WiFiNINA)

#include <WiFiNINA.h>
#ifndef FORCE_DISABLE_SSL
	#define EMAIL_NETWORK_CLASS WiFiSSLClient
#else
    #define EMAIL_NETWORK_CLASS WiFiClient
#endif
//#define EMAIL_NETWORK_SSL_CLASS WiFiSSLClient
#define EMAIL_NETWORK_SERVER_CLASS WiFiServer
#elif(EMAIL_NETWORK_TYPE == NETWORK_MBED_WIFI)

#include <WiFi.h>
#include <WiFiSSLClient.h>

#ifndef FORCE_DISABLE_SSL
	#define EMAIL_NETWORK_CLASS WiFiSSLClient
#else
    #define EMAIL_NETWORK_CLASS WiFiClient
#endif
//#define EMAIL_NETWORK_SSL_CLASS WiFiSSLClient
#define EMAIL_NETWORK_SERVER_CLASS WiFiServer

#else
#error "no network type selected!"
#endif

#ifdef STORAGE_INTERNAL_ENABLED
//			#define FS_NO_GLOBALS
		#if (INTERNAL_STORAGE == STORAGE_SPIFFS)
			#if defined(ESP32)
				#include <SPIFFS.h>
				#define INTERNAL_STORAGE_CLASS SPIFFS

				#define EMAIL_FILE_READ "r"
			#elif defined(ESP8266)
				#ifdef ARDUINO_ESP8266_RELEASE_2_4_2
					#define DIFFERENT_FILE_MANAGE
				#endif
				#include "FS.h"

				#define INTERNAL_STORAGE_CLASS SPIFFS
				#define EMAIL_FILE_READ "r"
			#endif
			#define EMAIL_FILE fs::File
		#elif (INTERNAL_STORAGE == STORAGE_LITTLEFS)
			#if defined(ESP32)
				#if ESP_ARDUINO_VERSION_MAJOR >= 2
						#include "FS.h"
						#include "LittleFS.h"
						#define INTERNAL_STORAGE_CLASS LittleFS
				#else
						#include "LITTLEFS.h"
						#define INTERNAL_STORAGE_CLASS LITTLEFS
				#endif
			#else
				#include "LittleFS.h"
				#define INTERNAL_STORAGE_CLASS LittleFS
			#endif
			#define EMAIL_FILE_READ "r"
			#define EMAIL_FILE fs::File
		#elif (INTERNAL_STORAGE == STORAGE_FFAT)
			#include "FFat.h"
			#define INTERNAL_STORAGE_CLASS FFat
			#define EMAIL_FILE_READ 'r'
			#define EMAIL_FILE fs::File
		#elif (INTERNAL_STORAGE == STORAGE_SPIFM)
			#include <SPI.h>

			#include "SdFat.h"
			#include "Adafruit_SPIFlash.h"

			#define INTERNAL_STORAGE_CLASS fatfs
			extern FatFileSystem INTERNAL_STORAGE_CLASS;

			#define EMAIL_FILE_READ O_READ
			#define EMAIL_FILE File
		#endif
#endif

#ifdef STORAGE_EXTERNAL_ENABLED
	#include <SPI.h>
	#if (EXTERNAL_STORAGE == STORAGE_SDFAT_RP2040_ESP8266)
		#include <SdFat.h>
		#include <sdios.h>

		#define EXTERNAL_STORAGE_CLASS sd
		extern SdFat EXTERNAL_STORAGE_CLASS;

		#define EMAIL_FILE_READ_EX FILE_READ
		#define EMAIL_FILE_EX File32

		#define DIFFERENT_FILE_MANAGE
	#elif (EXTERNAL_STORAGE == STORAGE_SDFAT2)
		#include <SdFat.h>
		#include <sdios.h>

		#define EXTERNAL_STORAGE_CLASS sd
		extern SdFat EXTERNAL_STORAGE_CLASS;

		#define EMAIL_FILE_READ_EX FILE_READ
		#define EMAIL_FILE_EX FsFile

		#define DIFFERENT_FILE_MANAGE
	#elif (EXTERNAL_STORAGE == STORAGE_SD)
		#include <SD.h>

		#define EXTERNAL_STORAGE_CLASS SD
		#define EMAIL_FILE_READ_EX 'r'

		#define EMAIL_FILE_EX File
	#endif
#endif

//#ifdef EMAIL_NETWORK_SSL_CLASS
//	#ifndef FORCE_DISABLE_SSL
//		#define EMAIL_NETWORK_CLASS WiFiClientSecure
//	#else
//	    #define EMAIL_NETWORK_CLASS WiFiClient
//	#endif
//#endif

#define OPEN_CLOSE_INTERNAL
#define OPEN_CLOSE_SD

// Setup debug printing macros.
#ifdef EMAIL_SENDER_DEBUG
	#define EMAIL_DEBUG_PRINT(...) { DEBUG_PRINTER.print(__VA_ARGS__); }
	#define EMAIL_DEBUG_PRINTLN(...) { DEBUG_PRINTER.println(__VA_ARGS__); }
#else
	#define EMAIL_DEBUG_PRINT(...) {}
	#define EMAIL_DEBUG_PRINTLN(...) {}
#endif

// Debug level for SSLClient
#ifndef EMAIL_SENDER_SSL_CLIENT_DEBUG
	#define EMAIL_SENDER_SSL_CLIENT_DEBUG 2
#endif

class EMailSender {
public:
	EMailSender(const char* email_login, const char* email_password, const char* email_from, const char* name_from, const char* smtp_server, uint16_t smtp_port );
	EMailSender(const char* email_login, const char* email_password, const char* email_from, const char* smtp_server, uint16_t smtp_port);
	EMailSender(const char* email_login, const char* email_password, const char* email_from, const char* name_from );
	EMailSender(const char* email_login, const char* email_password, const char* email_from);
	EMailSender(const char* email_login, const char* email_password);

#define STORAGE_SPIFFS (1)
#define STORAGE_LITTLEFS (2)
#define STORAGE_FFAT (3)
#define STORAGE_SPIFM  (5) 	// Libraries Adafruit_SPIFlash and SdFat-Adafruit-Fork
// EXTERNAL STORAGE
#define STORAGE_SD (4)
#define STORAGE_SDFAT2 (6) 	// Library SdFat version >= 2.0.2
#define STORAGE_SDFAT_RP2040_ESP8266 (7) 	// Library ESP8266SdFat on Raspberry Pi Pico


	enum StorageType {
		EMAIL_STORAGE_TYPE_SPIFFS,
		EMAIL_STORAGE_TYPE_LITTLE_FS,
		EMAIL_STORAGE_TYPE_FFAT,
		EMAIL_STORAGE_TYPE_SPIFM,
		EMAIL_STORAGE_TYPE_SD
	};

#define MIME_TEXT_HTML F("text/html")
#define MIME_TEXT_PLAIN F("text/plain")
#define MIME_IMAGE_JPG F("image/jpg")
#define MIME_IMAGE_PNG F("image/png")

	typedef struct {
		String mime = "text/html";
		String subject;
		String message;
	} EMailMessage;

#ifdef ENABLE_ATTACHMENTS
	typedef struct {
		StorageType storageType = EMAIL_STORAGE_TYPE_SD;
		String mime;
		bool encode64 = false;
		String filename;
		String url;
	} FileDescriptior;

	typedef struct {
		byte number;
		FileDescriptior *fileDescriptor;
	} Attachments;
#else
	typedef struct {
		char* noop1;
		char* noop2;
	} Attachments;
#endif

	typedef struct {
		String code;
		String desc;
		bool status = false;
	} Response;

	void setSMTPPort(uint16_t smtp_port);
	void setSMTPServer(const char* smtp_server);
	void setEMailLogin(const char* email_login);
	void setEMailFrom(const char* email_from);
	void setNameFrom(const char* name_from);
	void setEMailPassword(const char* email_password);

	EMailSender::Response send(char* to[], byte sizeOfTo, EMailMessage &email, Attachments att = {0, 0});
	EMailSender::Response send(char* to[], byte sizeOfTo,  byte sizeOfCc, EMailMessage &email, Attachments att = {0, 0});
	EMailSender::Response send(char* to[], byte sizeOfTo,  byte sizeOfCc, byte sizeOfCCn, EMailMessage &email, Attachments att = {0, 0});

	EMailSender::Response send(const char* to, EMailMessage &email, Attachments att = {0, 0});
	EMailSender::Response send(const char* to[], byte sizeOfTo, EMailMessage &email, Attachments att = {0, 0});
	EMailSender::Response send(const char* to[], byte sizeOfTo,  byte sizeOfCc, EMailMessage &email, Attachments att = {0, 0});
	EMailSender::Response send(const char* to[], byte sizeOfTo,  byte sizeOfCc, byte sizeOfCCn, EMailMessage &email, Attachments att = {0, 0});

	EMailSender::Response send(String to, EMailMessage &email, Attachments att = {0, 0});
	EMailSender::Response send(String to[], byte sizeOfTo, EMailMessage &email, Attachments att = {0, 0});
	EMailSender::Response send(String to[], byte sizeOfTo,  byte sizeOfCc, EMailMessage &email, Attachments att = {0, 0});
	EMailSender::Response send(String to[], byte sizeOfTo,  byte sizeOfCc, byte sizeOfCCn, EMailMessage &email, Attachments att = {0, 0});

	void setIsSecure(bool isSecure = false);

	void setUseAuth(bool useAuth = true) {
		this->useAuth = useAuth;
	}

	void setPublicIpDescriptor(const char *publicIpDescriptor = "mischianti") {
		publicIPDescriptor = publicIpDescriptor;
	}

	void setEHLOCommand(bool useEHLO = false) {
		this->useEHLO = useEHLO;
	}

	void setSASLLogin(bool isSASLLogin = false) {
		this->isSASLLogin = isSASLLogin;
	}

#if defined(ESP32)
	// Conditional - as it relies on considerable crypto infra.
 	void setCramMD5Login(bool onoff= false) {
 		this->isCramMD5Login = onoff;
 	}
#endif

	void setAdditionalResponseLineOnConnection(uint8_t numLines = 0) {
		this->additionalResponseLineOnConnection = numLines;
	}
	void setAdditionalResponseLineOnHELO(uint8_t numLines = 0) {
		this->additionalResponseLineOnHELO = numLines;
	}

private:
	uint16_t smtp_port = 465;
	char* smtp_server = strdup("smtp.gmail.com");
	char* email_login = 0;
	char* email_from  = 0;
	char* name_from  = 0;
	char* email_password = 0;

	const char* publicIPDescriptor = "mischianti";

	bool isSecure = false;

	bool useEHLO = false;
	bool isSASLLogin = false;

	bool useAuth = true;
        bool isCramMD5Login = false;

    String _serverResponce;

    uint8_t additionalResponseLineOnConnection = 0;
    uint8_t additionalResponseLineOnHELO = 0;

#ifdef SSLCLIENT_WRAPPER
    Response awaitSMTPResponse(SSLClient &client, const char* resp = "", const char* respDesc = "", uint16_t timeOut = 10000);
#else
    Response awaitSMTPResponse(EMAIL_NETWORK_CLASS &client, const char* resp = "", const char* respDesc = "", uint16_t timeOut = 10000);
#endif
};

#endif
