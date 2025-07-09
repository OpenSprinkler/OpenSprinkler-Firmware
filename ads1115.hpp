#pragma once

#include "analog.h"
#include <stdint.h>

#define ADS1115_SCALE_FACTOR (6144.0 / 32768.0)

#if defined(ARDUINO)
namespace ADS1X15 {
#include <ADS1X15.h>
}
#else
#include "i2cd.h"
#endif

class ADS1115 : AnalogSensor {
public:
  ADS1115(uint8_t address);
  int16_t get_pin_value(uint8_t pin);
  bool begin();

  uint8_t pin_count() {
    return 4;
  }

  float get_scale_constant(uint8_t pin) {
    return 0;
  }

  float get_scale_factor(uint8_t pin) {
    return ADS1115_SCALE_FACTOR;
  }

  private:
#if defined(ARDUINO)
  ADS1X15::ADS1115 adc;
#else
  uint8_t _addr;
  I2CDevice i2c;
  void _write_register(uint8_t reg, uint16_t value);
  uint16_t _read_register(uint8_t reg);
#endif
};