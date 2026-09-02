#include "drivers/os_w5500.h"

#if defined(ESP8266)

#include <string.h>

OSWiznet5500::OSWiznet5500(int8_t cs, SPIClass& spi, int8_t intr)
	: _spi(spi), _cs(cs), _operation_busy(false), _frame_pending(false),
	  _fault_pending(false), _pending_rx_rd(0), _pending_encoded_length(0),
	  _pending_payload_length(0) {
	(void)intr;
	memset(_mac_address, 0, sizeof(_mac_address));
	memset(&_diagnostics, 0, sizeof(_diagnostics));
}

void OSWiznet5500::select() {
	digitalWrite(_cs, LOW);
}

void OSWiznet5500::deselect() {
	digitalWrite(_cs, HIGH);
}

uint8_t OSWiznet5500::readRegister(uint8_t block, uint16_t address) {
	select();
	_spi.transfer(static_cast<uint8_t>(address >> 8));
	_spi.transfer(static_cast<uint8_t>(address));
	_spi.transfer(block | ACCESS_READ);
	uint8_t value = _spi.transfer(0);
	deselect();
	return value;
}

uint16_t OSWiznet5500::readWord(uint8_t block, uint16_t address) {
	return (static_cast<uint16_t>(readRegister(block, address)) << 8) |
		readRegister(block, address + 1);
}

void OSWiznet5500::readBuffer(uint8_t block, uint16_t address,
	uint8_t *buffer, uint16_t length) {
	select();
	_spi.transfer(static_cast<uint8_t>(address >> 8));
	_spi.transfer(static_cast<uint8_t>(address));
	_spi.transfer(block | ACCESS_READ);
	for (uint16_t i = 0; i < length; i++) buffer[i] = _spi.transfer(0);
	deselect();
}

void OSWiznet5500::writeRegister(uint8_t block, uint16_t address, uint8_t value) {
	select();
	_spi.transfer(static_cast<uint8_t>(address >> 8));
	_spi.transfer(static_cast<uint8_t>(address));
	_spi.transfer(block | ACCESS_WRITE);
	_spi.transfer(value);
	deselect();
}

void OSWiznet5500::writeWord(uint8_t block, uint16_t address, uint16_t value) {
	writeRegister(block, address, static_cast<uint8_t>(value >> 8));
	writeRegister(block, address + 1, static_cast<uint8_t>(value));
}

void OSWiznet5500::writeBuffer(uint8_t block, uint16_t address,
	const uint8_t *buffer, uint16_t length) {
	select();
	_spi.transfer(static_cast<uint8_t>(address >> 8));
	_spi.transfer(static_cast<uint8_t>(address));
	_spi.transfer(block | ACCESS_WRITE);
	for (uint16_t i = 0; i < length; i++) _spi.transfer(buffer[i]);
	deselect();
}

bool OSWiznet5500::beginOperation() {
	if (_operation_busy) return false;
	_operation_busy = true;
	return true;
}

void OSWiznet5500::endOperation() {
	_frame_pending = false;
	_operation_busy = false;
}

bool OSWiznet5500::readStableWord(uint8_t block, uint16_t address, uint16_t& value) {
	uint16_t previous = readWord(block, address);
	uint32_t deadline = millis() + REGISTER_TIMEOUT_MS;
	do {
		uint16_t current = readWord(block, address);
		if (current == previous) {
			value = current;
			return true;
		}
		previous = current;
	} while (!deadlineReached(deadline));
	return false;
}

bool OSWiznet5500::issueCommand(uint8_t command, os_w5500::Fault timeout_fault) {
	writeRegister(BLOCK_SOCKET0, Sn_CR, command);
	uint32_t deadline = millis() + COMMAND_TIMEOUT_MS;
	while (readRegister(BLOCK_SOCKET0, Sn_CR) != 0) {
		if (deadlineReached(deadline)) {
			latchFault(timeout_fault);
			return false;
		}
	}
	return true;
}

