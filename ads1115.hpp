#pragma once

#include "analog.h"
#include <stdint.h>

class ADS1115 : AnalogSensor {
public:
  void poll();
  uint8_t pin_count() {
    return 4;
  }
  uint16_t get_pin_value(uint8_t pin);
  float get_scale_constant(uint8_t pin) {
    return -32768.0;
  }
  float get_scale_factor(uint8_t pin) {
    return 6144.0 / 32768.0;
  }
};