#include "system_internal_sensor.h"

#include <cmath>

#if defined(ARDUINO)
	#if defined(ESP8266)
	#include <ESP8266WiFi.h>
	#else
	#include <WiFi.h>
	#endif
#include <LittleFS.h>
extern bool useEth;   // declared in OpenSprinkler.h; for the RSSI guard
#else
#include <cstdio>
#endif

bool system_metric_is_supported(SystemMetric metric) {
	switch (metric) {
	#if defined(ARDUINO)
			case SystemMetric::FREE_HEAP:
			case SystemMetric::FREE_FLASH:
			case SystemMetric::WIFI_RSSI:
			#if defined(ESP8266)
			case SystemMetric::HEAP_FRAGMENTATION:
			#endif
			return true;
#else
		case SystemMetric::CPU_TEMPERATURE:
			return true;
#endif
		default:
			return false;
	}
}

SensorUnit system_metric_native_unit(SystemMetric metric) {
	switch (metric) {
		case SystemMetric::CPU_TEMPERATURE:
			return SensorUnit::Celsius;
		case SystemMetric::FREE_HEAP:
		case SystemMetric::FREE_FLASH:
			return SensorUnit::Kilobyte;
		case SystemMetric::HEAP_FRAGMENTATION:
			return SensorUnit::Percent;
		// WIFI_RSSI is in dBm — no SensorUnit fits, reported as unitless.
		default:
			return SensorUnit::None;
	}
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

#if defined(ARDUINO)
	bfill->emit_p(PSTR("{\"id\":0,\"l\":\"Free Heap\","
	                    "\"dfl\":{\"unit\":$D,\"max\":\"50\"},"
	                    "\"lk\":[\"unit\"]}"),
	              static_cast<uint8_t>(SensorUnit::Kilobyte));
	bfill->emit_p(PSTR(",{\"id\":1,\"l\":\"Free Flash\","
	                    "\"dfl\":{\"unit\":$D,\"max\":\"2000\"},"
	                    "\"lk\":[\"unit\"]}"),
	              static_cast<uint8_t>(SensorUnit::Kilobyte));
	bfill->emit_p(PSTR(",{\"id\":2,\"l\":\"WiFi RSSI\","
	                    "\"dfl\":{\"unit\":$D,\"min\":\"-100\",\"max\":\"-30\"},"
	                    "\"lk\":[\"unit\"]}"),
	              static_cast<uint8_t>(SensorUnit::None));
	#if defined(ESP8266)
	bfill->emit_p(PSTR(",{\"id\":4,\"l\":\"Heap Fragmentation\","
	                    "\"dfl\":{\"unit\":$D,\"max\":\"100\"},"
	                    "\"lk\":[\"unit\"]}"),
	              static_cast<uint8_t>(SensorUnit::Percent));
	#endif
#else
	bfill->emit_p(PSTR("{\"id\":3,\"l\":\"CPU Temperature\","
	                    "\"dfl\":{\"unit\":$D,\"min\":\"-40\",\"max\":\"185\"},"
	                    "\"ug\":$D}"),
	              static_cast<uint8_t>(SensorUnit::Celsius),
	              static_cast<uint8_t>(SensorUnitGroup::Temperature));
#endif

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
		default:
			break;
	}
#else
	switch (this->metric) {
		case SystemMetric::CPU_TEMPERATURE: {
			// /sys/class/thermal/thermal_zone0/temp returns millidegrees Celsius.
			// Path exists on Pi and most x86 Linux; absent in some containers/VMs.
			FILE *f = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
			if (!f) break;
			int millidegrees = 0;
			int n = fscanf(f, "%d", &millidegrees);
			fclose(f);
			if (n == 1) raw = millidegrees / 1000.0f;
			break;
		}
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
