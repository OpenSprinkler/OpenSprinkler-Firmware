#ifndef I2CD_H
#define I2CD_H

#include <fcntl.h>
#include <sys/ioctl.h>

extern "C" {
#include <i2c/smbus.h>
#include <linux/i2c-dev.h>
}
#include <string.h>
#include "utils.h"

class I2CBus {
public:
  I2CBus() {}

  int begin(const char *bus) {
    _file = open(bus, O_RDWR);
    if (_file < 0) {
      return _file;
    }

    return 0;
  }

  int begin() { return begin(getDefaultBus()); }

  int send(unsigned char addr, unsigned char reg, unsigned char data) {
    int res = ioctl(_file, I2C_SLAVE, addr);
    if (res < 0) return -1;
    return i2c_smbus_write_byte_data(_file, reg, data);
  }

  int send_transaction(unsigned char addr, unsigned char transaction_id, unsigned char *transaction_buffer, unsigned char transaction_buffer_length) {
    int res = ioctl(_file, I2C_SLAVE, addr);
    if (res < 0) return -1;
    return i2c_smbus_write_i2c_block_data(
        _file, transaction_id, transaction_buffer_length, transaction_buffer);
  }

  int read(unsigned char addr, unsigned char reg, unsigned char length, unsigned char *values) {
    int res = ioctl(_file, I2C_SLAVE, addr);
    if (res < 0) return -1;
    return i2c_smbus_read_i2c_block_data(_file, reg, length, values);
  }

private:
  int _file = -1;

  const char *getDefaultBus() { 
    switch (get_board_type()) {
            case BoardType::RaspberryPi_bcm2712:
            case BoardType::RaspberryPi_bcm2711:
            case BoardType::RaspberryPi_bcm2837:
            case BoardType::RaspberryPi_bcm2836:
            case BoardType::RaspberryPi_bcm2835:
                return "/dev/i2c-1";
            case BoardType::Unknown: 
            case BoardType::RaspberryPi_Unknown: 
            default:
                return "/dev/i2c-0";
        }
   }
};

class I2CDevice {
public:
  I2CDevice(I2CBus &bus, unsigned char addr) : _addr(addr), _bus(&bus) {}

  int begin_transaction(unsigned char id) {
    if (transaction) {
      return -1;
    } else {
      transaction_id = id;
      transaction = true;
      memset(transaction_buffer, 0x00, sizeof(transaction_buffer));
      transaction_buffer_length = 0;
      return 0;
    }
  }

  int end_transaction() {
    if (transaction) {
      transaction = false;
      return send_transaction();
    } else {
      return -1;
    }
  }

  int send(unsigned char reg, unsigned char data) {
    if (transaction) {
      if (reg != transaction_id) {
        return -1;
      }

      int res = 0;
      if (transaction_buffer_length >= sizeof(transaction_buffer)) {
        res = send_transaction();
        transaction_buffer_length = 0;
      }

      transaction_buffer[transaction_buffer_length] = data;
      transaction_buffer_length++;
      return res;
    } else {
      return _bus->send(_addr, reg, data);
    }
  }

  int read(unsigned char reg, unsigned char length, unsigned char *values) {
    return _bus->read(_addr, reg, length, values);
  }

private:
  I2CBus *_bus;
  unsigned char _addr;

  bool transaction = false;
  unsigned char transaction_id = 0;
  unsigned char transaction_buffer[32];
  unsigned char transaction_buffer_length = 0;

  const char *getDefaultBus() { 
    switch (get_board_type()) {
            case BoardType::RaspberryPi_bcm2712:
            case BoardType::RaspberryPi_bcm2711:
            case BoardType::RaspberryPi_bcm2837:
            case BoardType::RaspberryPi_bcm2836:
            case BoardType::RaspberryPi_bcm2835:
                return "/dev/i2c-1";
            case BoardType::Unknown: 
            case BoardType::RaspberryPi_Unknown: 
            default:
                return "/dev/i2c-0";
        }
   }

  int send_transaction() {
    return _bus->send_transaction(
        _addr, transaction_id, transaction_buffer_length, transaction_buffer);
  }
};

static I2CBus Bus;

#endif // I2CD_H