#pragma once

#include "defines.h"

#include <cstddef>
#include <cstdint>

constexpr uint32_t BUNDLE_MEMBER_TYPE_MASK = 1UL << STN_TYPE_STANDARD;

bool bundle_member_type_allowed(uint8_t station_type);
bool bundle_decode_hex(const char* encoded, size_t encoded_length,
	uint8_t* members, size_t member_bytes);
bool bundle_encode_hex(const uint8_t* members, size_t member_bytes,
	char* encoded, size_t encoded_size);

bool bundle_is_station(uint8_t sid);
bool bundle_read_members(uint8_t leader_sid, uint8_t* members, size_t member_bytes);
bool bundle_claims_station(uint8_t leader_sid, uint8_t member_sid);
bool bundle_station_is_referenced(uint8_t sid);
bool bundle_validate_definition(uint8_t leader_sid, const char* encoded);
uint16_t bundle_minimum_duration(uint8_t leader_sid);
uint8_t bundle_master_mask(uint8_t leader_sid);
bool bundle_bound_to_master(uint8_t sid, uint8_t master_index);
void bundle_resolve(uint8_t* derived_bits, size_t derived_bytes);
void bundle_invalidate();
