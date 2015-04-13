// -------------------------------------------------------------------------------------------
// Port for CubieBoard a library LiquidCrystal version 1.2.1 to drive the PCF8574* I2C
//
// @version API 1.0.0
//
// @author Kabanov Andrew - andrew3007@kabit.ru
// -------------------------------------------------------------------------------------------

#include "LCD.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// CLASS CONSTRUCTORS AND DESTRUCTOR
// -----------------------------------------------------------------------------------------------------------------------
LCD::LCD(uint8_t i2cNumBus, uint8_t i2cDevAddr,
		 uint8_t rs, uint8_t enable,
		 uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3,
		 uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7)
{
	init(i2cNumBus, i2cDevAddr, LCD_8BIT, rs, 255, enable, d0, d1, d2, d3, d4, d5, d6, d7);
}

LCD::LCD(uint8_t i2cNumBus, uint8_t i2cDevAddr,
		 uint8_t rs, uint8_t rw, uint8_t enable,
		 uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3,
		 uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7)
{
	init(i2cNumBus, i2cDevAddr, LCD_8BIT, rs, rw, enable, d0, d1, d2, d3, d4, d5, d6, d7);
}

LCD::LCD(uint8_t i2cNumBus, uint8_t i2cDevAddr,
		 uint8_t rs, uint8_t rw, uint8_t enable,
		 uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3)
{
	init(i2cNumBus, i2cDevAddr, LCD_4BIT, rs, rw, enable, d0, d1, d2, d3, 0, 0, 0, 0);
}

LCD::LCD(uint8_t i2cNumBus, uint8_t i2cDevAddr,
		 uint8_t rs,  uint8_t enable,
		 uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3)
{
	init(i2cNumBus, i2cDevAddr, LCD_4BIT, rs, 255, enable, d0, d1, d2, d3, 0, 0, 0, 0);
}

// Contructors with backlight control
LCD::LCD(uint8_t i2cNumBus, uint8_t i2cDevAddr,
		 uint8_t rs, uint8_t enable,
		 uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3,
		 uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7,
		 uint8_t backlightPin, t_backlighPol pol)
{
	init(i2cNumBus, i2cDevAddr, LCD_8BIT, rs, 255, enable, d0, d1, d2, d3, d4, d5, d6, d7);
	setBacklightPin(backlightPin, pol);
}

LCD::LCD(uint8_t i2cNumBus, uint8_t i2cDevAddr,
		 uint8_t rs, uint8_t rw, uint8_t enable,
		 uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3,
		 uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7,
		 uint8_t backlightPin, t_backlighPol pol)
{
	init(i2cNumBus, i2cDevAddr, LCD_8BIT, rs, rw, enable, d0, d1, d2, d3, d4, d5, d6, d7);
	setBacklightPin(backlightPin, pol);
}

LCD::LCD(uint8_t i2cNumBus, uint8_t i2cDevAddr,
		 uint8_t rs, uint8_t rw, uint8_t enable,
		 uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3,
		 uint8_t backlightPin, t_backlighPol pol)
{
	init(i2cNumBus, i2cDevAddr, LCD_4BIT, rs, rw, enable, d0, d1, d2, d3, 0, 0, 0, 0);
	setBacklightPin(backlightPin, pol);
}

LCD::LCD(uint8_t i2cNumBus, uint8_t i2cDevAddr,
		 uint8_t rs, uint8_t enable,
		 uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3,
		 uint8_t backlightPin, t_backlighPol pol)
{
	init(i2cNumBus, i2cDevAddr, LCD_4BIT, rs, 255, enable, d0, d1, d2, d3, 0, 0, 0, 0);
	setBacklightPin(backlightPin, pol);
}

LCD::~LCD()
{
	close();
}

// ------------------------------------------------------- Init I2C-device -----------------------------------------------------

