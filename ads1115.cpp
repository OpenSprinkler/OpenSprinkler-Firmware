#include <ads1115.hpp>

#if defined(ARDUINO)
ADS1115::ADS1115(uint8_t address) {
    this->adc = ADS1X15::ADS1115(address);
}

bool ADS1115::begin() {
    if (!this->adc.begin()) {
        return false;
    }

    this->adc.setGain(0);      //  6.144 volt
    this->adc.setDataRate(7);  //  0 = slow   4 = medium   7 = fast
    this->adc.setMode(1); // Once
    return true;
}

int16_t ADS1115::get_pin_value(uint8_t pin) {
    return this->adc.readADC(pin);
}
#else // OSPI
ADS1115::ADS1115(uint8_t address) {
    this->_addr = address;
    this->i2c = I2CDevice();
}

bool ADS1115::begin() {
    if (this->i2c.begin(_addr) < 0) {
        return false;
    }
    return true;
}

uint16_t ADS1115::get_pin_value(uint8_t pin) {
    {
    uint32_t start = millis();
    //  timeout == { 138, 74, 42, 26, 18, 14, 12, 11 }
    //  added 10 ms more than maximum conversion time from datasheet.
    //  to prevent premature timeout in RTOS context.
    //  See #82
    uint8_t timeOut = (128 >> (_datarate >> 5)) + 10;
    while (isBusy())
    {
      if ( (millis() - start) > timeOut)
      {
        _error = ADS1X15_ERROR_TIMEOUT;
        return ADS1X15_ERROR_TIMEOUT;
      }
      yield();   //  wait for conversion; yield for ESP.
    }
  }
}

void ADS1115::_write_register(uint8_t reg, uint16_t value) {
    this->i2c.begin_transaction(reg);
    this->i2c.send(reg, value >> 8);
    this->i2c.send(reg, value & 0xFF);
    this->i2c.end_transaction();
}

uint16_t _read_register(uint8_t reg) {
    uint8_t values[2];
    this->i2c.read(reg, 2, values);
    return values[0] << 8 | values[1]
}
#endif