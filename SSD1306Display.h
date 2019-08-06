#ifndef SSD1306_DISPLAY_H
#define SSD1306_DISPLAY_H

#if defined(ESP8266)

#include <SSD1306.h>
#include "font.h"
#include "images.h"

#define LCD_STD 0			// Standard LCD
#define LCD_I2C 1

class SSD1306Display : public SSD1306{
public:
	SSD1306Display(uint8_t _addr, uint8_t _sda, uint8_t _scl) : SSD1306(_addr, _sda, _scl) {
		cx = 0;
		cy = 0;
		for(byte i=0;i<NUM_CUSTOM_ICONS;i++) custom_chars[i]=NULL;
	}
	void begin() {
		Wire.setClock(400000L); // lower clock to 400kHz
		flipScreenVertically();
		setFont(Monospaced_plain_13);
		fontWidth = 8;
		fontHeight = 16;
	}
	void clear() {
		SSD1306::clear();
	}
	void clear(int start, int end) {
		setColor(BLACK);
		fillRect(0, (start+1)*fontHeight, 128, (end-start+1)*fontHeight);
		setColor(WHITE);
	}
	
	uint8_t type() { return LCD_I2C; }
	void noBlink() {/*no support*/}
	void blink() {/*no support*/}
	void setCursor(uint8_t col, int8_t row) {
		/* assume 4 lines, the middle two lines
			 are row 0 and 1 */
		cy = (row+1)*fontHeight;
		cx = col*fontWidth;
	}
	void noBacklight() {/*no support*/}
	void backlight() {/*no support*/}
	size_t write(uint8_t c) {
		setColor(BLACK);
		fillRect(cx, cy, fontWidth, fontHeight);
		setColor(WHITE);

		if(c<NUM_CUSTOM_ICONS && custom_chars[c]!=NULL) {
			drawXbm(cx, cy, fontWidth, fontHeight, (const byte*) custom_chars[c]);
		} else {
			drawString(cx, cy, String((char)c));
		}
		cx += fontWidth;
		display();	// todo: not very efficient
		return 1;
	}
	size_t write(const char* s) {
		uint8_t nc = strlen(s);
		setColor(BLACK);
		fillRect(cx, cy, fontWidth*nc, fontHeight);  
		setColor(WHITE);
		drawString(cx, cy, String(s));
		cx += fontWidth*nc;
		display();	// todo: not very efficient
		return nc;
	}
	void createChar(byte idx, PGM_P ptr) {
		if(idx>=0&&idx<NUM_CUSTOM_ICONS) custom_chars[idx]=ptr;
	}
private:
	uint8_t cx, cy;
	uint8_t fontWidth, fontHeight;
	PGM_P custom_chars[NUM_CUSTOM_ICONS];
};

#endif

#endif // SSD1306_DISPLAY_H



