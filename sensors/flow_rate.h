#pragma once

#include <stdint.h>

inline float flow_rate_from_pulse_span(uint32_t pulse_count, uint32_t elapsed_ms) {
	if (pulse_count < 2 || !elapsed_ms) return 0.0f;
	return 60000.0f * (pulse_count - 1) / elapsed_ms;
}

inline uint32_t flow_pulses_since(uint32_t start, uint32_t current) {
	return current - start; // Unsigned subtraction also handles counter wrap.
}

inline float flow_volume_from_pulses(uint32_t pulse_count, uint32_t volume_per_pulse100) {
	return static_cast<float>(pulse_count) * volume_per_pulse100 / 100.0f;
}
