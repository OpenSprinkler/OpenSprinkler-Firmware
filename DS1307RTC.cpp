/*
 * DS1307RTC.h - library for DS1307 RTC
  
  Copyright (c) Michael Margolis 2009
  This library is intended to be uses with Arduino Time.h library functions

  The library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
  
  30 Dec 2009 - Initial release
  5 Sep 2011 updated for Arduino 1.0
  
  23 Dec 2013 -- modified by Ray Wang (Rayshobby LLC) to add support for MCP7940
 */

#include "Wire.h"
#include "DS1307RTC.h"

#define DS1307_CTRL_ID 0x68
#define MCP7940_CTRL_ID 0x6F // ray: ctrl id for MCP7940N

int DS1307RTC::ctrl_id = 0;

DS1307RTC::DS1307RTC()
{
  Wire.begin();
}

uint8_t DS1307RTC::detect()
{
  uint8_t ret;
  ctrl_id = DS1307_CTRL_ID;  // ray: detect DS1307
  Wire.beginTransmission(ctrl_id);
  Wire.write((uint8_t)(0x00));
  ret = Wire.endTransmission();
  if (ret == 0) {return 0;}

  ctrl_id = MCP7940_CTRL_ID;  // ray: detect MCP7940
  Wire.beginTransmission(ctrl_id);  
  Wire.write((uint8_t)(0x00));
  ret = Wire.endTransmission();
  if (ret == 0) {return 0;}
  
  ctrl_id = 0;
  return ret;
}

// PUBLIC FUNCTIONS
time_t DS1307RTC::get()   // Aquire data from buffer and convert to time_t
{
  tmElements_t tm;
  read(tm);
  return(makeTime(tm));
}

void DS1307RTC::set(time_t t)
{
  tmElements_t tm;
  breakTime(t, tm);
  //tm.Second |= 0x80;  // stop the clock   ray: removed this step
  //write(tm); 
  //tm.Second &= 0x7f;  // start the clock  ray: moved to write function
  write(tm); 
}

// Aquire data from the RTC chip in BCD format
void DS1307RTC::read( tmElements_t &tm)
{
  if (!ctrl_id) return;
  Wire.beginTransmission(ctrl_id);

  Wire.write((uint8_t)0x00); 
  Wire.endTransmission();

  // request the 7 data fields   (secs, min, hr, dow, date, mth, yr)
  Wire.requestFrom(ctrl_id, tmNbrFields);
  tm.Second = bcd2dec(Wire.read() & 0x7f);   
  tm.Minute = bcd2dec(Wire.read() );
  tm.Hour =   bcd2dec(Wire.read() & 0x3f);  // mask assumes 24hr clock
  tm.Wday = bcd2dec(Wire.read() & 0x07);
  tm.Day = bcd2dec(Wire.read() );
  tm.Month = bcd2dec(Wire.read() );
  tm.Year = y2kYearToTm((bcd2dec(Wire.read())));

}

void DS1307RTC::write(tmElements_t &tm)
{
  static uint8_t initialized = 0;
  
  if (ctrl_id == DS1307_CTRL_ID) {
    Wire.beginTransmission(ctrl_id);  
    Wire.write((uint8_t)0x00); // reset register pointer  
    Wire.write(dec2bcd(tm.Second) & 0x7f);  // ray: start clock by setting CH bit low
    Wire.write(dec2bcd(tm.Minute));
    Wire.write(dec2bcd(tm.Hour));      // sets 24 hour format
    Wire.write(dec2bcd(tm.Wday));   
    Wire.write(dec2bcd(tm.Day));
    Wire.write(dec2bcd(tm.Month));
    Wire.write(dec2bcd(tmYearToY2k(tm.Year))); 
    Wire.endTransmission();
  } else if (ctrl_id == MCP7940_CTRL_ID) {
    Wire.beginTransmission(ctrl_id);  
    Wire.write((uint8_t)0x00); // reset register pointer  
    Wire.write(dec2bcd(tm.Second) | 0x80);  // ray: start clock by setting ST bit high
    Wire.write(dec2bcd(tm.Minute));
    Wire.write(dec2bcd(tm.Hour));      // sets 24 hour format
    Wire.write(dec2bcd(tm.Wday) | 0x08);  // ray: turn on battery backup by setting VBATEN bit high
    Wire.write(dec2bcd(tm.Day));
    Wire.write(dec2bcd(tm.Month));
    Wire.write(dec2bcd(tmYearToY2k(tm.Year))); 
    Wire.endTransmission();  
  } else {
    // undefined RTC type
  }  
}
// PRIVATE FUNCTIONS

// Convert Decimal to Binary Coded Decimal (BCD)
uint8_t DS1307RTC::dec2bcd(uint8_t num)
{
  return ((num/10 * 16) + (num % 10));
}

// Convert Binary Coded Decimal (BCD) to Decimal
uint8_t DS1307RTC::bcd2dec(uint8_t num)
{
  return ((num/16 * 10) + (num % 16));
}

DS1307RTC RTC = DS1307RTC(); // create an instance for the user

