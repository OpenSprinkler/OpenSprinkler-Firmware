#if defined(ARDUINO)

#include "ch224.h"
#include <Arduino.h> // For delay()

// CH224 Register Addresses
const uint8_t REG_STATUS = 0x09;
const uint8_t REG_VOL_CFG = 0x0A;
const uint8_t REG_CURRENT = 0x50;
const uint8_t REG_AVS_CFG_H = 0x51;
const uint8_t REG_AVS_CFG_L = 0x52;
const uint8_t REG_PPS_CFG = 0x53;
const uint8_t REG_PD_SRCCAP_START = 0x60;


CH224::CH224(uint8_t address, TwoWire& wire) : _address(address), _wire(&wire) {
	// Initialize PDO arrays to zero
	for (size_t i = 0; i < CH224_MAX_PDO_COUNT; i++) {
		supported_fixed_voltages[i] = {0, 0};
		supported_pps_ranges[i] = {false, 0, 0, 0};
		supported_avs_ranges[i] = {0, 0, 0};
	}
	_fixed_pdo_count = 0;
	_pps_apdo_count = 0;
	_avs_apdo_count = 0;
}

CH224::CH224() : CH224(0x23, Wire) {}

CH224::CH224(uint8_t address) : CH224(address, Wire) {}

void CH224::begin() {
	// The user should call Wire.begin() in their setup function.
	// This is just a placeholder.
}

ch224_status_t CH224::get_status() {
	uint8_t raw_status = this->read_byte(REG_STATUS);
	ch224_status_t status;
	status.bc_activation = (raw_status >> 0) & 1;
	status.qc2_activation = (raw_status >> 1) & 1;
	status.qc3_activation = (raw_status >> 2) & 1;
	status.pd_activation = (raw_status >> 3) & 1;
	status.epr_activation = (raw_status >> 4) & 1;
	status.epr_exists = (raw_status >> 5) & 1;
	status.avs_exists = (raw_status >> 6) & 1;
	status.reserved = (raw_status >> 7) & 1;
	return status;
}

bool CH224::update_power_data() {
	if (!get_status().pd_activation) {
		return false;
	}

	// Clear previous data and reset counts
	_fixed_pdo_count = 0;
	_pps_apdo_count = 0;
	_avs_apdo_count = 0;
	for (size_t i = 0; i < CH224_MAX_PDO_COUNT; i++) {
		supported_fixed_voltages[i] = {0, 0};
		supported_pps_ranges[i] = {false, 0, 0, 0};
		supported_avs_ranges[i] = {0, 0, 0};
	}

	read_sequential(REG_PD_SRCCAP_START, this->_src_cap_buffer, CH224_SRC_CAP_SIZE);

	uint16_t header_raw = (uint16_t)(this->_src_cap_buffer[1] << 8) | this->_src_cap_buffer[0];
	uint8_t num_data_objects = (header_raw >> 12) & 0b111;
	size_t offset = 2; // Start after the 2-byte header


	for (size_t i = 0; i < num_data_objects && i < CH224_MAX_PDO_COUNT; i++) {
		uint32_t pdo = 0;
		// The data is little-endian, so we build the 32-bit integer from the buffer.
		pdo |= (uint32_t)this->_src_cap_buffer[offset + (4 * i) + 3] << 24;
		pdo |= (uint32_t)this->_src_cap_buffer[offset + (4 * i) + 2] << 16;
		pdo |= (uint32_t)this->_src_cap_buffer[offset + (4 * i) + 1] << 8;
		pdo |= (uint32_t)this->_src_cap_buffer[offset + (4 * i)];

		uint8_t pdo_type = (pdo >> 30) & 0b11;

		switch (pdo_type) {
		case 0b00: // Fixed Supply PDO
			if (_fixed_pdo_count < CH224_MAX_PDO_COUNT) {
				supported_fixed_voltages[_fixed_pdo_count++] = {
					voltage_mv: (uint16_t)(((pdo >> 10) & 0x3FF) * 50),
					max_current_ma: (uint16_t)((pdo & 0x3FF) * 10)
				};
			}
			break;

		case 0b11: // Augmented PDO (APDO)
			{
				uint8_t apdo_type = (pdo >> 28) & 0b11;
				switch (apdo_type) {
				case 0b00: // SPR PPS APDO
					if (_pps_apdo_count < CH224_MAX_PDO_COUNT) {
						supported_pps_ranges[_pps_apdo_count++] = {
							power_limited: (bool)((pdo >> 27) & 1),
							max_voltage_mv: (uint16_t)(((pdo >> 17) & 0xFF) * 100),
							min_voltage_mv: (uint16_t)(((pdo >> 8) & 0xFF) * 100), // Note: C-style lib was incorrect, this is 100mV units not 20mV for SPR PPS
							max_current_ma: (uint16_t)((pdo & 0x7F) * 50)
						};
					}
					break;
				case 0b01: // EPR AVS APDO
					if (_avs_apdo_count < CH224_MAX_PDO_COUNT) {
						supported_avs_ranges[_avs_apdo_count++] = {
							max_voltage_mv: (uint16_t)(((pdo >> 17) & 0x1FF) * 100),
							min_voltage_mv: (uint16_t)(((pdo >> 8) & 0xFF) * 100),
							max_power_w: (uint8_t)(pdo & 0xFF)
						};
					}
					break;
				}
			}
			break;
		// Ignoring Battery and Variable Supply PDOs for now
		case 0b01: // Battery
		case 0b10: // Variable
		default:
			break;
		}
	}
	return true;
}

