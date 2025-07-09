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

#ifndef _UTILS_H
#define _UTILS_H

#if defined(ARDUINO)
	#include <Arduino.h>
#else // headers for RPI/LINUX
	#include <stdio.h>
	#include <limits.h>
	#include <sys/time.h>
	#include <arpa/inet.h>
	#include <sys/socket.h>
	#include <ifaddrs.h>
	#include <net/route.h>
#endif
#include "defines.h"

// File reading/writing functions
//remove unused functions: void write_to_file(const char *fname, const char *data, ulong size, ulong pos=0, bool trunc=true);
//remove unused functions: void read_from_file(const char *fname, char *data, ulong maxsize=TMP_BUFFER_SIZE, int pos=0);
void remove_file(const char *fname);
bool file_exists(const char *fname);

void file_read_block (const char *fname, void *dst, ulong pos, ulong len);
void file_write_block(const char *fname, const void *src, ulong pos, ulong len);
void file_copy_block (const char *fname, ulong from, ulong to, ulong len, void *tmp=0);
unsigned char file_read_byte (const char *fname, ulong pos);
void file_write_byte(const char *fname, ulong pos, unsigned char v);
unsigned char file_cmp_block(const char *fname, const char *buf, ulong pos);

// misc. string and time converstion functions
void strncpy_P0(char* dest, const char* src, int n);
ulong water_time_resolve(uint16_t v);
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

#if defined(ARDUINO)

#else // Arduino compatible functions for RPI/LINUX
	const char* get_data_dir();
	void set_data_dir(const char *new_data_dir);
	char* get_filename_fullpath(const char *filename);
	void delay(ulong ms);
	void delayMicroseconds(ulong us);
	void delayMicrosecondsHard(ulong us);
	ulong millis();
	ulong micros();
	void initialiseEpoch();
	#if defined(OSPI)
	unsigned int detect_rpi_rev();
	char* get_runtime_path();

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

#endif // _UTILS_H
