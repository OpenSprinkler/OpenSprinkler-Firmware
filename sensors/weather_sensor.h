#pragma once

#include "sensor.h"

class WeatherSensor : public Sensor {
	public:
	WeatherSensor(uint32_t interval, float min, float max, const char *name, SensorUnit unit, uint8_t flag, WeatherGetter weather_getter, WeatherAction action);
	WeatherSensor(WeatherGetter weather_getter, char *buf, uint32_t len);

	void emit_extra_json(BufferFiller *bfill);
	static void emit_description_json(BufferFiller *bfill);

	SensorType get_sensor_type() {
		return SensorType::Weather;
	}

	WeatherAction action;

	private:
	float _get_raw_value();
	uint32_t _serialize_internal(char *buf);

	WeatherGetter weather_getter;
};

SensorUnit weather_action_native_unit(WeatherAction action);
bool weather_action_unit_is_valid(WeatherAction action, SensorUnit unit);
void weather_action_defaults(WeatherAction action, uint32_t *interval, float *min, float *max, SensorUnit *unit);
