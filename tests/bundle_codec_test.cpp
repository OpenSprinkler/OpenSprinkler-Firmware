#include "core/bundle.h"

#include <cassert>
#include <cstdint>
#include <cstring>

int main() {
	assert(BUNDLE_MEMBER_TYPE_MASK == 1UL);
	for (uint8_t type = STN_TYPE_STANDARD; type <= STN_TYPE_BUNDLE; type++) {
		const bool advertised = BUNDLE_MEMBER_TYPE_MASK & (uint32_t(1) << type);
		assert(bundle_member_type_allowed(type) == advertised);
	}
	assert(!bundle_member_type_allowed(32));
	assert(!bundle_member_type_allowed(STN_TYPE_OTHER));

	uint8_t members[3] = {};
	assert(bundle_decode_hex("0180FF", 6, members, 3));
	assert(members[0] == 0x01 && members[1] == 0x80 && members[2] == 0xFF);
	assert(!bundle_decode_hex("0180F", 5, members, 3));
	assert(!bundle_decode_hex("0180XZ", 6, members, 3));

	char encoded[7] = {};
	assert(bundle_encode_hex(members, 3, encoded, sizeof(encoded)));
	assert(strcmp(encoded, "0180FF") == 0);
	assert(!bundle_encode_hex(members, 3, encoded, 6));
	return 0;
}