// init
bool LCD::init(uint8_t i2cNumBus, uint8_t i2cDevAddr,
					 uint8_t fourbitMode, uint8_t rs, uint8_t rw, uint8_t enable,
					 uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3,
					 uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7)
{
	if( !open(i2cNumBus, i2cDevAddr) )
	{
	   return false;
	}

	// Initialize the IO pins
	// -----------------------
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

	// Initialize the IO port direction to OUTPUT
	// ------------------------------------------
	uint8_t i;
	for( i = 0; i < 4; i++ )
	{
		pinMode(_data_pins[i], OUTPUT);
	}

	// Initialize the rest of the ports if it is an 8bit controlled LCD
	// ----------------------------------------------------------------
	if( fourbitMode == LCD_4BIT )
	{
		for( i = 4; i < 8; i++ )
		{
			pinMode(_data_pins[i], OUTPUT);
		}
	}
	pinMode(_rs_pin, OUTPUT);

	// we can save 1 pin by not using RW. Indicate by passing 255 instead of pin#
	if( _rw_pin != 255 )
	{
		pinMode(_rw_pin, OUTPUT);
	}

	pinMode(_enable_pin, OUTPUT);

	// Initialise displaymode functions to defaults: LCD_1LINE and LCD_5x8DOTS
	// -------------------------------------------------------------------------
	if( fourbitMode == LCD_4BIT )
	{
		_displayfunction = LCD_4BITMODE | LCD_1LINE | LCD_5x8DOTS;
	}
	else
	{
		_displayfunction = LCD_8BITMODE | LCD_1LINE | LCD_5x8DOTS;
	}

	// Now we pull both RS and R/W low to begin commands
	digitalWrite(_rs_pin, LOW);
	digitalWrite(_enable_pin, LOW);

	if( _rw_pin != 255 )
	{
		digitalWrite(_rw_pin, LOW);
	}

	// Initialise the backlight pin no nothing
	_backlightPin = LCD_NOBACKLIGHT;
	_polarity = POSITIVE;

	return true;
}

//
// setBacklightPin
void LCD::setBacklightPin(uint8_t pin, t_backlighPol pol)
{
	pinMode(pin, OUTPUT);       // Difine the backlight pin as output
	_backlightPin = pin;
	_polarity = pol;
	setBacklight(BACKLIGHT_OFF);   // Set the backlight low by default
}

//
// setBackligh
void LCD::setBacklight(uint8_t value)
{
	// Check if there is a pin assigned to the backlight
	// ---------------------------------------------------
	if( _backlightPin != LCD_NOBACKLIGHT )
	{
		if( ((value > 0) && (_polarity == POSITIVE)) ||
			((value == 0) && (_polarity == NEGATIVE)) )
		{
			digitalWrite(_backlightPin, HIGH);
		}
		else
		{
			digitalWrite(_backlightPin, LOW);
		}
	}
}

// PUBLIC METHODS
// ---------------------------------------------------------------------------
// When the display powers up, it is configured as follows:
//
// 1. Display clear
// 2. Function set:
//    DL = 1; 8-bit interface data 
//    N = 0; 1-line display 
//    F = 0; 5x8 dot character font 
// 3. Display on/off control: 
//    D = 0; Display off 
//    C = 0; Cursor off 
//    B = 0; Blinking off 
// 4. Entry mode set: 
//    I/D = 1; Increment by 1 
//    S = 0; No shift 
// ---------------------------------------------------------------------------
void LCD::begin(uint8_t cols, uint8_t lines, uint8_t dotsize)
{
	if( lines > 1 )
	{
		_displayfunction |= LCD_2LINE;
	}
	_numlines = lines;
	_cols = cols;

	// for some 1 line displays you can select a 10 pixel high font
	// ------------------------------------------------------------
	if( (dotsize != LCD_5x8DOTS) && (lines == 1) )
	{
		_displayfunction |= LCD_5x10DOTS;
	}

	// SEE PAGE 45/46 FOR INITIALIZATION SPECIFICATION!
	// according to datasheet, we need at least 40ms after power rises above 2.7V
	// before sending commands. Arduino can turn on way before 4.5V so we'll wait 
	// 50
	// ---------------------------------------------------------------------------
	usleep(100000); // 100 milliSeconds delay

	//put the LCD into 4 bit or 8 bit mode
	// -------------------------------------
	if( !(_displayfunction & LCD_8BITMODE) )
	{
		// this is according to the hitachi HD44780 datasheet
		// figure 24, pg 46

		// we start in 8bit mode, try to set 4 bit mode
		send(0x03, FOUR_BITS);
		usleep(4500); // wait min 4.1ms

		// second try
		send(0x03, FOUR_BITS);
		usleep(4500); // wait min 4.1ms

		// third go!
		send(0x03, FOUR_BITS);
		usleep(150);

		// finally, set to 4-bit interface
		send(0x02, FOUR_BITS);
	}
	else
	{
		// this is according to the hitachi HD44780 datasheet
		// page 45 figure 23

		// Send function set command sequence
		command(LCD_FUNCTIONSET | _displayfunction);
		usleep(4500);  // wait more than 4.1ms

		// second try
		command(LCD_FUNCTIONSET | _displayfunction);
		usleep(150);

		// third go
		command(LCD_FUNCTIONSET | _displayfunction);
	}

	// finally, set # lines, font size, etc.
	command(LCD_FUNCTIONSET | _displayfunction);

	// turn the display on with no cursor or blinking default
	_displaycontrol = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
	display();

	// clear the LCD
	clear();

	// Initialize to default text direction (for romance languages)
	_displaymode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
	// set the entry mode
	command(LCD_ENTRYMODESET | _displaymode);

	backlight();
}

