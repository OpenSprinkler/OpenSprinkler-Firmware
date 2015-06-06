/* OpenSprinkler Unified (AVR/RPI/BBB/LINUX) Firmware
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

#else // headers for RPI/BBB
  #include <stdio.h>
#endif
#include "defines.h"

byte strcmp_to_nvm(const char* src, int addr);
byte water_time_encode(uint16_t i);
uint16_t water_time_decode(byte i);
ulong water_time_resolve(uint16_t v);
byte water_time_encode_signed(int16_t i);
int16_t water_time_decode_signed(byte i);
void write_to_file(const char *name, const char *data);
bool read_from_file(const char *name, char *data, int maxsize=TMP_BUFFER_SIZE);
void remove_file(const char *name);
#if defined(ARDUINO)
  #define nvm_read_block  eeprom_read_block
  #define nvm_write_block eeprom_write_block
  #define nvm_read_byte   eeprom_read_byte
  #define nvm_write_byte  eeprom_write_byte
#else // NVM functions for RPI/BBB
  void nvm_read_block(void *dst, const void *src, int len);
  void nvm_write_block(const void *src, void *dst, int len);
  byte nvm_read_byte(const byte *p);
  void nvm_write_byte(const byte *p, byte v);
#if defined(OSPI)
  unsigned int detect_rpi_rev();
#endif
#endif  // NVM functions

#endif // _UTILS_H
