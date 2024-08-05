#ifndef SSD1306_DISPLAY_H
#define SSD1306_DISPLAY_H

#if defined(ESP8266)

#include <SSD1306.h>

#include "font.h"
#include "images.h"

#define LCD_STD 0 // Standard LCD
#define LCD_I2C 1

class SSD1306Display : public SSD1306 {
public:
  SSD1306Display(uint8_t _addr, uint8_t _sda, uint8_t _scl)
      : SSD1306(_addr, _sda, _scl) {
    cx = 0;
    cy = 0;
    for (unsigned char i = 0; i < NUM_CUSTOM_ICONS; i++)
      custom_chars[i] = NULL;
  }
  void begin() {
    Wire.setClock(400000L); // lower clock to 400kHz
    flipScreenVertically();
    setFont(Monospaced_plain_13);
    fontWidth = 8;
    fontHeight = 16;
  }
  void clear() { SSD1306::clear(); }
  void clear(int start, int end) {
    setColor(BLACK);
    fillRect(0, (start + 1) * fontHeight, 128, (end - start + 1) * fontHeight);
    setColor(WHITE);
  }

  uint8_t type() { return LCD_I2C; }
  void noBlink() { /*no support*/ }
  void blink() { /*no support*/ }
  void setCursor(uint8_t col, int8_t row) {
    /* assume 4 lines, the middle two lines
                     are row 0 and 1 */
    cy = (row + 1) * fontHeight;
    cx = col * fontWidth;
  }
  void noBacklight() { /*no support*/ }
  void backlight() { /*no support*/ }
  size_t write(uint8_t c) {
    setColor(BLACK);
    fillRect(cx, cy, fontWidth, fontHeight);
    setColor(WHITE);

    if (c < NUM_CUSTOM_ICONS && custom_chars[c] != NULL) {
      drawXbm(cx, cy, fontWidth, fontHeight,
              (const unsigned char *)custom_chars[c]);
    } else {
      drawString(cx, cy, String((char)c));
    }
    cx += fontWidth;
    if (auto_display)
      display(); // todo: not very efficient
    return 1;
  }
  size_t write(const char *s) {
    uint8_t nc = strlen(s);
    setColor(BLACK);
    fillRect(cx, cy, fontWidth * nc, fontHeight);
    setColor(WHITE);
    drawString(cx, cy, String(s));
    cx += fontWidth * nc;
    if (auto_display)
      display(); // todo: not very efficient
    return nc;
  }
  void createChar(unsigned char idx, PGM_P ptr) {
    if (idx >= 0 && idx < NUM_CUSTOM_ICONS)
      custom_chars[idx] = ptr;
  }

  void createChar(unsigned char idx, const unsigned char *ptr) {
    createChar(idx, (const char *)ptr);
  }

  void setAutoDisplay(bool v) { auto_display = v; }

private:
  bool auto_display = true;
  uint8_t cx, cy;
  uint8_t fontWidth, fontHeight;
  PGM_P custom_chars[NUM_CUSTOM_ICONS];
};

#else
#include <stdint.h>
#include <string.h>

#include <cstdio>

#include "i2cd.h"

#include "font.h"
#include "images.h"
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#define LCD_STD 0 // Standard LCD
#define LCD_I2C 1

#define BLACK 0
#define WHITE 1

// Header Values
#define JUMPTABLE_BYTES 4

#define JUMPTABLE_LSB 1
#define JUMPTABLE_SIZE 2
#define JUMPTABLE_WIDTH 3
#define JUMPTABLE_START 4

#define WIDTH_POS 0
#define HEIGHT_POS 1
#define FIRST_CHAR_POS 2
#define CHAR_NUM_POS 3

// SSD1306
// Codes
#define SSD1306_COMMAND_ADDRESS 0x00
#define SSD1306_DATA_CONTINUE_ADDRESS 0x40

// Fundamental Commands
#define SSD1306_SET_CONTRAST_CONTROL 0x81
#define SSD1306_DISPLAY_ALL_ON_RESUME 0xA4
#define SSD1306_DISPLAY_ALL_ON 0xA5
#define SSD1306_NORMAL_DISPLAY 0xA6
#define SSD1306_INVERT_DISPLAY 0xA7
#define SSD1306_DISPLAY_OFF 0xAE
#define SSD1306_DISPLAY_ON 0xAF
#define SSD1306_NOP 0xE3

