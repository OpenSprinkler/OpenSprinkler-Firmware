#include "core/cycle_soak.h"

#include <algorithm>
#include <limits>

namespace {

bool valid_seconds(uint16_t value) {
	return value >= CYCLE_SOAK_MIN_SECONDS && value <= CYCLE_SOAK_MAX_SECONDS;
}

uint32_t assign_cycle(CycleSoakState& state) {
	if (!state.cycles_left || !state.remaining) return 0;
	const uint32_t duration = state.remaining / state.cycles_left +
		(state.remaining % state.cycles_left != 0);
	state.remaining -= duration;
	state.cycles_left--;
	return duration;
}

} // namespace

bool cycle_soak_policy_valid(const CycleSoakPolicy& policy, uint16_t bundle_minimum) {
	if (!policy.enabled) {
		return (!policy.cycle_seconds || valid_seconds(policy.cycle_seconds)) &&
			(!policy.soak_seconds || valid_seconds(policy.soak_seconds));
	}
	return valid_seconds(policy.cycle_seconds) && valid_seconds(policy.soak_seconds) &&
		uint32_t(policy.cycle_seconds) >= 2UL * bundle_minimum;
}

bool cycle_soak_begin(const CycleSoakPolicy& policy, uint32_t total,
	bool apply_policy, uint16_t bundle_minimum, CycleSoakState& state,
	uint32_t& first_duration) {
	CycleSoakState next;
	uint32_t duration = total;
	if (apply_policy && policy.enabled) {
		if (!cycle_soak_policy_valid(policy, bundle_minimum)) return false;
		const uint32_t cycles = total / policy.cycle_seconds + (total % policy.cycle_seconds != 0);
		if (cycles > std::numeric_limits<uint16_t>::max()) return false;
		next.soak_seconds = policy.soak_seconds;
		next.remaining = total;
		next.cycles_left = static_cast<uint16_t>(cycles);
		duration = assign_cycle(next);
	}
	// Retain the existing short Bundle run extension, including on bypass runs.
	if (duration && duration < bundle_minimum) duration = bundle_minimum;
	state = next;
	first_duration = duration;
	return true;
}

bool cycle_soak_deadline(uint32_t start, uint32_t seconds, uint32_t& result) {
	if (seconds > std::numeric_limits<uint32_t>::max() - start) return false;
	result = start + seconds;
	return true;
}

void cycle_soak_note_off(CycleSoakState& state, uint32_t off_time) {
	// Saturating a final deadline cannot cause a premature restart.
	if (!cycle_soak_deadline(off_time, state.soak_seconds, state.soak_until)) {
		state.soak_until = std::numeric_limits<uint32_t>::max();
	}
}

void cycle_soak_cancel_future(CycleSoakState& state) {
	state.remaining = 0;
	state.cycles_left = 0;
}

bool cycle_soak_finish(CycleSoakState& state, uint32_t segment_end,
	uint32_t off_time, CycleSoakStop reason, uint32_t& next_duration) {
	state.prev_end = segment_end;
	cycle_soak_note_off(state, off_time);
	next_duration = 0;
	if (reason == CycleSoakStop::Cancelled) {
		cycle_soak_cancel_future(state);
		return false;
	}
	if (!state.remaining || !state.cycles_left) return false;
	next_duration = assign_cycle(state);
	return next_duration != 0;
}

bool cycle_soak_place(uint32_t now, uint32_t soak_until, uint32_t cooldown_until,
	bool sequential, uint32_t group_tail, int16_t station_delay, uint32_t& start) {
	uint32_t earliest;
	if (!cycle_soak_deadline(now, 1, earliest)) return false;
	earliest = std::max(earliest, std::max(soak_until, cooldown_until));
	if (sequential) {
		const int64_t tail = int64_t(group_tail) + station_delay;
		if (tail > std::numeric_limits<uint32_t>::max()) return false;
		if (tail > int64_t(earliest)) earliest = static_cast<uint32_t>(tail);
	}
	start = earliest;
	return true;
}

bool cycle_soak_reserve(uint32_t start, uint32_t duration, uint16_t soak,
	uint32_t& cooldown_until) {
	uint32_t end, deadline;
	if (!cycle_soak_deadline(start, duration, end) ||
		!cycle_soak_deadline(end, soak, deadline)) return false;
	cooldown_until = std::max(cooldown_until, deadline);
	return true;
}

bool cycle_soak_master_tail(uint32_t prev_end, int16_t off_adjustment,
	uint32_t& deadline) {
	if (!prev_end) return false;
	const int64_t end = int64_t(prev_end) + off_adjustment;
	if (end < 0 || end > std::numeric_limits<uint32_t>::max()) return false;
	deadline = static_cast<uint32_t>(end);
	return true;
}

bool cycle_soak_resume_before(uint32_t previous_start_a, uint8_t sid_a,
	uint32_t previous_start_b, uint8_t sid_b) {
	return previous_start_a < previous_start_b ||
		(previous_start_a == previous_start_b && sid_a < sid_b);
}
