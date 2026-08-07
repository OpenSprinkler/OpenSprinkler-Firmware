#include "weather_sensor.h"

WeatherSensor::WeatherSensor(uint32_t interval, float min, float max, const char* name, SensorUnit unit, uint8_t flag, WeatherGetter weather_getter, WeatherAction action) :
	Sensor(interval, min, max, name, unit, flag),
	action(action),
	weather_getter(weather_getter) {
}

void WeatherSensor::emit_extra_json(BufferFiller* bfill) {
	bfill->emit_p(PSTR("{\"action\":$D}"), this->action);
}

void WeatherSensor::emit_description_json(BufferFiller* bfill) {
	bfill->emit_p(PSTR(
		"{\"n\":\"Weather Sensor\","
		"\"as\":["
			"{\"n\":\"Weather Information\","
			 "\"a\":\"action\","
			 "\"t\":\"enum\","
			 "\"d\":\"0\","
			 "\"o\":["
	));
	bfill->emit_p(PSTR("{\"id\":0,\"l\":\"Current Temperature\",\"dfl\":{\"interval\":\"360\",\"unit\":$D,\"min\":\"-40\",\"max\":\"185\"},\"ug\":$D}"),
		static_cast<uint8_t>(SensorUnit::Fahrenheit), static_cast<uint8_t>(SensorUnitGroup::Temperature));
	bfill->emit_p(PSTR(",{\"id\":1,\"l\":\"Current Humidity\",\"dfl\":{\"interval\":\"360\",\"unit\":$D,\"min\":\"0\",\"max\":\"100\"},\"lk\":[\"unit\"]}"),
		static_cast<uint8_t>(SensorUnit::Percent));
	bfill->emit_p(PSTR(",{\"id\":2,\"l\":\"Current Wind Speed\",\"dfl\":{\"interval\":\"360\",\"unit\":$D,\"min\":\"0\",\"max\":\"200\"},\"ug\":$D}"),
		static_cast<uint8_t>(SensorUnit::MilesPerHour), static_cast<uint8_t>(SensorUnitGroup::Velocity));
	bfill->emit_p(PSTR(",{\"id\":3,\"l\":\"Currently Raining\",\"dfl\":{\"interval\":\"360\",\"unit\":$D,\"min\":\"0\",\"max\":\"1\"},\"lk\":[\"unit\"]}"),
		static_cast<uint8_t>(SensorUnit::None));
	bfill->emit_p(PSTR(",{\"id\":4,\"l\":\"Today's Low Temperature\",\"dfl\":{\"interval\":\"360\",\"unit\":$D,\"min\":\"-40\",\"max\":\"185\"},\"ug\":$D}"),
		static_cast<uint8_t>(SensorUnit::Fahrenheit), static_cast<uint8_t>(SensorUnitGroup::Temperature));
	bfill->emit_p(PSTR(",{\"id\":5,\"l\":\"Today's High Temperature\",\"dfl\":{\"interval\":\"360\",\"unit\":$D,\"min\":\"-40\",\"max\":\"185\"},\"ug\":$D}"),
		static_cast<uint8_t>(SensorUnit::Fahrenheit), static_cast<uint8_t>(SensorUnitGroup::Temperature));
	bfill->emit_p(PSTR(",{\"id\":6,\"l\":\"Today's Precipitation\",\"dfl\":{\"interval\":\"360\",\"unit\":$D,\"min\":\"0\",\"max\":\"20\"},\"ug\":$D}"),
		static_cast<uint8_t>(SensorUnit::Inch), static_cast<uint8_t>(SensorUnitGroup::Length));
	bfill->emit_p(PSTR(",{\"id\":7,\"l\":\"Previous Day Temperature\",\"dfl\":{\"interval\":\"1440\",\"unit\":$D,\"min\":\"-40\",\"max\":\"185\"},\"ug\":$D}"),
		static_cast<uint8_t>(SensorUnit::Fahrenheit), static_cast<uint8_t>(SensorUnitGroup::Temperature));
	bfill->emit_p(PSTR(",{\"id\":8,\"l\":\"Previous Day Humidity\",\"dfl\":{\"interval\":\"1440\",\"unit\":$D,\"min\":\"0\",\"max\":\"100\"},\"lk\":[\"unit\"]}"),
		static_cast<uint8_t>(SensorUnit::Percent));
	bfill->emit_p(PSTR(",{\"id\":9,\"l\":\"Previous Day Precipitation\",\"dfl\":{\"interval\":\"1440\",\"unit\":$D,\"min\":\"0\",\"max\":\"20\"},\"ug\":$D}"),
		static_cast<uint8_t>(SensorUnit::Inch), static_cast<uint8_t>(SensorUnitGroup::Length));
	bfill->emit_p(PSTR(",{\"id\":10,\"l\":\"Previous Day Wind Speed\",\"dfl\":{\"interval\":\"1440\",\"unit\":$D,\"min\":\"0\",\"max\":\"200\"},\"ug\":$D}"),
		static_cast<uint8_t>(SensorUnit::MilesPerHour), static_cast<uint8_t>(SensorUnitGroup::Velocity));
	bfill->emit_p(PSTR(",{\"id\":11,\"l\":\"Previous Day Solar Radiation\",\"dfl\":{\"interval\":\"1440\",\"unit\":$D,\"min\":\"0\",\"max\":\"20\"},\"lk\":[\"unit\"]}"),
		static_cast<uint8_t>(SensorUnit::KilowattHoursPerSquareMeterPerDay));
	bfill->emit_p(PSTR(",{\"id\":12,\"l\":\"Previous Day ETo\",\"dfl\":{\"interval\":\"1440\",\"unit\":$D,\"min\":\"0\",\"max\":\"2\"},\"ug\":$D}"),
		static_cast<uint8_t>(SensorUnit::InchesPerDay), static_cast<uint8_t>(SensorUnitGroup::Precipitation));
	bfill->emit_p(PSTR("]}]}"));
}