// Common LCD Commands
// ---------------------------------------------------------------------------------------------------------------------
void LCD::clear()
{
	command(LCD_CLEARDISPLAY);             // clear display, set cursor position to zero
	usleep(HOME_CLEAR_EXEC);   // this command is time consuming
}

void LCD::home()
{
	command(LCD_RETURNHOME);              // set cursor position to zero
	usleep(HOME_CLEAR_EXEC);  // This command is time consuming
}

void LCD::setCursor(uint8_t col, uint8_t row)
{
	const uint8_t row_offsetsDef[]   = { 0x00, 0x40, 0x14, 0x54 }; // For regular LCDs
	const uint8_t row_offsetsLarge[] = { 0x00, 0x40, 0x10, 0x50 }; // For 16x4 LCDs

	if( row >= _numlines ) 
	{
		row = _numlines-1;    // rows start at 0
	}

	// 16x4 LCDs have special memory map layout
	// ----------------------------------------
	if( (_cols == 16) && (_numlines == 4) )
	{
		command(LCD_SETDDRAMADDR | (col + row_offsetsLarge[row]));
	}
	else
	{
		command(LCD_SETDDRAMADDR | (col + row_offsetsDef[row]));
	}
}

// Turn the display on/off
void LCD::noDisplay()
{
	_displaycontrol &= ~LCD_DISPLAYON;
	command(LCD_DISPLAYCONTROL | _displaycontrol);
}

void LCD::display()
{
	_displaycontrol |= LCD_DISPLAYON;
	command(LCD_DISPLAYCONTROL | _displaycontrol);
}

// Turns the underline cursor on/off
void LCD::noCursor()
{
	_displaycontrol &= ~LCD_CURSORON;
	command(LCD_DISPLAYCONTROL | _displaycontrol);
}

void LCD::cursor()
{
	_displaycontrol |= LCD_CURSORON;
	command(LCD_DISPLAYCONTROL | _displaycontrol);
}

// Turns on/off the blinking cursor
void LCD::noBlink()
{
	_displaycontrol &= ~LCD_BLINKON;
	command(LCD_DISPLAYCONTROL | _displaycontrol);
}

void LCD::blink()
{
	_displaycontrol |= LCD_BLINKON;
	command(LCD_DISPLAYCONTROL | _displaycontrol);
}

// These commands scroll the display without changing the RAM
void LCD::scrollDisplayLeft()
{
	command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVELEFT);
}

void LCD::scrollDisplayRight()
{
	command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVERIGHT);
}

// This is for text that flows Left to Right
void LCD::leftToRight()
{
	_displaymode |= LCD_ENTRYLEFT;
	command(LCD_ENTRYMODESET | _displaymode);
}

// This is for text that flows Right to Left
void LCD::rightToLeft()
{
	_displaymode &= ~LCD_ENTRYLEFT;
	command(LCD_ENTRYMODESET | _displaymode);
}

