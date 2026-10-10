#pragma once

using temperature_sensor_handle_t = void*;
struct temperature_sensor_config_t { int range_min; int range_max; };
#define TEMPERATURE_SENSOR_CONFIG_DEFAULT(low, high) { low, high }
constexpr int ESP_OK = 0;
int temperature_sensor_install(const temperature_sensor_config_t*, temperature_sensor_handle_t*);
int temperature_sensor_enable(temperature_sensor_handle_t);
int temperature_sensor_uninstall(temperature_sensor_handle_t);
int temperature_sensor_get_celsius(temperature_sensor_handle_t, float*);