bool CH224::is_pd_supported() {
	return get_status().pd_activation;
}

bool CH224::has_pps() {
	return _pps_apdo_count > 0;
}

bool CH224::has_avs() {
	return _avs_apdo_count > 0;
}

bool CH224::has_epr() {
	return get_status().epr_exists;
}

bool CH224::is_voltage_supported(uint8_t type, uint16_t req_mv) {
	if (type == PD_TYPE_FIXED_SUPPLY) {
		for (size_t i = 0; i < _fixed_pdo_count; i++) {
			if (supported_fixed_voltages[i].voltage_mv == req_mv) return true;
		}
	} else if (type == PD_TYPE_PPS_SUPPLY) {
		for (size_t i = 0; i < _pps_apdo_count; i++) {
			if (req_mv >= supported_pps_ranges[i].min_voltage_mv && req_mv <= supported_pps_ranges[i].max_voltage_mv) return true;
		}
	} else if (type == PD_TYPE_AVS_SUPPLY) {
		for (size_t i = 0; i < _avs_apdo_count; i++) {
			if (req_mv >= supported_avs_ranges[i].min_voltage_mv && req_mv <= supported_avs_ranges[i].max_voltage_mv) return true;
		}
	}
	return false;
}

bool CH224::request_fixed_voltage(uint8_t vol) {
	if (!is_voltage_supported(PD_TYPE_FIXED_SUPPLY, vol * 1000)) {
		return false;
	}
	CH224VoltageMode mode;
	switch (vol) {
		case 5:  mode = CH224VoltageMode::Voltage_5V;  break;
		case 9:  mode = CH224VoltageMode::Voltage_9V;  break;
		case 12: mode = CH224VoltageMode::Voltage_12V; break;
		case 15: mode = CH224VoltageMode::Voltage_15V; break;
		case 20: mode = CH224VoltageMode::Voltage_20V; break;
		case 28: mode = CH224VoltageMode::Voltage_28V; break;
		default: return false;
	}
	set_voltage_mode(mode);
	delay(5); // Allow time for voltage to change
	return (read_byte(REG_VOL_CFG) == (uint8_t)mode);
}

bool CH224::request_pps_voltage(float vol) {
	uint16_t req_mv = (uint16_t)(vol * 1000.0f);
	if (!is_voltage_supported(PD_TYPE_PPS_SUPPLY, req_mv)) {
		return false;
	}

	set_pps_voltage_mv(req_mv);
	delay(2);
	set_voltage_mode(CH224VoltageMode::Mode_PPS);
	delay(5); // Allow time for mode switch

	uint8_t confirm_mode = read_byte(REG_VOL_CFG);
	return (confirm_mode == (uint8_t)CH224VoltageMode::Mode_PPS);
}

