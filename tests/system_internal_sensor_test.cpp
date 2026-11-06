#include <cassert>
#include <cmath>
#include <cstring>

#include "sensors/system_internal_sensor.h"
#include "external/ArduinoJson.hpp"

static float temperature = 25.0f;
float chip_temperature_celsius() { return temperature; }
char dec2hexchar(unsigned char value) { return value < 10 ? '0' + value : 'A' + value - 10; }

int main() {
	char output[2048] = {};
	BufferFiller filler(output, sizeof(output));
	SystemInternalSensor::emit_description_json(&filler);
	assert(!filler.overflowed());
	ArduinoJson::JsonDocument description;
	assert(!ArduinoJson::deserializeJson(description, output));
	bool advertised[5] = {};
	for (auto option : description["as"][0]["o"].as<ArduinoJson::JsonArray>()) {
		const unsigned id = option["id"];
		assert(id < 5 && !advertised[id]);
		advertised[id] = true;
		assert(system_metric_is_supported(static_cast<SystemMetric>(id)));
		assert(option["dfl"]["unit"].as<int>() == static_cast<int>(system_metric_native_unit(static_cast<SystemMetric>(id))));
	}
	for (unsigned id = 0; id < 5; ++id) {
		assert(advertised[id] == system_metric_is_supported(static_cast<SystemMetric>(id)));
	}
	assert(advertised[3] && description["as"][0]["d"].as<int>() == 3);
	assert(!system_metric_is_supported(SystemMetric::MAX_VALUE));
	assert(!system_metric_is_supported(static_cast<SystemMetric>(255)));
	assert(system_metric_native_unit(SystemMetric::FREE_HEAP) == SensorUnit::Kilobyte);
	assert(system_metric_native_unit(SystemMetric::WIFI_RSSI) == SensorUnit::None);

	SystemInternalSensor sensor(15, -40, 185, "Temperature", SensorUnit::Celsius,
		1, SystemMetric::CPU_TEMPERATURE);
	uint8_t status = 0;
	assert(sensor.get_new_value(&status) == 25.0f && status == SENSOR_STATUS_VALID);
	sensor.unit = SensorUnit::Fahrenheit;
	assert(sensor.get_new_value(&status) == 77.0f && status == SENSOR_STATUS_VALID);
	sensor.unit = SensorUnit::Kelvin;
	sensor.max = 400;
	assert(std::fabs(sensor.get_new_value(&status) - 298.15f) < 0.001f);
	temperature = NAN;
	assert(std::isnan(sensor.get_new_value(&status)) && status == SENSOR_STATUS_ERROR);
	temperature = 25;
	assert(std::isfinite(sensor.get_new_value(&status)) && status == SENSOR_STATUS_VALID);

	sensor.uuid = 123;
	char record[TMP_BUFFER_SIZE] = {};
	const uint32_t length = sensor.serialize(record);
	assert(record[0] == 3 && record[2] == 1);
	assert(record[SENSOR_RECORD_HEADER_LEN + SENSOR_COMMON_PAYLOAD_LEN] == 3);
	SystemInternalSensor restored(record, length);
	assert(restored.uuid == 123 && restored.metric == SystemMetric::CPU_TEMPERATURE);
	assert(restored.unit == SensorUnit::Kelvin && restored.max == 400);
	assert(std::fabs(restored.get_new_value() - 298.15f) < 0.001f);
	// Unknown IDs retain today's unavailable-on-read behavior, with no migration.
	record[SENSOR_RECORD_HEADER_LEN + SENSOR_COMMON_PAYLOAD_LEN] = 99;
	SystemInternalSensor unknown(record, length);
	assert(std::isnan(unknown.get_new_value(&status)) && status == SENSOR_STATUS_ERROR);
}
