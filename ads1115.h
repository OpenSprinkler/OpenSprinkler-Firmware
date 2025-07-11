#pragma once

#include <stdint.h>
#include <sensor.h>

#define ADS1115_SCALE_FACTOR (6144.0 / 32768.0)

#if defined(ARDUINO)
#include <Arduino.h>
#include <Wire.h>
#else
#include "i2cd.h"
#endif

class ADS1115 {
public:
#if defined(ARDUINO)
  ADS1115(uint8_t address, TwoWire& wire);
#else
  ADS1115(uint8_t address, I2CBus& bus);
#endif
ADS1115(uint8_t address);
  int16_t get_pin_value(uint8_t pin);
  bool begin();

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
  I2CDevice _i2c;
  uint16_t swap_reg(uint16_t val) {
    return (val << 8) | (val >> 8);
  }
#endif

  void _write_register(uint8_t reg, uint16_t value);
  uint16_t _read_register(uint8_t reg);
};


class ADS1115Sensor : public Sensor {
    public:
    ADS1115Sensor(unsigned long interval, float min, float max, float scale, float offset, const char *name, SensorUnit unit, ADS1115 **sensors, uint8_t sensor_index, uint8_t pin);

    SensorType get_sensor_type() {
        return SensorType::ADS1115;
    }

    private:
    void _update_raw_value();
    
    ADS1115 **sensors;
    uint8_t sensor_index;
    uint8_t pin;
};