bool CH224::request_avs_voltage(float vol) {
	uint16_t req_mv = (uint16_t)(vol * 1000.0f);
	if (!is_voltage_supported(PD_TYPE_AVS_SUPPLY, req_mv)) {
		return false;
	}

	set_avs_voltage_mv(req_mv);
	delay(2);
	set_voltage_mode(CH224VoltageMode::Mode_AVS);
	delay(5); // Allow time for mode switch

	uint8_t confirm_mode = read_byte(REG_VOL_CFG);
	return (confirm_mode == (uint8_t)CH224VoltageMode::Mode_AVS);
}

bool CH224::request_voltage_closest(uint16_t voltage_mv) {
	// 1. Exact fixed match
	for (uint8_t i = 0; i < _fixed_pdo_count; i++) {
		if (supported_fixed_voltages[i].voltage_mv == voltage_mv) {
			request_fixed_voltage(voltage_mv / 1000);
			return true;
		}
	}

	// 2. PPS range match
	for (uint8_t i = 0; i < _pps_apdo_count; i++) {
		if (voltage_mv >= supported_pps_ranges[i].min_voltage_mv && voltage_mv <= supported_pps_ranges[i].max_voltage_mv) {
			request_pps_voltage(voltage_mv / 1000.0f);
			return true;
		}
	}

	// 3. AVS range match
	 for (uint8_t i = 0; i < _avs_apdo_count; i++) {
		if (voltage_mv >= supported_avs_ranges[i].min_voltage_mv && voltage_mv <= supported_avs_ranges[i].max_voltage_mv) {
			request_avs_voltage(voltage_mv / 1000.0f);
			return true;
		}
	}

	// If we get here, no exact match was found. Now find the closest possible voltage.
	enum class BestMatchType { NONE, FIXED, PPS, AVS };
	BestMatchType best_type = BestMatchType::NONE;
	uint16_t best_voltage_mv = 0;
	uint16_t min_diff = UINT16_MAX;

	// Check fixed voltages
	for (uint8_t i = 0; i < _fixed_pdo_count; i++) {
		uint16_t current_voltage = supported_fixed_voltages[i].voltage_mv;
		uint16_t diff = abs(voltage_mv - current_voltage);
		if (diff < min_diff) {
			min_diff = diff;
			best_voltage_mv = current_voltage;
			best_type = BestMatchType::FIXED;
		}
	}

	// Check PPS ranges (min and max)
	for (uint8_t i = 0; i < _pps_apdo_count; i++) {
		// Check min
		uint16_t min_v = supported_pps_ranges[i].min_voltage_mv;
		uint16_t diff_min = abs(voltage_mv - min_v);
		if (diff_min < min_diff) {
			min_diff = diff_min;
			best_voltage_mv = min_v;
			best_type = BestMatchType::PPS;
		}
		// Check max
		uint16_t max_v = supported_pps_ranges[i].max_voltage_mv;
		uint16_t diff_max = abs(voltage_mv - max_v);
		if (diff_max < min_diff) {
			min_diff = diff_max;
			best_voltage_mv = max_v;
			best_type = BestMatchType::PPS;
		}
	}

	// Check AVS ranges (min and max)
	 for (uint8_t i = 0; i < _avs_apdo_count; i++) {
		// Check min
		uint16_t min_v = supported_avs_ranges[i].min_voltage_mv;
		uint16_t diff_min = abs(voltage_mv - min_v);
		if (diff_min < min_diff) {
			min_diff = diff_min;
			best_voltage_mv = min_v;
			best_type = BestMatchType::AVS;
		}
		// Check max
		uint16_t max_v = supported_avs_ranges[i].max_voltage_mv;
		uint16_t diff_max = abs(voltage_mv - max_v);
		if (diff_max < min_diff) {
			min_diff = diff_max;
			best_voltage_mv = max_v;
			best_type = BestMatchType::AVS;
		}
	}

	// Now, make the request for the best match found
	if (best_type != BestMatchType::NONE) {
		switch(best_type) {
			case BestMatchType::FIXED:
				request_fixed_voltage(best_voltage_mv / 1000);
				break;
			case BestMatchType::PPS:
				request_pps_voltage(best_voltage_mv / 1000.0f);
				break;
			case BestMatchType::AVS:
				request_avs_voltage(best_voltage_mv / 1000.0f);
				break;
			case BestMatchType::NONE: // Should not happen if there's at least one PDO
				break;
		}
	}

	return false; // Return false because we didn't get an exact match
}

