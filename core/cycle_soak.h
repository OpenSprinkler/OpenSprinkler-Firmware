#pragma once

#include <cstdint>

constexpr uint16_t CYCLE_SOAK_MIN_SECONDS = 60;
constexpr uint16_t CYCLE_SOAK_MAX_SECONDS = 240 * 60;

struct CycleSoakPolicy {
	bool enabled = false;
	uint16_t cycle_seconds = 0;
	uint16_t soak_seconds = 0;
};

// Only future, unassigned watering lives here. The runtime queue owns the
// currently assigned segment, including any remaining part after a pause.
struct CycleSoakState {
	uint32_t remaining = 0;
	uint32_t soak_until = 0;
	uint32_t prev_end = 0;
	uint32_t prev_start = 0;
	uint16_t cycles_left = 0;
	uint16_t soak_seconds = 0;
};

static_assert(sizeof(CycleSoakState) == 20, "Keep cycle progress compact");

enum class CycleSoakStop { Natural, Cancelled };

bool cycle_soak_policy_valid(const CycleSoakPolicy& policy, uint16_t bundle_minimum = 0);

// Failure leaves outputs untouched. Manual bypass / RE supplies apply_policy=false.
bool cycle_soak_begin(const CycleSoakPolicy& policy, uint32_t total,
	bool apply_policy, uint16_t bundle_minimum, CycleSoakState& state,
	uint32_t& first_duration);

// Called once for a real segment OFF, not for mere deque_time expiry.
// Returns true only when another cycle was assigned; cancellation always wins.
bool cycle_soak_finish(CycleSoakState& state, uint32_t segment_end,
	uint32_t off_time, CycleSoakStop reason, uint32_t& next_duration);
void cycle_soak_cancel_future(CycleSoakState& state);
void cycle_soak_note_off(CycleSoakState& state, uint32_t off_time);

// Checked wall-clock arithmetic. Overflow fails instead of wrapping a start
// into the past; no monotonic-clock immunity is implied.
bool cycle_soak_deadline(uint32_t start, uint32_t seconds, uint32_t& result);
bool cycle_soak_place(uint32_t now, uint32_t soak_until, uint32_t cooldown_until,
	bool sequential, uint32_t group_tail, int16_t station_delay, uint32_t& start);
bool cycle_soak_reserve(uint32_t start, uint32_t duration, uint16_t soak,
	uint32_t& cooldown_until);
bool cycle_soak_master_tail(uint32_t prev_end, int16_t off_adjustment,
	uint32_t& deadline);
bool cycle_soak_resume_before(uint32_t previous_start_a, uint8_t sid_a,
	uint32_t previous_start_b, uint8_t sid_b);
