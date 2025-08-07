#include "ads1115.h"

#if defined(ARDUINO)
ADS1115::ADS1115(uint8_t address, TwoWire& wire) : _address(address), _wire(&wire) {}
ADS1115::ADS1115(uint8_t address) : ADS1115(address, Wire) {}

bool ADS1115::begin() {
    if ((this->_address < 0x48) || (this->_address > 0x4B)) {
        return false;
    }
    this->_wire->beginTransmission(_address);
    return (this->_wire->endTransmission() == 0);
}


void ADS1115::_write_register(uint8_t reg, uint16_t value) {
    this->_wire->beginTransmission(this->_address);
    this->_wire->write(reg);
    this->_wire->write((uint8_t)(value >> 8));
    this->_wire->write((uint8_t)(value & 0xFF));
    this->_wire->endTransmission();
}

uint16_t ADS1115::_read_register(uint8_t reg) {
    this->_wire->beginTransmission(this->_address);
    this->_wire->write(reg);
    if (!this->_wire->endTransmission()) {
        if (this->_wire->requestFrom((int)_address, (int)2) == 2) {
            uint16_t val = ((uint16_t)this->_wire->read()) << 8;
            val += (uint16_t)this->_wire->read();
            return val;
        }
    }

    return 0;
}

#else // OSPI
ADS1115::ADS1115(uint8_t address, I2CBus& bus) : _address(address), _i2c(bus, address) {}
ADS1115::ADS1115(uint8_t address) : ADS1115(address, Bus) {}

bool ADS1115::begin() {
    if ((this->_address < 0x48) || (this->_address > 0x4B)) {
        return false;
    }
    if (this->_i2c.detect() < 0) {
        return false;
    }
    return true;
}

void ADS1115::_write_register(uint8_t reg, uint16_t value) {
    this->_i2c.send_word(reg, this->swap_reg(value));
}

uint16_t ADS1115::_read_register(uint8_t reg) {
    return this->swap_reg((uint16_t)(this->_i2c.read_word(reg) & 0xFFFF));
}
#endif

int16_t ADS1115::get_pin_value(uint8_t pin) {
    this->request_pin(pin);
    ulong start = millis();
    while (this->is_busy()) {
        // if ((millis() - start) > 11) {
        if ((millis() - start) > 18) {
            return 0;
        }

#if defined(ARDUINO)
        yield();
#endif
    }

    return this->get_value();
}

void ADS1115::request_pin(uint8_t pin) {
    uint16_t config = 0x8000 | ((4 + ((uint16_t)pin)) << 12) | 0x0100 | (4 << 5);
    this->_write_register(0x01, config);
}

ADS1115Sensor::ADS1115Sensor(unsigned long interval, double min, double max, double scale, double offset, const char* name, SensorUnit unit, uint32_t flags, ADS1115** sensors, uint8_t sensor_index, uint8_t pin) : 
Sensor(interval, min, max, scale, offset, name, unit, flags), 
sensor_index(sensor_index), 
pin(pin),
sensors(sensors) {}

void ADS1115Sensor::emit_extra_json(BufferFiller *bfill) {
    bfill->emit_p(PSTR("{\"index\":$D,\"pin\":$D}"), this->sensor_index, this->pin);
}

double ADS1115Sensor::get_inital_value() {
    return 0.0;
}

double ADS1115Sensor::_get_raw_value() {
    if (this->sensors[sensor_index] == nullptr) {
        return 0.0;
    }
    else {
        return ((double)this->sensors[sensor_index]->get_pin_value(this->pin)) * ADS1115_SCALE_FACTOR;
    }
}

uint32_t ADS1115Sensor::_serialize_internal(char *buf) {
    uint32_t i = 0;
    buf[i++] = static_cast<uint8_t>(this->sensor_index);
    buf[i++] = static_cast<uint8_t>(this->pin);
    return i;
}

ADS1115Sensor::ADS1115Sensor(ADS1115 **sensors, char *buf) {
    uint32_t i = Sensor::_deserialize(buf);
    this->sensor_index = static_cast<uint8_t>(buf[i++]);
    this->pin = static_cast<uint8_t>(buf[i++]);
    this->sensors = sensors;
}