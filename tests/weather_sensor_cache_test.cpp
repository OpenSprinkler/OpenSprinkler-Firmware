#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>

#include "services/weather.h"
#include "sensors/sensor.h"

static uint32_t fake_millis = 1000;
static std::string test_location;
static std::string test_weather_options;
static std::string test_weather_url;

uint32_t millis() {
	return fake_millis;
}

static void load_test_option(uint8_t oid, char *buffer, uint16_t maxlen) {
	const std::string *value = &test_weather_url;
	if (oid == SOPT_LOCATION) value = &test_location;
	if (oid == SOPT_WEATHER_OPTS) value = &test_weather_options;
	size_t length = value->size();
	if (length > maxlen) length = maxlen;
	memcpy(buffer, value->data(), length);
	buffer[length] = 0;
}

static size_t build_test_request(char *output, size_t output_size,
	char *scratch, size_t scratch_size, const char *endpoint,
	const char *extra_query, WeatherHttpTarget *target) {
	bool result = weather_build_http_request(output, output_size,
		scratch, scratch_size, endpoint, extra_query, "OpenSprinkler/test",
		load_test_option, target);
	return result ? strlen(output) : 0;
}

int main() {
	// Exercise the exact request composer used by both weather code paths.
	test_location = "New York, NY";
	test_weather_options = "\"provider\":\"Apple\"";
	test_weather_url = "http://weather.example:8080";
	char request[512];
	char scratch[TMP_BUFFER_ALLOC_SIZE];
	WeatherHttpTarget target;
	size_t request_length = build_test_request(request, sizeof(request), scratch,
		sizeof(scratch), "weatherSensorData", "&scope=cfh", &target);
	assert(request_length > 0);
	assert(strncmp(request, "GET /weatherSensorData?", 23) == 0);
	assert(strstr(request, "loc=New%20York,%20NY") != nullptr);
	assert(strstr(request, "wto=%22provider%22:%22Apple%22") != nullptr);
	assert(strstr(request, "&scope=cfh HTTP/1.0\r\n") != nullptr);
	assert(strstr(request, "GET%20") == nullptr);
	assert(strstr(request, "%20HTTP") == nullptr);
	assert(strcmp(target.host, "weather.example") == 0);
	assert(target.port == 8080);
	assert(!target.use_ssl);

	test_location = "-90.00000,-180.00000";
	test_weather_options.clear();
	test_weather_url = "https://weather.opensprinkler.com";
	request_length = build_test_request(request, sizeof(request), scratch,
		sizeof(scratch), "0", "&fwv=221", &target);
	assert(request_length > 0);
	const char *legacy_prefix = "GET /0?loc=-90.00000,-180.00000&wto=&fwv=221 HTTP/1.0\r\n";
	assert(strncmp(request, legacy_prefix, strlen(legacy_prefix)) == 0);
	assert(strcmp(target.host, "weather.opensprinkler.com") == 0);
	assert(target.port == 443);
	assert(target.use_ssl);

	// Exact capacity succeeds; one byte less fails without touching guard bytes.
	char full_request[2048];
	request_length = build_test_request(full_request, sizeof(full_request), scratch,
		sizeof(scratch), "weatherSensorData", "&scope=cfh", &target);
	assert(request_length > 0);
	char guarded[2050];
	memset(guarded, 0x5A, sizeof(guarded));
	assert(weather_build_http_request(guarded + 1, request_length + 1,
		scratch, sizeof(scratch), "weatherSensorData", "&scope=cfh",
		"OpenSprinkler/test", load_test_option, &target));
	assert(static_cast<unsigned char>(guarded[0]) == 0x5A);
	assert(static_cast<unsigned char>(guarded[request_length + 2]) == 0x5A);
	memset(guarded, 0x5A, sizeof(guarded));
	assert(!weather_build_http_request(guarded + 1, request_length,
		scratch, sizeof(scratch), "weatherSensorData", "&scope=cfh",
		"OpenSprinkler/test", load_test_option, &target));
	assert(static_cast<unsigned char>(guarded[0]) == 0x5A);
	assert(static_cast<unsigned char>(guarded[request_length + 1]) == 0x5A);

	// A maximally escaped option must fail cleanly when the request cannot fit.
	test_location.assign(MAX_SOPTS_SIZE - 1, ' ');
	test_weather_options.assign(MAX_SOPTS_SIZE - 1, '"');
	memset(guarded, 0x5A, sizeof(guarded));
	assert(!weather_build_http_request(guarded + 1, 512,
		scratch, sizeof(scratch), "weatherSensorData", "&scope=cfh",
		"OpenSprinkler/test", load_test_option, &target));
	assert(static_cast<unsigned char>(guarded[0]) == 0x5A);
	assert(static_cast<unsigned char>(guarded[513]) == 0x5A);

	weather_sensor_reset_cache();
	test_location.clear();
	test_weather_options.clear();
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

	assert(weather_sensor_expire_groups() == 0);
	fake_millis += WEATHER_SENSOR_STALE_MS + 1;
	assert(std::isnan(weather_sensor_get_value(WeatherAction::CurrentTemperature)));
	assert(weather_sensor_expire_groups() == current_forecast);
	assert(weather_sensor_expire_groups() == 0);

	// A refreshed group clears only its own stale state.
	assert(weather_sensor_parse_response(
		"{\"v\":1,\"u\":\"us\",\"c\":{\"t\":72}}",
		WEATHER_SENSOR_GROUP_CURRENT));
	assert(weather_sensor_expire_groups() == 0);
	fake_millis += WEATHER_SENSOR_STALE_MS + 1;
	assert(weather_sensor_expire_groups() == WEATHER_SENSOR_GROUP_CURRENT);
	return 0;
}
