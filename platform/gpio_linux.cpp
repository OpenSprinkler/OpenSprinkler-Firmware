#include "gpio.h"

#if defined(OSPI)

#include "../utils.h"

#include <lgpio.h>
#include <stdio.h>

namespace {

int lgpio_handle = -1;

int init_lgpio() {
	if (lgpio_handle >= 0) return 0;

	int chip_id = 0;
	if (get_board_type() == BoardType::RaspberryPi_bcm2712) chip_id = 4;

	lgpio_handle = lgGpiochipOpen(chip_id);
	if (lgpio_handle < 0 && chip_id != 0) {
		DEBUG_PRINTLN("Could not open gpiochip4, falling back to gpiochip0...");
		chip_id = 0;
		lgpio_handle = lgGpiochipOpen(chip_id);
	}

	if (lgpio_handle < 0) {
		DEBUG_PRINTLN("Could not open a valid GPIO chip.");
		return -1;
	}

	DEBUG_PRINT("Successfully opened gpiochip");
	DEBUG_PRINTLN(chip_id);
	return 0;
}

} // namespace

void pinMode(int pin, unsigned char mode) {
	if (init_lgpio() != 0) return;

	switch(mode) {
	case INPUT:
		lgGpioClaimInput(lgpio_handle, 0, pin);
		break;
	case INPUT_PULLUP:
		lgGpioClaimInput(lgpio_handle, LG_SET_PULL_UP, pin);
		break;
	case OUTPUT:
		lgGpioClaimOutput(lgpio_handle, 0, pin, LOW);
		break;
	default:
		DEBUG_PRINTLN("invalid pin direction");
		break;
	}
}

unsigned char digitalRead(int pin) {
	if (lgpio_handle < 0) {
		DEBUG_PRINT("tried to read from uninitialized lgpio handle for pin ");
		DEBUG_PRINTLN(pin);
		return LOW;
	}
	int value = lgGpioRead(lgpio_handle, pin);
	if (value < 0) {
		DEBUG_PRINT("failed to read value on pin ");
		DEBUG_PRINTLN(pin);
		return LOW;
	}
	return (unsigned char)value;
}

void digitalWrite(int pin, unsigned char value) {
	if (lgpio_handle < 0) {
		DEBUG_PRINT("tried to write to uninitialized lgpio handle for pin ");
		DEBUG_PRINTLN(pin);
		return;
	}

	if (lgGpioWrite(lgpio_handle, pin, value) < 0) {
		DEBUG_PRINT("failed to write value on pin ");
		DEBUG_PRINTLN(pin);
	}
}

#elif defined(DEMO)

void pinMode(int pin, unsigned char mode) {}
void digitalWrite(int pin, unsigned char value) {}
unsigned char digitalRead(int pin) { return LOW; }

#endif
