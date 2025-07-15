#include <sensor.h>

Sensor::Sensor(unsigned long interval, double min, double max, double scale, double offset, const char *name, SensorUnit unit) :
interval(interval), min(min), max(max), scale(scale), offset(offset), unit(unit) {
    strncpy(this->name, name, SENSOR_NAME_LEN);
    this->name[SENSOR_NAME_LEN] = 0;
}

void Sensor::poll() {
    if ((millis() - this->_last_update) > this->interval) {
        this->_update_raw_value();
        this->value = (this->value * this->scale) + this->offset;
        if (this->value < this->min) this->value = this->min;
        if (this->value > this->max) this->value = this->max;
        this->_last_update = millis();
    }
}

int write_double(char *buf, double val) {
    std::memcpy(buf, &val, sizeof(val));
    return sizeof(val);
}

int write_ulong(char *buf, ulong val) {
    std::memcpy(buf, &val, sizeof(val));
    return sizeof(val);
}

int Sensor::serialize(char *buf) {
    int i = 0;

    buf[i++] = static_cast<uint8_t>(this->get_sensor_type());
    memcpy(buf+i, this->name, SENSOR_NAME_LEN);
    i += SENSOR_NAME_LEN;
    buf[i++] = static_cast<uint8_t>(this->unit);
    i += write_ulong(buf+i, this->interval);
    i += write_double(buf+i, this->scale);
    i += write_double(buf+i, this->offset);
    i += write_double(buf+i, this->min);
    i += write_double(buf+i, this->max);

    i += this->serialize_internal(buf + i);
    return i;
}

EnsembleSensor::EnsembleSensor(unsigned long interval, double min, double max, double scale, double offset, const char* name, SensorUnit unit, Sensor **sensors, uint64_t sensor_mask, EnsembleAction action) : 
Sensor(interval, min, max, scale, offset, name, unit), 
sensors(sensors),
sensor_mask(sensor_mask), 
action(action) {}

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

int EnsembleSensor::_serialize_internal(char *buf) {
    return 0;
}


WeatherSensor::WeatherSensor(unsigned long interval, double min, double max, double scale, double offset, const char* name, SensorUnit unit, Sensor **sensors, uint64_t sensor_mask, EnsembleAction action) : 
Sensor(interval, min, max, scale, offset, name, unit), 
sensors(sensors),
sensor_mask(sensor_mask), 
action(action) {}

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

int WeatherSensor::_serialize_internal(char *buf) {
    return 0;
}