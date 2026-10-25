#include "system_internal_sensor.h"
#include "platform/chip_temperature.h"

#include <cmath>

#if defined(ARDUINO)
	#if defined(ESP8266)
	#include <ESP8266WiFi.h>
	#else
	#include <WiFi.h>
	#endif
#include <LittleFS.h>
extern bool useEth;   // declared in OpenSprinkler.h; for the RSSI guard
#endif

namespace {

struct MetricDescription {
	SystemMetric id;
	const char* label;
	SensorUnit unit;
	int min;
	int max;
	bool supported;
};

#if defined(ARDUINO)
constexpr bool embedded = true;
#else
constexpr bool embedded = false;
#endif

const MetricDescription* metric_description(SystemMetric metric) {
	static const MetricDescription descriptions[] = {
		{SystemMetric::FREE_HEAP, PSTR("Free Heap"), SensorUnit::Kilobyte, 0,
#if defined(ESP32)
			512,
#else
			50,
#endif
			embedded},
		{SystemMetric::FREE_FLASH, PSTR("Free Flash"), SensorUnit::Kilobyte, 0, 2000, embedded},
		// dBm is not yet a SensorUnit; retain the existing unitless RSSI contract.
		{SystemMetric::WIFI_RSSI, PSTR("WiFi RSSI"), SensorUnit::None, -100, -30, embedded},
#if defined(ESP32)
		{SystemMetric::CPU_TEMPERATURE, PSTR("Chip Temperature"), SensorUnit::Celsius,
			CHIP_TEMPERATURE_MIN_C, CHIP_TEMPERATURE_MAX_C, true},
#else
		{SystemMetric::CPU_TEMPERATURE, PSTR("CPU Temperature"), SensorUnit::Celsius, -40, 185, !embedded},
#endif
		{SystemMetric::HEAP_FRAGMENTATION, PSTR("Heap Fragmentation"), SensorUnit::Percent, 0, 100,
#if defined(ESP8266)
			true},
#else
			false},
#endif
	};
	static_assert(sizeof(descriptions) / sizeof(descriptions[0]) == static_cast<uint8_t>(SystemMetric::MAX_VALUE),
		"Every system metric needs a description");
	for (const auto& description : descriptions) {
		if (description.id == metric) return &description;
	}
	return nullptr;
}

} // namespace

bool system_metric_is_supported(SystemMetric metric) {
	const auto* description = metric_description(metric);
	return description && description->supported;
}

SensorUnit system_metric_native_unit(SystemMetric metric) {
	const auto* description = metric_description(metric);
	return description ? description->unit : SensorUnit::None;
}

SystemInternalSensor::SystemInternalSensor(uint32_t interval, float min, float max, const char* name, SensorUnit unit, uint8_t flag,
                                            SystemMetric metric) :
	Sensor(interval, min, max, name, unit, flag),
	metric(metric) {
}

void SystemInternalSensor::emit_extra_json(BufferFiller *bfill) {
	bfill->emit_p(PSTR("{\"metric\":$D}"), static_cast<uint8_t>(this->metric));
}

void SystemInternalSensor::emit_description_json(BufferFiller* bfill) {
	bfill->emit_p(PSTR(
		"{\"n\":\"System Internal\","
		"\"as\":["
			"{\"n\":\"Metric\","
			 "\"a\":\"metric\","
			 "\"t\":\"enum\","
#if defined(ARDUINO)
			 "\"d\":\"0\","
#else
			 "\"d\":\"3\","
#endif
			 "\"o\":["
	));

	bool comma = false;
	for (uint8_t id = 0; id < static_cast<uint8_t>(SystemMetric::MAX_VALUE); id++) {
		const auto* description = metric_description(static_cast<SystemMetric>(id));
		if (!description || !description->supported) continue;
		if (comma) bfill->emit_p(PSTR(","));
		comma = true;
		bfill->emit_p(PSTR("{\"id\":$D,\"l\":\"$F\",\"dfl\":{\"unit\":$D,\"min\":\"$D\",\"max\":\"$D\"}"),
			id, description->label, static_cast<uint8_t>(description->unit), description->min, description->max);
		if (description->unit == SensorUnit::Celsius) {
			bfill->emit_p(PSTR(",\"ug\":$D}"), static_cast<uint8_t>(SensorUnitGroup::Temperature));
		} else {
			bfill->emit_p(PSTR(",\"lk\":[\"unit\"]}"));
		}
	}

	bfill->emit_p(PSTR("]}"
		"]}"
	));
}


float SystemInternalSensor::_get_raw_value() {
	float raw = NAN;

#if defined(ARDUINO)
	switch (this->metric) {
		case SystemMetric::FREE_HEAP:
			raw = (float)ESP.getFreeHeap() / 1024.0f;                        // bytes -> KB
			raw = roundf(raw * 100.0f) / 100.0f;                             // 2 decimal places
			break;
		case SystemMetric::FREE_FLASH: {
		#if defined(ESP8266)
			FSInfo info;
			if (LittleFS.info(info)) {
				raw = (float)(info.totalBytes - info.usedBytes) / 1024.0f;   // bytes -> KB
				raw = roundf(raw * 100.0f) / 100.0f;                         // 2 decimal places
			}
		#else
			raw = (float)(LittleFS.totalBytes() - LittleFS.usedBytes()) / 1024.0f;
			raw = roundf(raw * 100.0f) / 100.0f;
		#endif
			break;
		}
		case SystemMetric::WIFI_RSSI:
			// RSSI is only meaningful when the WiFi station is connected.
			// Disconnected / Ethernet-mode → return NAN so the sensor surfaces
			// as STATUS_ERROR rather than reporting a clamped sentinel value.
			if (!useEth && WiFi.status() == WL_CONNECTED) {
				raw = (float)WiFi.RSSI();
			}
			break;
		case SystemMetric::HEAP_FRAGMENTATION:
		#if defined(ESP8266)
			raw = (float)ESP.getHeapFragmentation();
		#endif
			break;
		case SystemMetric::CPU_TEMPERATURE:
		#if defined(ESP32)
			raw = chip_temperature_celsius();
		#endif
			break;
		default:
			break;
	}
#else
	switch (this->metric) {
		case SystemMetric::CPU_TEMPERATURE:
			raw = chip_temperature_celsius();
			break;
		default:
			break;
	}
#endif

	if (isnan(raw)) return NAN;

	// Convert from native to user's chosen unit (currently only Temperature has
	// meaningful conversions; other metrics' native is None and skip this path).
	SensorUnit native = system_metric_native_unit(this->metric);
	if (native != SensorUnit::None) {
		raw = convert_unit(raw, native, this->unit);
	}
	return raw;
}

uint32_t SystemInternalSensor::_serialize_internal(char *buf) {
	uint32_t i = 0;
	buf[i++] = static_cast<uint8_t>(this->metric);
	return i;
}

SystemInternalSensor::SystemInternalSensor(char *buf, uint32_t len) {
	uint8_t subclass_len = 0;
	uint32_t i = Sensor::_deserialize(buf, len, &subclass_len);
	uint32_t end = i + subclass_len;

	this->metric = SystemMetric::MAX_VALUE;   // sentinel: "unknown / unavailable on this platform"
	if (i + 1 <= end) {
		uint8_t m = static_cast<uint8_t>(buf[i]);
		if (m < static_cast<uint8_t>(SystemMetric::MAX_VALUE)) {
			this->metric = static_cast<SystemMetric>(m);
		}
	}
}