// This method moves the cursor one space to the right
void LCD::moveCursorRight()
{
	command(LCD_CURSORSHIFT | LCD_CURSORMOVE | LCD_MOVERIGHT);
}

// This method moves the cursor one space to the left
void LCD::moveCursorLeft()
{
	command(LCD_CURSORSHIFT | LCD_CURSORMOVE | LCD_MOVELEFT);
}


// This will 'right justify' text from the cursor
void LCD::autoscroll()
{
	_displaymode |= LCD_ENTRYSHIFTINCREMENT;
	command(LCD_ENTRYMODESET | _displaymode);
}

// This will 'left justify' text from the cursor
void LCD::noAutoscroll()
{
	_displaymode &= ~LCD_ENTRYSHIFTINCREMENT;
	command(LCD_ENTRYMODESET | _displaymode);
}

// Write to CGRAM of new characters
void LCD::createChar(uint8_t location, uint8_t charmap[])
{
	location &= 0x7; // we only have 8 locations 0-7

	command(LCD_SETCGRAMADDR | (location << 3));
	usleep(30);

	for(int i = 0; i < 8; i++)
	{
		write(charmap[i]);
		usleep(40);
	}
}

//
// Switch on the backlight
void LCD::backlight()
{
	setBacklight(255);
}

//
// Switch off the backlight
void LCD::noBacklight()
{
	setBacklight(0);
}

//
// Switch fully on the LCD (backlight and LCD)
void LCD::on()
{
	display();
	backlight();
}

//
// Switch fully off the LCD (backlight and LCD) 
void LCD::off()
{
	noBacklight();
	noDisplay();
}

// ------------------------------------------------------------ Print text --------------------------------------------------------------
void LCD::command(uint8_t value)
{
	send(value, COMMAND);
}

ssize_t LCD::write(uint8_t value)
{
	return send(value, DATA);
}

int LCD::print(const char* text)
{
	const char* ptr = text;
	int i;
	while( *ptr != '\0' )
	{
		i = write(*ptr);
		if( i < 1 )
		{
			break;
		}
		else
		{
			ptr++;
		}
	}
	return ptr - text;
}

// ---------------------------------------------------- Low level push data to I2C-device --------------------------------------------
//
// send
ssize_t LCD::send(uint8_t value, uint8_t mode)
{
	// Only interested in COMMAND or DATA
	ssize_t ret = digitalWrite(_rs_pin, (mode == DATA));
	if( ret == -1 )	return (-1);

	// if there is a RW pin indicated, set it low to Write
	// ---------------------------------------------------
	if( _rw_pin != 255 )
	{
		ret = digitalWrite(_rw_pin, LOW);
		if( ret == -1 )	return (-1);
	}

	if( mode != FOUR_BITS )
	{
		if( (_displayfunction & LCD_8BITMODE) )
		{
			ret = writeNbits(value, 8);
			if( ret == -1 )	return (-1);
		}
		else
		{
			ret = writeNbits(value >> 4, 4);
			if( ret == -1 )	return (-1);
			ret = writeNbits(value, 4);
			if( ret == -1 )	return (-1);
		}
	}
	else
	{
		ret = writeNbits(value, 4);
		if( ret == -1 )	return (-1);
	}
	waitUsec(TIME_EXEC_COMMAND_BY_LCD); // wait for the command to execute by the LCD

	return 1;
}

//
// write4bits
ssize_t LCD::writeNbits(uint8_t value, uint8_t numBits)
{
	ssize_t ret = -1;

	for(uint8_t i = 0; i < numBits; i++)
	{
		ret = digitalWrite(_data_pins[i], (value >> i) & 0x01);
		if( ret == -1 ) return (-1);
	}
	pulseEnable();

	return numBits;
}

//
// pulseEnable
void LCD::pulseEnable()
{
	// There is no need for the delays, since the digitalWrite operation
	// takes longer.
	digitalWrite(_enable_pin, HIGH);
	waitUsec(1);          // enable pulse must be > 450ns
	digitalWrite(_enable_pin, LOW);
}
