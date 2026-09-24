#include "gpio.h"

#if defined(ARDUINO)

#include "../boards/board_profile.h"

namespace {

IOEXP* main_expander = nullptr;

} // namespace

void gpio_set_main_expander(IOEXP* expander) {
	main_expander = expander;
}

void pinModeExt(unsigned char pin, unsigned char mode) {
	if(pin==osboard::UNUSED_PIN) return;
	if(pin>=osboard::IO_EXPANDER_PIN_BASE) {
		if(main_expander) main_expander->pinMode(pin-osboard::IO_EXPANDER_PIN_BASE, mode);
	} else {
		pinMode(pin, mode);
	}
}

void digitalWriteExt(unsigned char pin, unsigned char value) {
	if(pin==osboard::UNUSED_PIN) return;
	if(pin>=osboard::IO_EXPANDER_PIN_BASE) {
		if(main_expander) main_expander->digitalWrite(pin-osboard::IO_EXPANDER_PIN_BASE, value);
	} else {
		digitalWrite(pin, value);
	}
}

unsigned char digitalReadExt(unsigned char pin) {
	if(pin==osboard::UNUSED_PIN) return HIGH;
	if(pin>=osboard::IO_EXPANDER_PIN_BASE) {
		return main_expander ? main_expander->digitalRead(pin-osboard::IO_EXPANDER_PIN_BASE) : HIGH;
	}
	return digitalRead(pin);
}

#endif
