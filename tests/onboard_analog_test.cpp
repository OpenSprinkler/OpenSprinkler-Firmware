#include "OpenSprinkler.h"
#include "services/sensor_ports.h"
#include "external/ArduinoJson.hpp"
#include <cassert>
#include <cmath>
#include <cstring>

unsigned char OpenSprinkler::iopts[NUM_IOPTS] = {};
unsigned char OpenSprinkler::nsensors = 0;
SensorState OpenSprinkler::sn_sensors[NUM_SENSORS] = {};
const SensorIoptKeys sensor_iopt_keys[NUM_SENSORS] = {
	{IOPT_SENSOR1_TYPE, IOPT_SENSOR1_OPTION, IOPT_SENSOR1_ON_DELAY, IOPT_SENSOR1_OFF_DELAY},
	{IOPT_SENSOR2_TYPE, IOPT_SENSOR2_OPTION, IOPT_SENSOR2_ON_DELAY, IOPT_SENSOR2_OFF_DELAY},
	{IOPT_SENSOR3_TYPE, IOPT_SENSOR3_OPTION, IOPT_SENSOR3_ON_DELAY, IOPT_SENSOR3_OFF_DELAY},
	{IOPT_SENSOR4_TYPE, IOPT_SENSOR4_OPTION, IOPT_SENSOR4_ON_DELAY, IOPT_SENSOR4_OFF_DELAY},
};
char tmp_buffer[TMP_BUFFER_SIZE];
bool sensor_available(uint8_t input) { return input < osboard::active().sensor_count; }
char dec2hexchar(unsigned char v) { return v < 10 ? '0' + v : 'A' + v - 10; }
float sensor_input_voltage(uint8_t) { return 1.5f; }
static bool high_impedance[4];
void sensor_input_configure(uint8_t input, bool high_z) { high_impedance[input] = high_z; }
int16_t ADS1115::get_pin_value(uint8_t) { return 8000; } // exactly 1.5 V
ADS1115::ADS1115(uint8_t address) : _address(address) {}