// Scrolling Commands
#define SSD1306_RIGHT_HORIZONTAL_SCROLL 0x26
#define SSD1306_LEFT_HORIZONTAL_SCROLL 0x27
#define SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL 0x29
#define SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL 0x2A
#define SSD1306_DEACTIVATE_SCROLL 0x2E
#define SSD1306_ACTIVATE_SCROLL 0x2F
#define SSD1306_SET_VERTICAL_SCROLL_AREA 0xA3

// Addressing Setting Commands
#define SSD1306_SET_LOWER_COLUMN 0x00
#define SSD1306_SET_HIGHER_COLUMN 0x10
#define SSD1306_MEMORY_ADDR_MODE 0x20
#define SSD1306_SET_COLUMN_ADDR 0x21
#define SSD1306_SET_PAGE_ADDR 0x22

// Hardware Configuration Commands
#define SSD1306_SET_START_LINE 0x40
#define SSD1306_SET_SEGMENT_REMAP 0xA0
#define SSD1306_SET_MULTIPLEX_RATIO 0xA8
#define SSD1306_COM_SCAN_DIR_INC 0xC0
#define SSD1306_COM_SCAN_DIR_DEC 0xC8
#define SSD1306_SET_DISPLAY_OFFSET 0xD3
#define SSD1306_SET_COM_PINS 0xDA
#define SSD1306_CHARGE_PUMP 0x8D

// Timing & Driving Scheme Setting Commands
#define SSD1306_SET_DISPLAY_CLOCK_DIV_RATIO 0xD5
#define SSD1306_SET_PRECHARGE_PERIOD 0xD9
#define SSD1306_SET_VCOM_DESELECT 0xDB

class SSD1306Display {
public:
  SSD1306Display(uint8_t addr, uint8_t _sda, uint8_t _scl) {
    cx = 0;
    cy = 0;
    _addr = addr;
    for (uint8_t i = 0; i < NUM_CUSTOM_ICONS; i++)
      custom_chars[i] = 0;

    clear_buffer();

    height = 64;
    width = 128;

    i2c = I2CDevice();
  }

  ~SSD1306Display() {
    displayOff();
    close(file);
  }

  void init() {} // Dummy function to match ESP8266

  int begin() {
    i2c.begin(_addr);

    setFont(Monospaced_plain_13);
    fontWidth = 8;
    fontHeight = 16;

    i2c.begin_transaction(SSD1306_COMMAND_ADDRESS);
    ssd1306_command(SSD1306_DISPLAY_OFF);
    ssd1306_command(SSD1306_SET_DISPLAY_CLOCK_DIV_RATIO);
    ssd1306_command(0x80);
    ssd1306_command(SSD1306_SET_MULTIPLEX_RATIO);
    ssd1306_command(height - 1);
    ssd1306_command(SSD1306_SET_DISPLAY_OFFSET);
    ssd1306_command(0x00);
    ssd1306_command(SSD1306_SET_START_LINE);
    ssd1306_command(SSD1306_CHARGE_PUMP);
    ssd1306_command(0x14);
    ssd1306_command(SSD1306_MEMORY_ADDR_MODE);
    ssd1306_command(0x00);
    ssd1306_command(SSD1306_SET_SEGMENT_REMAP | 0x01);
    ssd1306_command(SSD1306_COM_SCAN_DIR_DEC);

    switch (height) {
    case 64:
      ssd1306_command(SSD1306_SET_COM_PINS);
      ssd1306_command(0x12);
      ssd1306_command(SSD1306_SET_CONTRAST_CONTROL);
      ssd1306_command(0xCF);
      break;
    case 32:
      ssd1306_command(SSD1306_SET_COM_PINS);
      ssd1306_command(0x02);
      ssd1306_command(SSD1306_SET_CONTRAST_CONTROL);
      ssd1306_command(0x8F);
      break;
    case 16: // NOTE: not tested, lacking part.
      ssd1306_command(SSD1306_SET_COM_PINS);
      ssd1306_command(0x2);
      ssd1306_command(SSD1306_SET_CONTRAST_CONTROL);
      ssd1306_command(0xAF);
      break;
    }

    ssd1306_command(SSD1306_SET_PRECHARGE_PERIOD);
    ssd1306_command(0xF1);

    ssd1306_command(SSD1306_SET_VCOM_DESELECT);
    ssd1306_command(0x40);

    ssd1306_command(SSD1306_DISPLAY_ALL_ON_RESUME);
    ssd1306_command(SSD1306_NORMAL_DISPLAY);
    ssd1306_command(SSD1306_DEACTIVATE_SCROLL);
    ssd1306_command(SSD1306_DISPLAY_ON);

    i2c.end_transaction();

    return 0;
  }

