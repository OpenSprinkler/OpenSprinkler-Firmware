#include "platform/chip_temperature.h"

#include <cmath>

#if defined(ESP32)
#include <driver/temperature_sensor.h>
#elif !defined(ARDUINO)
#include <cstdio>
#endif

float chip_temperature_celsius() {
#if defined(ESP32)
	static temperature_sensor_handle_t handle = nullptr;
	if (!handle) {
		temperature_sensor_handle_t candidate = nullptr;
		temperature_sensor_config_t config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(
			CHIP_TEMPERATURE_MIN_C, CHIP_TEMPERATURE_MAX_C);
		if (temperature_sensor_install(&config, &candidate) != ESP_OK) return NAN;
		if (temperature_sensor_enable(candidate) != ESP_OK) {
			temperature_sensor_uninstall(candidate);
			return NAN;
		}
		handle = candidate;
	}
	float value = NAN;
	if (temperature_sensor_get_celsius(handle, &value) != ESP_OK) return NAN;
	return std::isfinite(value) ? value : NAN;
#elif !defined(ARDUINO)
	// Available on Raspberry Pi and some Linux hosts, but not every container.
	FILE* file = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
	if (!file) return NAN;
	int millidegrees = 0;
	const int count = fscanf(file, "%d", &millidegrees);
	fclose(file);
	return count == 1 ? millidegrees / 1000.0f : NAN;
#else
	return NAN;
#endif
}