SensorUnit weather_action_native_unit(WeatherAction action) {
	switch (action) {
		case WeatherAction::CurrentTemperature:
		case WeatherAction::ForecastLowTemperature:
		case WeatherAction::ForecastHighTemperature:
		case WeatherAction::HistoricalTemperature:
			return SensorUnit::Fahrenheit;
		case WeatherAction::CurrentHumidity:
		case WeatherAction::HistoricalHumidity:
			return SensorUnit::Percent;
		case WeatherAction::CurrentWindSpeed:
		case WeatherAction::HistoricalWindSpeed:
			return SensorUnit::MilesPerHour;
		case WeatherAction::CurrentRaining:
			return SensorUnit::None;
		case WeatherAction::ForecastPrecipitation:
		case WeatherAction::HistoricalPrecipitation:
			return SensorUnit::Inch;
		case WeatherAction::HistoricalSolarRadiation:
			return SensorUnit::KilowattHoursPerSquareMeterPerDay;
		case WeatherAction::HistoricalETo:
			return SensorUnit::InchesPerDay;
		case WeatherAction::MAX_VALUE:
			return SensorUnit::None;
	}
	return SensorUnit::None;
}

bool weather_action_unit_is_valid(WeatherAction action, SensorUnit unit) {
	SensorUnit native = weather_action_native_unit(action);
	if (action == WeatherAction::CurrentRaining ||
		action == WeatherAction::CurrentHumidity ||
		action == WeatherAction::HistoricalHumidity ||
		action == WeatherAction::HistoricalSolarRadiation) {
		return unit == native;
	}
	return get_sensor_unit_group(unit) == get_sensor_unit_group(native);
}

void weather_action_defaults(WeatherAction action, uint32_t *interval, float *min, float *max, SensorUnit *unit) {
	*interval = action >= WeatherAction::HistoricalTemperature ? 1440 : 360;
	*unit = weather_action_native_unit(action);
	*min = 0;
	*max = 100;

	switch (action) {
		case WeatherAction::CurrentTemperature:
		case WeatherAction::ForecastLowTemperature:
		case WeatherAction::ForecastHighTemperature:
		case WeatherAction::HistoricalTemperature:
			*min = -40;
			*max = 185;
			break;
		case WeatherAction::CurrentWindSpeed:
		case WeatherAction::HistoricalWindSpeed:
			*max = 200;
			break;
		case WeatherAction::CurrentRaining:
			*max = 1;
			break;
		case WeatherAction::ForecastPrecipitation:
		case WeatherAction::HistoricalPrecipitation:
		case WeatherAction::HistoricalSolarRadiation:
			*max = 20;
			break;
		case WeatherAction::HistoricalETo:
			*max = 2;
			break;
		default:
			break;
	}
}

float WeatherSensor::_get_raw_value() {
	if (this->action >= WeatherAction::MAX_VALUE) return NAN;
	float raw = this->weather_getter(this->action);
	if (isnan(raw)) return raw;
	SensorUnit native = weather_action_native_unit(this->action);
	return native == SensorUnit::None ? raw : convert_unit(raw, native, this->unit);
}

uint32_t WeatherSensor::_serialize_internal(char* buf) {
	uint32_t i = 0;
	buf[i++] = static_cast<uint8_t>(this->action);
	return i;
}

WeatherSensor::WeatherSensor(WeatherGetter weather_getter, char* buf, uint32_t len) {
	uint8_t subclass_len = 0;
	uint32_t i = Sensor::_deserialize(buf, len, &subclass_len);
	uint32_t end = i + subclass_len;

	this->action = WeatherAction::MAX_VALUE;
	if (i + 1 <= end && static_cast<uint8_t>(buf[i]) < static_cast<uint8_t>(WeatherAction::MAX_VALUE)) {
		this->action = static_cast<WeatherAction>(buf[i]);
	}
	this->weather_getter = weather_getter;
}
