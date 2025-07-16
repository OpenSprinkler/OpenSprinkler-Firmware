#ifndef SENSOR_H
#define SENSOR_H

#include <stdint.h>
#if defined(ARDUINO)
#include <Arduino.h>
#else
#include "utils.h"
#endif
#include <defines.h>

#define SENSOR_NAME_LEN 33

enum class SensorType {
    Ensemble,
    ADS1115,
    Weather,
    MAX_VALUE,
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
    MAX_VALUE,
};

class Sensor {
public:
  Sensor(unsigned long interval, double min, double max, double scale, double offset, const char *name, SensorUnit unit);
  Sensor();
  virtual ~Sensor() {}
  
  bool poll();
  uint32_t serialize(char *buf);

  unsigned long interval = 1;
  double value = 0.0;
  double min = 0.0;
  double max = 0.0;
  double scale = 0.0;
  double offset = 0.0;
  char name[SENSOR_NAME_LEN] = {0};
  SensorUnit unit = SensorUnit::None;
  
  SensorType virtual get_sensor_type() = 0;
private:
  unsigned long _last_update = 0;
  void virtual _update_raw_value() = 0;
protected:
  uint32_t _deserialize(char *buf);
  template <typename T>
  uint32_t write_buf(char *buf, T val);
  template <typename T>
  T read_buf(char *buf, uint32_t *i);

  uint32_t virtual _serialize_internal(char *buf) = 0;
};

enum class EnsembleAction {
    Min,
    Max,
    Average,
    Sum,
    Product,
    MAX_VALUE,
};


class EnsembleSensor : public Sensor {
    public:
    EnsembleSensor(unsigned long interval, double min, double max, double scale, double offset, const char *name, SensorUnit unit, Sensor **sensors, uint64_t sensor_mask, EnsembleAction action);
    EnsembleSensor(Sensor **sensors, char *buf);

    SensorType get_sensor_type() {
        return SensorType::Ensemble;
    }

    uint64_t sensor_mask;
    EnsembleAction action;

    private:
    void _update_raw_value();
    uint32_t _serialize_internal(char *buf);
    
    Sensor **sensors;
};

class WeatherSensor : public Sensor {
    public:
    WeatherSensor(unsigned long interval, double min, double max, double scale, double offset, const char *name, SensorUnit unit, Sensor **sensors, uint64_t sensor_mask, EnsembleAction action);
    WeatherSensor(Sensor **sensors, char *buf);


    SensorType get_sensor_type() {
        return SensorType::Weather;
    }

    uint64_t sensor_mask;
    EnsembleAction action;

    private:
    void _update_raw_value();
    uint32_t _serialize_internal(char *buf);
    
    Sensor **sensors;
};

#endif //SENSOR_H