  void setFont(const uint8_t *f) { font = (uint8_t *)f; }

  void display() {
    i2c.begin_transaction(SSD1306_COMMAND_ADDRESS);
    ssd1306_command(SSD1306_SET_PAGE_ADDR);
    ssd1306_command(0x00); // Page start address (0 = reset)
    switch (height) {
    case 64:
      ssd1306_command(7);
      break;
    case 32:
      ssd1306_command(3);
      break;
    case 16:
      ssd1306_command(1);
      break;
    }

    ssd1306_command(SSD1306_SET_COLUMN_ADDR);
    ssd1306_command(0x00); // Column start address (0 = reset)
	ssd1306_command(width - 1); // Column end address (127 = reset)

    i2c.end_transaction();

    i2c.begin_transaction(SSD1306_DATA_CONTINUE_ADDRESS);

    int b;
    for (b = 0; b < 1024; b++) {
      ssd1306_data(frame[b]);
    }

    i2c.end_transaction();
  }

  void clear() {
    clear_buffer();
    display();
  }

  void setBrightness(uint8_t brightness) {
    ssd1306_command(SSD1306_SET_CONTRAST_CONTROL);
    ssd1306_command(brightness);
  }

  void displayOn() { ssd1306_command(SSD1306_DISPLAY_ON); }

  void displayOff() { ssd1306_command(SSD1306_DISPLAY_OFF); }

  void setColor(uint8_t color) { this->color = color; }

  void drawPixel(uint8_t x, uint8_t y) {
    if (x >= 128 || y >= 64)
      return;

    if (color == WHITE) {
      frame[x + (y / 8) * 128] |= 1 << (y % 8);
    } else {
      frame[x + (y / 8) * 128] &= ~(1 << (y % 8));
    }
  }

  void fillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    for (int _x = x; _x < x + w; _x++) {
      for (int _y = y; _y < y + h; _y++) {
        drawPixel(_x, _y);
      }
    }
  }

  void clear(int start, int end) {
    setColor(BLACK);
    fillRect(0, (start + 1) * fontHeight, 128, (end - start + 1) * fontHeight);
    setColor(WHITE);
  }

  void print(const char *s) { write(s); }

  void print(char s) { write(s); }

  void print(int i) {
    char buf[100];
    snprintf(buf, 100, "%d", i);
    print((const char *)buf);
  }

  void print(unsigned int i) {
    char buf[100];
    snprintf(buf, 100, "%u", i);
    print((const char *)buf);
  }
  void print(float f) {
    char buf[100];
    snprintf(buf, 100, "%f", f);
    print((const char *)buf);
  }

