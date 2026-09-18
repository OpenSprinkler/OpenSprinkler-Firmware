#include "io_expander.h"

#if defined(ARDUINO)

#include "../boards/board_profile.h"

#include <Wire.h>

unsigned char IOEXP::detectType(uint8_t address) {
	Wire.beginTransmission(address);
	if(Wire.endTransmission()!=0) return IOEXP_TYPE_NONEXIST;

	Wire.beginTransmission(address);
	Wire.write(NXP_INVERT_REG);
	Wire.endTransmission();

	if(Wire.requestFrom(address, (uint8_t)2) != 2) return IOEXP_TYPE_UNKNOWN;
	uint8_t low = Wire.read();
	uint8_t high = Wire.read();
	if(low==0x00 && high==0x00) return IOEXP_TYPE_9555;
	return IOEXP_TYPE_8575;
}

void PCA9555::pinMode(uint8_t pin, uint8_t IOMode) {
	uint16_t config = i2c_read(NXP_CONFIG_REG);
	if(IOMode == OUTPUT) {
		config &= ~(1 << pin);
	} else {
		config |= (1 << pin);
	}
	i2c_write(NXP_CONFIG_REG, config);
}

uint16_t PCA9555::i2c_read(uint8_t reg) {
	if(address==255) return 0xFFFF;
	Wire.beginTransmission(address);
	Wire.write(reg);
	Wire.endTransmission();
	if(Wire.requestFrom(address, (uint8_t)2) != 2) return 0xFFFF;
	uint16_t data0 = Wire.read();
	uint16_t data1 = Wire.read();
	return data0+(data1<<8);
}

void PCA9555::i2c_write(uint8_t reg, uint16_t v) {
	if(address==255) return;
	Wire.beginTransmission(address);
	Wire.write(reg);
	Wire.write(v&0xff);
	Wire.write(v>>8);
	Wire.endTransmission();
}

void PCA9555::shift_out(uint8_t plat, uint8_t pclk, uint8_t pdat, uint8_t v) {
	const uint8_t pin_base = osboard::IO_EXPANDER_PIN_BASE;
	if(plat<pin_base || pclk<pin_base || pdat<pin_base) return;

	plat-=pin_base;
	pclk-=pin_base;
	pdat-=pin_base;

	uint16_t output = i2c_read(NXP_OUTPUT_REG);
	output &= ~(1<<plat); i2c_write(NXP_OUTPUT_REG, output);

	for(uint8_t s=0;s<8;s++) {
		output &= ~(1<<pclk); i2c_write(NXP_OUTPUT_REG, output);
		if(v & ((unsigned char)1<<(7-s))) {
			output |= (1<<pdat);
		} else {
			output &= ~(1<<pdat);
		}
		i2c_write(NXP_OUTPUT_REG, output);
		output |= (1<<pclk); i2c_write(NXP_OUTPUT_REG, output);
	}

	output |= (1<<plat); i2c_write(NXP_OUTPUT_REG, output);
}

uint16_t PCF8575::i2c_read(uint8_t reg) {
	if(address==255) return 0xFFFF;
	Wire.beginTransmission(address);
	if(Wire.requestFrom(address, (uint8_t)2) != 2) return 0xFFFF;
	uint16_t data0 = Wire.read();
	uint16_t data1 = Wire.read();
	Wire.endTransmission();
	return data0+(data1<<8);
}

void PCF8575::i2c_write(uint8_t reg, uint16_t v) {
	if(address==255) return;
	Wire.beginTransmission(address);
	Wire.write(v&0xff);
	Wire.write(v>>8);
	Wire.endTransmission();
}

uint16_t PCF8574::i2c_read(uint8_t reg) {
	if(address==255) return 0xFFFF;
	Wire.beginTransmission(address);
	if(Wire.requestFrom(address, (uint8_t)1) != 1) return 0xFFFF;
	uint16_t data = Wire.read();
	Wire.endTransmission();
	return data;
}

void PCF8574::i2c_write(uint8_t reg, uint16_t v) {
	if(address==255) return;
	Wire.beginTransmission(address);
	Wire.write((uint8_t)(v&0xFF) | inputmask);
	Wire.endTransmission();
}

#endif
