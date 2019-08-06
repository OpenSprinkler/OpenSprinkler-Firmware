#ifndef LIQUID_CRYSTAL_DUAL_H
#define LIQUID_CRYSTAL_DUAL_H

#if defined(ARDUINO) && !defined(ESP8266)

#include <inttypes.h>
#include <Print.h>

// commands
#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME 0x02
#define LCD_ENTRYMODESET 0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT 0x10
#define LCD_FUNCTIONSET 0x20
#define LCD_SETCGRAMADDR 0x40
#define LCD_SETDDRAMADDR 0x80

// flags for display entry mode
#define LCD_ENTRYRIGHT 0x00
#define LCD_ENTRYLEFT 0x02
#define LCD_ENTRYSHIFTINCREMENT 0x01
#define LCD_ENTRYSHIFTDECREMENT 0x00

// flags for display on/off control
#define LCD_DISPLAYON 0x04
#define LCD_DISPLAYOFF 0x00
#define LCD_CURSORON 0x02
#define LCD_CURSOROFF 0x00
#define LCD_BLINKON 0x01
#define LCD_BLINKOFF 0x00

// flags for display/cursor shift
#define LCD_DISPLAYMOVE 0x08
#define LCD_CURSORMOVE 0x00
#define LCD_MOVERIGHT 0x04
#define LCD_MOVELEFT 0x00

// flags for function set
#define LCD_8BITMODE 0x10
#define LCD_4BITMODE 0x00
#define LCD_2LINE 0x08
#define LCD_1LINE 0x00
#define LCD_5x10DOTS 0x04
#define LCD_5x8DOTS 0x00

// flags for backlight control
#define LCD_BACKLIGHT 0x08
#define LCD_NOBACKLIGHT 0x00

#define En B00000100	// Enable bit
#define Rw B00000010	// Read/Write bit
#define Rs B00000001	// Register select bit

#define LCD_STD 0			// Standard LCD
#define LCD_I2C 1			// I2C LCD
#define LCD_I2C_ADDR1 0x27 // type using PCF8574,  at address 0x27
#define LCD_I2C_ADDR2 0x3F // type using PCF8574A, at address 0x3F

class LiquidCrystal : public Print {
public:
	LiquidCrystal() {}
	void init(uint8_t fourbitmode, uint8_t rs, uint8_t rw, uint8_t enable,
				uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3,
				uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7);

	void begin();

	void clear();
	void clear(int start, int end) { clear(); }
	void home();

	void noDisplay();
	void display();
	void noBlink();
	void blink();
	void noCursor();
	void cursor();
	void scrollDisplayLeft();
	void scrollDisplayRight();
	void leftToRight();
	void rightToLeft();
	void autoscroll();
	void noAutoscroll();

	//void createChar(uint8_t, uint8_t[]);
	void createChar(uint8_t, PGM_P ptr);
	void setCursor(uint8_t, uint8_t); 
	virtual size_t write(uint8_t);
	void command(uint8_t);

	inline uint8_t type() { return _type; }
	void noBacklight();
	void backlight();
	 
	using Print::write;	 
private:
	void send(uint8_t, uint8_t);
	void write4bits(uint8_t);
	void pulseEnable();

	void expanderWrite(uint8_t);
	void pulseEnable(uint8_t);
	uint8_t _addr;
	uint8_t _cols;
	uint8_t _rows;
	uint8_t _charsize;
	uint8_t _backlightval;

	uint8_t _type;		// LCD type. 0: standard; 1: I2C
	uint8_t _rs_pin; // LOW: command.  HIGH: character.
	uint8_t _rw_pin; // LOW: write to LCD.	HIGH: read from LCD.
	uint8_t _enable_pin; // activated by a HIGH pulse.
	uint8_t _data_pins[8];

	uint8_t _displayfunction;
	uint8_t _displaycontrol;
	uint8_t _displaymode;

	uint8_t _initialized;

	uint8_t _numlines,_currline;
};

#endif

#endif // LIQUID_CRYSTAL_DUAL_H
