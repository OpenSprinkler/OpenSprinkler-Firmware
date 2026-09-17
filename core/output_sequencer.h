#pragma once

#include <cstddef>
#include <cstdint>

constexpr uint16_t OUTPUT_STAGGER_MS = 250;
constexpr uint8_t OUTPUT_RISES_PER_SECOND = 1000 / OUTPUT_STAGGER_MS;

struct OutputTransition {
	bool changed;
	int16_t rising_sid;
};

class OutputSequencer {
public:
	OutputSequencer();

	OutputTransition advance(const uint8_t* desired, const uint8_t* priority,
		uint8_t* applied, size_t station_count, uint32_t now_ms, bool allow_rise = true);
	void complete_rise(uint32_t now_ms);
	void reset_timing();

private:
	uint32_t next_rise_ms;
	bool rise_timing_active;
};
