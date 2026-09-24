#include "core/bundle.h"

#include "OpenSprinkler.h"
#include "core/output_sequencer.h"
#include "storage/files.h"

#include <cstring>

extern OpenSprinkler os;

namespace {

uint8_t cached_master_masks[MAX_NUM_STATIONS];
uint8_t cached_master_valid[MAX_NUM_BOARDS];

int8_t hex_value(char value) {
	if (value >= '0' && value <= '9') return value - '0';
	if (value >= 'a' && value <= 'f') return value - 'a' + 10;
	if (value >= 'A' && value <= 'F') return value - 'A' + 10;
	return -1;
}

bool member_is_eligible(uint8_t sid) {
	if (sid >= os.nstations || os.is_master_station(sid)) return false;
	if (os.attrib_dis[sid >> 3] & (uint8_t(1) << (sid & 7))) return false;
	return bundle_member_type_allowed(os.get_station_type(sid));
}

bool membership_bit(const uint8_t* members, uint8_t sid) {
	return members[sid >> 3] & (uint8_t(1) << (sid & 7));
}

void set_membership_bit(uint8_t* members, uint8_t sid) {
	members[sid >> 3] |= uint8_t(1) << (sid & 7);
}

bool cache_valid(uint8_t sid) {
	return cached_master_valid[sid >> 3] & (uint8_t(1) << (sid & 7));
}

void mark_cache_valid(uint8_t sid) {
	cached_master_valid[sid >> 3] |= uint8_t(1) << (sid & 7);
}

} // namespace

bool bundle_member_type_allowed(uint8_t station_type) {
	return station_type < 32 &&
		(BUNDLE_MEMBER_TYPE_MASK & (uint32_t(1) << station_type));
}

bool bundle_decode_hex(const char* encoded, size_t encoded_length,
	uint8_t* members, size_t member_bytes) {
	if (!encoded || !members || encoded_length != member_bytes * 2) return false;
	for (size_t index = 0; index < member_bytes; index++) {
		const int8_t high = hex_value(encoded[index * 2]);
		const int8_t low = hex_value(encoded[index * 2 + 1]);
		if (high < 0 || low < 0) return false;
		members[index] = static_cast<uint8_t>((high << 4) | low);
	}
	return true;
}

bool bundle_encode_hex(const uint8_t* members, size_t member_bytes,
	char* encoded, size_t encoded_size) {
	static const char digits[] = "0123456789ABCDEF";
	if (!members || !encoded || encoded_size < member_bytes * 2 + 1) return false;
	for (size_t index = 0; index < member_bytes; index++) {
		encoded[index * 2] = digits[members[index] >> 4];
		encoded[index * 2 + 1] = digits[members[index] & 0x0F];
	}
	encoded[member_bytes * 2] = 0;
	return true;
}

bool bundle_is_station(uint8_t sid) {
	if (sid >= os.nstations) return false;
	return os.attrib_bundle[sid >> 3] & (uint8_t(1) << (sid & 7));
}

bool bundle_read_members(uint8_t leader_sid, uint8_t* members, size_t member_bytes) {
	if (!members || member_bytes < os.nboards || !bundle_is_station(leader_sid)) return false;
	memset(members, 0, member_bytes);
	char encoded[MAX_NUM_BOARDS * 2 + 1] = {};
	const size_t encoded_length = os.nboards * 2;
	const uint32_t offset = static_cast<uint32_t>(leader_sid) * sizeof(StationData) +
		offsetof(StationData, sped);
	if (!file_read_block(STATIONS_FILENAME, encoded, offset, encoded_length)) return false;
	encoded[encoded_length] = 0;
	// A later zone-expander addition increases nboards. Preserve the existing
	// membership and treat newly available board bytes as zero.
	size_t stored_length = 0;
	while (stored_length < encoded_length && encoded[stored_length]) stored_length++;
	if (stored_length & 1) return false;
	return bundle_decode_hex(encoded, stored_length, members, stored_length / 2);
}

bool bundle_claims_station(uint8_t leader_sid, uint8_t member_sid) {
	if (member_sid >= os.nstations) return false;
	uint8_t members[MAX_NUM_BOARDS] = {};
	return bundle_read_members(leader_sid, members, sizeof(members)) &&
		membership_bit(members, member_sid) && member_is_eligible(member_sid);
}

