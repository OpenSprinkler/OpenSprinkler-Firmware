#ifndef I2CD_H
#define I2CD_H

#include <fcntl.h>
#include <sys/ioctl.h>

extern "C" {
#include <i2c/smbus.h>
#include <linux/i2c-dev.h>
}

class I2CDevice {
public:
  I2CDevice() {}

  int begin(const char *bus, unsigned char addr) {
    _file = open(bus, O_RDWR);
    if (_file < 0) {
      return _file;
    }

    return ioctl(file, I2C_SLAVE, addr);
  }

  int begin(unsigned char addr) {
    return begin(getDefaultBus(), addr);
  }

  int i2c_begin_transaction(unsigned char id) {
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

  int i2c_send_transaction() {
    return i2c_smbus_write_i2c_block_data(
        file, transaction_id, transaction_buffer_length, transaction_buffer);
  }

  int i2c_end_transaction() {
    if (transaction) {
      transaction = false;
      return i2c_send_transaction();
    } else {
      return -1;
    }
  }

  int i2c_send(unsigned char reg, unsigned char data) {
    if (transaction) {
      if (reg != transaction_id) {
        return -1;
      }

      int res = 0;
      if (transaction_buffer_length >= sizeof(transaction_buffer)) {
        res = i2c_send_transaction();
        transaction_buffer_length = 0;
      }

      transaction_buffer[transaction_buffer_length] = data;
      transaction_buffer_length++;
      return res;
    } else {
      return i2c_smbus_write_byte_data(file, reg, data);
    }
  }

private:
  int _file = -1;
  bool transaction = false;
  unsigned char transaction_id = 0;
  unsigned char transaction_buffer[32];
  unsigned char transaction_buffer_length = 0;

  const char *getDefaultBus() {
    return NULL;
  }
};

#endif // I2CD_H