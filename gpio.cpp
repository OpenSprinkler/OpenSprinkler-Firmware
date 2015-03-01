/* OpenSprinkler Unified (AVR/RPI/BBB/LINUX) Firmware
 * Copyright (C) 2014 by Ray Wang (ray@opensprinkler.com)
 *
 * GPIO functions
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
 
#include "gpio.h"

#if defined(ARDUINO)

#elif defined(OSPI) || defined(OSBO)

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static byte GPIOExport(int pin) {
#define BUFFER_MAX 3
  char buffer[BUFFER_MAX];
  ssize_t bytes_written;
  int fd;

  fd = open("/sys/class/gpio/export", O_WRONLY);
  if (-1 == fd) {
    DEBUG_PRINTLN("failed to open export for writing");
    return 0;
  }

  bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
  write(fd, buffer, bytes_written);
  close(fd);
  return 1;

}

void pinMode(int pin, byte mode) {
  static const char s_directions_str[]  = "in\0out";

#define DIRECTION_MAX 35
  char path[DIRECTION_MAX];
  int fd;

  snprintf(path, DIRECTION_MAX, "/sys/class/gpio/gpio%d/direction", pin);

  struct stat st;
  if(stat(path, &st)) {
    if (!GPIOExport(pin)) return;
  }

  fd = open(path, O_WRONLY);
  if (-1 == fd) {
    DEBUG_PRINTLN("failed to open gpio direction for writing");
    return;
  }

  if (-1 == write(fd, &s_directions_str[INPUT == mode ? 0 : 3], INPUT == mode ? 2 : 3)) {
    DEBUG_PRINTLN("failed to set direction");
    return;
  }

  close(fd);
  return;
}

byte digitalRead(int pin) {
#define VALUE_MAX 30
  char path[VALUE_MAX];
  char value_str[3];
  int fd;

  snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
  fd = open(path, O_RDONLY);
  if (-1 == fd) {
    DEBUG_PRINTLN("failed to open gpio value for reading");
    return 0;
  }

  if (-1 == read(fd, value_str, 3)) {
    DEBUG_PRINTLN("failed to read value");
    return 0;
  }

  close(fd);
  return(atoi(value_str));
}

void digitalWrite(int pin, byte value) {
  static const char s_values_str[] = "01";

  char path[VALUE_MAX];
  int fd;

  snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
  fd = open(path, O_WRONLY);
  if (-1 == fd) {
    DEBUG_PRINTLN("failed to open gpio value for writing");
    return;
  }

  if (1 != write(fd, &s_values_str[LOW == value ? 0 : 1], 1)) {
    DEBUG_PRINTLN("failed to write value");
    DEBUG_PRINTLN(pin);
    return;
  }

  close(fd);
}

#else

void pinMode(int pin, byte mode) {}
void digitalWrite(int pin, byte value) {}
byte digitalRead(int pin) {return 0;}

#endif
