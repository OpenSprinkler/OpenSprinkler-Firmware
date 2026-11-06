#include <assert.h>
#include <stdio.h>
#include "drivers/ethernet_lifecycle.h"

struct FakeRaw {
	unsigned begins = 0, ends = 0, reads = 0, writes = 0, links = 0;
	bool begin_ok = true;
	const uint8_t* retained_mac = nullptr;
	void (*on_end)() = nullptr;
	bool begin(const uint8_t* mac) {
		++begins; retained_mac = mac; return begin_ok;
	}
	void end() { ++ends; if (on_end) on_end(); }
	bool isLinked() { ++links; return true; }
	uint16_t sendFrame(const uint8_t*, uint16_t n) { ++writes; return n; }
	uint16_t readFrame(uint8_t*, uint16_t n) { ++reads; return n; }
protected:
	uint16_t readFrameSize() { ++reads; return 64; }
	uint16_t readFrameData(uint8_t*, uint16_t n) { ++reads; return n; }
	void discardFrame(uint16_t) { ++reads; }
};

using Device = ManagedEthernet<FakeRaw>;
static Device* yielding_device;
static void poll_during_end() { assert(yielding_device->poll() == 0); }

static void assert_stopped(Device& d) {
	const netif* n = d.getNetIf();
	assert(!n->up && !n->link && !n->dhcp && !n->ip.addr);
	assert(!n->status_callback && netif_default != n && !d.started());
	unsigned reads = d.reads, writes = d.writes, links = d.links;
	assert(d.poll() == 0 && !d.isLinked());
	assert(d.sendFrame(nullptr, 64) == 0 && d.readFrame(nullptr, 64) == 0);
	assert(d.reads == reads && d.writes == writes && d.links == links);
	unsigned ends = d.ends;
	d.end();
	assert(d.ends == ends); // Idempotent; no duplicate callback registrations.
	uint8_t mac[6] = {};
	assert(!d.begin(mac) && !d.raw_begin(mac));
}

int main() {
	uint8_t mac[] = {2, 3, 4, 5, 6, 7};
	Device no_link;
	assert(no_link.raw_begin(mac));
	assert(no_link.retained_mac != mac && !memcmp(no_link.retained_mac, mac, 6));
	assert(!no_link.started() && !no_link.getNetIf()->dhcp && !netif_default);
	no_link.end();
	assert_stopped(no_link);

	Device dhcp_timeout;
	assert(dhcp_timeout.raw_begin(mac) && dhcp_timeout.begin(mac));
	assert(dhcp_timeout.begins == 2 && dhcp_timeout.poll() == 64);
	yielding_device = &dhcp_timeout;
	dhcp_timeout.on_end = poll_during_end;
	dhcp_timeout.end();
	assert_stopped(dhcp_timeout);

	Device failed_begin;
	failed_begin.fail_registration = true;
	assert(!failed_begin.begin(mac));
	assert_stopped(failed_begin);

	Device other_route;
	assert(other_route.begin(mac));
	netif wifi;
	netif_default = &wifi;
	other_route.end();
	assert_stopped(other_route);
	assert(netif_default == &wifi); // Never clear another interface's route.
	netif_default = nullptr;

	Device no_chip;
	no_chip.begin_ok = false;
	assert(!no_chip.raw_begin(mac));
	no_chip.end();
	assert_stopped(no_chip);
	puts("ETHERNET LIFECYCLE TEST PASS");
}
