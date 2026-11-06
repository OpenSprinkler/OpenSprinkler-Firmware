#include "platform/hostname.h"
#include <WiFi.h>
#include <cassert>
#include <cstring>

TestWiFi WiFi;

int main(int argc, char** argv) {
	assert(argc == 3);
	const uint8_t mac[] = {0xc4, 0x5b, 0xbe, 0x01, 0xa2, 0xff};
	hostname_init(argv[1], mac);
	assert(!strcmp(hostname_effective(), argv[2]));
	assert(hostname_apply_wifi());
	assert(!strcmp(WiFi.STA.name, argv[2]));
	// Clearing /co stores a concrete default, but must not change the active
	// name before reboot (even if the active name is custom).
	char saved_default[HOSTNAME_MAX_LENGTH + 1];
	hostname_default(saved_default, mac);
	assert(!strcmp(saved_default, "os-01a2ff"));
	hostname_init(saved_default, mac);
	assert(!strcmp(hostname_effective(), argv[2]));
	// Reconfiguration/reconnect must not pick up a saved-but-unrebooted change.
	hostname_init("another-name", mac);
	WiFi.STA.name[0] = 0;
	assert(hostname_apply_wifi());
	assert(!strcmp(WiFi.STA.name, argv[2]));
}
