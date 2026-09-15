#include <cassert>
#include <cstdint>
#include <limits>

#include "defines.h"
#include "services/weather_failsafe.h"

int main() {
	WeatherResponseFreshness freshness;
	assert(!freshness.is_current(0));

	freshness.mark_success(1000);
	assert(freshness.is_current(1000));
	assert(freshness.is_current(1000 + WEATHER_RESPONSE_MAX_AGE_MS));
	assert(!freshness.is_current(1000 + WEATHER_RESPONSE_MAX_AGE_MS + 1));
	assert(!freshness.is_current(500));

	// A success close to millis() wrap remains current after the wrap.
	freshness.mark_success(std::numeric_limits<uint32_t>::max() - 1000);
	assert(freshness.is_current(500));
	freshness.reset();
	assert(!freshness.is_current(500));

	assert(weather_policy_water_percent(42, false, false) == 42);
	assert(weather_policy_water_percent(42, true, true) == 42);
	assert(weather_policy_water_percent(42, true, false) == 100);
	assert(!weather_method_uses_remote_scale(WEATHER_METHOD_MANUAL));
	assert(weather_method_uses_remote_scale(WEATHER_METHOD_ZIMMERMAN));
	assert(!weather_method_uses_remote_scale(WEATHER_METHOD_AUTORAINDELAY));
	assert(weather_method_uses_remote_scale(WEATHER_METHOD_ETO));
	assert(!weather_method_uses_remote_scale(WEATHER_METHOD_MONTHLY));

	WeatherStaleActions actions = weather_stale_actions(true, 42, 1, true, 3, 0);
	assert(actions.any());
	assert(actions.reset_water_percent);
	assert(actions.clear_restriction);
	assert(actions.clear_raw_data);
	assert(actions.clear_multi_day);
	assert(actions.set_no_response_error);

	// Restrictions expire even for methods whose water percentage is local.
	actions = weather_stale_actions(false, 42, 1, false, 0, -2);
	assert(actions.any());
	assert(!actions.reset_water_percent);
	assert(actions.clear_restriction);
	assert(!actions.set_no_response_error);

	// A useful weather error survives every convergence pass.
	actions = weather_stale_actions(true, 100, 0, false, 0, 10);
	assert(!actions.any());
	actions = weather_stale_actions(true, 100, 0, false, 0, 10);
	assert(!actions.any());
	return 0;
}
