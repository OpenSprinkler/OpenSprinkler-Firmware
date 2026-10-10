#include "services/sensor_ports.h"
#include "OpenSprinkler.h"
#include "platform/sensor_input.h"

bool sensor_port_analog_capable(uint8_t input) {
	const auto &board = osboard::active();
	return input < board.sensor_count && (board.analog_sensor_mask & (1U << input));
}

bool sensor_port_is_analog(uint8_t input) {
	return input < NUM_SENSORS &&
		OpenSprinkler::iopts[sensor_iopt_keys[input].type] == SENSOR_TYPE_ANALOG;
}

void sensor_ports_initialize() {
	for (uint8_t i = 0; i < osboard::active().sensor_count; i++) {
		sensor_input_configure(i, sensor_port_analog_capable(i));
	}
}

void sensor_ports_apply() {
	for (uint8_t i = 0; i < osboard::active().sensor_count; i++) {
		sensor_input_configure(i, sensor_port_analog_capable(i) && sensor_port_is_analog(i));
	}
}

bool sensor_port_transition_allowed(uint8_t input, uint8_t current, uint8_t requested,
	uint8_t analog_mask, uint8_t digital_mask) {
	if (current == requested) return true;
	if (input >= osboard::active().sensor_count) return requested != SENSOR_TYPE_ANALOG;
	const uint8_t bit = 1U << input;
	if (requested == SENSOR_TYPE_ANALOG) {
		return sensor_port_analog_capable(input) && !(digital_mask & bit);
	}
	return !(analog_mask & bit);
}

bool sensor_port_references(uint8_t &analog_mask, uint8_t &digital_mask) {
	analog_mask = digital_mask = 0;
	FileReader reader(SENSORS_FILENAME);
	if (!reader) return !file_exists(SENSORS_FILENAME) && !OpenSprinkler::nsensors;
	uint8_t count;
	if (!reader.read_block(&count, 0, 1) || count > MAX_SENSORS) return false;
	// Only inspect the header and first subtype byte. Do not disturb Sensor's
	// shared scratch object, and never hold this reader across a sensor write.
	char header[SENSOR_RECORD_HEADER_LEN];
	for (uint8_t i = 0; i < count; i++) {
		const uint32_t pos = 1 + static_cast<uint32_t>(i) * TMP_BUFFER_SIZE;
		if (!reader.read_block(header, pos, sizeof(header))) return false;
		const auto type = static_cast<SensorType>(header[0]);
		if (type != SensorType::OnboardAnalog && type != SensorType::OnboardDigital) continue;
		const uint8_t common_len = static_cast<uint8_t>(header[1]);
		const uint8_t extra_len = static_cast<uint8_t>(header[2]);
		// Pre-UUID records have the same port payload but two fewer common bytes.
		if (common_len < SENSOR_COMMON_PAYLOAD_LEN - sizeof(uint16_t) || !extra_len ||
			SENSOR_RECORD_HEADER_LEN + common_len + extra_len > TMP_BUFFER_SIZE) return false;
		uint8_t input;
		if (!reader.read_block(&input, pos + SENSOR_RECORD_HEADER_LEN + common_len, 1)) return false;
		if (input >= NUM_SENSORS) continue;
		if (type == SensorType::OnboardAnalog) analog_mask |= 1U << input;
		else digital_mask |= 1U << input;
	}
	return true;
}
