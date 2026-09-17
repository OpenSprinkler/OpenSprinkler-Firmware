#pragma once

#include <stdint.h>

inline float flow_rate_from_pulse_span(uint32_t pulse_count, uint32_t elapsed_ms) {
	if (pulse_count < 2 || !elapsed_ms) return 0.0f;
	return 60000.0f * (pulse_count - 1) / elapsed_ms;
}
