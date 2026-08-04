#pragma once

#if defined(OSPI)

class I2CBus {
public:
	I2CBus() = default;

	int begin(const char* bus);
	int begin();
	int send(unsigned char addr, unsigned char reg, unsigned char data);
	int send_transaction(unsigned char addr, unsigned char transaction_id,
		unsigned char transaction_buffer_length, unsigned char* transaction_buffer);
	int read(unsigned char addr, unsigned char reg, unsigned char length, unsigned char* values);
	int send_word(unsigned char addr, unsigned char reg, unsigned short data);
	int read_word(unsigned char addr, unsigned char reg);
	int detect(unsigned char addr);

private:
	int _file = -1;

	const char* get_default_bus() const;
};

class I2CDevice {
public:
	I2CDevice(I2CBus& bus, unsigned char addr);

	bool detect();
	int begin_transaction(unsigned char id);
	int end_transaction();
	int send(unsigned char reg, unsigned char data);
	int read(unsigned char reg, unsigned char length, unsigned char* values);
	int send_word(unsigned char reg, unsigned short data);
	int read_word(unsigned char reg);

private:
	I2CBus* _bus;
	unsigned char _addr;
	bool transaction = false;
	unsigned char transaction_id = 0;
	unsigned char transaction_buffer[32];
	unsigned char transaction_buffer_length = 0;

	int send_transaction();
};

extern I2CBus Bus;

#endif
