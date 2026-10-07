#include "platform/hostname.h"
#include "defines.h"

#include <cassert>
#include <cstring>
#include <string>

int main() {
	static_assert(SOPT_HOSTNAME == 13, "Preserve existing string-option offsets");
	for (const char* valid : {"", "a", "OS-A1B2C3", "1-zone", "12-34", "a--b"}) {
		char name[64];
		strcpy(name, valid);
		assert(hostname_normalize(name));
	}
	for (const char* invalid : {"-a", "a-", "123", "0", "a.local", "a_b", "a b", "a/b", "a\n", "\xc3\xa9"}) {
		char name[64];
		strcpy(name, invalid);
		assert(!hostname_normalize(name));
	}
	char name[64];
	strcpy(name, "OS-A1B2C3");
	assert(hostname_normalize(name) && !strcmp(name, "os-a1b2c3"));
	memset(name, 'a', 32); name[32] = 0;
	assert(hostname_normalize(name));
	name[32] = 'a'; name[33] = 0;
	assert(!hostname_normalize(name));
	const uint8_t mac[] = {0xc4, 0x5b, 0xbe, 0x01, 0xa2, 0xff};
	hostname_default(name, mac);
	assert(!strcmp(name, "os-01a2ff"));
}
