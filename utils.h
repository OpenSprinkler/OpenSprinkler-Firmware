/* OpenSprinkler Unified Firmware
 * Copyright (C) 2015 by Ray Wang (ray@opensprinkler.com)
 *
 * Utility functions header file
 * Feb 2015 @ OpenSprinkler.com
 *
 * This file is part of the OpenSprinkler library
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

 #pragma once

#if defined(ESP8266)
	#include <Arduino.h>
#else // headers for RPI/LINUX
	#include <stdio.h>
	#include <limits.h>
	#include <arpa/inet.h>
	#include <sys/socket.h>
	#include <ifaddrs.h>
	#include <net/route.h>
	#include "platform/clock.h"
#endif
#include "defines.h"
#include "storage/files.h"

// misc. string and time converstion functions
void strncpy_P0(char* dest, const char* src, int n);
uint32_t water_time_resolve(uint16_t v);
uint32_t water_time_scale(uint32_t duration, uint8_t weather_percent, float sensor_factor);
bool parse_program_duration(const char *value, uint32_t *duration);
unsigned char water_time_encode_signed(int16_t i);
int16_t water_time_decode_signed(unsigned char i);
void urlDecode(char *);
void urlEncode(char *);
void strReplaceQuoteBackslash(char *);
void peel_http_header(char*);
void strReplace(char *, char c, char r);

#define date_encode(m,d) ((m<<5)+d)
#define MIN_ENCODED_DATE date_encode(1,1)
#define MAX_ENCODED_DATE date_encode(12, 31)
bool isLastDayofMonth(unsigned char month, unsigned char day);
bool isValidDate(uint16_t date);
bool isLeapYear(uint16_t year);	// whether a 4 digit year is a leap year
#if defined(ESP8266)
unsigned char hex2dec(const char *hex);
bool isHex(char c);
bool isValidMAC(const char *_mac);
void str2mac(const char *_str, unsigned char mac[]);
#endif

#if defined(ESP8266)

#else // Arduino compatible functions for RPI/LINUX
	#if defined(OSPI)
	unsigned int detect_rpi_rev();

	struct route_t {
		char iface[16];
		in_addr_t gateway;
		in_addr_t destination;
	};

	route_t get_route();
	in_addr_t get_ip_address(char *iface);
	#endif

	enum BoardType {
		Unknown,
		RaspberryPi_Unknown,
		RaspberryPi_bcm2712,
		RaspberryPi_bcm2711,
		RaspberryPi_bcm2837,
		RaspberryPi_bcm2836,
		RaspberryPi_bcm2835,
	};

	BoardType get_board_type();
#endif

char dec2hexchar(unsigned char dec);
