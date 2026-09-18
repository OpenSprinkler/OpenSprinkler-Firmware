#include "drivers/w5500_frame.h"

namespace os_w5500 {

ReceiveStallTracker::ReceiveStallTracker() {
	reset();
}

void ReceiveStallTracker::reset() {
	_active = false;
	_started_ms = 0;
	_available = 0;
	_read_pointer = 0;
	_polls = 0;
}

bool ReceiveStallTracker::observe(uint32_t now, uint16_t available,
	uint16_t read_pointer, uint32_t timeout_ms, uint16_t minimum_polls) {
	if (!_active || available != _available || read_pointer != _read_pointer) {
		_active = true;
		_started_ms = now;
		_available = available;
		_read_pointer = read_pointer;
		_polls = 1;
		return false;
	}
	if (_polls < UINT16_MAX) _polls++;
	return _polls >= minimum_polls &&
		static_cast<uint32_t>(now - _started_ms) >= timeout_ms;
}

FrameValidation validate_frame(uint16_t available, uint8_t length_high,
	uint8_t length_low, uint16_t buffer_size, uint16_t max_frame_size) {
	FrameValidation result = {FrameDisposition::Empty, Fault::None, 0, 0};
	if (available == 0) return result;
	if (available > buffer_size) {
		result.disposition = FrameDisposition::Fault;
		result.fault = Fault::InvalidReceiveSize;
		return result;
	}
	if (available < 2) {
		result.disposition = FrameDisposition::Wait;
		return result;
	}

	result.encoded_length = (static_cast<uint16_t>(length_high) << 8) | length_low;
	if (result.encoded_length <= 2 || result.encoded_length > max_frame_size + 2) {
		result.disposition = FrameDisposition::Fault;
		result.fault = Fault::InvalidFrameLength;
		return result;
	}

	result.payload_length = result.encoded_length - 2;
	if (result.encoded_length > available) {
		result.disposition = FrameDisposition::Wait;
		return result;
	}

	result.disposition = FrameDisposition::Accept;
	return result;
}

} // namespace os_w5500
