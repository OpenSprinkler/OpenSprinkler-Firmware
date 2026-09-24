#include <cassert>
#include <cmath>

#include "sensors/sensor.h"
#include "sensors/weather_sensor.h"

static bool nearly_equal(float actual, float expected, float tolerance = 0.001f) {
	return std::fabs(actual - expected) <= tolerance;
}

int main() {
	assert(nearly_equal(convert_unit(32.0f, SensorUnit::Fahrenheit, SensorUnit::Celsius), 0.0f));
	assert(nearly_equal(convert_unit(1.0f, SensorUnit::Inch, SensorUnit::Millimeter), 25.4f));
	assert(nearly_equal(convert_unit(1.0f, SensorUnit::MilesPerHour, SensorUnit::MetersPerSecond), 0.44704f));
	assert(nearly_equal(convert_unit(1.0f, SensorUnit::InchesPerDay, SensorUnit::MillimetersPerDay), 25.4f));

	assert(weather_action_native_unit(WeatherAction::CurrentTemperature) == SensorUnit::Fahrenheit);
	assert(weather_action_native_unit(WeatherAction::ForecastPrecipitation) == SensorUnit::Inch);
	assert(weather_action_native_unit(WeatherAction::HistoricalETo) == SensorUnit::InchesPerDay);
	assert(weather_action_unit_is_valid(WeatherAction::CurrentTemperature, SensorUnit::Celsius));
	assert(!weather_action_unit_is_valid(WeatherAction::CurrentHumidity, SensorUnit::PartsPerMillion));

	uint32_t interval;
	float min;
	float max;
	SensorUnit unit;
	weather_action_defaults(WeatherAction::HistoricalETo, &interval, &min, &max, &unit);
	assert(interval == 1440);
	assert(nearly_equal(min, 0.0f));
	assert(nearly_equal(max, 2.0f));
	assert(unit == SensorUnit::InchesPerDay);
	return 0;
}