uint16_t CH224::get_output_voltage_mv() {
	uint8_t mode = read_byte(REG_VOL_CFG);
	uint16_t vol_mv = 0;
	switch ((CH224VoltageMode)mode) {
		case CH224VoltageMode::Voltage_5V:  vol_mv = 5000;  break;
		case CH224VoltageMode::Voltage_9V:  vol_mv = 9000;  break;
		case CH224VoltageMode::Voltage_12V: vol_mv = 12000; break;
		case CH224VoltageMode::Voltage_15V: vol_mv = 15000; break;
		case CH224VoltageMode::Voltage_20V: vol_mv = 20000; break;
		case CH224VoltageMode::Voltage_28V: vol_mv = 28000; break;
		case CH224VoltageMode::Mode_PPS: {
			uint8_t pps_val = read_byte(REG_PPS_CFG);
			vol_mv = (uint16_t)pps_val * 100;
			break;
		}
		case CH224VoltageMode::Mode_AVS: {
			uint8_t avs_h = read_byte(REG_AVS_CFG_H);
			uint8_t avs_l = read_byte(REG_AVS_CFG_L);
			vol_mv = ((uint16_t)avs_h << 8 | avs_l) * 25;
			break;
		}
	}
	return vol_mv;
}

uint16_t CH224::get_current_ma() {
	return read_byte(REG_CURRENT) * 50;
}

uint8_t CH224::get_fixed_pdo_count() const {
	return _fixed_pdo_count;
}

uint8_t CH224::get_pps_apdo_count() const {
	return _pps_apdo_count;
}

uint8_t CH224::get_avs_apdo_count() const {
	return _avs_apdo_count;
}


void CH224::set_voltage_mode(CH224VoltageMode voltage) {
	send_byte(REG_VOL_CFG, (uint8_t)voltage);
}

void CH224::set_avs_voltage_mv(uint16_t voltage_mv) {
	uint16_t avs_val = voltage_mv / 25; // Voltage is in 25mV steps
	send_byte(REG_AVS_CFG_H, avs_val >> 8);
	send_byte(REG_AVS_CFG_L, avs_val & 0xFF);
}

void CH224::set_pps_voltage_mv(uint16_t voltage_mv) {
	uint8_t pps_val = voltage_mv / 100; // Voltage is in 100mV steps
	send_byte(REG_PPS_CFG, pps_val);
}

void CH224::send_byte(uint8_t reg, uint8_t data) {
	_wire->beginTransmission(_address);
	_wire->write(reg);
	_wire->write(data);
	_wire->endTransmission();
}

uint8_t CH224::read_byte(uint8_t reg) {
	_wire->beginTransmission(_address);
	_wire->write(reg);
	_wire->endTransmission(false);
	_wire->requestFrom(_address, (uint8_t)1);
	if (_wire->available()) {
		return _wire->read();
	}
	return 0xFF; // Error
}

void CH224::read_sequential(uint8_t reg, uint8_t* buffer, uint8_t len) {
	_wire->beginTransmission(_address);
	_wire->write(reg);
	_wire->endTransmission(false);
	_wire->requestFrom(_address, len);
	for (uint8_t i = 0; i < len; i++) {
		if (_wire->available()) {
			buffer[i] = _wire->read();
		} else {
			buffer[i] = 0xFF; // Error
		}
	}
}

#endif
