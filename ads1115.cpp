#include "ads1115.h"

#if defined(ARDUINO)
ADS1115::ADS1115(uint8_t address, TwoWire& wire) : _address(address), _wire(&wire) {}
ADS1115::ADS1115(uint8_t address) : ADS1115(address, Wire) {}

bool ADS1115::begin() {
    if ((this->_address < 0x48) || (this->_address > 0x4B)) {
        return false;
    }
   this-> _wire->beginTransmission(_address);
    return (this->_wire->endTransmission() == 0);
}


void ADS1115::_write_register(uint8_t reg, uint16_t value) {
    this->_wire->beginTransmission(this->_address);
    this->_wire->write(reg);
    this->_wire->write((uint8_t) (value >> 8));
    this->_wire->write((uint8_t) (value & 0xFF));
    this->_wire->endTransmission();
}

uint16_t ADS1115::_read_register(uint8_t reg) {
    this->_wire->beginTransmission(this->_address);
    this->_wire->write(reg);
    if (!this->_wire->endTransmission()) {
        if (this->_wire->requestFrom((int) _address, (int) 2) == 2) {
            uint16_t val = ((uint16_t) this->_wire->read()) << 8;
            val += (uint16_t) this->_wire->read();
            return val;
        }
    }

    return 0;
}

#else // OSPI
ADS1115::ADS1115(uint8_t address) {
    this->_address = address;
    this->i2c = I2CDevice();
}

bool ADS1115::begin() {
    if (this->i2c.begin(_address) < 0) {
        return false;
    }
    return true;
}

void ADS1115::_write_register(uint8_t reg, uint16_t value) {
    this->i2c.begin_transaction(reg);
    this->i2c.send(reg, value >> 8);
    this->i2c.send(reg, value & 0xFF);
    this->i2c.end_transaction();
}

uint16_t ADS1115::_read_register(uint8_t reg) {
    uint8_t values[2];
    this->i2c.read(reg, 2, values);
    return values[0] << 8 | values[1];
}
#endif

int16_t ADS1115::get_pin_value(uint8_t pin) {
    this->request_pin(pin);

    uint32_t start = millis();
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
    uint16_t config = 0x8000 | ((4 + ((uint16_t) pin)) << 12) | 0x0100 | (4 << 5);
    this->_write_register(0x01, config);
}