#include <Wire.h>

#ifndef ESP8266
#include <SdFat.h>
#endif

#include <OpenSprinkler.h>

extern OpenSprinkler os;

void do_setup();
void do_loop();

void setup() {
  do_setup();
}

void loop() {
  do_loop();
}