int main(int argc, char **argv) {
	assert(argc == 2);
	set_data_dir(argv[1]);
	osboard::select(osboard::PROFILE_OS_40);
	sensor_ports_initialize();
	for (bool value : high_impedance) assert(value);
	OpenSprinkler::iopts[IOPT_SENSOR1_TYPE] = SENSOR_TYPE_ANALOG;
	sensor_ports_apply();
	assert(high_impedance[0] && !high_impedance[1]);
	assert(sensor_port_transition_allowed(0, 0, 4, 0, 0));
	assert(!sensor_port_transition_allowed(0, 0, 4, 0, 1));
	assert(!sensor_port_transition_allowed(0, 4, 0, 1, 0));
	assert(!sensor_port_transition_allowed(0, 1, 3, 1, 0));
	assert(sensor_port_transition_allowed(0, 4, 4, 1, 1));
	assert(sensor_port_transition_allowed(0, 1, 1, 1, 1));

	sensor_adjustment_point_t points[] = {{0, 10}, {3, 70}};
	OnboardAnalogSensor analog(15, -1000, 1000, "Analog", SensorUnit::Volt, 0, 0,
		2, 3, ADS1115Subtype::LINEAR, 0, nullptr);
	uint8_t status;
	assert(analog.get_new_value(&status) == 6 && status == SENSOR_STATUS_VALID);
	analog.uuid = 321;
	char record[TMP_BUFFER_SIZE] = {};
	uint32_t len = analog.serialize(record);
	assert(record[0] == 5 && record[2] == 10);
	OnboardAnalogSensor restored(record, len);
	assert(restored.uuid == 321 && restored.input == 0 && restored.flag == 0);
	assert(restored.get_new_value() == 6);
	assert(Sensor::write(&analog, 0));
	OpenSprinkler::nsensors = 1;
	assert(file_write_byte(SENSORS_FILENAME, 0, 1));
	uint8_t ar, dr;
	assert(sensor_port_references(ar, dr) && ar == 1 && dr == 0);
	OnboardDigitalSensor digital(15, 0, 1, "Digital", SensorUnit::None, 0, OnboardInput::SN2);
	assert(Sensor::write(&digital, 1));
	OpenSprinkler::nsensors = 2;
	assert(file_write_byte(SENSORS_FILENAME, 0, 2));
	assert(sensor_port_references(ar, dr) && ar == 1 && dr == 2);
	// Reference scanning also accepts pre-UUID digital records.
	char legacy_digital[TMP_BUFFER_SIZE] = {};
	digital.serialize(legacy_digital);
	const uint32_t old_extra = SENSOR_RECORD_HEADER_LEN + SENSOR_COMMON_PAYLOAD_LEN;
	legacy_digital[1] -= sizeof(uint16_t);
	legacy_digital[old_extra - sizeof(uint16_t)] = legacy_digital[old_extra];
	assert(file_write_block(SENSORS_FILENAME, legacy_digital, 1 + TMP_BUFFER_SIZE, TMP_BUFFER_SIZE));
	assert(sensor_port_references(ar, dr) && ar == 1 && dr == 2);
	OpenSprinkler::sn_sensors[1].active = 1;
	assert(digital.get_new_value() == 1);
	OpenSprinkler::iopts[IOPT_SENSOR2_TYPE] = SENSOR_TYPE_ANALOG;
	assert(std::isnan(digital.get_new_value(&status)) && status == SENSOR_STATUS_ERROR);
	OpenSprinkler::iopts[IOPT_SENSOR1_TYPE] = SENSOR_TYPE_RAIN;
	assert(std::isnan(analog.get_new_value(&status)) && status == SENSOR_STATUS_ERROR);
	OpenSprinkler::iopts[IOPT_SENSOR1_TYPE] = SENSOR_TYPE_ANALOG;
	OpenSprinkler::nsensors = 0;
	sensor_ports_apply();
	assert(high_impedance[0] && sensor_port_is_analog(0));

	ADS1115 fake(0x48);
	ADS1115 *devices[4] = {&fake, nullptr, nullptr, nullptr};
	for (uint8_t id = 0; id < static_cast<uint8_t>(ADS1115Subtype::MAX_VALUE); id++) {
		auto subtype = static_cast<ADS1115Subtype>(id);
		if (!ads1115_subtype_is_supported(subtype)) continue;
		analog.subtype = subtype;
		analog.num_points = 2;
		memcpy(analog.points, points, sizeof(points));
		analog.unit = ads1115_subtype_native_unit(subtype);
		if (analog.unit == SensorUnit::Celsius) analog.unit = SensorUnit::Fahrenheit;
		ADS1115Sensor ads(15, -1000, 1000, "ADS", analog.unit, 1, devices, 0, 0,
			2, 3, subtype, 2, points);
		assert(std::fabs(ads.get_new_value() - analog.get_new_value()) < 0.001f);
		char a[TMP_BUFFER_SIZE] = {}, b[TMP_BUFFER_SIZE] = {};
		const uint32_t alen = ads.serialize(a);
		const uint32_t blen = analog.serialize(b);
		const uint32_t payload = SENSOR_RECORD_HEADER_LEN + SENSOR_COMMON_PAYLOAD_LEN;
		assert(alen == blen + 1);
		assert(!memcmp(a + payload + 2, b + payload + 1, blen - payload - 1));
		ADS1115Sensor ads_copy(devices, a, alen);
		OnboardAnalogSensor analog_copy(b, blen);
		assert(std::fabs(ads_copy.get_new_value() - analog_copy.get_new_value()) < 0.001f);
	}
	ADS1115Sensor ads(15, 0, 100, "Legacy", SensorUnit::Volt, 1, devices, 0, 0, 2, 3,
		ADS1115Subtype::LINEAR, 0, nullptr);
	len = ads.serialize(record);
	record[2] = 10; // old payload without subtype byte
	ADS1115Sensor legacy(devices, record, len - 1);
	assert(legacy.get_new_value() == 6);

	char output[8192] = {};
	BufferFiller bfill(output, sizeof(output));
	OnboardAnalogSensor::emit_description_json(&bfill);
	ArduinoJson::JsonDocument json;
	assert(!bfill.overflowed() && !ArduinoJson::deserializeJson(json, output));
	assert(json["as"][0]["o"].size() == 4);
	assert(json["as"][0]["o"][0]["dfl"]["max"].as<float>() == 3.3f);
	osboard::select(osboard::PROFILE_OS_34);
	assert(!sensor_port_analog_capable(0));
	assert(!sensor_port_transition_allowed(0, 0, 4, 0, 0));
	assert(std::isnan(analog.get_new_value()));
	BufferFiller disabled(output, sizeof(output));
	OnboardAnalogSensor::emit_description_json(&disabled);
	assert(!strcmp(output, "{\"dis\":1}"));
}
