#if defined(ESP8266)

#include "defines.h"

#if defined(SONOFF_4CH_PRO_R3)

// Sonoff 4CH Pro R3 GPIO mapping
// Relays are active HIGH: HIGH = ON, LOW = OFF
// Button 1 and Button 4 are swapped in software because Button 1 is wired to GPIO0,
// and holding GPIO0 low during boot puts the ESP8266 into flash mode.
unsigned char PIN_BUTTON_1 = 14; // Physical Button 4 -> GPIO14
unsigned char PIN_BUTTON_2 = 9;  // Physical Button 2 -> GPIO9
unsigned char PIN_BUTTON_3 = 10; // Physical Button 3 -> GPIO10
unsigned char PIN_BUTTON_4 = 0;  // Physical Button 1 -> GPIO0 (avoid on boot)
unsigned char PIN_RELAY_1 = 12;  // GPIO12 (D1)
unsigned char PIN_RELAY_2 = 5;   // GPIO5 (D2)  -- WARNING: also ESP8266 I2C SCL
unsigned char PIN_RELAY_3 = 4;   // GPIO4 (D3)  -- WARNING: also ESP8266 I2C SDA
unsigned char PIN_RELAY_4 = 15;  // GPIO15 (D5)
unsigned char PIN_RELAY_5 = 255; // No external relay connected
unsigned char PIN_LED = 13;      // GPIO13 (D4), blue WiFi LED (inverted: LOW = ON)
unsigned char PIN_RFRX = 255;
unsigned char PIN_RFTX = 255;
unsigned char PIN_BOOST = 255;
unsigned char PIN_BOOST_EN = 255;
unsigned char PIN_LATCH_COM = 255;
unsigned char PIN_LATCH_COMA = 255;
unsigned char PIN_LATCH_COMK = 255;
unsigned char PIN_SENSOR1 = 16;  // GPIO16 (D0), for rain/flow sensor
unsigned char PIN_SENSOR2 = 255;

#else

// Default OS3.x ESP8266 pin initialization
// Hardware auto-detection in begin() will set actual values based on I2C scan
unsigned char PIN_BUTTON_1 = 255;
unsigned char PIN_BUTTON_2 = 255;
unsigned char PIN_BUTTON_3 = 255;
unsigned char PIN_BUTTON_4 = 255;
unsigned char PIN_RELAY_1 = 255;
unsigned char PIN_RELAY_2 = 255;
unsigned char PIN_RELAY_3 = 255;
unsigned char PIN_RELAY_4 = 255;
unsigned char PIN_RELAY_5 = 255;
unsigned char PIN_LED = 255;
unsigned char PIN_RFRX = 255;
unsigned char PIN_RFTX = 255;
unsigned char PIN_BOOST = 255;
unsigned char PIN_BOOST_EN = 255;
unsigned char PIN_LATCH_COM = 255;
unsigned char PIN_LATCH_COMA = 255;
unsigned char PIN_LATCH_COMK = 255;
unsigned char PIN_SENSOR1 = 255;
unsigned char PIN_SENSOR2 = 255;

#endif

unsigned char PIN_IOEXP_INT = 255;

#endif
