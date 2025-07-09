#ifndef ANALOG_H
#define ANALOG_H

#include <stdint.h>

class AnalogSensor {
public:
  virtual ~AnalogSensor() = default;
  virtual uint8_t pin_count() = 0;
  virtual int16_t get_pin_value(uint8_t pin) = 0;
  virtual float get_scale_constant(uint8_t pin) = 0;
  virtual float get_scale_factor(uint8_t pin) = 0;
};

#endif // ANALOG_H