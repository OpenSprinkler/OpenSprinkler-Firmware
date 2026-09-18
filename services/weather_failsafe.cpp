#include "services/weather_failsafe.h"

#include "defines.h"

WeatherResponseFreshness::WeatherResponseFreshness() : success_ms(0), valid(false) {
}

void WeatherResponseFreshness::mark_success(uint32_t now_ms) {
	success_ms = now_ms;
	valid = true;
}

void WeatherResponseFreshness::reset() {
	success_ms = 0;
	valid = false;
}

bool WeatherResponseFreshness::is_current(uint32_t now_ms) {
	if (!valid) return false;
	if (static_cast<uint32_t>(now_ms - success_ms) > WEATHER_RESPONSE_MAX_AGE_MS) {
		valid = false;
	}
	return valid;
}

bool WeatherStaleActions::any() const {
	return reset_water_percent || clear_restriction || clear_raw_data ||
		clear_multi_day || set_no_response_error;
}

uint8_t weather_policy_water_percent(uint8_t stored_water_percent,
	bool uses_remote_scale, bool response_current) {
	return uses_remote_scale && !response_current ? 100 : stored_water_percent;
}

bool weather_method_uses_remote_scale(uint8_t method) {
	return method == WEATHER_METHOD_ZIMMERMAN || method == WEATHER_METHOD_ETO;
}

WeatherStaleActions weather_stale_actions(bool uses_remote_scale,
	uint8_t stored_water_percent, uint8_t restricted, bool has_raw_data,
	uint8_t multi_day_count, int error_code) {
	WeatherStaleActions actions;
	actions.reset_water_percent = uses_remote_scale && stored_water_percent != 100;
	actions.clear_restriction = restricted != 0;
	actions.clear_raw_data = has_raw_data;
	actions.clear_multi_day = multi_day_count != 0;
	actions.set_no_response_error = error_code == 0;
	return actions;
}
