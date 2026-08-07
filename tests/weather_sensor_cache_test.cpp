#include <cassert>
#include <cmath>
#include <cstdint>

#include "services/weather.h"
#include "sensors/sensor.h"

static uint32_t fake_millis = 1000;

uint32_t millis() {
	return fake_millis;
}

int main() {
	weather_sensor_reset_cache();
	const uint8_t current_forecast =
		WEATHER_SENSOR_GROUP_CURRENT | WEATHER_SENSOR_GROUP_FORECAST;
	assert(weather_sensor_parse_response(
		"{\"v\":1,\"u\":\"us\","
		"\"c\":{\"at\":100,\"t\":0,\"h\":0,\"r\":0},"
		"\"f\":{\"at\":100,\"lo\":32,\"hi\":50}}",
		current_forecast));
	assert(weather_sensor_get_value(WeatherAction::CurrentTemperature) == 0);
	assert(weather_sensor_get_value(WeatherAction::CurrentHumidity) == 0);
	assert(weather_sensor_get_value(WeatherAction::CurrentRaining) == 0);
	assert(std::isnan(weather_sensor_get_value(WeatherAction::CurrentWindSpeed)));
	assert(std::isnan(weather_sensor_get_value(WeatherAction::ForecastPrecipitation)));
	assert(!weather_sensor_parse_response(
		"{\"v\":1,\"u\":\"us\",\"c\":{\"t\":70}}",
		WEATHER_SENSOR_GROUP_CURRENT,
		1U << static_cast<uint8_t>(WeatherAction::CurrentHumidity)));

	assert(!weather_sensor_parse_response(
		"{\"v\":1,\"u\":\"metric\",\"c\":{\"t\":70}}",
		WEATHER_SENSOR_GROUP_CURRENT));

	fake_millis += WEATHER_SENSOR_STALE_MS + 1;
	assert(std::isnan(weather_sensor_get_value(WeatherAction::CurrentTemperature)));
	return 0;
}
