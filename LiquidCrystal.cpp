#if defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__)

#include "LiquidCrystal.h"
#include <inttypes.h>
#include <Arduino.h>
#include <Wire.h>

// When the display powers up, it is configured as follows:
//
// 1. Display clear
// 2. Function set:
//		DL = 1; 8-bit interface data
//		N = 0; 1-line display
//		F = 0; 5x8 dot character font
// 3. Display on/off control:
//		D = 0; Display off
//		C = 0; Cursor off
//		B = 0; Blinking off
// 4. Entry mode set:
//		I/D = 1; Increment by 1
//		S = 0; No shift
//
// Note, however, that resetting the Arduino doesn't reset the LCD, so we
// can't assume that its in that state when a sketch starts (and the
// LiquidCrystal constructor is called)

void LiquidCrystal::begin() {
	if (_type == LCD_I2C) {
		_displayfunction = LCD_4BITMODE | LCD_2LINE | LCD_5x8DOTS;

		// SEE PAGE 45/46 FOR INITIALIZATION SPECIFICATION!
		// according to datasheet, we need at least 40ms after power rises above 2.7V
		// before sending commands. Arduino can turn on way befer 4.5V so we'll wait 50
		delay(50);

		// Now we pull both RS and R/W low to begin commands
		expanderWrite(_backlightval);	// reset expanderand turn backlight off (Bit 8 =1)
		delay(1000);

		//put the LCD into 4 bit mode
		// this is according to the hitachi HD44780 datasheet
		// figure 24, pg 46

		// we start in 8bit mode, try to set 4 bit mode
		write4bits(0x03 << 4);
		delayMicroseconds(4500); // wait min 4.1ms

		// second try
		write4bits(0x03 << 4);
		delayMicroseconds(4500); // wait min 4.1ms

		// third go!
		write4bits(0x03 << 4);
		delayMicroseconds(150);

		// finally, set to 4-bit interface
		write4bits(0x02 << 4);

		// set # lines, font size, etc.
		command(LCD_FUNCTIONSET | _displayfunction);

		// turn the display on with no cursor or blinking default
		_displaycontrol = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
		display();

		// clear it off
		clear();

		// Initialize to default text direction (for roman languages)
		_displaymode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;

		// set the entry mode
		command(LCD_ENTRYMODESET | _displaymode);

		home();
	}

	if (_type == LCD_STD) {
		_displayfunction |= LCD_2LINE;
		_numlines = 2;
		_currline = 0;

		// SEE PAGE 45/46 FOR INITIALIZATION SPECIFICATION!
		// according to datasheet, we need at least 40ms after power rises above 2.7V
		// before sending commands. Arduino can turn on way befer 4.5V so we'll wait 50
		delayMicroseconds(50000);
		// Now we pull both RS and R/W low to begin commands
		digitalWrite(_rs_pin, LOW);
		digitalWrite(_enable_pin, LOW);
		if (_rw_pin != 255) {
			digitalWrite(_rw_pin, LOW);
		}

		//put the LCD into 4 bit or 8 bit mode
		if (! (_displayfunction & LCD_8BITMODE)) {
			// this is according to the hitachi HD44780 datasheet
			// figure 24, pg 46

			// we start in 8bit mode, try to set 4 bit mode
			write4bits(0x03);
			delayMicroseconds(4500); // wait min 4.1ms

			// second try
			write4bits(0x03);
			delayMicroseconds(4500); // wait min 4.1ms

			// third go!
			write4bits(0x03);
			delayMicroseconds(150);

			// finally, set to 4-bit interface
			write4bits(0x02);
		} else {
			// this is according to the hitachi HD44780 datasheet
			// page 45 figure 23

			// Send function set command sequence
			command(LCD_FUNCTIONSET | _displayfunction);
			delayMicroseconds(4500);	// wait more than 4.1ms

			// second try
			command(LCD_FUNCTIONSET | _displayfunction);
			delayMicroseconds(150);

			// third go
			command(LCD_FUNCTIONSET | _displayfunction);
		}

		// finally, set # lines, font size, etc.
		command(LCD_FUNCTIONSET | _displayfunction);

		// turn the display on with no cursor or blinking default
		_displaycontrol = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
		display();

		// clear it off
		clear();

		// Initialize to default text direction (for romance languages)
		_displaymode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
		// set the entry mode
		command(LCD_ENTRYMODESET | _displaymode);
	}
}

