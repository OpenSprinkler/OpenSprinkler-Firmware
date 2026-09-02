/*
    Copyright (c) 2013, WIZnet Co., Ltd.
    Copyright (c) 2016, Nicholas Humfrey
    Copyright (c) 2026, OpenSprinkler
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
    1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holder nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
    FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
    COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES.
*/

#pragma once

#include "drivers/w5500_frame.h"

#if defined(ESP8266)

#include <Arduino.h>
#include <LwipIntfDev.h>
#include <SPI.h>

struct W5500Diagnostics {
	uint32_t bad_frame_count;
	uint32_t receive_stall_count;
	uint32_t timeout_count;
	uint32_t chip_fault_count;
	uint32_t recovery_count;
	uint32_t recovery_failure_count;
	uint32_t last_fault_ms;
	uint16_t rx_rsr;
	uint16_t rx_rd;
	uint16_t rx_wr;
	uint16_t tx_fsr;
	uint8_t socket_status;
	uint8_t socket_interrupt;
	uint8_t phy_config;
	uint8_t version;
	os_w5500::Fault last_fault;
};

class OSWiznet5500 {
public:
	OSWiznet5500(int8_t cs = SS, SPIClass& spi = SPI, int8_t intr = -1);

	boolean begin(const uint8_t *address);
	bool reinitialize();
	void end();
	uint16_t sendFrame(const uint8_t *data, uint16_t datalen);
	uint16_t readFrame(uint8_t *buffer, uint16_t bufsize);
	bool isLinked();
	constexpr bool isLinkDetectable() const { return true; }

	bool faultPending() const { return _fault_pending; }
	os_w5500::Fault faultCode() const { return _diagnostics.last_fault; }
	const W5500Diagnostics& diagnostics() const { return _diagnostics; }
	bool healthCheck();

protected:
	static constexpr bool interruptIsPossible() { return false; }
	uint16_t readFrameSize();
	void discardFrame(uint16_t framesize);
	uint16_t readFrameData(uint8_t *frame, uint16_t framesize);

private:
	static const uint8_t ACCESS_READ = 0x00 << 2;
	static const uint8_t ACCESS_WRITE = 0x01 << 2;
	static const uint8_t BLOCK_COMMON = 0x00 << 3;
	static const uint8_t BLOCK_SOCKET0 = 0x01 << 3;
	static const uint8_t BLOCK_TX0 = 0x02 << 3;
	static const uint8_t BLOCK_RX0 = 0x03 << 3;

	enum CommonRegister : uint16_t {
		MR = 0x0000,
		SHAR = 0x0009,
		PHYCFGR = 0x002E,
		VERSIONR = 0x0039,
	};

	enum SocketRegister : uint16_t {
		Sn_MR = 0x0000,
		Sn_CR = 0x0001,
		Sn_IR = 0x0002,
		Sn_SR = 0x0003,
		Sn_RXBUF_SIZE = 0x001E,
		Sn_TXBUF_SIZE = 0x001F,
		Sn_TX_FSR = 0x0020,
		Sn_TX_WR = 0x0024,
		Sn_RX_RSR = 0x0026,
		Sn_RX_RD = 0x0028,
		Sn_RX_WR = 0x002A,
	};

	enum RegisterValue : uint8_t {
		MR_RST = 0x80,
		Sn_MR_MACRAW = 0x04,
		Sn_CR_OPEN = 0x01,
		Sn_CR_CLOSE = 0x10,
		Sn_CR_SEND = 0x20,
		Sn_CR_RECV = 0x40,
		Sn_IR_TIMEOUT = 0x08,
		Sn_IR_SENDOK = 0x10,
		SOCK_CLOSED = 0x00,
		SOCK_MACRAW = 0x42,
		PHY_LINK_ON = 0x01,
		W5500_VERSION = 0x04,
	};

	static const uint32_t REGISTER_TIMEOUT_MS = 5;
	static const uint32_t COMMAND_TIMEOUT_MS = 10;
	static const uint32_t TX_SPACE_TIMEOUT_MS = 100;
	static const uint32_t SEND_TIMEOUT_MS = 250;
	static const uint32_t CLOSE_TIMEOUT_MS = 20;
	static const uint32_t RECEIVE_STALL_MS = 250;
	static const uint16_t RECEIVE_STALL_POLLS = 8;

	SPIClass& _spi;
	int8_t _cs;
	uint8_t _mac_address[6];
	bool _operation_busy;
	bool _frame_pending;
	bool _fault_pending;
	uint16_t _pending_rx_rd;
	uint16_t _pending_encoded_length;
	uint16_t _pending_payload_length;
	os_w5500::ReceiveStallTracker _stall_tracker;
	W5500Diagnostics _diagnostics;

	static uint8_t socketRegisterBlock(uint8_t socket) {
		return static_cast<uint8_t>(((socket * 4U) + 1U) << 3);
	}
	static bool deadlineReached(uint32_t deadline) {
		return static_cast<int32_t>(millis() - deadline) >= 0;
	}

	void select();
	void deselect();
	uint8_t readRegister(uint8_t block, uint16_t address);
	uint16_t readWord(uint8_t block, uint16_t address);
	void readBuffer(uint8_t block, uint16_t address, uint8_t *buffer, uint16_t length);
	void writeRegister(uint8_t block, uint16_t address, uint8_t value);
	void writeWord(uint8_t block, uint16_t address, uint16_t value);
	void writeBuffer(uint8_t block, uint16_t address, const uint8_t *buffer, uint16_t length);

	bool beginOperation();
	void endOperation();
	bool initialize();
	bool readStableWord(uint8_t block, uint16_t address, uint16_t& value);
	bool issueCommand(uint8_t command, os_w5500::Fault timeout_fault);
	void latchFault(os_w5500::Fault fault);
	void captureSnapshot();
	void resetStallObservation();
	void observeIncompleteFrame(uint16_t rsr, uint16_t rd);
	void completePendingFrame(bool read_data, uint8_t *buffer);
};

using OSWiznet5500lwIP = LwipIntfDev<OSWiznet5500>;

#endif // ESP8266
