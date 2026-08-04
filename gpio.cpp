/* OpenSprinkler Unified Firmware
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

#include "defines.h"

#include "OpenSprinkler.h"

extern OpenSprinkler os;

void pinModeExt(unsigned char pin, unsigned char mode) {
	if(pin==255) return;
	if(pin>=IOEXP_PIN) {
		os.mainio->pinMode(pin-IOEXP_PIN, mode);
	} else {
		pinMode(pin, mode);
	}
}

void digitalWriteExt(unsigned char pin, unsigned char value) {
	if(pin==255) return;
	if(pin>=IOEXP_PIN) {

		os.mainio->digitalWrite(pin-IOEXP_PIN, value);
	} else {
		digitalWrite(pin, value);
	}
}

unsigned char digitalReadExt(unsigned char pin) {
	if(pin==255) return HIGH;
	if(pin>=IOEXP_PIN) {
		return os.mainio->digitalRead(pin-IOEXP_PIN);
		// a pin on IO expander
		//return pcf_read(MAIN_I2CADDR)&(1<<(pin-IOEXP_PIN));
	} else {
		return digitalRead(pin);
	}
}

#elif defined(OSPI)

/**
 * GPIO Implementation for Raspberry Pi using lgpio library.
 * This replaces the previous libgpiod implementation for simplicity and robustness.
 */
#include <stdio.h>
#include <lgpio.h>

#include "utils.h"

// GPIO handle for lgpio library
int lgpio_handle = -1;

// Initialize lgpio connection
int init_lgpio() {
	if (lgpio_handle >= 0) {
		return 0; // Already initialized
	}

	int chip_id = 0; // Default to gpiochip0 for older Raspberry Pi models

	// On Raspberry Pi 5 (bcm2712), the primary GPIOs are on gpiochip4
	if (get_board_type() == BoardType::RaspberryPi_bcm2712) {
		chip_id = 4;
	}

	lgpio_handle = lgGpiochipOpen(chip_id);
	if (lgpio_handle < 0) {
		// If we tried the non-default chip and failed, try the default one as a fallback.
		if (chip_id != 0) {
			DEBUG_PRINTLN("Could not open gpiochip4, falling back to gpiochip0...");
			chip_id = 0;
			lgpio_handle = lgGpiochipOpen(chip_id);
		}
	}

	// If handle is still invalid after potential fallback, report error and exit.
	if (lgpio_handle < 0) {
		DEBUG_PRINTLN("Could not open a valid GPIO chip.");
		return -1;
	}

	DEBUG_PRINT("Successfully opened gpiochip");
	DEBUG_PRINTLN(chip_id);
	return 0;
}

/** Set pin mode, in or out */
void pinMode(int pin, unsigned char mode) {
	if (init_lgpio() != 0) return;

	switch(mode) {
		case INPUT:
			// int lgGpioClaimInput(int h, unsigned lFlags, unsigned gpio);
			lgGpioClaimInput(lgpio_handle, 0, pin);
			break;
		case INPUT_PULLUP:
			// int lgGpioClaimInput(int h, unsigned lFlags, unsigned gpio);
			lgGpioClaimInput(lgpio_handle, LG_SET_PULL_UP, pin);
			break;
		case OUTPUT:
			// int lgGpioClaimOutput(int h, unsigned lFlags, unsigned gpio, unsigned level);
			// Claim pin as output, with a default state of LOW
			lgGpioClaimOutput(lgpio_handle, 0, pin, LOW);
			break;
		default:
			DEBUG_PRINTLN("invalid pin direction");
			break;
	}
	return;
}

/** Read digital value */
unsigned char digitalRead(int pin) {
	if (lgpio_handle < 0) {
		DEBUG_PRINT("tried to read from uninitialized lgpio handle for pin ");
		DEBUG_PRINTLN(pin);
		return 0;
	}
	int val = lgGpioRead(lgpio_handle, pin);
	if (val < 0) {
		DEBUG_PRINT("failed to read value on pin ");
		DEBUG_PRINTLN(pin);
		return 0; // Return LOW on error
	}
	return (unsigned char)val;
}

/** Write digital value */
void digitalWrite(int pin, unsigned char value) {
	if (lgpio_handle < 0) {
		DEBUG_PRINT("tried to write to uninitialized lgpio handle for pin ");
		DEBUG_PRINTLN(pin);
		return;
	}

	int res = lgGpioWrite(lgpio_handle, pin, value);
	if (res < 0) {
		DEBUG_PRINT("failed to write value on pin ");
		DEBUG_PRINTLN(pin);
	}
}

#else

void pinMode(int pin, unsigned char mode) {}
void digitalWrite(int pin, unsigned char value) {}
unsigned char digitalRead(int pin) {return 0;}

#endif
