#ifndef I2CD_H
#define I2CD_H

#include <fcntl.h>
#include <sys/ioctl.h>

extern "C" {
#include <i2c/smbus.h>
#include <linux/i2c-dev.h>
}
#include <map>
#include <string>
#include <mutex>
#include "utils.h"


class I2CDevice {
public:
  I2CDevice() = default;
  ~I2CDevice() {
    closeBusIfUnused();
  }

  int begin(const char *bus, unsigned char addr) {
    std::lock_guard<std::mutex> lock(_bus_mutex);

    _bus = bus;
    _addr = addr;

    // Open bus if not already opened
    if (_bus_fds.count(bus) == 0) {
      int fd = open(bus, O_RDWR);
      if (fd < 0) return fd;
      _bus_fds[bus] = fd;
      _bus_refcount[bus] = 1;
    } else {
      _bus_refcount[bus]++;
    }

    _file = _bus_fds[bus];

    return ioctl(_file, I2C_SLAVE, addr);
  }

  int begin(unsigned char addr) { return begin(getDefaultBus(), addr); }

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
    std::lock_guard<std::mutex> lock(_bus_mutex);
    int res = ioctl(_file, I2C_SLAVE, _addr);
    if (res < 0) return res;
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
      return i2c_smbus_write_byte_data(_file, reg, data);
    }
  }

  int read(unsigned char reg, unsigned char length, unsigned char *values) {
    std::lock_guard<std::mutex> lock(_bus_mutex);
    int res = ioctl(_file, I2C_SLAVE, _addr);
    if (res < 0) return res;
    return i2c_smbus_read_i2c_block_data(_file, reg, length, values);
  }

private:
  int _file = -1;
  const char *_bus = nullptr;
  unsigned char _addr = 0;

  // Static map of open bus file descriptors
  static std::map<std::string, int> _bus_fds;
  static std::map<std::string, int> _bus_refcount;
  static std::mutex _bus_mutex;

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
    return i2c_smbus_write_i2c_block_data(
        _file, transaction_id, transaction_buffer_length, transaction_buffer);
  }

  void closeBusIfUnused() {
    std::lock_guard<std::mutex> lock(_bus_mutex);
    if (!_bus) return;
    auto it = _bus_refcount.find(_bus);
    if (it != _bus_refcount.end()) {
      it->second--;
      if (it->second <= 0) {
        close(_bus_fds[_bus]);
        _bus_fds.erase(_bus);
        _bus_refcount.erase(it);
      }
    }
  }
};

// Static members initialization
std::map<std::string, int> I2CDevice::_bus_fds;
std::map<std::string, int> I2CDevice::_bus_refcount;
std::mutex I2CDevice::_bus_mutex;

#endif // I2CD_H