bool bundle_station_is_referenced(uint8_t sid) {
	if (sid >= os.nstations) return false;
	uint8_t members[MAX_NUM_BOARDS] = {};
	for (uint8_t leader = 0; leader < os.nstations; leader++) {
		if (bundle_read_members(leader, members, sizeof(members)) && membership_bit(members, sid)) {
			return true;
		}
	}
	return false;
}

bool bundle_validate_definition(uint8_t leader_sid, const char* encoded) {
	if (leader_sid >= os.nstations || os.is_master_station(leader_sid) || !encoded) return false;
	uint8_t members[MAX_NUM_BOARDS] = {};
	const size_t encoded_length = strlen(encoded);
	if (!bundle_decode_hex(encoded, encoded_length, members, os.nboards)) return false;
	if (membership_bit(members, leader_sid)) return false;
	for (uint8_t sid = 0; sid < os.nstations; sid++) {
		if (!membership_bit(members, sid)) continue;
		if (os.is_master_station(sid) ||
			!bundle_member_type_allowed(os.get_station_type(sid))) return false;
	}
	return true;
}

uint16_t bundle_minimum_duration(uint8_t leader_sid) {
	if (!bundle_is_station(leader_sid)) return 0;
	uint8_t members[MAX_NUM_BOARDS] = {};
	if (!bundle_read_members(leader_sid, members, sizeof(members))) return 1;
	uint16_t eligible_members = 0;
	for (uint8_t sid = 0; sid < os.nstations; sid++) {
		if (membership_bit(members, sid) && member_is_eligible(sid)) eligible_members++;
	}
	const uint32_t startup_ms = static_cast<uint32_t>(eligible_members) * OUTPUT_STAGGER_MS;
	return static_cast<uint16_t>((startup_ms + 999) / 1000 + 1);
}

uint8_t bundle_master_mask(uint8_t leader_sid) {
	if (!bundle_is_station(leader_sid)) return 0;
	if (cache_valid(leader_sid)) return cached_master_masks[leader_sid];

	uint8_t mask = 0;
	for (uint8_t master = 0; master < NUM_MASTER_ZONES; master++) {
		if (os.bound_to_master(leader_sid, master)) mask |= uint8_t(1) << master;
	}
	uint8_t members[MAX_NUM_BOARDS] = {};
	if (bundle_read_members(leader_sid, members, sizeof(members))) {
		for (uint8_t sid = 0; sid < os.nstations; sid++) {
			if (!membership_bit(members, sid) || !member_is_eligible(sid)) continue;
			for (uint8_t master = 0; master < NUM_MASTER_ZONES; master++) {
				if (os.bound_to_master(sid, master)) mask |= uint8_t(1) << master;
			}
		}
	}
	cached_master_masks[leader_sid] = mask;
	mark_cache_valid(leader_sid);
	return mask;
}

bool bundle_bound_to_master(uint8_t sid, uint8_t master_index) {
	if (master_index >= NUM_MASTER_ZONES) return false;
	if (!bundle_is_station(sid)) return os.bound_to_master(sid, master_index);
	return bundle_master_mask(sid) & (uint8_t(1) << master_index);
}

void bundle_resolve(uint8_t* derived_bits, size_t derived_bytes) {
	if (!derived_bits || derived_bytes < os.nboards) return;
	memset(derived_bits, 0, derived_bytes);
	uint8_t members[MAX_NUM_BOARDS] = {};
	for (uint8_t leader = 0; leader < os.nstations; leader++) {
		if (!bundle_is_station(leader) || !os.is_running(leader) ||
			!os.get_applied_station_bit(leader)) continue;
		if (!bundle_read_members(leader, members, sizeof(members))) continue;
		for (uint8_t sid = 0; sid < os.nstations; sid++) {
			if (membership_bit(members, sid) && member_is_eligible(sid)) {
				set_membership_bit(derived_bits, sid);
			}
		}
	}
}

void bundle_invalidate() {
	memset(cached_master_valid, 0, sizeof(cached_master_valid));
	os.mark_bundle_dirty();
}