#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2

  void print(int i, int base) {
    char buf[100];
    switch (base) {
    case DEC:
      snprintf(buf, 100, "%d", i);
      break;
    case HEX:
      snprintf(buf, 100, "%x", i);
      break;
    case OCT:
      snprintf(buf, 100, "%o", i);
      break;
    case BIN:
      snprintf(buf, 100, "%b", i);
      break;
    default:
      snprintf(buf, 100, "%d", i);
      break;
    }
    print((const char *)buf);
  }

  uint8_t type() { return LCD_I2C; }
  void noBlink() { /*no support*/ }
  void blink() { /*no support*/ }
  void setCursor(uint8_t col, int8_t row) {
    /* assume 4 lines, the middle two lines
                    are row 0 and 1 */
    cy = (row + 1) * fontHeight;
    cx = col * fontWidth;
  }
  void noBacklight() { /*no support*/ }
  void backlight() { /*no support*/ }
  void drawXbm(int x, int y, int w, int h, const char *xbm) {
    int xbmWidth = (w + 7) / 8;
    uint8_t data = 0;

    for (int i = 0; i < h; i++) {
      for (int j = 0; j < w; j++) {
        if (j & 7) {
          data >>= 1;
        } else {
          data = xbm[(i * xbmWidth) + (j / 8)];
        }

        if (data & 0x01) {
          drawPixel(x + j, y + i);
        }
      }
    }
  }

  void drawXbm(int x, int y, int w, int h, const uint8_t *data) {
    drawXbm(x, y, w, h, (const char *)data);
  }

  void fillCircle(int x0, int y0, int r) {
    for (int y = -r; y <= r; y++) {
      for (int x = -r; x <= r; x++) {
        if (x * x + y * y <= r * r) {
          drawPixel(x0 + x, y0 + y);
        }
      }
    }
  }

  void drawChar(int x, int y, char c) {
    uint8_t textHeight = font[HEIGHT_POS];
    uint8_t firstChar = font[FIRST_CHAR_POS];
    uint8_t numChars = font[CHAR_NUM_POS];
    uint16_t sizeOfJumpTable = numChars * JUMPTABLE_BYTES;

    if (c < firstChar || c >= firstChar + numChars)
      return;

    // 4 Bytes per char code
    uint8_t charCode = c - firstChar;

    uint8_t msbJumpToChar =
        font[JUMPTABLE_START + charCode * JUMPTABLE_BYTES]; // MSB \ JumpAddress
    uint8_t lsbJumpToChar = font[JUMPTABLE_START + charCode * JUMPTABLE_BYTES +
                                 JUMPTABLE_LSB]; // LSB /
    uint8_t charByteSize = font[JUMPTABLE_START + charCode * JUMPTABLE_BYTES +
                                JUMPTABLE_SIZE]; // Size
    uint8_t currentCharWidth =
        font[JUMPTABLE_START + (c - firstChar) * JUMPTABLE_BYTES +
             JUMPTABLE_WIDTH]; // Width

    // Test if the char is drawable
    if (!(msbJumpToChar == 255 && lsbJumpToChar == 255)) {
      // Get the position of the char data
      uint16_t charDataPosition = JUMPTABLE_START + sizeOfJumpTable +
                                  ((msbJumpToChar << 8) + lsbJumpToChar);
      int _y = y;
      int _x = x;

      setColor(WHITE);

      for (int b = 0; b < charByteSize; b++) {
        for (int i = 0; i < 8; i++) {
          if (font[charDataPosition + b] & (1 << i)) {
            drawPixel(_x, _y);
          }

          _y++;
          if (_y >= y + textHeight) {
            _y = y;
            _x++;
            break;
          }
        }
      }
    }
  }

  void drawString(int x, int y, const char *text) {
    int _x = x;
    int _y = y;

    while (*text) {
      if (*text == '\n') {
        _x = x;
        _y += fontHeight;
      } else {
        drawChar(_x, _y, *text);
        _x += fontWidth;
      }

      text++;
    }
  }

  size_t write(uint8_t c) {
    setColor(BLACK);
    fillRect(cx, cy, fontWidth, fontHeight);
    setColor(WHITE);
    char cc[2] = {(char)c, 0};

    if (c < NUM_CUSTOM_ICONS && custom_chars[c] != 0) {
      drawXbm(cx, cy, fontWidth, fontHeight, (const char *)custom_chars[c]);
    } else {
      drawString(cx, cy, cc);
    }
    cx += fontWidth;
    if (auto_display)
      display(); // todo: not very efficient
    return 1;
  }

  uint8_t write(const char *s) {
    uint8_t nc = strlen(s);
    bool temp_auto_display = auto_display;
    auto_display = false;
    setColor(BLACK);
    fillRect(cx, cy, fontWidth * nc, fontHeight);
    setColor(WHITE);
    drawString(cx, cy, s);
    auto_display = temp_auto_display;
    cx += fontWidth * nc;
    if (auto_display)
      display(); // todo: not very efficient
    return nc;
  }

  void createChar(uint8_t idx, const char *ptr) {
    if (idx >= 0 && idx < NUM_CUSTOM_ICONS)
      custom_chars[idx] = ptr;
  }

  void createChar(unsigned char idx, const unsigned char *ptr) {
    createChar(idx, (const char *)ptr);
  }

  void setAutoDisplay(bool v) { auto_display = v; }

private:
  int file = -1;
  bool auto_display = false;
  uint8_t cx, cy = 0;
  uint8_t fontWidth, fontHeight;
  const char *custom_chars[NUM_CUSTOM_ICONS];
  uint8_t frame[1024];
  int i2cd;
  bool color;
  uint8_t *font;

  I2CDevice i2c;
  unsigned char _addr;

  unsigned char height;
  unsigned char width;

  void clear_buffer() { memset(frame, 0x00, sizeof(frame)); }

  int ssd1306_command(unsigned char command) { return i2c.send(0x00, command); }

  int ssd1306_data(unsigned char value) { return i2c.send(0x40, value); }
};
#endif

#endif // SSD1306_DISPLAY_H
