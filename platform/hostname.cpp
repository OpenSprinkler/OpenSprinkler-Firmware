#include "platform/hostname.h"

#include <cstdio>
#include <cstring>

bool hostname_normalize(char* name) {
	const size_t length = strlen(name);
	if (!length) return true;
	if (length > HOSTNAME_MAX_LENGTH || name[0] == '-' || name[length - 1] == '-') return false;
	bool all_digits = true;
	for (size_t i = 0; i < length; ++i) {
		char& c = name[i];
		if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
		if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
		if (c < '0' || c > '9') all_digits = false;
	}
	return !all_digits;
}

void hostname_default(char* destination, const uint8_t mac[6]) {
	snprintf(destination, HOSTNAME_MAX_LENGTH + 1, "os-%02x%02x%02x", mac[3], mac[4], mac[5]);
}

#if defined(ARDUINO)
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

namespace {
char effective_name[HOSTNAME_MAX_LENGTH + 1];
}

void hostname_init(const char* configured, const uint8_t mac[6]) {
	if (effective_name[0]) return;
	if (strlen(configured) <= HOSTNAME_MAX_LENGTH) strcpy(effective_name, configured);
	if (!hostname_normalize(effective_name) || !effective_name[0]) hostname_default(effective_name, mac);
#if defined(ESP32)
	// The core uses this default when it creates/recreates the WiFi interface.
	WiFi.setHostname(effective_name);
#else
	WiFi.hostname(effective_name);
#endif
}

const char* hostname_effective() { return effective_name; }

bool hostname_apply_wifi() {
#if defined(ESP32)
	// Unlike the core's global default (31 chars), the interface accepts all 32.
	return WiFi.STA.setHostname(effective_name);
#else
	// The ESP8266 core shares this name with all lwIP Ethernet interfaces.
	return WiFi.hostname(effective_name);
#endif
}
#endif
