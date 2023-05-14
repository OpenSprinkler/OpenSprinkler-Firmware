#ifndef SH1106_DISPLAY_H
#define SH1106_DISPLAY_H

#if defined(ESP8266) || defined(ESP32)

#include <SH1106.h>
#include "font.h"
#include "images.h"

#define LCD_STD 0  // Standard LCD
#define LCD_I2C 1

class SH1106Display : public SH1106{
public:
	SH1106Display(uint8_t _addr, uint8_t _sda, uint8_t _scl) : SH1106(_addr, _sda, _scl) {
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
		SH1106::clear();
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
		if(auto_display) display();  // todo: not very efficient
		return 1;
	}
	size_t write(const char* s) {
		uint8_t nc = strlen(s);
		setColor(BLACK);
		fillRect(cx, cy, fontWidth*nc, fontHeight);
		setColor(WHITE);
		drawString(cx, cy, String(s));
		cx += fontWidth*nc;
		if(auto_display) display();	// todo: not very efficient
		return nc;
	}
	void createChar(byte idx, PGM_P ptr) {
		if(idx>=0&&idx<NUM_CUSTOM_ICONS) custom_chars[idx]=ptr;
	}
	void setAutoDisplay(bool v) {auto_display=v;}
private:
	bool auto_display = true;
	uint8_t cx, cy;
	uint8_t fontWidth, fontHeight;
	PGM_P custom_chars[NUM_CUSTOM_ICONS];
};

#endif

#endif // SH1106_DISPLAY_H



