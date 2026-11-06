#pragma once

#include <cstddef>
#include <cstdint>

constexpr size_t HOSTNAME_MAX_LENGTH = 32;

// Empty is a request to reset to the device default. Normalize ASCII uppercase.
bool hostname_normalize(char* name);
void hostname_default(char* destination, const uint8_t mac[6]);

#if defined(ARDUINO)
// Called once at boot, before either interface starts DHCP. Saves do not change it.
void hostname_init(const char* configured, const uint8_t mac[6]);
const char* hostname_effective();
bool hostname_apply_wifi();
#endif
