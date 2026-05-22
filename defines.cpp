#if defined(ESP8266)

#include "defines.h"

unsigned char PIN_BUTTON_1 = 14; // this is GPIO from Button 4, swaped because ESP8266 goes in firmware flash mode when HOLD on power ON.
unsigned char PIN_BUTTON_2 = 9;
unsigned char PIN_BUTTON_3 = 10;
unsigned char PIN_BUTTON_4 = 0; // this is GPIO from Button 1;
unsigned char PIN_RELAY_1 = 12; //D1;
unsigned char PIN_RELAY_2 = 5; //D2;
unsigned char PIN_RELAY_3 = 4; //D3;
unsigned char PIN_RELAY_4 = 15; //D5;
unsigned char PIN_RELAY_5 = 255; //D6;
unsigned char PIN_LED = 13; //D4;
unsigned char PIN_RFRX = 255;
unsigned char PIN_RFTX = 255;
unsigned char PIN_BOOST = 255;
unsigned char PIN_BOOST_EN = 255;
unsigned char PIN_LATCH_COM = 255;
unsigned char PIN_LATCH_COMA = 255;
unsigned char PIN_LATCH_COMK = 255;
// unsigned char PIN_SENSOR1 = 255; //byte PIN_FLOWSENSOR = 16; //D0
unsigned char PIN_SENSOR1 = 16; // D0; //byte PIN_FLOWSENSOR = 16; //D0
unsigned char PIN_SENSOR2 = 255;
unsigned char PIN_IOEXP_INT = 255;

#endif
