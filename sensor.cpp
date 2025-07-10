#include <sensor.h>

Sensor::Sensor(unsigned long interval, float min, float max, float scale, float offset, char *name, SensorUnit unit) :
interval(interval), min(min), max(max), scale(scale), offset(offset), unit(unit) {
    strncpy(this->name, name, 32);
}

void Sensor::poll() {
    if ((this->_last_update - millis()) > this->interval) {
        this->_update_raw_value();
        if (this->value < this->min) this->value = this->min;
        if (this->value > this->max) this->value = this->max;
        this->value = (this->value * this->scale) + this->offset;
        this->_last_update = millis();
    }
}

EnsembleSensor::EnsembleSensor(unsigned long interval, float min, float max, float scale, float offset, char* name, SensorUnit unit, Sensor **sensors, uint64_t sensor_mask, EnsembleAction action) : 
Sensor(interval, min, max, scale, offset, name, unit), 
sensors(sensors),
sensor_mask(sensor_mask), 
action(action) {}

void EnsembleSensor::_update_raw_value() {
    float inital;
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
            float value = sensors[i]->value;

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
        this->value = inital / (float) count;
    } else {
        this->value = inital;
    }
}
