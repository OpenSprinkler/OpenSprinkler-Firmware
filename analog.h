#ifndef ANALOG_H
#define ANALOG_H

#include <stdint.h>

class AnalogSensor {
public:
  virtual uint8_t pin_count();
  virtual int16_t get_pin_value(uint8_t pin);
  virtual float get_scale_constant(uint8_t pin);
  virtual float get_scale_factor(uint8_t pin);
};

#endif // ANALOG_H