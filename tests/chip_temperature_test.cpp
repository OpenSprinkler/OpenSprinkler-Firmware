#include <cassert>
#include <cmath>
#include "platform/chip_temperature.h"
#include <driver/temperature_sensor.h>

static int installs, enables, uninstalls, reads;
static bool fail_install = true, fail_enable = true, fail_read = false;
static float sample = 62.5f;
static int token;

int temperature_sensor_install(const temperature_sensor_config_t* config, temperature_sensor_handle_t* handle) {
	++installs;
	assert(config->range_min == -10 && config->range_max == 80);
	if (fail_install) return -1;
	*handle = &token;
	return ESP_OK;
}
int temperature_sensor_enable(temperature_sensor_handle_t handle) {
	assert(handle == &token);
	++enables;
	return fail_enable ? -1 : ESP_OK;
}
int temperature_sensor_uninstall(temperature_sensor_handle_t handle) {
	assert(handle == &token);
	++uninstalls;
	return ESP_OK;
}
int temperature_sensor_get_celsius(temperature_sensor_handle_t handle, float* value) {
	assert(handle == &token);
	++reads;
	*value = sample;
	return fail_read ? -1 : ESP_OK;
}
int main() {
	assert(std::isnan(chip_temperature_celsius()));
	assert(installs == 1 && enables == 0);
	fail_install = false;
	assert(std::isnan(chip_temperature_celsius()));
	assert(installs == 2 && enables == 1 && uninstalls == 1);
	fail_enable = false;
	assert(chip_temperature_celsius() == sample);
	assert(installs == 3 && enables == 2 && reads == 1);
	fail_read = true;
	assert(std::isnan(chip_temperature_celsius()));
	fail_read = false;
	sample = INFINITY;
	assert(std::isnan(chip_temperature_celsius()));
	sample = NAN;
	assert(std::isnan(chip_temperature_celsius()));
	sample = 35;
	assert(chip_temperature_celsius() == sample);
	assert(installs == 3 && enables == 2 && uninstalls == 1 && reads == 5);
}
