#include "core/output_sequencer.h"

namespace {

bool bit_is_set(const uint8_t* bits, size_t sid) {
	return bits[sid >> 3] & (uint8_t(1) << (sid & 7));
}

void set_bit(uint8_t* bits, size_t sid, bool value) {
	const uint8_t mask = uint8_t(1) << (sid & 7);
	if (value) bits[sid >> 3] |= mask;
	else bits[sid >> 3] &= ~mask;
}

int16_t first_pending_rise(const uint8_t* desired, const uint8_t* applied,
	const uint8_t* priority, size_t station_count, bool priority_only) {
	for (size_t sid = 0; sid < station_count; sid++) {
		if (priority_only && (!priority || !bit_is_set(priority, sid))) continue;
		if (bit_is_set(desired, sid) && !bit_is_set(applied, sid)) {
			return static_cast<int16_t>(sid);
		}
	}
	return -1;
}

} // namespace

OutputSequencer::OutputSequencer() : next_rise_ms(0), rise_timing_active(false) {
}

OutputTransition OutputSequencer::advance(const uint8_t* desired, const uint8_t* priority,
	uint8_t* applied, size_t station_count, uint32_t now_ms, bool allow_rise) {
	OutputTransition result = {false, -1};

	// Falling edges are never staggered.
	for (size_t sid = 0; sid < station_count; sid++) {
		if (bit_is_set(applied, sid) && !bit_is_set(desired, sid)) {
			set_bit(applied, sid, false);
			result.changed = true;
		}
	}

	if (!allow_rise || (rise_timing_active && static_cast<int32_t>(now_ms - next_rise_ms) < 0)) {
		return result;
	}

	int16_t sid = first_pending_rise(desired, applied, priority, station_count, true);
	if (sid < 0) sid = first_pending_rise(desired, applied, priority, station_count, false);
	if (sid >= 0) {
		set_bit(applied, static_cast<size_t>(sid), true);
		result.changed = true;
		result.rising_sid = sid;
		next_rise_ms = now_ms + OUTPUT_STAGGER_MS;
		rise_timing_active = true;
	}

	return result;
}

void OutputSequencer::complete_rise(uint32_t now_ms) {
	next_rise_ms = now_ms + OUTPUT_STAGGER_MS;
	rise_timing_active = true;
}

void OutputSequencer::reset_timing() {
	next_rise_ms = 0;
	rise_timing_active = false;
}
