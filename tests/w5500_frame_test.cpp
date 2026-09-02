#include <assert.h>

#include "drivers/w5500_frame.h"

using os_w5500::Fault;
using os_w5500::FrameDisposition;
using os_w5500::FrameValidation;

static FrameValidation check(uint16_t available, uint16_t encoded) {
	return os_w5500::validate_frame(available,
		static_cast<uint8_t>(encoded >> 8), static_cast<uint8_t>(encoded));
}

int main() {
	FrameValidation result = check(0, 0);
	assert(result.disposition == FrameDisposition::Empty);

	result = check(1, 0);
	assert(result.disposition == FrameDisposition::Wait);

	for (uint16_t encoded = 0; encoded <= 2; encoded++) {
		result = check(2, encoded);
		assert(result.disposition == FrameDisposition::Fault);
		assert(result.fault == Fault::InvalidFrameLength);
	}

	result = check(100, 102);
	assert(result.disposition == FrameDisposition::Wait);
	assert(result.payload_length == 100);

	result = check(102, 102);
	assert(result.disposition == FrameDisposition::Accept);
	assert(result.payload_length == 100);

	result = check(200, os_w5500::MAX_FRAME_SIZE + 3);
	assert(result.disposition == FrameDisposition::Fault);
	assert(result.fault == Fault::InvalidFrameLength);

	result = check(os_w5500::RX_BUFFER_SIZE + 1, 102);
	assert(result.disposition == FrameDisposition::Fault);
	assert(result.fault == Fault::InvalidReceiveSize);

	result = check(os_w5500::RX_BUFFER_SIZE, os_w5500::MAX_FRAME_SIZE + 2);
	assert(result.disposition == FrameDisposition::Accept);
	assert(result.payload_length == os_w5500::MAX_FRAME_SIZE);

	os_w5500::ReceiveStallTracker stall;
	assert(!stall.observe(0, 10, 20, 250, 8));
	// Elapsed time alone cannot trip a stall after polling was paused.
	assert(!stall.observe(1000, 10, 20, 250, 8));
	for (uint16_t poll = 3; poll < 8; poll++)
		assert(!stall.observe(1000, 10, 20, 250, 8));
	assert(stall.observe(1000, 10, 20, 250, 8));

	stall.reset();
	assert(!stall.observe(0, 10, 20, 250, 2));
	// Additional bytes or pointer progress starts a fresh observation window.
	assert(!stall.observe(300, 11, 20, 250, 2));
	assert(!stall.observe(600, 11, 21, 250, 2));
	return 0;
}
