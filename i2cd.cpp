#include "i2cd.h"

std::map<std::string, int> I2CDevice::_bus_fds;
std::map<std::string, int> I2CDevice::_bus_refcount;
std::mutex I2CDevice::_bus_mutex;