#include "core/output_sequencer.h"

#include <cassert>
#include <cstdint>
#include <cstring>

namespace {

bool bit(const uint8_t* bits, uint8_t sid) {
	return bits[sid >> 3] & (uint8_t(1) << (sid & 7));
}

} // namespace

int main() {
	OutputSequencer sequencer;
	uint8_t desired[2] = {};
	uint8_t priority[2] = {};
	uint8_t applied[2] = {};

	// A priority output wins the first rising slot.
	desired[0] = 0x07;
	priority[0] = 0x04;
	OutputTransition transition = sequencer.advance(desired, priority, applied, 16, 1000);
	assert(transition.changed && transition.rising_sid == 2);
	assert(bit(applied, 2));
	// Hardware work completed later; spacing starts at physical completion.
	sequencer.complete_rise(1100);

	// No second rising edge before the interval expires.
	transition = sequencer.advance(desired, priority, applied, 16, 1349);
	assert(!transition.changed && transition.rising_sid < 0);
	transition = sequencer.advance(desired, priority, applied, 16, 1350);
	assert(transition.changed && transition.rising_sid == 0);

	// Falling edges apply immediately, even while the rising timer is active.
	desired[0] &= ~uint8_t(1);
	transition = sequencer.advance(desired, priority, applied, 16, 1351);
	assert(transition.changed && transition.rising_sid < 0);
	assert(!bit(applied, 0) && bit(applied, 2));

	// Rise blocking does not delay falling edges.
	desired[0] = 0x02;
	transition = sequencer.advance(desired, priority, applied, 16, 1600, false);
	assert(transition.changed && transition.rising_sid < 0);
	assert(!bit(applied, 2));
	transition = sequencer.advance(desired, priority, applied, 16, 1600, true);
	assert(transition.changed && transition.rising_sid == 1);

	// Unsigned deadline comparison survives millis() wraparound.
	memset(applied, 0, sizeof(applied));
	desired[0] = 0x03;
	sequencer.reset_timing();
	transition = sequencer.advance(desired, nullptr, applied, 16, 0xFFFFFFF0UL);
	assert(transition.rising_sid == 0);
	sequencer.complete_rise(0xFFFFFFF0UL);
	transition = sequencer.advance(desired, nullptr, applied, 16, 0x000000E9UL);
	assert(transition.rising_sid < 0);
	transition = sequencer.advance(desired, nullptr, applied, 16, 0x000000EAUL);
	assert(transition.rising_sid == 1);

	return 0;
}
