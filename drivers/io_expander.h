#pragma once

#if defined(ARDUINO)

#include <Arduino.h>

// PCA9555 register addresses.
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
	virtual ~IOEXP() {}

	virtual void pinMode(uint8_t pin, uint8_t IOMode) { }
	virtual uint16_t i2c_read(uint8_t reg) { return 0xFFFF; }
	virtual void i2c_write(uint8_t reg, uint16_t v) { }
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
	uint8_t inputmask = 0;
};

#endif