void OSWiznet5500::captureSnapshot() {
	_diagnostics.socket_status = readRegister(BLOCK_SOCKET0, Sn_SR);
	_diagnostics.socket_interrupt = readRegister(BLOCK_SOCKET0, Sn_IR);
	_diagnostics.rx_rsr = readWord(BLOCK_SOCKET0, Sn_RX_RSR);
	_diagnostics.rx_rd = readWord(BLOCK_SOCKET0, Sn_RX_RD);
	_diagnostics.rx_wr = readWord(BLOCK_SOCKET0, Sn_RX_WR);
	_diagnostics.tx_fsr = readWord(BLOCK_SOCKET0, Sn_TX_FSR);
	_diagnostics.phy_config = readRegister(BLOCK_COMMON, PHYCFGR);
	_diagnostics.version = readRegister(BLOCK_COMMON, VERSIONR);
}

void OSWiznet5500::latchFault(os_w5500::Fault fault) {
	if (fault == os_w5500::Fault::None || _fault_pending) return;
	_fault_pending = true;
	_diagnostics.last_fault = fault;
	_diagnostics.last_fault_ms = millis();
	switch (fault) {
	case os_w5500::Fault::InvalidReceiveSize:
	case os_w5500::Fault::InvalidFrameLength:
	case os_w5500::Fault::IncompleteFrame:
	case os_w5500::Fault::FrameStateMismatch:
		_diagnostics.bad_frame_count++;
		break;
	case os_w5500::Fault::ReceiveStall:
		_diagnostics.receive_stall_count++;
		break;
	case os_w5500::Fault::RegisterTimeout:
	case os_w5500::Fault::CommandTimeout:
	case os_w5500::Fault::TransmitSpaceTimeout:
	case os_w5500::Fault::SendTimeout:
	case os_w5500::Fault::CloseTimeout:
		_diagnostics.timeout_count++;
		break;
	case os_w5500::Fault::InvalidChipState:
	case os_w5500::Fault::InvalidSocketState:
		_diagnostics.chip_fault_count++;
		break;
	default:
		break;
	}
	captureSnapshot();
}

void OSWiznet5500::resetStallObservation() {
	_stall_tracker.reset();
}

void OSWiznet5500::observeIncompleteFrame(uint16_t rsr, uint16_t rd) {
	if (_stall_tracker.observe(millis(), rsr, rd,
		RECEIVE_STALL_MS, RECEIVE_STALL_POLLS)) {
		latchFault(os_w5500::Fault::ReceiveStall);
	}
}

bool OSWiznet5500::initialize() {
	pinMode(_cs, OUTPUT);
	deselect();
	writeRegister(BLOCK_COMMON, MR, MR_RST);

	uint32_t deadline = millis() + COMMAND_TIMEOUT_MS;
	while (readRegister(BLOCK_COMMON, MR) != 0) {
		if (deadlineReached(deadline)) {
			latchFault(os_w5500::Fault::CommandTimeout);
			return false;
		}
	}
	if (readRegister(BLOCK_COMMON, VERSIONR) != W5500_VERSION) {
		latchFault(os_w5500::Fault::InvalidChipState);
		return false;
	}

	for (uint8_t socket = 0; socket < 8; socket++) {
		uint8_t block = socketRegisterBlock(socket);
		writeRegister(block, Sn_RXBUF_SIZE, 0);
		writeRegister(block, Sn_TXBUF_SIZE, 0);
	}
	writeRegister(BLOCK_SOCKET0, Sn_RXBUF_SIZE, 16);
	writeRegister(BLOCK_SOCKET0, Sn_TXBUF_SIZE, 16);
	writeBuffer(BLOCK_COMMON, SHAR, _mac_address, sizeof(_mac_address));
	writeRegister(BLOCK_SOCKET0, Sn_MR, Sn_MR_MACRAW);
	if (!issueCommand(Sn_CR_OPEN, os_w5500::Fault::CommandTimeout)) return false;

	if (readRegister(BLOCK_SOCKET0, Sn_SR) != SOCK_MACRAW ||
		readRegister(BLOCK_SOCKET0, Sn_RXBUF_SIZE) != 16 ||
		readRegister(BLOCK_SOCKET0, Sn_TXBUF_SIZE) != 16) {
		latchFault(os_w5500::Fault::InvalidSocketState);
		return false;
	}
	resetStallObservation();
	_frame_pending = false;
	return true;
}

