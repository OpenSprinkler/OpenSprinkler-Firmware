#pragma once

#include <stdint.h>

#if defined(WEATHER_RESPONSE_TEST_MAX_AGE_MS)
static const uint32_t WEATHER_RESPONSE_MAX_AGE_MS = WEATHER_RESPONSE_TEST_MAX_AGE_MS;
#else
static const uint32_t WEATHER_RESPONSE_MAX_AGE_MS = 24UL * 60UL * 60UL * 1000UL;
#endif

class WeatherResponseFreshness {
public:
	WeatherResponseFreshness();

	void mark_success(uint32_t now_ms);
	void reset();
	bool is_current(uint32_t now_ms);

private:
	uint32_t success_ms;
	bool valid;
};

struct WeatherStaleActions {
	bool reset_water_percent;
	bool clear_restriction;
	bool clear_raw_data;
	bool clear_multi_day;
	bool set_no_response_error;

	bool any() const;
};

uint8_t weather_policy_water_percent(uint8_t stored_water_percent,
	bool uses_remote_scale, bool response_current);
bool weather_method_uses_remote_scale(uint8_t method);
WeatherStaleActions weather_stale_actions(bool uses_remote_scale,
	uint8_t stored_water_percent, uint8_t restricted, bool has_raw_data,
	uint8_t multi_day_count, int error_code);
