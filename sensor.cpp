#include <sensor.h>
#include "OpenSprinkler.h"

Sensor::Sensor(unsigned long interval, double min, double max, double scale, double offset, const char *name, SensorUnit unit) :
interval(interval), min(min), max(max), scale(scale), offset(offset), unit(unit) {
    strncpy(this->name, name, SENSOR_NAME_LEN);
    this->name[SENSOR_NAME_LEN-1] = 0;
}

Sensor::Sensor() {}

bool Sensor::poll() {
    if ((millis() - this->_last_update) > this->interval) {
        this->_update_raw_value();
        this->value = (this->value * this->scale) + this->offset;
        if (this->value < this->min) this->value = this->min;
        if (this->value > this->max) this->value = this->max;
        this->_last_update = millis();
        return true;
    }
    return false;
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

EnsembleSensor::EnsembleSensor(unsigned long interval, double min, double max, double scale, double offset, const char* name, SensorUnit unit, Sensor **sensors, uint64_t sensor_mask, EnsembleAction action) : 
Sensor(interval, min, max, scale, offset, name, unit), 
sensor_mask(sensor_mask), 
action(action),
sensors(sensors) {}

void EnsembleSensor::_update_raw_value() {
    double inital;
    uint8_t count = 0;
    switch (this->action) {
        case EnsembleAction::Min:
            inital = this->max;
            break;
        case EnsembleAction::Max:
            inital = this->min;
            break;
        case EnsembleAction::Average:
        case EnsembleAction::Sum:
            inital = 0;
            break;
        case EnsembleAction::Product:
            inital = 1;
            break;
        default:
            return;
    }

    uint64_t mask = this->sensor_mask;
    uint8_t i = 0;
    while (mask) {
        if ((mask & 1) && (sensors[i])) {
            double value = sensors[i]->value;

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
                    return;
            }

            count += 1;
        }

        i += 1;
        mask >>= 1;
    }

    if (count == 0) {
        this->value = 0.0;
    } else if (this->action == EnsembleAction::Average) {
        this->value = inital / (double) count;
    } else {
        this->value = inital;
    }
}

uint32_t EnsembleSensor::_serialize_internal(char *buf) {
    uint32_t i = 0;
    i += write_buf<uint64_t>(buf+i, this->sensor_mask);
    buf[i++] = static_cast<uint8_t>(this->action);
    return i;
}

EnsembleSensor::EnsembleSensor(Sensor **sensors, char *buf) {
    uint32_t i = Sensor::_deserialize(buf);
    this->sensor_mask = read_buf<uint64_t>(buf, &i);
    this->action = static_cast<EnsembleAction>(buf[i++]);
    this->sensors = sensors;
}


WeatherSensor::WeatherSensor(unsigned long interval, double min, double max, double scale, double offset, const char* name, SensorUnit unit, Sensor **sensors, uint64_t sensor_mask, EnsembleAction action) : 
Sensor(interval, min, max, scale, offset, name, unit), 
sensor_mask(sensor_mask), 
action(action),
sensors(sensors) {}

void WeatherSensor::_update_raw_value() {
    double inital;
    uint8_t count = 0;
    switch (this->action) {
        case EnsembleAction::Min:
            inital = this->max;
            break;
        case EnsembleAction::Max:
            inital = this->min;
            break;
        case EnsembleAction::Average:
        case EnsembleAction::Sum:
            inital = 0;
            break;
        case EnsembleAction::Product:
            inital = 1;
            break;
        default:
            return;
    }

    uint64_t mask = this->sensor_mask;
    uint8_t i = 0;
    while (mask) {
        if ((mask & 1) && (sensors[i])) {
            double value = sensors[i]->value;

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
                    return;
            }

            count += 1;
        }

        i += 1;
        mask >>= 1;
    }

    if (count == 0) {
        this->value = 0.0;
    } else if (this->action == EnsembleAction::Average) {
        this->value = inital / (double) count;
    } else {
        this->value = inital;
    }
}

uint32_t WeatherSensor::_serialize_internal(char *buf) {
    return 0;
}

WeatherSensor::WeatherSensor(Sensor **sensors, char *buf) {
    uint32_t i = Sensor::_deserialize(buf);
}