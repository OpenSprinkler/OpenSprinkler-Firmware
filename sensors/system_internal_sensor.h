#pragma once

#include "../defines.h"
#include "sensor.h"

// Diagnostic metrics surfaced as Sensor instances for logging / UI display.
// Each metric is platform-specific; system_metric_is_supported() determines
// whether a metric can be created on the current build, and the description
// endpoint advertises only the supported ones.
//
// Numeric values are stable across platforms (so a record written on one
// build remains parseable on another, even if the metric isn't supported
// there — it just produces NAN at runtime).
enum class SystemMetric : uint8_t {
	FREE_HEAP          = 0,  // ESP8266/ESP32     — KB
	FREE_FLASH         = 1,  // ESP8266/ESP32     — free filesystem KB
	WIFI_RSSI          = 2,  // ESP8266/ESP32     — dBm, WiFi connected only
	CPU_TEMPERATURE    = 3,  // ESP32/OSPi/Linux  — chip temperature in °C
	HEAP_FRAGMENTATION = 4,  // ESP8266 only      — %
	MAX_VALUE,
};

bool system_metric_is_supported(SystemMetric metric);
SensorUnit system_metric_native_unit(SystemMetric metric);

class SystemInternalSensor : public Sensor {
	public:
	SystemInternalSensor(uint32_t interval, float min, float max, const char *name, SensorUnit unit, uint8_t flag,
	                     SystemMetric metric);
	SystemInternalSensor(char *buf, uint32_t len);

	void emit_extra_json(BufferFiller *bfill);
	static void emit_description_json(BufferFiller *bfill);

	SensorType get_sensor_type() {
		return SensorType::SystemInternal;
	}

	SystemMetric metric;

	private:
	float _get_raw_value();
	uint32_t _serialize_internal(char *buf);
};
