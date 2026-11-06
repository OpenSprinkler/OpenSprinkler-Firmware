#pragma once

#include <stdint.h>
#include <string.h>

// Minimal lifecycle model of the ESP8266 core. In particular, its recurring
// callback calls Raw::readFrameSize even after _started becomes false.
struct ip4_addr_t { uint32_t addr; };
struct netif {
	bool up = false, link = false, dhcp = false;
	ip4_addr_t ip = {}, mask = {}, gateway = {};
	void (*status_callback)(netif*) = nullptr;
};
static netif* netif_default = nullptr;
inline void netif_set_default(netif* n) { netif_default = n; }
inline void dhcp_stop(netif* n) { n->dhcp = false; }
inline void netif_set_down(netif* n) { n->up = false; }
inline void netif_set_link_down(netif* n) { n->link = false; }
inline void netif_set_addr(netif* n, const ip4_addr_t* ip,
	const ip4_addr_t* mask, const ip4_addr_t* gateway) {
	n->ip = *ip; n->mask = *mask; n->gateway = *gateway;
}

template<class Raw>
class LwipIntfDev : public Raw {
public:
	using Raw::Raw;
	bool fail_registration = false;
	bool begin(const uint8_t* mac) {
		memcpy(_macAddress, mac, 6);
		if (!Raw::begin(_macAddress)) return false;
		_netif.up = _netif.link = _netif.dhcp = true;
		_netif.ip.addr = 123;
		_netif.status_callback = [](netif*) {};
		netif_default = &_netif;
		if (fail_registration) return false;
		_started = true;
		return true;
	}
	uint16_t poll() { return Raw::readFrameSize(); }
	const netif* getNetIf() const { return &_netif; }
	bool started() const { return _started; }
protected:
	netif _netif;
	uint8_t _macAddress[6] = {};
	bool _started = false, _default = false;
};
