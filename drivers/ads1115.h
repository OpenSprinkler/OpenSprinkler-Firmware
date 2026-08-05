#pragma once

#include "../defines.h"

#include <stdint.h>

#define ADS1115_SCALE_FACTOR (6144.0 / 32768.0)

// Hardware ADS1115 chip is wired up on ESP8266 (Arduino I2C via Wire) and OSPi
// (Linux I2C via libi2c). Other targets (DEMO/SIM) use a mock backend that
// produces synthetic counts so the sensor pipeline is exercisable end-to-end
// without real hardware.
#if defined(ARDUINO) || defined(OSPI)
#define ADS1115_HARDWARE
#endif

#if defined(ARDUINO)
#include <Arduino.h>
#include <Wire.h>
#elif defined(OSPI)
#include "../platform/i2c.h"
#endif

class ADS1115 {
public:
#if defined(ARDUINO)
	ADS1115(uint8_t address, TwoWire& wire);
#elif defined(OSPI)
	ADS1115(uint8_t address, I2CBus& bus);
#endif
	ADS1115(uint8_t address);
	int16_t get_pin_value(uint8_t pin);
	bool begin();

	int16_t get_value() {
		return (int16_t) this->_read_register(0x00);
	}

	void request_pin(uint8_t pin);

	bool is_busy() {
		return (this->_read_register(0x01) & 0x8000) == 0;
	}

	private:
	uint8_t _address;
#if defined(ARDUINO)
	TwoWire *_wire;
#elif defined(OSPI)
	I2CDevice _i2c;
	uint16_t swap_reg(uint16_t val) {
		return (val << 8) | (val >> 8);
	}
#endif

	void _write_register(uint8_t reg, uint16_t value);
	uint16_t _read_register(uint8_t reg);
};
