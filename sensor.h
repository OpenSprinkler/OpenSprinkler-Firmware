#ifndef SENSOR_H
#define SENSOR_H

#include <stdint.h>
#if defined(ARDUINO)
#include <Arduino.h>
#else
#include "utils.h"
#endif
#include <defines.h>

enum class SensorType {
    Constant,
    Ensemble,
    ADS1115,
    Weather,
};

enum class SensorUnit {
    None,
    Celsius,
    Fahrenheit,
    Kelvin,
    Milimeter,
    Centieter,
    Meter,
    Kilometer,
    Inch,
    Foot,
    Mile,
    Lux,
    Lumen,
    Milivolt,
    Volt,
    Miliamp,
    Amp,
    Percent,
    MilesPerHour,
    KilometersPerHour,
    MetersPerSection,
    DialetricConstant,
    PartsPerMillion,
    Ohm,
    Miliohm,
    Kiloohm,
    Barr, // Pressure
    Kilopascal,
    Pascal,
    LitersPerSecond,
    GallonsPerSecond,
};

class Sensor {
public:
  Sensor(unsigned long interval, float min, float max, float scale, float offset, const char *name, SensorUnit unit);
  void poll();

  unsigned long interval = 1;
  float value = 0.0;
  float min = 0.0;
  float max = 0.0;
  float scale = 0.0;
  float offset = 0.0;
  char name[33] = {0};
  SensorUnit unit = SensorUnit::None;
  
  SensorType virtual get_sensor_type() = 0;
private:
  unsigned long _last_update = 0;
  void virtual _update_raw_value() = 0;
};

enum class EnsembleAction {
    Min,
    Max,
    Average,
    Sum,
    Product,
};


class EnsembleSensor : public Sensor {
    public:
    EnsembleSensor(unsigned long interval, float min, float max, float scale, float offset, const char *name, SensorUnit unit, Sensor **sensors, uint64_t sensor_mask, EnsembleAction action);

    SensorType get_sensor_type() {
        return SensorType::Ensemble;
    }

    private:
    void _update_raw_value();
    
    Sensor **sensors;
    uint64_t sensor_mask;
    EnsembleAction action;
};



#endif //SENSOR_H