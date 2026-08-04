#include "i2c.h"

#if defined(OSPI)

#include "../utils.h"

#include <fcntl.h>
#include <sys/ioctl.h>

extern "C" {
#include <i2c/smbus.h>
#include <linux/i2c-dev.h>
}

#include <cstring>

I2CBus Bus;

int I2CBus::begin(const char* bus) {
	_file = open(bus, O_RDWR);
	return (_file < 0) ? _file : 0;
}

int I2CBus::begin() {
	return begin(get_default_bus());
}

int I2CBus::send(unsigned char addr, unsigned char reg, unsigned char data) {
	if (_file < 0 || ioctl(_file, I2C_SLAVE, addr) < 0) return -1;
	return i2c_smbus_write_byte_data(_file, reg, data);
}

int I2CBus::send_transaction(unsigned char addr, unsigned char transaction_id,
	unsigned char transaction_buffer_length, unsigned char* transaction_buffer) {
	if (_file < 0 || ioctl(_file, I2C_SLAVE, addr) < 0) return -1;
	return i2c_smbus_write_i2c_block_data(
		_file, transaction_id, transaction_buffer_length, transaction_buffer);
}

int I2CBus::read(unsigned char addr, unsigned char reg, unsigned char length,
	unsigned char* values) {
	if (_file < 0 || ioctl(_file, I2C_SLAVE, addr) < 0) return -1;
	return i2c_smbus_read_i2c_block_data(_file, reg, length, values);
}

int I2CBus::send_word(unsigned char addr, unsigned char reg, unsigned short data) {
	if (_file < 0 || ioctl(_file, I2C_SLAVE, addr) < 0) return -1;
	return i2c_smbus_write_word_data(_file, reg, data);
}

int I2CBus::read_word(unsigned char addr, unsigned char reg) {
	if (_file < 0 || ioctl(_file, I2C_SLAVE, addr) < 0) return -1;
	return i2c_smbus_read_word_data(_file, reg);
}

int I2CBus::detect(unsigned char addr) {
	if (_file < 0 || ioctl(_file, I2C_SLAVE, addr) < 0) return -1;
	int result = i2c_smbus_read_byte(_file);
	return (result < 0) ? result : 0;
}

const char* I2CBus::get_default_bus() const {
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

I2CDevice::I2CDevice(I2CBus& bus, unsigned char addr) : _bus(&bus), _addr(addr) {}

bool I2CDevice::detect() {
	return _bus->detect(_addr) == 0;
}

int I2CDevice::begin_transaction(unsigned char id) {
	if (transaction) return -1;

	transaction_id = id;
	transaction = true;
	memset(transaction_buffer, 0, sizeof(transaction_buffer));
	transaction_buffer_length = 0;
	return 0;
}

int I2CDevice::end_transaction() {
	if (!transaction) return -1;

	transaction = false;
	return send_transaction();
}

int I2CDevice::send(unsigned char reg, unsigned char data) {
	if (!transaction) return _bus->send(_addr, reg, data);
	if (reg != transaction_id) return -1;

	int result = 0;
	if (transaction_buffer_length >= sizeof(transaction_buffer)) {
		result = send_transaction();
		transaction_buffer_length = 0;
	}

	transaction_buffer[transaction_buffer_length++] = data;
	return result;
}

int I2CDevice::read(unsigned char reg, unsigned char length, unsigned char* values) {
	return _bus->read(_addr, reg, length, values);
}

int I2CDevice::send_word(unsigned char reg, unsigned short data) {
	return _bus->send_word(_addr, reg, data);
}

int I2CDevice::read_word(unsigned char reg) {
	return _bus->read_word(_addr, reg);
}

int I2CDevice::send_transaction() {
	return _bus->send_transaction(
		_addr, transaction_id, transaction_buffer_length, transaction_buffer);
}

#endif
