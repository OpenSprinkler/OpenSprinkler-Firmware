#pragma once

#include <LwipIntfDev.h>

// LwipIntfDev retains its polling callback for the lifetime of the object.
// Guard the raw entry points, not handlePackets() (which is non-virtual).
template<class Raw>
class GatedEthernetRaw : public Raw {
public:
	using Raw::Raw;
	bool begin(const uint8_t* mac) {
		active_ = Raw::begin(mac);
		return active_;
	}
	void end() {
		active_ = false; // Raw::end() may yield to the receive callback.
		Raw::end();
	}
	bool active() const { return active_; }
	bool isLinked() { return active_ && Raw::isLinked(); }
	uint16_t sendFrame(const uint8_t* data, uint16_t size) {
		return active_ ? Raw::sendFrame(data, size) : 0;
	}
	uint16_t readFrame(uint8_t* data, uint16_t size) {
		return active_ ? Raw::readFrame(data, size) : 0;
	}
protected:
	uint16_t readFrameSize() { return active_ ? Raw::readFrameSize() : 0; }
	uint16_t readFrameData(uint8_t* data, uint16_t size) {
		return active_ ? Raw::readFrameData(data, size) : 0;
	}
	void discardFrame(uint16_t size) { if (active_) Raw::discardFrame(size); }
private:
	bool active_ = false;
};

// Boot-time Wi-Fi fallback only: keep the netif and callback storage alive,
// but leave them inert until reboot. Do not register a second polling callback.
template<class Raw>
class ManagedEthernet : public LwipIntfDev<GatedEthernetRaw<Raw>> {
	using Gate = GatedEthernetRaw<Raw>;
	using Base = LwipIntfDev<Gate>;
public:
	using Base::Base;
	bool raw_begin(const uint8_t* mac) {
		if (ended_ || begun_) return false;
		// ENC28J60 retains this address rather than copying it.
		memcpy(this->_macAddress, mac, 6);
		return Gate::begin(this->_macAddress);
	}
	bool begin(const uint8_t* mac) {
		if (ended_ || begun_) return false;
		begun_ = true;
		if (Base::begin(mac)) return true;
		end(); // Includes partially registered netifs on core begin() failure.
		return false;
	}
	void end() {
		if (ended_) return;
		ended_ = true;
		Gate::end();
		this->_default = false;
		this->_netif.status_callback = nullptr;
		if (netif_default == &this->_netif) netif_set_default(nullptr);
		dhcp_stop(&this->_netif);
		netif_set_down(&this->_netif);
		netif_set_link_down(&this->_netif);
		ip4_addr_t zero = {};
		netif_set_addr(&this->_netif, &zero, &zero, &zero);
		this->_started = false;
	}
private:
	bool begun_ = false;
	bool ended_ = false;
};
