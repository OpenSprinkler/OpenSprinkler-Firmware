#include "ads1115.h"
#include "util/utils.h"   // millis() on Linux/DEMO

#include <cmath>

#if defined(ARDUINO)
ADS1115::ADS1115(uint8_t address, TwoWire& wire) : _address(address), _wire(&wire) {}
ADS1115::ADS1115(uint8_t address) : ADS1115(address, Wire) {}

bool ADS1115::begin() {
	if ((this->_address < 0x48) || (this->_address > 0x4B)) {
		return false;
	}
	this->_wire->beginTransmission(_address);
	return (this->_wire->endTransmission() == 0);
}


void ADS1115::_write_register(uint8_t reg, uint16_t value) {
	this->_wire->beginTransmission(this->_address);
	this->_wire->write(reg);
	this->_wire->write((uint8_t)(value >> 8));
	this->_wire->write((uint8_t)(value & 0xFF));
	this->_wire->endTransmission();
}

uint16_t ADS1115::_read_register(uint8_t reg) {
	this->_wire->beginTransmission(this->_address);
	this->_wire->write(reg);
	if (!this->_wire->endTransmission(false)) {
		if (this->_wire->requestFrom((int)_address, (int)2) == 2) {
			uint16_t val = ((uint16_t)this->_wire->read()) << 8;
			val += (uint16_t)this->_wire->read();
			return val;
		}
	}

	return 0;
}

#elif defined(OSPI)
ADS1115::ADS1115(uint8_t address, I2CBus& bus) : _address(address), _i2c(bus, address) {}
ADS1115::ADS1115(uint8_t address) : ADS1115(address, Bus) {}

bool ADS1115::begin() {
	if ((this->_address < 0x48) || (this->_address > 0x4B)) {
		return false;
	}
	if (!this->_i2c.detect()) {
		return false;
	}
	return true;
}

void ADS1115::_write_register(uint8_t reg, uint16_t value) {
	this->_i2c.send_word(reg, this->swap_reg(value));
}

uint16_t ADS1115::_read_register(uint8_t reg) {
	return this->swap_reg((uint16_t)(this->_i2c.read_word(reg) & 0xFFFF));
}

#else
// Simulator/mock backend: no I2C hardware, methods that touch the chip are stubs.
// get_pin_value() below produces synthetic counts based on time + channel index.
ADS1115::ADS1115(uint8_t address) : _address(address) {}

bool ADS1115::begin() {
	return (this->_address >= 0x48) && (this->_address <= 0x4B);
}

void ADS1115::_write_register(uint8_t reg, uint16_t value) { (void)reg; (void)value; }
uint16_t ADS1115::_read_register(uint8_t reg) { (void)reg; return 0; }
#endif

int16_t ADS1115::get_pin_value(uint8_t pin) {
#if defined(ADS1115_HARDWARE)
	this->request_pin(pin);
	uint32_t start = millis();
	while (this->is_busy()) {
		// if ((millis() - start) > 11) {
		if ((millis() - start) > 18) {
			return -1;
		}

#if defined(ARDUINO)
		yield();
#else
		delay(1);
#endif
	}

	return this->get_value();
#else
	// Mock: per-channel slow sine, ~0.5..2.5 V, mapped to single-ended counts [0, 32767].
	float t = millis() * 0.001f;
	float phase = ((this->_address - 0x48) * 4 + pin) * 0.7f;
	float volts = 1.5f + 1.0f * sinf(t * 0.1f + phase);
	int32_t counts = (int32_t)(volts * 1000.0f / ADS1115_SCALE_FACTOR);
	if (counts < 0) counts = 0;
	if (counts > 32767) counts = 32767;
	return (int16_t)counts;
#endif
}

void ADS1115::request_pin(uint8_t pin) {
#if defined(ADS1115_HARDWARE)
	uint16_t config = 0x8000 | ((4 + ((uint16_t)pin)) << 12) | 0x0100 | (4 << 5);
	this->_write_register(0x01, config);
#else
	(void)pin;
#endif
}
