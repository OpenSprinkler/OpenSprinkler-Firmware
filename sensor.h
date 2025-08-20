#ifndef SENSOR_H
#define SENSOR_H

#include <stdint.h>
#if defined(ARDUINO)
#include <Arduino.h>
#else
#include "utils.h"
#endif
#include "defines.h"
#include "bfiller.h"

#define SENSOR_NAME_LEN 33
#define SENSOR_CUSTOM_UNIT_LEN 9

typedef struct {
    ulong interval;
    uint32_t flags;
    ulong next_update;
    double value;
} sensor_memory_t;

enum class SensorType {
    Ensemble,
    ADS1115,
    Weather,
    MAX_VALUE,
};

enum class SensorUnitGroup {
    None,
    Temperature,
    Length,
    Volume,
    Light,
    Energy,
    Velocity,
    Pressure,
    Flow,
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
    Miliampere,
    Ampere,
    Percent,
    MilesPerHour,
    KilometersPerHour,
    MetersPerSecond,
    DialetricConstant,
    PartsPerMillion,
    Ohm,
    Miliohm,
    Kiloohm,
    Bar,
    Kilopascal,
    Pascal,
    Torr,
    LitersPerSecond,
    GallonsPerSecond,
    MAX_VALUE,
};

typedef enum {
    SENSOR_FLAG_ENABLE = 0,
    SENSOR_FLAG_LOG,
    SENSOR_FLAG_COUNT
} sensor_flags;

class Sensor {
public:
  Sensor(unsigned long interval, double min, double max, double scale, double offset, const char *name, SensorUnit unit, uint32_t flags);
  Sensor();
  virtual ~Sensor() {}
  
  double get_new_value();
  uint32_t serialize(char *buf);

  void virtual emit_extra_json(BufferFiller *bfill) = 0;

  unsigned long interval = 1;
  double min = 0.0;
  double max = 0.0;
  double scale = 0.0;
  double offset = 0.0;
  char name[SENSOR_NAME_LEN] = {0};
  SensorUnit unit = SensorUnit::None;

  uint32_t flags = 0;
  
  SensorType virtual get_sensor_type() = 0;
  double virtual get_inital_value() = 0;

private:
  double virtual _get_raw_value() = 0;
protected:
  uint32_t _deserialize(char *buf);
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

typedef struct {
    uint8_t sensor_id;
    double min;
    double max;
    double scale;
    double offset;
} ensemble_children_t;

#define ENSEMBLE_SENSOR_CHILDREN_COUNT 4

class EnsembleSensor : public Sensor {
    public:
    EnsembleSensor(unsigned long interval, double min, double max, double scale, double offset, const char *name, SensorUnit unit, uint32_t flags, sensor_memory_t *sensors, ensemble_children_t *children, uint8_t children_count, EnsembleAction action);
    EnsembleSensor(sensor_memory_t *sensors, char *buf);

    void emit_extra_json(BufferFiller *bfill);
    static void emit_description_json(BufferFiller *bfill);

    SensorType get_sensor_type() {
        return SensorType::Ensemble;
    }

    ensemble_children_t children[ENSEMBLE_SENSOR_CHILDREN_COUNT];
    EnsembleAction action;

    double get_inital_value();

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
    WeatherSensor(unsigned long interval, double min, double max, double scale, double offset, const char *name, SensorUnit unit, uint32_t flags, WeatherGetter weather_getter, WeatherAction action);
    WeatherSensor(WeatherGetter weather_getter, char *buf);

    void emit_extra_json(BufferFiller *bfill);
    static void emit_description_json(BufferFiller *bfill);

    SensorType get_sensor_type() {
        return SensorType::Weather;
    }

    WeatherAction action;

    double get_inital_value();

    private:
    double _get_raw_value();
    uint32_t _serialize_internal(char *buf);
    
    WeatherGetter weather_getter;
};

typedef struct {
    double x;
    double y;
} sensor_adjustment_point_t;

#define SENSOR_ADJUSTMENT_POINTS 8

typedef enum {
    SENADJ_FLAG_ENABLE = 0,
} senadj_flags;

class SensorAdjustment {
public:
  SensorAdjustment(uint8_t flags, uint8_t sid, uint8_t point_count, sensor_adjustment_point_t *points);
  SensorAdjustment(char *buf);
  
  double get_adjustment_factor(sensor_memory_t *sensors);
  uint32_t serialize(char *buf);

  uint8_t flags;
  uint8_t sid;
  uint8_t point_count;
  sensor_adjustment_point_t points[SENSOR_ADJUSTMENT_POINTS];
};

#define SENSOR_ADJUSTMENT_SIZE (3 + (SENSOR_ADJUSTMENT_POINTS * sizeof(sensor_adjustment_point_t)))

const char *enum_string(SensorUnitGroup group);
const char *enum_string(EnsembleAction action);
const char *enum_string(WeatherAction action);

const char* get_sensor_unit_name(SensorUnit unit);
const char* get_sensor_unit_short(SensorUnit unit);
const SensorUnitGroup get_sensor_unit_group(SensorUnit unit);
const ulong get_sensor_unit_index(SensorUnit unit);

#endif //SENSOR_H