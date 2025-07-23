#ifndef SENSOR_H
#define SENSOR_H

#include <stdint.h>
#if defined(ARDUINO)
#include <Arduino.h>
#else
#include "utils.h"
#endif
#include "defines.h"

#define SENSOR_NAME_LEN 33
#define SENSOR_CUSTOM_UNIT_LEN 9

typedef struct {
    ulong interval;
    ulong next_update;
    double value;
} sensor_memory_t;

enum class SensorType {
    Ensemble,
    ADS1115,
    Weather,
    MAX_VALUE,
};

enum class SensorUnit {
    None,
    UserDefined,
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
  
  double get_new_value();
  uint32_t serialize(char *buf);

  unsigned long interval = 1;
  double min = 0.0;
  double max = 0.0;
  double scale = 0.0;
  double offset = 0.0;
  char name[SENSOR_NAME_LEN] = {0};
  SensorUnit unit = SensorUnit::None;
  
  SensorType virtual get_sensor_type() = 0;
private:
  double virtual _get_raw_value() = 0;
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

typedef Sensor* (*SensorGetter)(uint8_t);

class EnsembleSensor : public Sensor {
    public:
    EnsembleSensor(unsigned long interval, double min, double max, double scale, double offset, const char *name, SensorUnit unit, sensor_memory_t *sensors, uint64_t sensor_mask, EnsembleAction action);
    EnsembleSensor(sensor_memory_t *sensors, char *buf);

    SensorType get_sensor_type() {
        return SensorType::Ensemble;
    }

    uint64_t sensor_mask;
    EnsembleAction action;

    private:
    double _get_raw_value();
    uint32_t _serialize_internal(char *buf);
    
    sensor_memory_t *sensors;
};

enum class WeatherAction {
    MAX_VALUE,
};

typedef double (*WeatherGetter)(WeatherAction);

class WeatherSensor : public Sensor {
    public:
    WeatherSensor(unsigned long interval, double min, double max, double scale, double offset, const char *name, SensorUnit unit, WeatherGetter weather_getter, WeatherAction action);
    WeatherSensor(WeatherGetter weather_getter, char *buf);


    SensorType get_sensor_type() {
        return SensorType::Weather;
    }

    WeatherAction action;

    private:
    double _get_raw_value();
    uint32_t _serialize_internal(char *buf);
    
    WeatherGetter weather_getter;
};

#endif //SENSOR_H