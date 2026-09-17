#include <assert.h>
#include <stdint.h>

#include "sensors/flow_rate.h"
#include "sensors/flow_rate_window.h"

int main() {
	assert(flow_rate_from_pulse_span(0, 1000) == 0.0f);
	assert(flow_rate_from_pulse_span(1, 1000) == 0.0f);
	assert(flow_rate_from_pulse_span(2, 0) == 0.0f);
	assert(flow_rate_from_pulse_span(2, 1000) == 60.0f);
	assert(flow_rate_from_pulse_span(30, 29000) == 60.0f);
	assert(flow_rate_from_pulse_span(30, 28900) > 60.20f);
	assert(flow_rate_from_pulse_span(30, 28900) < 60.21f);
	assert(flow_rate_from_pulse_span(301, 30000) == 600.0f);
	assert(flow_pulses_since(100, 100) == 0);
	assert(flow_pulses_since(100, 125) == 25);
	assert(flow_pulses_since(UINT32_MAX - 2, 2) == 5);
	assert(flow_volume_from_pulses(50, 100) == 50.0f);
	assert(flow_volume_from_pulses(100000, 65535) == 65535000.0f);

	FlowRateWindow rate;
	uint32_t count = 0;
	for (uint32_t ms = 0; ms <= 10000; ms += 5) {
		if (ms && ms % 100 == 0) count++;
		uint32_t reported = rate.update(ms, count, 1000);
		if (ms >= 5000) assert(reported >= 9800 && reported <= 10200);
	}

	// The estimator handles sparse reads of a cumulative counter; polling cannot
	// capture edges that occur while its loop is blocked.
	assert(rate.update(11000, 110, 1000) >= 9800);
	assert(rate.update(12000, 120, 1000) >= 9800);

	// A stopped sensor must eventually report zero.
	uint32_t reported = 0;
	for (uint32_t ms = 12500; ms <= 18000; ms += 500) {
		reported = rate.update(ms, 120, 1000);
	}
	assert(reported == 0);
	assert(rate.update(19000, 121, 1000) == 0);
	assert(rate.update(20000, 122, 1000) == 1000);

	rate.reset();
	count = 0;
	for (uint32_t ms = 0; ms <= 10000; ms += 5) {
		if (ms && ms % 1000 == 0) count++;
		reported = rate.update(ms, count, 1000);
		if (ms >= 2000) assert(reported >= 990 && reported <= 1010);
	}
	assert(rate.update(21000, count, 1000) == 0);

	rate.reset();
	rate.update(0, 0, 1000);
	assert(rate.update(5000, 1, 1000) == 0);
	assert(rate.update(10000, 2, 1000) == 200);
	assert(rate.update(15000, 3, 1000) == 200);

	rate.reset();
	count = 0;
	for (uint32_t ms = 0; ms <= 10000; ms += 5) {
		if (ms && ms % 10 == 0) count++;
		reported = rate.update(ms, count, 1000);
		if (ms >= 5000) assert(reported >= 99000 && reported <= 101000);
	}

	// Millisecond and pulse counters may wrap independently.
	rate.reset();
	const uint32_t start = UINT32_MAX - 2000;
	const uint32_t wrapped_count = UINT32_MAX - 20;
	rate.update(start, wrapped_count, 1000);
	rate.update(start + 1000, wrapped_count + 10, 1000);
	assert(rate.update(start + 2000, wrapped_count + 20, 1000) == 10000);
	assert(rate.update(start + 5000, wrapped_count + 50, 1000) == 10000);

	rate.reset();
	assert(rate.update(1000, 100, 1000) == 0);
	assert(rate.update(2000, 110, 1000) == 0);
	assert(rate.update(3000, 120, 1000) == 10000);
	return 0;
}