void LiquidCrystal::init(uint8_t fourbitmode, uint8_t rs, uint8_t rw, uint8_t enable,
			 uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3,
			 uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7)
{
	_rs_pin = rs;
	_rw_pin = rw;
	_enable_pin = enable;

	_data_pins[0] = d0;
	_data_pins[1] = d1;
	_data_pins[2] = d2;
	_data_pins[3] = d3;
	_data_pins[4] = d4;
	_data_pins[5] = d5;
	_data_pins[6] = d6;
	_data_pins[7] = d7;

	Wire.begin();
	_type = LCD_STD;

	// detect I2C and assign _type variable accordingly
	Wire.beginTransmission(LCD_I2C_ADDR1);	// check type 1
	//Wire.write(0x00);
	uint8_t ret1 = Wire.endTransmission();
	Wire.beginTransmission(LCD_I2C_ADDR2);	// check type 2
	//Wire.write(0x00);
	uint8_t ret2 = Wire.endTransmission();

	if (!ret1 || !ret2)  _type = LCD_I2C;
	if (_type == LCD_I2C) {
		if(!ret1) _addr = LCD_I2C_ADDR1;
		else _addr = LCD_I2C_ADDR2;
		_cols = 16;
		_rows = 2;
		_charsize = LCD_5x8DOTS;
		_backlightval = LCD_BACKLIGHT;
	}

	if (_type == LCD_STD) {
		pinMode(_rs_pin, OUTPUT);
		// we can save 1 pin by not using RW. Indicate by passing 255 instead of pin#
		if (_rw_pin != 255) {
			pinMode(_rw_pin, OUTPUT);
		}
		pinMode(_enable_pin, OUTPUT);

	}
	_displayfunction = LCD_4BITMODE | LCD_2LINE | LCD_5x8DOTS;

}

/********** high level commands, for the user! */
void LiquidCrystal::clear()
{
	command(LCD_CLEARDISPLAY);// clear display, set cursor position to zero
	delayMicroseconds(2000);	// this command takes a long time!
}

void LiquidCrystal::home()
{
	command(LCD_RETURNHOME);	// set cursor position to zero
	delayMicroseconds(2000);	// this command takes a long time!
}

void LiquidCrystal::setCursor(uint8_t col, uint8_t row)
{
	int row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };
	if (_type == LCD_I2C) {
		if (row > _rows) {
			row = _rows-1;		// we count rows starting w/0
		}
	}
	if (_type == LCD_STD) {
		if (row >= _numlines) {
			row = _numlines-1;
		}
	}
	command(LCD_SETDDRAMADDR | (col + row_offsets[row]));
}

// Turn the display on/off (quickly)
void LiquidCrystal::noDisplay() {
	_displaycontrol &= ~LCD_DISPLAYON;
	command(LCD_DISPLAYCONTROL | _displaycontrol);
}
void LiquidCrystal::display() {
	_displaycontrol |= LCD_DISPLAYON;
	command(LCD_DISPLAYCONTROL | _displaycontrol);
}

// Turns the underline cursor on/off
void LiquidCrystal::noCursor() {
	_displaycontrol &= ~LCD_CURSORON;
	command(LCD_DISPLAYCONTROL | _displaycontrol);
}
void LiquidCrystal::cursor() {
	_displaycontrol |= LCD_CURSORON;
	command(LCD_DISPLAYCONTROL | _displaycontrol);
}

// Turn on and off the blinking cursor
void LiquidCrystal::noBlink() {
	_displaycontrol &= ~LCD_BLINKON;
	command(LCD_DISPLAYCONTROL | _displaycontrol);
}
void LiquidCrystal::blink() {
	_displaycontrol |= LCD_BLINKON;
	command(LCD_DISPLAYCONTROL | _displaycontrol);
}

