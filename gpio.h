/* OpenSprinkler Unified Firmware
 * Copyright (C) 2015 by Ray Wang (ray@opensprinkler.com)
 *
 * GPIO header file
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

#ifndef GPIO_H
#define GPIO_H

#if defined(ARDUINO)

#if defined(ESP8266)

#include "Arduino.h"

// PCA9555 register defines
#define NXP_INPUT_REG  0
#define NXP_OUTPUT_REG 2
#define NXP_INVERT_REG 4
#define NXP_CONFIG_REG 6

#define IOEXP_TYPE_8574 0
#define IOEXP_TYPE_8575 1
#define IOEXP_TYPE_9555 2
#define IOEXP_TYPE_UNKNOWN 254
#define IOEXP_TYPE_NONEXIST 255

class IOEXP {
public:
	IOEXP(uint8_t addr=255) { address = addr; type = IOEXP_TYPE_NONEXIST; }

	virtual void pinMode(uint8_t pin, uint8_t IOMode) { }
	virtual uint16_t i2c_read(uint8_t reg) { return 0xFFFF; }
	virtual void i2c_write(uint8_t reg, uint16_t v) { }
	// software implementation of shift register out
	virtual void shift_out(uint8_t plat, uint8_t pclk, uint8_t pdat, uint8_t v) { }

	void digitalWrite(uint16_t v) {
		i2c_write(NXP_OUTPUT_REG, v);
	}

	uint16_t digitalRead() {
		return i2c_read(NXP_INPUT_REG);
	}

	uint8_t digitalRead(uint8_t pin) {
		return (digitalRead() & (1<<pin)) ? HIGH : LOW;
	}

	void digitalWrite(uint8_t pin, uint8_t v) {
		uint16_t values = i2c_read(NXP_OUTPUT_REG);
		if(v > 0) values |= (1<<pin);
		else values &= ~(1 << pin);
		i2c_write(NXP_OUTPUT_REG, values);
	}

	static unsigned char detectType(uint8_t address);
	uint8_t address;
	uint8_t type;
};

class PCA9555 : public IOEXP {
public:
	PCA9555(uint8_t addr) { address = addr; type = IOEXP_TYPE_9555; }
	void pinMode(uint8_t pin, uint8_t IOMode);
	uint16_t i2c_read(uint8_t reg);
	void i2c_write(uint8_t reg, uint16_t v);
	void shift_out(uint8_t plat, uint8_t pclk, uint8_t pdat, uint8_t v);
};

class PCF8575 : public IOEXP {
public:
	PCF8575(uint8_t addr) { address = addr; type = IOEXP_TYPE_8575; }
	void pinMode(uint8_t pin, uint8_t IOMode) {
		if(IOMode!=OUTPUT) inputmask |= (1<<pin);
	}
	uint16_t i2c_read(uint8_t reg);
	void i2c_write(uint8_t reg, uint16_t v);
private:
	uint16_t inputmask = 0;
};

class PCF8574 : public IOEXP {
public:
	PCF8574(uint8_t addr) { address = addr; type = IOEXP_TYPE_8574; }
	void pinMode(uint8_t pin, uint8_t IOMode) {
		if(IOMode!=OUTPUT) inputmask |= (1<<pin);
	}
	uint16_t i2c_read(uint8_t reg);
	void i2c_write(uint8_t reg, uint16_t v);
private:
	uint8_t inputmask = 0;	// mask bits for input pins
};

void pinModeExt(unsigned char pin, unsigned char mode);
void digitalWriteExt(unsigned char pin, unsigned char value);
unsigned char digitalReadExt(unsigned char pin);

#endif	// ESP8266

#else

#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

#include "defines.h"
#define OUTPUT 0
#define INPUT  1

#if defined(OSPI)
#define INPUT_PULLUP 2
#else
#define INPUT_PULLUP INPUT
#endif

#define HIGH   1
#define LOW    0

void pinMode(int pin, unsigned char mode);
void digitalWrite(int pin, unsigned char value);
int gpio_fd_open(int pin, int mode = O_WRONLY);
void gpio_fd_close(int fd);
void gpio_write(int fd, unsigned char value);
unsigned char digitalRead(int pin);
// mode can be any of 'rising', 'falling', 'both'
void attachInterrupt(int pin, const char* mode, void (*isr)(void));

#endif

#endif // GPIO_H