boolean OSWiznet5500::begin(const uint8_t *address) {
	if (!address || !beginOperation()) return false;
	memcpy(_mac_address, address, sizeof(_mac_address));
	bool success = initialize();
	if (success) _fault_pending = false;
	endOperation();
	return success;
}

bool OSWiznet5500::reinitialize() {
	if (!beginOperation()) return false;
	bool success = initialize();
	if (success) {
		_fault_pending = false;
		_diagnostics.recovery_count++;
	} else {
		_diagnostics.recovery_failure_count++;
	}
	endOperation();
	return success;
}

void OSWiznet5500::end() {
	if (!beginOperation()) return;
	if (issueCommand(Sn_CR_CLOSE, os_w5500::Fault::CloseTimeout)) {
		writeRegister(BLOCK_SOCKET0, Sn_IR, 0x1F);
		uint32_t deadline = millis() + CLOSE_TIMEOUT_MS;
		while (readRegister(BLOCK_SOCKET0, Sn_SR) != SOCK_CLOSED) {
			if (deadlineReached(deadline)) {
				latchFault(os_w5500::Fault::CloseTimeout);
				break;
			}
		}
	}
	endOperation();
}

bool OSWiznet5500::isLinked() {
	return (readRegister(BLOCK_COMMON, PHYCFGR) & PHY_LINK_ON) != 0;
}

bool OSWiznet5500::healthCheck() {
	if (_fault_pending) return false;
	if (!beginOperation()) return true;
	// Read into locals rather than the diagnostic block: the snapshot belongs
	// to the last latched fault and must survive a successful recovery so it
	// is still reportable through /db afterwards.
	bool chip_healthy = readRegister(BLOCK_COMMON, VERSIONR) == W5500_VERSION;
	bool socket_healthy = readRegister(BLOCK_SOCKET0, Sn_SR) == SOCK_MACRAW &&
		readRegister(BLOCK_SOCKET0, Sn_RXBUF_SIZE) == 16 &&
		readRegister(BLOCK_SOCKET0, Sn_TXBUF_SIZE) == 16;
	if (!chip_healthy) latchFault(os_w5500::Fault::InvalidChipState);
	else if (!socket_healthy) latchFault(os_w5500::Fault::InvalidSocketState);
	endOperation();
	return chip_healthy && socket_healthy;
}

uint16_t OSWiznet5500::readFrameSize() {
	if (_fault_pending || !beginOperation()) return 0;

	uint16_t available = 0;
	if (!readStableWord(BLOCK_SOCKET0, Sn_RX_RSR, available)) {
		latchFault(os_w5500::Fault::RegisterTimeout);
		endOperation();
		return 0;
	}
	if (available == 0) {
		resetStallObservation();
		endOperation();
		return 0;
	}

	uint16_t rx_rd = readWord(BLOCK_SOCKET0, Sn_RX_RD);
	uint8_t header[2] = {0, 0};
	if (available >= 2 && available <= os_w5500::RX_BUFFER_SIZE)
		readBuffer(BLOCK_RX0, rx_rd, header, sizeof(header));
	os_w5500::FrameValidation validation =
		os_w5500::validate_frame(available, header[0], header[1]);
	if (validation.disposition == os_w5500::FrameDisposition::Fault) {
		latchFault(validation.fault);
		endOperation();
		return 0;
	}
	if (validation.disposition != os_w5500::FrameDisposition::Accept) {
		observeIncompleteFrame(available, rx_rd);
		endOperation();
		return 0;
	}

	resetStallObservation();
	_pending_rx_rd = rx_rd;
	_pending_encoded_length = validation.encoded_length;
	_pending_payload_length = validation.payload_length;
	_frame_pending = true;
	return _pending_payload_length;
}

