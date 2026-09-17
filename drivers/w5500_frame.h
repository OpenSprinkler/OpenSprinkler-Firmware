#pragma once

#include <stdint.h>

namespace os_w5500 {

static const uint16_t RX_BUFFER_SIZE = 16U * 1024U;
static const uint16_t MAX_FRAME_SIZE = 1600U;

enum class FrameDisposition : uint8_t {
	Empty,
	Wait,
	Accept,
	Fault,
};

enum class Fault : uint8_t {
	None = 0,
	InvalidReceiveSize,
	InvalidFrameLength,
	IncompleteFrame,
	ReceiveStall,
	RegisterTimeout,
	CommandTimeout,
	TransmitSpaceTimeout,
	SendTimeout,
	CloseTimeout,
	InvalidChipState,
	InvalidSocketState,
	FrameStateMismatch,
};

struct FrameValidation {
	FrameDisposition disposition;
	Fault fault;
	uint16_t encoded_length;
	uint16_t payload_length;
};

class ReceiveStallTracker {
public:
	ReceiveStallTracker();
	void reset();
	bool observe(uint32_t now, uint16_t available, uint16_t read_pointer,
		uint32_t timeout_ms, uint16_t minimum_polls);

private:
	bool _active;
	uint32_t _started_ms;
	uint16_t _available;
	uint16_t _read_pointer;
	uint16_t _polls;
};

FrameValidation validate_frame(uint16_t available, uint8_t length_high,
	uint8_t length_low, uint16_t buffer_size = RX_BUFFER_SIZE,
	uint16_t max_frame_size = MAX_FRAME_SIZE);

} // namespace os_w5500
