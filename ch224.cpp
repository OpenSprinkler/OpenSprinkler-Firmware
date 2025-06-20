#include "ch224.hpp"
CH224::CH224(uint8_t address, TwoWire &wire) : _address(address), _wire(&wire) {
    for (size_t i = 0; i < CH224_PDO_COUNT; i++) {
        supported_voltages[i] = {0, 0};
        supported_pps_ranges[i] = {0, 0, 0};
    }
    
}

CH224::CH224() : CH224(0x23, Wire) {}

CH224::CH224(uint8_t address) : CH224(address, Wire) {}


// 0x09
ch224_status_t CH224::get_status() {
  uint8_t raw_status = this->read_byte(0x09);
  ch224_status_t status;
    status.bc_activation   = (raw_status >> 0) & 1;
    status.qc2_activation  = (raw_status >> 1) & 1;
    status.qc3_activation  = (raw_status >> 2) & 1;
    status.pd_activation   = (raw_status >> 3) & 1;
    status.epr_activation  = (raw_status >> 4) & 1;
    status.epr_exists      = (raw_status >> 5) & 1;
    status.avs_exists      = (raw_status >> 6) & 1;
    status.reserved        = (raw_status >> 7) & 1;
    return status;
}

// 0x0A
void CH224::set_voltage_mode(CH224VoltageMode voltage) {
    this->send_byte(0x0A, (uint8_t) voltage);
}

// 0x50 (50mA)
uint8_t CH224::get_current() {
    return this->read_byte(0x50);
}
uint16_t CH224::get_current_ma() {
    return this->get_current() * 50;
}

// high: 0x51, low: 0x52 (25mV)
void CH224::set_avs_voltage(uint16_t voltage) {
    this->send_byte(0x51, voltage >> 8);
    this->send_byte(0x52, voltage & 0xFF);
}
void CH224::set_avs_voltage_mv(uint32_t voltage) {
    this->set_avs_voltage(voltage/25);
}

// 0x53 (100mV)
void CH224::set_pps_voltage(uint8_t voltage) {
    this->send_byte(0x53, voltage);
}
void CH224::set_pps_voltage_mv(uint16_t voltage) {
    this->set_pps_voltage(voltage / 100);
}

#include <Arduino.h>

// base: 0x60, count: 0x30
void CH224::update_power_data() {
    size_t pps_count = 0, fixed_count = 0;

    for (size_t i = 0; i < CH224_PDO_COUNT; i++) {
        uint32_t v = 0;
        for (size_t j = 0; j < 4; j++) {
            v <<= 8;
            v |= this->read_byte(0x60 + (4 * i) + j);
        }
     
        switch (v >> 30) {
            // Fixed
            case 0b00:
                Serial.println(v & 0b1111111111);
                this->supported_voltages[fixed_count++] = { voltage: (uint16_t) (((v >> 10) & 0b1111111111) * 50), max_current: (uint16_t) ((v & 0b1111111111) * 10) };
                break;
            // Battery
            case 0b01:
            // Ignore for now
                break;
            // Variable
            case 0b10:
                this->supported_pps_ranges[pps_count++] = { max_voltage: (uint16_t) (((v >> 20) & 0b1111111111) * 50), min_voltage: (uint16_t) (((v >> 10) & 0b1111111111) * 50), max_current: (uint16_t) ((v & 0b1111111111) * 10) };
                break;
            
            // Reserved
            case 0b11:
            default:
                break;
        }
    }
}

void CH224::send_byte(uint8_t address, uint8_t data) {
    this->_wire->beginTransmission(this->_address);
    this->_wire->write(address);
    this->_wire->write(data);
    this->_wire->endTransmission();
}

uint8_t CH224::read_byte(uint8_t address) {
    this->_wire->beginTransmission(this->_address);
    this->_wire->write(address);
    this->_wire->endTransmission(false);
    this->_wire->requestFrom(this->_address, (uint8_t) 1);
    if (this->_wire->available()) {
        return this->_wire->read();
    } else {
        return 0xFF;
    }
}