void OSWiznet5500::completePendingFrame(bool read_data, uint8_t *buffer) {
	if (read_data) {
		readBuffer(BLOCK_RX0, _pending_rx_rd + 2, buffer, _pending_payload_length);
	}
	writeWord(BLOCK_SOCKET0, Sn_RX_RD, _pending_rx_rd + _pending_encoded_length);
	issueCommand(Sn_CR_RECV, os_w5500::Fault::CommandTimeout);
}

void OSWiznet5500::discardFrame(uint16_t framesize) {
	if (!_operation_busy || !_frame_pending || framesize != _pending_payload_length) {
		latchFault(os_w5500::Fault::FrameStateMismatch);
		endOperation();
		return;
	}
	completePendingFrame(false, nullptr);
	endOperation();
}

uint16_t OSWiznet5500::readFrameData(uint8_t *buffer, uint16_t framesize) {
	if (!buffer || !_operation_busy || !_frame_pending ||
		framesize != _pending_payload_length) {
		latchFault(os_w5500::Fault::FrameStateMismatch);
		endOperation();
		return 0;
	}
	completePendingFrame(true, buffer);
	uint16_t result = _fault_pending ? 0 : framesize;
	endOperation();
	return result;
}

uint16_t OSWiznet5500::readFrame(uint8_t *buffer, uint16_t bufsize) {
	uint16_t frame_size = readFrameSize();
	if (frame_size == 0) return 0;
	if (frame_size > bufsize) {
		discardFrame(frame_size);
		return 0;
	}
	return readFrameData(buffer, frame_size);
}

uint16_t OSWiznet5500::sendFrame(const uint8_t *data, uint16_t length) {
	if (!data || length == 0 || _fault_pending || !beginOperation()) return 0;
	uint32_t deadline = millis() + TX_SPACE_TIMEOUT_MS;
	uint16_t free_size = 0;
	while (true) {
		if (!isLinked()) {
			endOperation();
			return 0;
		}
		if (readRegister(BLOCK_SOCKET0, Sn_SR) != SOCK_MACRAW) {
			latchFault(os_w5500::Fault::InvalidSocketState);
			endOperation();
			return 0;
		}
		if (!readStableWord(BLOCK_SOCKET0, Sn_TX_FSR, free_size)) {
			latchFault(os_w5500::Fault::RegisterTimeout);
			endOperation();
			return 0;
		}
		if (length <= free_size) break;
		if (deadlineReached(deadline)) {
			latchFault(os_w5500::Fault::TransmitSpaceTimeout);
			endOperation();
			return 0;
		}
	}

	uint16_t tx_wr = readWord(BLOCK_SOCKET0, Sn_TX_WR);
	writeBuffer(BLOCK_TX0, tx_wr, data, length);
	writeWord(BLOCK_SOCKET0, Sn_TX_WR, tx_wr + length);
	if (!issueCommand(Sn_CR_SEND, os_w5500::Fault::CommandTimeout)) {
		endOperation();
		return 0;
	}

	deadline = millis() + SEND_TIMEOUT_MS;
	while (true) {
		uint8_t interrupt = readRegister(BLOCK_SOCKET0, Sn_IR);
		if (interrupt & Sn_IR_SENDOK) {
			writeRegister(BLOCK_SOCKET0, Sn_IR, Sn_IR_SENDOK);
			endOperation();
			return length;
		}
		if (interrupt & Sn_IR_TIMEOUT) {
			writeRegister(BLOCK_SOCKET0, Sn_IR, Sn_IR_TIMEOUT);
			if (isLinked()) latchFault(os_w5500::Fault::SendTimeout);
			endOperation();
			return 0;
		}
		if (deadlineReached(deadline)) {
			if (isLinked()) latchFault(os_w5500::Fault::SendTimeout);
			endOperation();
			return 0;
		}
	}
}

#endif // ESP8266
