/*
 * I2CRTC.h - library for common I2C RTCs
 * This library is intended to be uses with Arduino Time.h library functions
 */


#ifndef I2CRTC_h
#define I2CRTC_h

#define DS1307_ADDR  0x68
#define MCP7940_ADDR 0x6F
#define PCF8563_ADDR 0x51

#include "TimeLib.h"

// library interface description
class I2CRTC
{
	// user-accessible "public" interface
	public:
	I2CRTC();
	static time_t get();
	static void set(time_t t);
	static void read(tmElements_t &tm);
	static void write(tmElements_t &tm);
	static bool detect();
	
	private:
	static uint8_t dec2bcd(uint8_t num);
	static uint8_t bcd2dec(uint8_t num);
	static uint8_t addr;
};

extern I2CRTC RTC;

#endif
 