// These commands scroll the display without changing the RAM
void LiquidCrystal::scrollDisplayLeft(void) {
	command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVELEFT);
}
void LiquidCrystal::scrollDisplayRight(void) {
	command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVERIGHT);
}

// This is for text that flows Left to Right
void LiquidCrystal::leftToRight(void) {
	_displaymode |= LCD_ENTRYLEFT;
	command(LCD_ENTRYMODESET | _displaymode);
}

// This is for text that flows Right to Left
void LiquidCrystal::rightToLeft(void) {
	_displaymode &= ~LCD_ENTRYLEFT;
	command(LCD_ENTRYMODESET | _displaymode);
}

// This will 'right justify' text from the cursor
void LiquidCrystal::autoscroll(void) {
	_displaymode |= LCD_ENTRYSHIFTINCREMENT;
	command(LCD_ENTRYMODESET | _displaymode);
}

// This will 'left justify' text from the cursor
void LiquidCrystal::noAutoscroll(void) {
	_displaymode &= ~LCD_ENTRYSHIFTINCREMENT;
	command(LCD_ENTRYMODESET | _displaymode);
}

// Allows us to fill the first 8 CGRAM locations
// with custom characters
//void LiquidCrystal::createChar(uint8_t location, uint8_t charmap[]) {
void LiquidCrystal::createChar(uint8_t location, PGM_P ptr) {
	location &= 0x7; // we only have 8 locations 0-7
	command(LCD_SETCGRAMADDR | (location << 3));
	for (int i=0; i<8; i++) {
		//write(charmap[i]);
		write(pgm_read_byte(ptr++));
	}
}

// Turn the (optional) backlight off/on
void LiquidCrystal::noBacklight(void) {
	_backlightval=LCD_NOBACKLIGHT;
	expanderWrite(0);
}

void LiquidCrystal::backlight(void) {
	_backlightval=LCD_BACKLIGHT;
	expanderWrite(0);
}

/*********** mid level commands, for sending data/cmds */

inline void LiquidCrystal::command(uint8_t value) {
	send(value, 0);
}

inline size_t LiquidCrystal::write(uint8_t value) {
	send(value, Rs);
	return 1; // assume sucess
}

/************ low level data pushing commands **********/

// write either command or data
void LiquidCrystal::send(uint8_t value, uint8_t mode) {
	if (_type == LCD_I2C) {
		uint8_t highnib=value&0xf0;
		uint8_t lownib=(value<<4)&0xf0;
		write4bits((highnib)|mode);
		write4bits((lownib)|mode);
	}
	if (_type == LCD_STD) {
		digitalWrite(_rs_pin, mode);

		// if there is a RW pin indicated, set it low to Write
		if (_rw_pin != 255) {
			digitalWrite(_rw_pin, LOW);
		}

		write4bits(value>>4);
		write4bits(value);
	}
}

void LiquidCrystal::write4bits(uint8_t value) {
	if (_type == LCD_I2C) {
		expanderWrite(value);
		pulseEnable(value);
	}
	if (_type == LCD_STD) {
		for (int i = 0; i < 4; i++) {
			pinMode(_data_pins[i], OUTPUT);
			digitalWrite(_data_pins[i], (value >> i) & 0x01);
		}

		pulseEnable();
	}
}

void LiquidCrystal::expanderWrite(uint8_t _data){
	Wire.beginTransmission(_addr);
	Wire.write((int)(_data) | _backlightval);
	Wire.endTransmission();
}

void LiquidCrystal::pulseEnable(uint8_t _data){
	expanderWrite(_data | En);	// En high
	delayMicroseconds(1);		// enable pulse must be >450ns

	expanderWrite(_data & ~En);	// En low
	delayMicroseconds(50);		// commands need > 37us to settle
}

void LiquidCrystal::pulseEnable(void) {
	digitalWrite(_enable_pin, LOW);
	delayMicroseconds(1);
	digitalWrite(_enable_pin, HIGH);
	delayMicroseconds(1);		 // enable pulse must be >450ns
	digitalWrite(_enable_pin, LOW);
	delayMicroseconds(100);		// commands need > 37us to settle
}

#endif
