#include "core/cycle_soak.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <limits>
#include <vector>

namespace {
CycleSoakPolicy policy(uint16_t cycle, uint16_t soak) {
	CycleSoakPolicy p;
	p.enabled = true; p.cycle_seconds = cycle; p.soak_seconds = soak;
	return p;
}

std::vector<uint32_t> split(uint32_t total, uint16_t maximum) {
	CycleSoakState s;
	uint32_t duration, end = 1000;
	assert(cycle_soak_begin(policy(maximum, 60), total, true, 0, s, duration));
	std::vector<uint32_t> parts;
	while (duration) {
		parts.push_back(duration);
		end += duration;
		if (!cycle_soak_finish(s, end, end, CycleSoakStop::Natural, duration)) break;
		end += 60;
	}
	assert(!s.remaining && !s.cycles_left);
	return parts;
}

void arithmetic() {
	assert(split(1020, 300) == std::vector<uint32_t>({255, 255, 255, 255}));
	assert(split(1260, 600) == std::vector<uint32_t>({420, 420, 420}));
	assert(split(121, 60) == std::vector<uint32_t>({41, 40, 40}));
	assert(split(0, 60).empty());
	for (uint16_t maximum : {60, 300, 14400}) {
		for (uint32_t total = 1; total <= 10000; total++) {
			const auto parts = split(total, maximum);
			uint64_t sum = 0;
			for (auto part : parts) { assert(part <= maximum); sum += part; }
			assert(sum == total);
			assert(parts.size() == total / maximum + (total % maximum != 0));
			assert(parts.front() - parts.back() <= 1);
		}
	}
	CycleSoakState s;
	uint32_t d;
	const uint32_t largest = std::numeric_limits<uint32_t>::max();
	assert(cycle_soak_begin(policy(60, 60), largest, true, 0, s, d));
	assert(uint64_t(s.remaining) + d == largest && d <= 60);
}

void policy_and_bundle() {
	CycleSoakState s;
	uint32_t d = 123;
	assert(!cycle_soak_begin(policy(59, 60), 600, true, 0, s, d));
	assert(d == 123 && !s.remaining);
	assert(!cycle_soak_policy_valid(policy(60, 59)));
	assert(!cycle_soak_policy_valid(policy(14401, 60)));
	assert(cycle_soak_policy_valid(policy(60, 60), 30));
	assert(!cycle_soak_policy_valid(policy(60, 60), 31));
	assert(cycle_soak_begin(policy(60, 60), 1, true, 3, s, d));
	assert(d == 3 && !s.cycles_left);
	assert(cycle_soak_begin(policy(60, 60), 0, true, 3, s, d) && !d);
	assert(cycle_soak_begin(policy(60, 60), 61, true, 30, s, d) && d == 31);
	assert(cycle_soak_finish(s, 1031, 1031, CycleSoakStop::Natural, d) && d == 30);
	assert(cycle_soak_begin(policy(60, 60), 600, false, 0, s, d));
	assert(d == 600 && !s.cycles_left && !s.soak_seconds); // manual / RE bypass
	assert(cycle_soak_begin(CycleSoakPolicy{}, 600, true, 0, s, d));
	assert(d == 600 && !s.cycles_left); // disabled-policy opt-in
}

// Exercise real policy helpers with a deterministic append/re-arm harness.
void timeline() {
	const uint32_t origin = 1000;
	CycleSoakState a, b;
	uint32_t ad, bd, ac = 0, bc = 0, as, bs;
	assert(cycle_soak_begin(policy(600, 1800), 1200, true, 0, a, ad));
	assert(cycle_soak_begin(policy(300, 600), 600, true, 0, b, bd));
	assert(cycle_soak_reserve(origin, ad, a.soak_seconds, ac));
	assert(cycle_soak_reserve(origin + ad, bd, b.soak_seconds, bc));
	uint32_t tail = origin + ad + bd;
	assert(cycle_soak_finish(a, origin + 600, origin + 600, CycleSoakStop::Natural, ad));
	assert(cycle_soak_place(origin + 600, a.soak_until, ac, true, tail, 0, as));
	assert(as == origin + 2400);
	tail = std::max(tail, as + ad);
	assert(cycle_soak_finish(b, origin + 900, origin + 900, CycleSoakStop::Natural, bd));
	assert(cycle_soak_place(origin + 900, b.soak_until, bc, true, tail, 0, bs));
	assert(bs == origin + 3000 && bd == 300); // B 50-55, not 25-30
	assert(cycle_soak_place(origin + 900, b.soak_until, bc, false, tail, 0, bs));
	assert(bs == origin + 1500); // parallel ignores tail
	assert(!cycle_soak_finish(a, as + ad, as + ad, CycleSoakStop::Natural, ad));
	assert(cycle_soak_place(5000, 4000, 4000, true, 4500, 0, as));
	tail = as + 300;
	assert(cycle_soak_place(5000, 4000, 4000, true, tail, 0, bs));
	assert(bs >= as + 300); // publish before second completion in same pass
}

void cooldown_and_boundaries() {
	uint32_t cooldown = 0, start;
	assert(cycle_soak_reserve(1000, 300, 600, cooldown) && cooldown == 1900);
	assert(cycle_soak_place(1001, 0, cooldown, true, 1300, -30, start) && start == 1900);
	const uint32_t already_reserved = start;
	assert(cycle_soak_reserve(start, 300, 600, cooldown) && cooldown == 2800);
	assert(cycle_soak_reserve(1310, 0, 600, cooldown) && cooldown == 2800);
	assert(already_reserved - 1310 == 590); // accepted 10-second stall shortfall
	assert(cycle_soak_place(1000, 0, 0, true, 1300, -30, start) && start == 1270);
	assert(cycle_soak_place(1000, 0, 0, true, 1005, -120, start) && start == 1001);
	const uint32_t largest = std::numeric_limits<uint32_t>::max();
	assert(!cycle_soak_place(largest, 0, 0, false, 0, 0, start));
	assert(!cycle_soak_place(1000, 0, 0, true, largest, 1, start));
	assert(!cycle_soak_reserve(largest, 1, 0, cooldown) && cooldown == 2800);
}

void pause_cancel_masters() {
	CycleSoakState s;
	uint32_t d, start, deadline;
	assert(cycle_soak_begin(policy(300, 600), 900, true, 0, s, d));
	const uint32_t remaining = s.remaining, cycles = s.cycles_left;
	cycle_soak_note_off(s, 1100); // pause: current partial segment belongs to caller
	assert(s.soak_until == 1700 && s.remaining == remaining && s.cycles_left == cycles);
	assert(cycle_soak_place(1200, s.soak_until, 0, false, 0, 0, start) && start == 1700);
	assert(cycle_soak_place(1800, s.soak_until, 0, false, 0, 0, start) && start == 1801);
	assert(cycle_soak_finish(s, 2000, 2010, CycleSoakStop::Natural, d));
	assert(d == 300 && s.prev_end == 2000 && s.soak_until == 2610);
	assert(!cycle_soak_finish(s, 2910, 2910, CycleSoakStop::Cancelled, d));
	assert(!s.remaining && !s.cycles_left && !d && s.prev_end == 2910);
	assert(cycle_soak_begin(policy(300, 600), 900, true, 0, s, d));
	cycle_soak_cancel_future(s); // RE enable retains current duration
	assert(d == 300 && !s.remaining && !s.cycles_left);
	assert(!cycle_soak_finish(s, 1300, 1300, CycleSoakStop::Natural, d));
	s = {};
	assert(!s.prev_end && !s.soak_until); // reset/manual time setting
	assert(cycle_soak_master_tail(1000, 30, deadline) && deadline == 1030);
	assert(cycle_soak_master_tail(1000, 90, deadline) && deadline == 1090);
	assert(cycle_soak_master_tail(1000, -10, deadline) && deadline == 990);
	assert(!cycle_soak_master_tail(0, 30, deadline));
	assert(!cycle_soak_master_tail(5, -10, deadline));
	assert(cycle_soak_resume_before(1000, 7, 1001, 0));
	assert(cycle_soak_resume_before(1000, 0, 1000, 7));
	assert(!cycle_soak_resume_before(1000, 7, 1000, 0));
}
}

int main() {
	arithmetic(); policy_and_bundle(); timeline(); cooldown_and_boundaries(); pause_cancel_masters();
	printf("Cycle/soak policy PASS: %zu-byte state\n", sizeof(CycleSoakState));
}
