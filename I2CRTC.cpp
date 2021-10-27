/*
 * I2CRTC.cpp - library for I2C RTC
	
	Copyright (c) Michael Margolis 2009
	This library is intended to be uses with Arduino Time.h library functions

	The library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
	
	30 Dec 2009 - Initial release
	5 Sep 2011 updated for Arduino 1.0
	
	23 Dec 2013 -- modified by Ray Wang (Rayshobby LLC) to add support for MCP7940
 */


#if defined(ARDUINO)

#include "I2CRTC.h"
#include <Wire.h>

static uint8_t _addrs[] = {DS1307_ADDR, MCP7940_ADDR, PCF8563_ADDR};

uint8_t I2CRTC::addr = 0;

I2CRTC::I2CRTC()
{
	Wire.begin();
}

bool I2CRTC::exists() {
	return (addr!=0);
}

bool I2CRTC::detect()
{
	addr = 0;
	for(uint8_t i = 0;i < sizeof(_addrs);i ++) {
		Wire.beginTransmission(_addrs[i]);
		Wire.write((uint8_t)0x00);
		if(Wire.endTransmission()==0) { // if chip detected successfully
			addr = _addrs[i];
			return true;
		}
	}
	return false;
}

// PUBLIC FUNCTIONS
time_t I2CRTC::get()	 // Aquire data from buffer and convert to time_t
{
	tmElements_t tm;
	read(tm);
	return(makeTime(tm));
}

void I2CRTC::set(time_t t)
{
	tmElements_t tm;
	breakTime(t, tm);
	//tm.Second |= 0x80;	// stop the clock		ray: removed this step
	//write(tm); 
	//tm.Second &= 0x7f;	// start the clock	ray: moved to write function
	write(tm); 
}

// Aquire data from the RTC chip in BCD format
void I2CRTC::read( tmElements_t &tm)
{
	if(!addr) return;
	Wire.beginTransmission(addr);

	if(addr == PCF8563_ADDR) { 
			Wire.write((uint8_t)0x02);
			Wire.endTransmission();				 
			Wire.requestFrom(addr, (uint8_t)7);
			tm.Second = bcd2dec(Wire.read() & 0x7f);
			tm.Minute = bcd2dec(Wire.read() & 0x7f);
			tm.Hour =		bcd2dec(Wire.read() & 0x3f);	// mask assumes 24hr clock
			tm.Day = bcd2dec(Wire.read() & 0x3f);
			tm.Wday = bcd2dec(Wire.read() & 0x07);
			tm.Month = bcd2dec(Wire.read() & 0x1f);		// fix bug for MCP7940
			tm.Year = (bcd2dec(Wire.read()));
	} else {
			Wire.write((uint8_t)0x00); 
			Wire.endTransmission();				 
			Wire.requestFrom(addr, (uint8_t)7);
			tm.Second = bcd2dec(Wire.read() & 0x7f);
			tm.Minute = bcd2dec(Wire.read() & 0x7f);
			tm.Hour =		bcd2dec(Wire.read() & 0x3f);	// mask assumes 24hr clock
			tm.Wday = bcd2dec(Wire.read() & 0x07);
			tm.Day = bcd2dec(Wire.read() & 0x3f);
			tm.Month = bcd2dec(Wire.read() & 0x1f);		// fix bug for MCP7940
			tm.Year = y2kYearToTm((bcd2dec(Wire.read())));
	}
}

void I2CRTC::write(tmElements_t &tm)
{
	if(!addr) return;
	switch(addr) {
		case DS1307_ADDR:
			Wire.beginTransmission(addr);  
			Wire.write((uint8_t)0x00); // reset register pointer	
			Wire.write(dec2bcd(tm.Second) & 0x7f);	// ray: start clock by setting CH bit low
			Wire.write(dec2bcd(tm.Minute));
			Wire.write(dec2bcd(tm.Hour));			 // sets 24 hour format
			Wire.write(dec2bcd(tm.Wday));		
			Wire.write(dec2bcd(tm.Day));
			Wire.write(dec2bcd(tm.Month));
			Wire.write(dec2bcd(tmYearToY2k(tm.Year))); 
			Wire.endTransmission();
			break;
		case MCP7940_ADDR:
			Wire.beginTransmission(addr);  
			Wire.write((uint8_t)0x00); // reset register pointer	
			Wire.write(dec2bcd(tm.Second) | 0x80);	// ray: start clock by setting ST bit high
			Wire.write(dec2bcd(tm.Minute));
			Wire.write(dec2bcd(tm.Hour));			 // sets 24 hour format
			Wire.write(dec2bcd(tm.Wday) | 0x08);	// ray: turn on battery backup by setting VBATEN bit high
			Wire.write(dec2bcd(tm.Day));
			Wire.write(dec2bcd(tm.Month));
			Wire.write(dec2bcd(tmYearToY2k(tm.Year))); 
			Wire.endTransmission();
			break;
		case PCF8563_ADDR:
			Wire.beginTransmission(addr);  
			Wire.write((uint8_t)0x02); // reset register pointer	
			Wire.write(dec2bcd(tm.Second) & 0x7f);
			Wire.write(dec2bcd(tm.Minute));
			Wire.write(dec2bcd(tm.Hour));			 // sets 24 hour format
			Wire.write(dec2bcd(tm.Day));
			Wire.write(dec2bcd(tm.Wday));		
			Wire.write(dec2bcd(tm.Month));
			Wire.write(dec2bcd(tm.Year)); 
			Wire.endTransmission();
			break;
	}		 
}

// Convert Decimal to Binary Coded Decimal (BCD)
uint8_t I2CRTC::dec2bcd(uint8_t num)
{
	return ((num/10 * 16) + (num % 10));
}

// Convert Binary Coded Decimal (BCD) to Decimal
uint8_t I2CRTC::bcd2dec(uint8_t num)
{
	return ((num/16 * 10) + (num % 16));
}

I2CRTC RTC = I2CRTC(); // create an instance for the user

#endif
