#include "sensor.h"
#include "OpenSprinkler.h"

Sensor::Sensor(unsigned long interval, double min, double max, double scale, double offset, const char *name, SensorUnit unit) :
interval(interval), min(min), max(max), scale(scale), offset(offset), unit(unit) {
    strncpy(this->name, name, SENSOR_NAME_LEN);
    this->name[SENSOR_NAME_LEN-1] = 0;
}

Sensor::Sensor() {}

double Sensor::get_new_value() {
    double value = this->_get_raw_value();
    value = (value * this->scale) + this->offset;
    if (value < this->min) value = this->min;
    if (value > this->max) value = this->max;

    return value;
}

template <typename T>
uint32_t Sensor::write_buf(char *buf, T val) {
    std::memcpy(buf, &val, sizeof(val));
    return sizeof(val);
}

template <typename T>
T Sensor::read_buf(char *buf, uint32_t *i) {
    T val;
    std::memcpy(&val, buf + (*i), sizeof(T));
    *i += sizeof(T);
    return val;
}


uint32_t Sensor::serialize(char *buf) {
    uint32_t i = 0;

    buf[i++] = static_cast<uint8_t>(this->get_sensor_type());
    memcpy(buf+i, this->name, SENSOR_NAME_LEN);
    i += SENSOR_NAME_LEN;
    buf[i++] = static_cast<uint8_t>(this->unit);
    i += write_buf<ulong>(buf+i, this->interval);
    i += write_buf<double>(buf+i, this->scale);
    i += write_buf<double>(buf+i, this->offset);
    i += write_buf<double>(buf+i, this->min);
    i += write_buf<double>(buf+i, this->max);

    i += this->_serialize_internal(buf + i);
    return i;
}

uint32_t Sensor::_deserialize(char *buf) {
    uint32_t i = 1; // Skip sensor type

    memcpy(this->name, buf+i, SENSOR_NAME_LEN);
    i += SENSOR_NAME_LEN;
    this->unit = static_cast<SensorUnit>(buf[i++]);
    this->interval = read_buf<ulong>(buf, &i);
    this->scale = read_buf<double>(buf, &i);
    this->offset = read_buf<double>(buf, &i);
    this->min = read_buf<double>(buf, &i);
    this->max = read_buf<double>(buf, &i);

    return i;
}

EnsembleSensor::EnsembleSensor(unsigned long interval, double min, double max, double scale, double offset, const char* name, SensorUnit unit, sensor_memory_t *sensors, ensemble_children_t *children, uint8_t children_count, EnsembleAction action) : 
Sensor(interval, min, max, scale, offset, name, unit), 
action(action),
sensors(sensors) {
    for (size_t i = 0; i < ENSEMBLE_SENSOR_CHILDREN_COUNT; i++) {
        if (i < children_count) {
            this->children[i] = children[i];
        } else {
            this->children[i].sensor_id = 255;
        }
    }
}

double EnsembleSensor::get_inital_value() {
    switch (this->action) {
        case EnsembleAction::Min:
            return this->max;
            break;
        case EnsembleAction::Max:
            return this->min;
            break;
        case EnsembleAction::Average:
        case EnsembleAction::Sum:
            return 0;
            break;
        case EnsembleAction::Product:
            return 1;
            break;
        default:
            // Unreachable
            return 0.0;
    }
}

double EnsembleSensor::_get_raw_value() {
    double inital = this->get_inital_value();
    uint8_t count = 0;

    for (size_t i = 0; i < ENSEMBLE_SENSOR_CHILDREN_COUNT; i++) {
        uint8_t sensor = this->children[i].sensor_id;
        if (sensor < MAX_SENSORS && sensors[sensor].interval) {
            double value = sensors[sensor].value;
            value = (value * this->children[i].scale) + this->children[i].offset;
            if (value < this->children[i].min) value = this->children[i].min;
            if (value > this->children[i].max) value = this->children[i].max;

            switch (this->action) {
                case EnsembleAction::Min:
                    if (value < inital) inital = value;
                    break;
                case EnsembleAction::Max:
                    if (value > inital) inital = value;
                    break;
                case EnsembleAction::Average:
                case EnsembleAction::Sum:
                    inital += value;
                    break;
                case EnsembleAction::Product:
                    inital *= value;
                    break;
                default:
                    // Unreachable
                    return 0.0;
            }

            count += 1;
        }
    }

    if (count == 0) {
        return 0.0;
    } else if (this->action == EnsembleAction::Average) {
        return inital / (double) count;
    } else {
        return inital;
    }
}

uint32_t EnsembleSensor::_serialize_internal(char *buf) {
    uint32_t i = 0;
    for (size_t j = 0; j < ENSEMBLE_SENSOR_CHILDREN_COUNT; j++) {
        i += write_buf<uint8_t>(buf+i, this->children[j].sensor_id);
        i += write_buf<double>(buf+i, this->children[j].min);
        i += write_buf<double>(buf+i, this->children[j].max);
        i += write_buf<double>(buf+i, this->children[j].scale);
        i += write_buf<double>(buf+i, this->children[j].offset);
    }
    
    buf[i++] = static_cast<uint8_t>(this->action);
    return i;
}

EnsembleSensor::EnsembleSensor(sensor_memory_t *sensors, char *buf) {
    uint32_t i = Sensor::_deserialize(buf);
    for (size_t j = 0; j < ENSEMBLE_SENSOR_CHILDREN_COUNT; j++) {
        this->children[j].sensor_id = read_buf<uint8_t>(buf, &i);
        this->children[j].min = read_buf<double>(buf, &i);
        this->children[j].max = read_buf<double>(buf, &i);
        this->children[j].scale = read_buf<double>(buf, &i);
        this->children[j].offset = read_buf<double>(buf, &i);
    }
    this->action = static_cast<EnsembleAction>(buf[i++]);
    this->sensors = sensors;
}


WeatherSensor::WeatherSensor(unsigned long interval, double min, double max, double scale, double offset, const char* name, SensorUnit unit, WeatherGetter weather_getter, WeatherAction action) : 
Sensor(interval, min, max, scale, offset, name, unit), 
action(action),
weather_getter(weather_getter) {}

double WeatherSensor::get_inital_value() {
    return 0.0;
}

double WeatherSensor::_get_raw_value() {
    return this->weather_getter(this->action);
}

uint32_t WeatherSensor::_serialize_internal(char *buf) {
    uint32_t i = 0;
    buf[i++] = static_cast<uint8_t>(this->action);
    return i;
}

WeatherSensor::WeatherSensor(WeatherGetter weather_getter, char *buf) {
    uint32_t i = Sensor::_deserialize(buf);
    this->action = static_cast<WeatherAction>(buf[i++]);
}