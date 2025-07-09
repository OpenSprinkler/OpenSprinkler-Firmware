#pragma once

#include "analog.h"
#include <stdint.h>

#define ADS1115_SCALE_FACTOR (6144.0 / 32768.0)

#if defined(ARDUINO)
#include <Arduino.h>
#include <Wire.h>
#else
#include "i2cd.h"
#endif

class ADS1115 : AnalogSensor {
public:
#if defined(ARDUINO)
  ADS1115(uint8_t address, TwoWire& wire);
#endif
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

  int16_t get_value() {
      return (int16_t) this->_read_register(0x00);
  }

  void request_pin(uint8_t pin);

  bool is_busy() {
    return (this->_read_register(0x01) & 0x8000) == 0;
  }

  private:
  uint8_t _address;
#if defined(ARDUINO)
  TwoWire *_wire;
#else
  I2CDevice i2c;
#endif

  void _write_register(uint8_t reg, uint16_t value);
  uint16_t _read_register(uint8_t reg);
};