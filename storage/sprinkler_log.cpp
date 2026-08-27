#include "storage/sprinkler_log.h"

#include "defines.h"
#include "storage/files.h"
#include "storage/maintenance.h"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>

#if defined(ARDUINO)
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

extern char tmp_buffer[];

namespace {

constexpr const char* RING_INDEX_FILENAME = LOG_DIR "spr.hdr";
constexpr const char* RING_INDEX_TEMP_FILENAME = LOG_DIR "spr.tmp";
constexpr const char* SEGMENT_PREFIX = LOG_DIR "spr.log";
constexpr uint8_t MAX_INDEX_UPDATES = 2;

struct DescriptorUpdate {
	uint16_t slot;
	SprinklerLogDescriptor descriptor;
};

struct ActiveCache {
	bool valid;
	uint16_t slot;
	uint32_t generation;
	uint16_t count;
	uint32_t min_timestamp;
	uint32_t max_timestamp;
	bool closed;
};

bool ring_cache_valid = false;
SprinklerLogRingHeader ring_cache = {};
ActiveCache active_cache = {};

void cooperative_yield() {
#if defined(ARDUINO)
	yield();
#endif
}

void encode_u16(uint8_t* output, uint16_t value) {
	output[0] = (uint8_t)value;
	output[1] = (uint8_t)(value >> 8);
}

void encode_u32(uint8_t* output, uint32_t value) {
	output[0] = (uint8_t)value;
	output[1] = (uint8_t)(value >> 8);
	output[2] = (uint8_t)(value >> 16);
	output[3] = (uint8_t)(value >> 24);
}

uint16_t decode_u16(const uint8_t* input) {
	return (uint16_t)input[0] | ((uint16_t)input[1] << 8);
}

uint32_t decode_u32(const uint8_t* input) {
	return (uint32_t)input[0] |
		((uint32_t)input[1] << 8) |
		((uint32_t)input[2] << 16) |
		((uint32_t)input[3] << 24);
}

void encode_segment_header(const SprinklerLogSegmentHeader& header, uint8_t output[16]) {
	encode_u16(output, header.magic);
	output[2] = header.version;
	output[3] = header.flags;
	encode_u32(output + 4, header.generation);
	encode_u32(output + 8, header.min_timestamp);
	encode_u32(output + 12, header.max_timestamp);
}

void decode_segment_header(const uint8_t input[16], SprinklerLogSegmentHeader& header) {
	header.magic = decode_u16(input);
	header.version = input[2];
	header.flags = input[3];
	header.generation = decode_u32(input + 4);
	header.min_timestamp = decode_u32(input + 8);
	header.max_timestamp = decode_u32(input + 12);
}

void encode_ring_header(const SprinklerLogRingHeader& header, uint8_t output[16]) {
	encode_u16(output, header.magic);
	output[2] = header.version;
	output[3] = header.flags;
	encode_u16(output + 4, header.max_files);
	encode_u16(output + 6, header.records_per_file);
	encode_u16(output + 8, header.active_slot);
	encode_u16(output + 10, header.reserved);
	encode_u32(output + 12, header.next_generation);
}

void decode_ring_header(const uint8_t input[16], SprinklerLogRingHeader& header) {
	header.magic = decode_u16(input);
	header.version = input[2];
	header.flags = input[3];
	header.max_files = decode_u16(input + 4);
	header.records_per_file = decode_u16(input + 6);
	header.active_slot = decode_u16(input + 8);
	header.reserved = decode_u16(input + 10);
	header.next_generation = decode_u32(input + 12);
}

void encode_descriptor(const SprinklerLogDescriptor& descriptor, uint8_t output[16]) {
	encode_u32(output, descriptor.generation);
	encode_u32(output + 4, descriptor.min_timestamp);
	encode_u32(output + 8, descriptor.max_timestamp);
	encode_u16(output + 12, descriptor.record_count);
	output[14] = descriptor.flags;
	output[15] = descriptor.reserved;
}

void decode_descriptor(const uint8_t input[16], SprinklerLogDescriptor& descriptor) {
	descriptor.generation = decode_u32(input);
	descriptor.min_timestamp = decode_u32(input + 4);
	descriptor.max_timestamp = decode_u32(input + 8);
	descriptor.record_count = decode_u16(input + 12);
	descriptor.flags = input[14];
	descriptor.reserved = input[15];
}

bool valid_record_type(uint8_t type) {
	switch (type) {
	case LOGDATA_STATION:
	case LOGDATA_SENSOR1:
	case LOGDATA_RAINDELAY:
	case LOGDATA_WATERLEVEL:
	case LOGDATA_FLOWSENSE:
	case LOGDATA_SENSOR2:
	case LOGDATA_SENSOR3:
	case LOGDATA_SENSOR4:
	case LOGDATA_CURRENT:
		return true;
	default:
		return false;
	}
}

bool valid_ring_header(const SprinklerLogRingHeader& header) {
	return header.magic == SPRINKLER_LOG_MAGIC &&
		header.version == SPRINKLER_LOG_VERSION &&
		header.max_files == SPRINKLER_LOG_MAX_FILES &&
		header.records_per_file == SPRINKLER_LOG_RECORDS_PER_FILE &&
		header.active_slot < header.max_files && header.next_generation > 0;
}

bool valid_segment_header(const SprinklerLogSegmentHeader& header) {
	return header.magic == SPRINKLER_LOG_MAGIC &&
		header.version == SPRINKLER_LOG_VERSION && header.generation > 0;
}

void segment_filename(uint16_t slot, char* output, size_t output_size) {
	snprintf(output, output_size, "%s%03u", SEGMENT_PREFIX, slot);
}

bool read_ring_header(SprinklerLogRingHeader& header) {
	uint8_t bytes[16];
	os_file_type file = file_open(RING_INDEX_FILENAME, FileOpenMode::Read);
	if (!file) return false;
	bool ok = file_read(file, bytes, sizeof(bytes)) == (int)sizeof(bytes);
	file_close(file);
	if (!ok) return false;
	decode_ring_header(bytes, header);
	return valid_ring_header(header);
}

bool read_descriptor_from_file(os_file_type file, uint16_t slot,
	SprinklerLogDescriptor& descriptor) {
	if (!file || slot >= SPRINKLER_LOG_MAX_FILES) return false;
	uint8_t bytes[16];
	if (!file_seek(file, sizeof(SprinklerLogRingHeader) +
		(uint32_t)slot * sizeof(SprinklerLogDescriptor)) ||
		file_read(file, bytes, sizeof(bytes)) != (int)sizeof(bytes)) return false;
	decode_descriptor(bytes, descriptor);
	return true;
}

bool read_descriptor(uint16_t slot, SprinklerLogDescriptor& descriptor) {
	if (slot >= SPRINKLER_LOG_MAX_FILES) return false;
	os_file_type file = file_open(RING_INDEX_FILENAME, FileOpenMode::Read);
	if (!file) return false;
	bool ok = read_descriptor_from_file(file, slot, descriptor);
	file_close(file);
	return ok;
}

bool read_segment_header(uint16_t slot, SprinklerLogSegmentHeader& header) {
	char filename[24];
	uint8_t bytes[16];
	segment_filename(slot, filename, sizeof(filename));
	if (!file_read_block(filename, bytes, 0, sizeof(bytes))) return false;
	decode_segment_header(bytes, header);
	return valid_segment_header(header);
}

bool write_segment_header(uint16_t slot, const SprinklerLogSegmentHeader& header) {
	char filename[24];
	uint8_t bytes[16];
	segment_filename(slot, filename, sizeof(filename));
	encode_segment_header(header, bytes);
	return file_write_block(filename, bytes, 0, sizeof(bytes));
}

bool create_segment(uint16_t slot, uint32_t generation) {
	char filename[24];
	segment_filename(slot, filename, sizeof(filename));
	remove_file(filename);
	SprinklerLogSegmentHeader header = {};
	header.magic = SPRINKLER_LOG_MAGIC;
	header.version = SPRINKLER_LOG_VERSION;
	header.generation = generation;
	uint8_t bytes[16];
	encode_segment_header(header, bytes);
	os_file_type file = file_open(filename, FileOpenMode::WriteTruncate);
	if (!file) return false;
	bool ok = file_write(file, bytes, sizeof(bytes)) == (int)sizeof(bytes);
	file_close(file);
	return ok;
}

bool inspect_segment(uint16_t slot, SprinklerLogDescriptor& descriptor,
	SprinklerLogSegmentHeader* output_header = nullptr) {
	char filename[24];
	segment_filename(slot, filename, sizeof(filename));
	os_file_type file = file_open(filename, FileOpenMode::Read);
	if (!file) return false;
	uint8_t bytes[16];
	if (file_read(file, bytes, sizeof(bytes)) != (int)sizeof(bytes)) {
		file_close(file);
		return false;
	}
	SprinklerLogSegmentHeader header = {};
	decode_segment_header(bytes, header);
	if (!valid_segment_header(header)) {
		file_close(file);
		return false;
	}

	uint32_t size = file_size(file);
	uint32_t payload_size = size >= sizeof(header) ? size - sizeof(header) : 0;
	uint32_t count = payload_size / sizeof(SprinklerLogRecord);
	if (count > SPRINKLER_LOG_RECORDS_PER_FILE) count = SPRINKLER_LOG_RECORDS_PER_FILE;
	uint32_t valid_size = sizeof(header) + count * sizeof(SprinklerLogRecord);
	bool truncate_tail = size != valid_size;
	uint32_t min_timestamp = 0;
	uint32_t max_timestamp = 0;
	for (uint16_t i = 0; i < count; i++) {
		if ((i & 0x1f) == 0) cooperative_yield();
		uint8_t record_bytes[16];
		SprinklerLogRecord record = {};
		if (file_read(file, record_bytes, sizeof(record_bytes)) != (int)sizeof(record_bytes) ||
			!sprinkler_log_decode_record(record_bytes, record) || record.timestamp == 0) continue;
		if (min_timestamp == 0 || record.timestamp < min_timestamp) min_timestamp = record.timestamp;
		if (record.timestamp > max_timestamp) max_timestamp = record.timestamp;
	}
	file_close(file);
	if (truncate_tail && !truncate_file(filename, valid_size)) return false;

	descriptor = {};
	descriptor.generation = header.generation;
	descriptor.min_timestamp = min_timestamp;
	descriptor.max_timestamp = max_timestamp;
	descriptor.record_count = (uint16_t)count;
	descriptor.flags = SPRINKLER_DESCRIPTOR_FLAG_VALID;
	if (header.flags & SPRINKLER_SEGMENT_FLAG_CLOSED)
		descriptor.flags |= SPRINKLER_DESCRIPTOR_FLAG_CLOSED;
	if (output_header) *output_header = header;
	return true;
}

const SprinklerLogDescriptor* find_update(const DescriptorUpdate* updates,
	uint8_t update_count, uint16_t slot) {
	for (uint8_t i = 0; i < update_count; i++) {
		if (updates[i].slot == slot) return &updates[i].descriptor;
	}
	return nullptr;
}

bool write_index_atomic(const SprinklerLogRingHeader& header,
	const DescriptorUpdate* updates, uint8_t update_count, bool preserve_existing) {
	remove_file(RING_INDEX_TEMP_FILENAME);
	os_file_type output = file_open(RING_INDEX_TEMP_FILENAME, FileOpenMode::WriteTruncate);
	if (!output) return false;
	os_file_type input = preserve_existing ? file_open(RING_INDEX_FILENAME, FileOpenMode::Read) : os_file_type();
	uint8_t bytes[16];
	encode_ring_header(header, bytes);
	bool ok = file_write(output, bytes, sizeof(bytes)) == (int)sizeof(bytes);

	for (uint16_t slot = 0; ok && slot < SPRINKLER_LOG_MAX_FILES; slot++) {
		cooperative_yield();
		SprinklerLogDescriptor descriptor = {};
		const SprinklerLogDescriptor* update = find_update(updates, update_count, slot);
		if (update) {
			descriptor = *update;
		} else if (input) {
			if (file_seek(input, sizeof(SprinklerLogRingHeader) +
				(uint32_t)slot * sizeof(SprinklerLogDescriptor)) &&
				file_read(input, bytes, sizeof(bytes)) == (int)sizeof(bytes)) {
				decode_descriptor(bytes, descriptor);
			}
		}
		encode_descriptor(descriptor, bytes);
		ok = file_write(output, bytes, sizeof(bytes)) == (int)sizeof(bytes);
	}
	if (input) file_close(input);
	file_close(output);
	if (!ok) {
		remove_file(RING_INDEX_TEMP_FILENAME);
		return false;
	}
	if (!rename_file(RING_INDEX_TEMP_FILENAME, RING_INDEX_FILENAME)) {
		remove_file(RING_INDEX_TEMP_FILENAME);
		return false;
	}
	return true;
}

bool write_rebuilt_index(const SprinklerLogRingHeader& header) {
	remove_file(RING_INDEX_TEMP_FILENAME);
	os_file_type output = file_open(RING_INDEX_TEMP_FILENAME, FileOpenMode::WriteTruncate);
	if (!output) return false;
	uint8_t bytes[16];
	encode_ring_header(header, bytes);
	bool ok = file_write(output, bytes, sizeof(bytes)) == (int)sizeof(bytes);
	for (uint16_t slot = 0; ok && slot < SPRINKLER_LOG_MAX_FILES; slot++) {
		cooperative_yield();
		SprinklerLogDescriptor descriptor = {};
		inspect_segment(slot, descriptor);
		encode_descriptor(descriptor, bytes);
		ok = file_write(output, bytes, sizeof(bytes)) == (int)sizeof(bytes);
	}
	file_close(output);
	if (!ok || !rename_file(RING_INDEX_TEMP_FILENAME, RING_INDEX_FILENAME)) {
		remove_file(RING_INDEX_TEMP_FILENAME);
		return false;
	}
	return true;
}

bool rebuild_ring_state(SprinklerLogRingHeader& header, bool persist) {
	uint32_t highest_generation = 0;
	uint16_t active_slot = 0;
	for (uint16_t slot = 0; slot < SPRINKLER_LOG_MAX_FILES; slot++) {
		cooperative_yield();
		SprinklerLogDescriptor descriptor = {};
		if (!inspect_segment(slot, descriptor)) continue;
		if (descriptor.generation > highest_generation) {
			highest_generation = descriptor.generation;
			active_slot = slot;
		}
	}
	if (highest_generation == 0) return false;
	header = {};
	header.magic = SPRINKLER_LOG_MAGIC;
	header.version = SPRINKLER_LOG_VERSION;
	header.max_files = SPRINKLER_LOG_MAX_FILES;
	header.records_per_file = SPRINKLER_LOG_RECORDS_PER_FILE;
	header.active_slot = active_slot;
	header.next_generation = highest_generation + 1;
	if (header.next_generation == 0) header.next_generation = 1;
	return !persist || write_rebuilt_index(header);
}

bool load_ring_state(SprinklerLogRingHeader& header, bool persist_recovery) {
	if (ring_cache_valid) {
		header = ring_cache;
		return true;
	}
	if (!read_ring_header(header)) {
		if (!rebuild_ring_state(header, persist_recovery)) return false;
	}
	SprinklerLogDescriptor descriptor = {};
	SprinklerLogSegmentHeader segment = {};
	if (!read_descriptor(header.active_slot, descriptor) ||
		!(descriptor.flags & SPRINKLER_DESCRIPTOR_FLAG_VALID) ||
		!read_segment_header(header.active_slot, segment) ||
		segment.generation != descriptor.generation) {
		if (!rebuild_ring_state(header, persist_recovery)) return false;
	}
	ring_cache = header;
	ring_cache_valid = true;
	return true;
}

bool load_active_cache(const SprinklerLogRingHeader& header) {
	if (active_cache.valid && active_cache.slot == header.active_slot) return true;
	SprinklerLogDescriptor descriptor = {};
	SprinklerLogSegmentHeader segment = {};
	if (!inspect_segment(header.active_slot, descriptor, &segment)) return false;
	active_cache = {};
	active_cache.valid = true;
	active_cache.slot = header.active_slot;
	active_cache.generation = segment.generation;
	active_cache.count = descriptor.record_count;
	active_cache.min_timestamp = descriptor.min_timestamp;
	active_cache.max_timestamp = descriptor.max_timestamp;
	active_cache.closed = (segment.flags & SPRINKLER_SEGMENT_FLAG_CLOSED) != 0;
	return true;
}

SprinklerLogDescriptor active_descriptor() {
	SprinklerLogDescriptor descriptor = {};
	descriptor.generation = active_cache.generation;
	descriptor.min_timestamp = active_cache.min_timestamp;
	descriptor.max_timestamp = active_cache.max_timestamp;
	descriptor.record_count = active_cache.count;
	descriptor.flags = SPRINKLER_DESCRIPTOR_FLAG_VALID;
	if (active_cache.closed) descriptor.flags |= SPRINKLER_DESCRIPTOR_FLAG_CLOSED;
	return descriptor;
}

bool initialize_ring(SprinklerLogRingHeader& header) {
	if (!ensure_log_dir() || !prepare_log_write(true)) return false;
	header = {};
	header.magic = SPRINKLER_LOG_MAGIC;
	header.version = SPRINKLER_LOG_VERSION;
	header.max_files = SPRINKLER_LOG_MAX_FILES;
	header.records_per_file = SPRINKLER_LOG_RECORDS_PER_FILE;
	header.active_slot = 0;
	header.next_generation = 2;
	if (!create_segment(0, 1)) return false;
	SprinklerLogDescriptor descriptor = {};
	descriptor.generation = 1;
	descriptor.flags = SPRINKLER_DESCRIPTOR_FLAG_VALID;
	DescriptorUpdate update = {0, descriptor};
	if (!write_index_atomic(header, &update, 1, false)) {
		char filename[24];
		segment_filename(0, filename, sizeof(filename));
		remove_file(filename);
		return false;
	}
	ring_cache = header;
	ring_cache_valid = true;
	active_cache = {};
	return load_active_cache(header);
}

bool finalize_active(SprinklerLogRingHeader& header) {
	if (!load_active_cache(header)) return false;
	SprinklerLogSegmentHeader segment = {};
	if (!read_segment_header(header.active_slot, segment)) return false;
	segment.flags |= SPRINKLER_SEGMENT_FLAG_CLOSED;
	segment.min_timestamp = active_cache.min_timestamp;
	segment.max_timestamp = active_cache.max_timestamp;
	if (!write_segment_header(header.active_slot, segment)) return false;
	active_cache.closed = true;
	DescriptorUpdate update = {header.active_slot, active_descriptor()};
	return write_index_atomic(header, &update, 1, true);
}

bool rotate_active(SprinklerLogRingHeader& header) {
	if (!load_active_cache(header)) return false;
	if (!active_cache.closed && !finalize_active(header)) return false;
	uint16_t next_slot = (uint16_t)((header.active_slot + 1) % header.max_files);
	char filename[24];
	segment_filename(next_slot, filename, sizeof(filename));
	remove_file(filename); // Evict the oldest slot before asking for new free space.
	if (!prepare_log_write(true)) return false;
	uint32_t generation = header.next_generation ? header.next_generation : 1;
	if (!create_segment(next_slot, generation)) return false;

	SprinklerLogDescriptor old_descriptor = active_descriptor();
	SprinklerLogDescriptor new_descriptor = {};
	new_descriptor.generation = generation;
	new_descriptor.flags = SPRINKLER_DESCRIPTOR_FLAG_VALID;
	DescriptorUpdate updates[MAX_INDEX_UPDATES] = {
		{header.active_slot, old_descriptor},
		{next_slot, new_descriptor},
	};
	header.active_slot = next_slot;
	header.next_generation = generation + 1;
	if (header.next_generation == 0) header.next_generation = 1;
	if (!write_index_atomic(header, updates, MAX_INDEX_UPDATES, true)) return false;
	ring_cache = header;
	active_cache = {};
	return load_active_cache(header);
}

bool rewrite_matching_records(uint16_t slot, uint32_t first_timestamp,
	uint32_t last_timestamp, bool before_mode, bool& changed) {
	char filename[24];
	segment_filename(slot, filename, sizeof(filename));
	os_file_type file = file_open(filename, FileOpenMode::ReadWrite);
	if (!file) return false;
	uint32_t size = file_size(file);
	uint32_t count = size >= sizeof(SprinklerLogSegmentHeader) ?
		(size - sizeof(SprinklerLogSegmentHeader)) / sizeof(SprinklerLogRecord) : 0;
	if (count > SPRINKLER_LOG_RECORDS_PER_FILE) count = SPRINKLER_LOG_RECORDS_PER_FILE;
	constexpr uint16_t batch_capacity = TMP_BUFFER_SIZE / sizeof(SprinklerLogRecord);
	changed = false;
	for (uint32_t first = 0; first < count;) {
		uint16_t batch = (uint16_t)std::min<uint32_t>(batch_capacity, count - first);
		uint32_t offset = sizeof(SprinklerLogSegmentHeader) +
			first * sizeof(SprinklerLogRecord);
		uint32_t bytes = (uint32_t)batch * sizeof(SprinklerLogRecord);
		if (!file_seek(file, offset) || file_read(file, tmp_buffer, bytes) != (int)bytes) {
			file_close(file);
			return false;
		}
		bool batch_changed = false;
		for (uint16_t i = 0; i < batch; i++) {
			SprinklerLogRecord record = {};
			uint8_t* encoded = (uint8_t*)tmp_buffer + i * sizeof(SprinklerLogRecord);
			if (!sprinkler_log_decode_record(encoded, record)) continue;
			bool match = before_mode ? record.timestamp < first_timestamp :
				record.timestamp >= first_timestamp && record.timestamp < last_timestamp;
			if (!match || (record.flags & SPRINKLER_LOG_FLAG_TOMBSTONE)) continue;
			record.flags |= SPRINKLER_LOG_FLAG_TOMBSTONE;
			sprinkler_log_encode_record(record, encoded);
			batch_changed = true;
			changed = true;
		}
		if (batch_changed && (!file_seek(file, offset) ||
			file_write(file, tmp_buffer, bytes) != (int)bytes)) {
			file_close(file);
			return false;
		}
		first += batch;
		#if defined(ARDUINO)
		yield();
		#endif
	}
	file_close(file);
	return true;
}

uint32_t file_allocation(uint32_t size, uint32_t block_size) {
	if (size == 0 || block_size == 0) return 0;
	if (size <= block_size) return block_size;
	// Every additional CTZ data block uses at least one four-byte back-pointer.
	return ((size + 4 + block_size - 1) / block_size) * block_size;
}

bool special_type_from_name(const char* name, uint8_t& type) {
	if (strncmp(name, "s1", 2) == 0) type = LOGDATA_SENSOR1;
	else if (strncmp(name, "rd", 2) == 0) type = LOGDATA_RAINDELAY;
	else if (strncmp(name, "wl", 2) == 0) type = LOGDATA_WATERLEVEL;
	else if (strncmp(name, "fl", 2) == 0) type = LOGDATA_FLOWSENSE;
	else if (strncmp(name, "s2", 2) == 0) type = LOGDATA_SENSOR2;
	else if (strncmp(name, "s3", 2) == 0) type = LOGDATA_SENSOR3;
	else if (strncmp(name, "s4", 2) == 0) type = LOGDATA_SENSOR4;
	else if (strncmp(name, "cu", 2) == 0) type = LOGDATA_CURRENT;
	else return false;
	return true;
}

bool parse_uint32(char*& cursor, uint32_t& value) {
	while (*cursor == ' ' || *cursor == '\t') cursor++;
	char* end = nullptr;
	unsigned long parsed = strtoul(cursor, &end, 10);
	if (end == cursor || parsed > UINT32_MAX) return false;
	value = (uint32_t)parsed;
	cursor = end;
	return true;
}

bool consume_char(char*& cursor, char expected) {
	while (*cursor == ' ' || *cursor == '\t') cursor++;
	if (*cursor != expected) return false;
	cursor++;
	return true;
}

bool parse_legacy_record(char* line, SprinklerLogRecord& record) {
	record = {};
	char* cursor = line;
	if (!consume_char(cursor, '[')) return false;
	uint32_t first = 0;
	if (!parse_uint32(cursor, first) || !consume_char(cursor, ',')) return false;
	while (*cursor == ' ' || *cursor == '\t') cursor++;
	if (*cursor == '"') {
		cursor++;
		if (!special_type_from_name(cursor, record.type)) return false;
		cursor += 2;
		if (!consume_char(cursor, '"') || !consume_char(cursor, ',')) return false;
		uint32_t value = 0;
		uint32_t timestamp = 0;
		record.aux = first;
		if (!parse_uint32(cursor, value) || !consume_char(cursor, ',') ||
			!parse_uint32(cursor, timestamp)) return false;
		record.value = value;
		record.timestamp = timestamp;
	} else {
		uint32_t station = 0;
		uint32_t duration = 0;
		uint32_t timestamp = 0;
		if (!parse_uint32(cursor, station) || !consume_char(cursor, ',') ||
			!parse_uint32(cursor, duration) || !consume_char(cursor, ',') ||
			!parse_uint32(cursor, timestamp) || first > UINT8_MAX || station > UINT8_MAX) return false;
		record.type = LOGDATA_STATION;
		record.program = (uint8_t)first;
		record.station = (uint8_t)station;
		record.value = duration;
		record.timestamp = timestamp;
		while (*cursor == ' ' || *cursor == '\t') cursor++;
		if (*cursor == ',') {
			cursor++;
			char* end = nullptr;
			float flow = strtof(cursor, &end);
			if (end == cursor) return false;
			record.flags |= SPRINKLER_LOG_FLAG_FLOW;
			float scaled = flow * 100.0f;
			if (scaled > 0.0f)
				record.aux = scaled >= 4294967295.0f ? UINT32_MAX : (uint32_t)(scaled + 0.5f);
			cursor = end;
		}
	}
	return record.timestamp != 0;
}

bool find_next_legacy_day(uint32_t from_day, uint32_t end_day, uint32_t& result) {
	bool found = false;
	uint32_t best = UINT32_MAX;
#if defined(ESP8266)
	Dir dir = LittleFS.openDir(LOG_DIR);
	while (dir.next()) {
		cooperative_yield();
		String name = dir.fileName();
		if (!is_sprinkler_log_filename(name.c_str())) continue;
		uint32_t day = (uint32_t)strtoul(name.c_str(), nullptr, 10);
		if (day >= from_day && day <= end_day && day < best) { best = day; found = true; }
	}
	#elif defined(ESP32)
	File dir = LittleFS.open(LOG_DIR);
	if (!dir || !dir.isDirectory()) return false;
	for (File file = dir.openNextFile(); file; file = dir.openNextFile()) {
		cooperative_yield();
		String name = file.name();
		file.close();
		if (!is_sprinkler_log_filename(name.c_str())) continue;
		const char* base = strrchr(name.c_str(), '/');
		base = base ? base + 1 : name.c_str();
		uint32_t day = (uint32_t)strtoul(base, nullptr, 10);
		if (day >= from_day && day <= end_day && day < best) { best = day; found = true; }
	}
	dir.close();
	#else
	char directory_name[PATH_MAX];
	strncpy(directory_name, get_filename_fullpath(LOG_DIR), sizeof(directory_name) - 1);
	directory_name[sizeof(directory_name) - 1] = 0;
	DIR* dir = opendir(directory_name);
	if (!dir) return false;
	struct dirent* entry;
	while ((entry = readdir(dir)) != nullptr) {
		if (!is_sprinkler_log_filename(entry->d_name)) continue;
		uint32_t day = (uint32_t)strtoul(entry->d_name, nullptr, 10);
		if (day >= from_day && day <= end_day && day < best) { best = day; found = true; }
	}
	closedir(dir);
	#endif
	if (found) result = best;
	return found;
}

void legacy_filename(uint32_t day, char* output, size_t output_size) {
	snprintf(output, output_size, "%s%lu.txt", LOG_DIR, (unsigned long)day);
}

uint32_t day_start_timestamp(uint32_t day) {
	constexpr uint32_t max_day = UINT32_MAX / 86400UL;
	return day >= max_day ? max_day * 86400UL : day * 86400UL;
}

uint32_t day_end_timestamp(uint32_t day) {
	constexpr uint32_t max_day = UINT32_MAX / 86400UL;
	return day >= max_day ? UINT32_MAX : (day + 1) * 86400UL;
}

class LegacyIterator {
public:
	LegacyIterator(uint32_t start_day, uint32_t end_day, uint32_t cursor_day, uint32_t cursor_offset)
		: start_day_(start_day), end_day_(end_day), day_(cursor_day ? cursor_day : start_day),
		  offset_(cursor_offset), file_(os_file_type()), file_day_(UINT32_MAX), peeked_(false),
		  done_(cursor_day == UINT32_MAX) {
		if (day_ < start_day_) { day_ = start_day_; offset_ = 0; }
		if (day_ > end_day_) done_ = true;
	}

	~LegacyIterator() { close_file(); }

	bool peek(SprinklerLogRecord& record, bool& live) {
		if (peeked_) { record = peek_record_; live = peek_live_; return true; }
		while (!done_) {
			if (!file_ && !open_next_file()) { done_ = true; break; }
			uint32_t line_start = offset_;
			bool overflow = false;
			uint32_t length = 0;
			char byte = 0;
			while (file_read(file_, &byte, 1) == 1) {
				offset_++;
				if (byte == '\n') break;
				if (byte == '\r') continue;
				if (length + 1 < TMP_BUFFER_SIZE) tmp_buffer[length++] = byte;
				else overflow = true;
			}
			if (length == 0 && byte != '\n') {
				uint32_t completed_day = file_day_;
				close_file();
				if (completed_day == UINT32_MAX || completed_day == UINT32_MAX - 1) {
					done_ = true;
					break;
				}
				day_ = completed_day + 1;
				offset_ = 0;
				continue;
			}
			tmp_buffer[length] = 0;
			peek_day_ = file_day_;
			peek_offset_ = line_start;
			peek_next_offset_ = offset_;
			peek_record_ = {};
			peek_live_ = !overflow && parse_legacy_record(tmp_buffer, peek_record_);
			if (!peek_live_) peek_record_.timestamp = file_day_ * 86400UL;
			uint32_t record_day = peek_record_.timestamp / 86400UL;
			if (record_day < start_day_ || record_day > end_day_) continue;
			peeked_ = true;
			record = peek_record_;
			live = peek_live_;
			return true;
		}
		return false;
	}

	void consume() {
		if (!peeked_) return;
		day_ = peek_day_;
		offset_ = peek_next_offset_;
		peeked_ = false;
	}

	void cursor(uint32_t& day, uint32_t& offset) const {
		if (done_) { day = UINT32_MAX; offset = 0; return; }
		day = peeked_ ? peek_day_ : day_;
		offset = peeked_ ? peek_offset_ : offset_;
	}

private:
	bool open_next_file() {
		while (day_ <= end_day_) {
			uint32_t next_day = 0;
			if (!find_next_legacy_day(day_, end_day_, next_day)) return false;
			char filename[24];
			legacy_filename(next_day, filename, sizeof(filename));
			file_ = file_open(filename, FileOpenMode::Read);
			if (!file_) {
				if (next_day == UINT32_MAX) return false;
				day_ = next_day + 1;
				offset_ = 0;
				continue;
			}
			file_day_ = next_day;
			if (next_day != day_) offset_ = 0;
			day_ = next_day;
			if (!offset_ || file_seek(file_, offset_)) return true;
			close_file();
			return false;
		}
		return false;
	}

	void close_file() {
		if (file_) file_close(file_);
		file_ = os_file_type();
		file_day_ = UINT32_MAX;
	}

	uint32_t start_day_;
	uint32_t end_day_;
	uint32_t day_;
	uint32_t offset_;
	os_file_type file_;
	uint32_t file_day_;
	bool peeked_;
	bool done_;
	uint32_t peek_day_;
	uint32_t peek_offset_;
	uint32_t peek_next_offset_;
	SprinklerLogRecord peek_record_;
	bool peek_live_;
};

class RingIterator {
public:
	RingIterator(uint32_t start_day, uint32_t end_day, uint32_t cursor_generation,
		uint16_t cursor_record)
		: start_timestamp_(day_start_timestamp(start_day)),
		  end_timestamp_(day_end_timestamp(end_day)),
		  cursor_generation_(cursor_generation), cursor_record_(cursor_record),
		  index_file_(os_file_type()), file_(os_file_type()),
		  slots_checked_(0), record_index_(0), peeked_(false), done_(cursor_generation == UINT32_MAX),
		  current_generation_(0) {
		has_ring_ = !done_ && load_ring_state(header_, false);
		if (!has_ring_) done_ = true;
		else index_file_ = file_open(RING_INDEX_FILENAME, FileOpenMode::Read);
	}

	~RingIterator() {
		close_file();
		if (index_file_) file_close(index_file_);
	}

	bool peek(SprinklerLogRecord& record, bool& live) {
		if (peeked_) { record = peek_record_; live = peek_live_; return true; }
		while (!done_) {
			if (!file_ && !open_next_segment()) { done_ = true; break; }
			if (record_index_ >= current_descriptor_.record_count) {
				close_file();
				continue;
			}
			uint16_t index = record_index_++;
			uint8_t bytes[16];
			SprinklerLogRecord candidate = {};
			bool decoded = file_read(file_, bytes, sizeof(bytes)) == (int)sizeof(bytes) &&
				sprinkler_log_decode_record(bytes, candidate);
			if (!decoded) candidate.timestamp = current_descriptor_.min_timestamp;
			if (candidate.timestamp < start_timestamp_ || candidate.timestamp >= end_timestamp_) continue;
			peek_generation_ = current_generation_;
			peek_index_ = index;
			peek_record_ = candidate;
			peek_live_ = decoded && !(candidate.flags & SPRINKLER_LOG_FLAG_TOMBSTONE);
			peeked_ = true;
			record = peek_record_;
			live = peek_live_;
			return true;
		}
		return false;
	}

	void consume() { peeked_ = false; }

	void cursor(uint32_t& generation, uint16_t& record) const {
		if (done_) { generation = UINT32_MAX; record = 0; return; }
		generation = peeked_ ? peek_generation_ : current_generation_;
		record = peeked_ ? peek_index_ : record_index_;
		if (!file_ && current_generation_ == 0) {
			generation = cursor_generation_;
			record = cursor_record_;
		}
	}

private:
	bool open_next_segment() {
		while (slots_checked_ < header_.max_files) {
			uint16_t slot = (uint16_t)((header_.active_slot + 1 + slots_checked_) % header_.max_files);
			slots_checked_++;
			cooperative_yield();
			SprinklerLogDescriptor descriptor = {};
			if (!read_descriptor_from_file(index_file_, slot, descriptor) ||
				!(descriptor.flags & SPRINKLER_DESCRIPTOR_FLAG_VALID) || slot == header_.active_slot) {
				if (!inspect_segment(slot, descriptor)) continue;
			}
			if (descriptor.record_count == 0 || descriptor.max_timestamp < start_timestamp_ ||
				descriptor.min_timestamp >= end_timestamp_) continue;
			if (cursor_generation_ != 0 && descriptor.generation < cursor_generation_) continue;
			uint16_t first_record = descriptor.generation == cursor_generation_ ? cursor_record_ : 0;
			if (first_record >= descriptor.record_count) continue;
			char filename[24];
			segment_filename(slot, filename, sizeof(filename));
			file_ = file_open(filename, FileOpenMode::Read);
			if (!file_) continue;
			if (!file_seek(file_, sizeof(SprinklerLogSegmentHeader) +
				(uint32_t)first_record * sizeof(SprinklerLogRecord))) {
				close_file();
				continue;
			}
			current_descriptor_ = descriptor;
			current_generation_ = descriptor.generation;
			record_index_ = first_record;
			return true;
		}
		return false;
	}

	void close_file() {
		if (file_) file_close(file_);
		file_ = os_file_type();
	}

	uint32_t start_timestamp_;
	uint32_t end_timestamp_;
	uint32_t cursor_generation_;
	uint16_t cursor_record_;
	SprinklerLogRingHeader header_;
	bool has_ring_;
	os_file_type index_file_;
	os_file_type file_;
	uint16_t slots_checked_;
	uint16_t record_index_;
	SprinklerLogDescriptor current_descriptor_;
	bool peeked_;
	bool done_;
	uint32_t current_generation_;
	uint32_t peek_generation_;
	uint16_t peek_index_;
	SprinklerLogRecord peek_record_;
	bool peek_live_;
};

} // namespace

void sprinkler_log_encode_record(const SprinklerLogRecord& record, uint8_t output[16]) {
	encode_u32(output, record.timestamp);
	encode_u32(output + 4, record.value);
	encode_u32(output + 8, record.aux);
	output[12] = record.type;
	output[13] = record.program;
	output[14] = record.station;
	output[15] = record.flags;
}

bool sprinkler_log_decode_record(const uint8_t input[16], SprinklerLogRecord& record) {
	record.timestamp = decode_u32(input);
	record.value = decode_u32(input + 4);
	record.aux = decode_u32(input + 8);
	record.type = input[12];
	record.program = input[13];
	record.station = input[14];
	record.flags = input[15];
	return valid_record_type(record.type) &&
		(record.flags & ~(SPRINKLER_LOG_FLAG_FLOW | SPRINKLER_LOG_FLAG_TOMBSTONE)) == 0;
}

void sprinkler_log_invalidate_cache() {
	ring_cache_valid = false;
	ring_cache = {};
	active_cache = {};
}

bool sprinkler_log_exists() {
	if (file_exists(RING_INDEX_FILENAME)) return true;
	char filename[24];
	for (uint16_t slot = 0; slot < SPRINKLER_LOG_MAX_FILES; slot++) {
		segment_filename(slot, filename, sizeof(filename));
		if (file_exists(filename)) return true;
		cooperative_yield();
	}
	return false;
}

bool sprinkler_log_append(const SprinklerLogRecord& record) {
	if (!valid_record_type(record.type) ||
		(record.flags & ~(SPRINKLER_LOG_FLAG_FLOW | SPRINKLER_LOG_FLAG_TOMBSTONE))) return false;
	SprinklerLogRingHeader header = {};
	if (!load_ring_state(header, true) && !initialize_ring(header)) return false;
	if (!load_active_cache(header)) return false;
	if (active_cache.closed || active_cache.count >= header.records_per_file) {
		if (!rotate_active(header)) return false;
	}
	if (!prepare_log_write(false)) return false;

	char filename[24];
	segment_filename(header.active_slot, filename, sizeof(filename));
	os_file_type file = file_open(filename, FileOpenMode::Append);
	if (!file) return false;
	uint8_t bytes[16];
	sprinkler_log_encode_record(record, bytes);
	bool ok = file_write(file, bytes, sizeof(bytes)) == (int)sizeof(bytes);
	file_close(file);
	if (!ok) return false;

	active_cache.count++;
	if (active_cache.min_timestamp == 0 || record.timestamp < active_cache.min_timestamp)
		active_cache.min_timestamp = record.timestamp;
	if (record.timestamp > active_cache.max_timestamp)
		active_cache.max_timestamp = record.timestamp;
	if (active_cache.count >= header.records_per_file) ok = finalize_active(header);
	return ok;
}

bool sprinkler_log_clear() {
	bool ok = remove_file(RING_INDEX_FILENAME) && remove_file(RING_INDEX_TEMP_FILENAME);
	char filename[24];
	for (uint16_t slot = 0; slot < SPRINKLER_LOG_MAX_FILES; slot++) {
		segment_filename(slot, filename, sizeof(filename));
		if (!remove_file(filename)) ok = false;
		cooperative_yield();
	}
	sprinkler_log_invalidate_cache();
	return ok;
}

bool sprinkler_log_delete_day(uint32_t day) {
	SprinklerLogRingHeader header = {};
	if (!load_ring_state(header, false)) return true;
	uint32_t first_timestamp = day * 86400UL;
	uint32_t last_timestamp = day_end_timestamp(day);
	for (uint16_t slot = 0; slot < header.max_files; slot++) {
		cooperative_yield();
		SprinklerLogDescriptor descriptor = {};
		if (!inspect_segment(slot, descriptor)) continue;
		if (descriptor.record_count == 0 || descriptor.max_timestamp < first_timestamp ||
			descriptor.min_timestamp >= last_timestamp) continue;
		bool changed = false;
		if (!rewrite_matching_records(slot, first_timestamp, last_timestamp, false, changed)) return false;
	}
	sprinkler_log_invalidate_cache();
	return true;
}

bool sprinkler_log_delete_before(uint32_t day) {
	SprinklerLogRingHeader header = {};
	if (!load_ring_state(header, false)) return true;
	uint32_t cutoff = day * 86400UL;
	bool removed_segment = false;
	for (uint16_t slot = 0; slot < header.max_files; slot++) {
		cooperative_yield();
		SprinklerLogDescriptor descriptor = {};
		if (!inspect_segment(slot, descriptor)) continue;
		if (descriptor.record_count == 0 || descriptor.min_timestamp >= cutoff) continue;
		if (slot != header.active_slot &&
			(descriptor.flags & SPRINKLER_DESCRIPTOR_FLAG_CLOSED) &&
			descriptor.max_timestamp < cutoff) {
			char filename[24];
			segment_filename(slot, filename, sizeof(filename));
			if (!remove_file(filename)) return false;
			removed_segment = true;
			continue;
		}
		bool changed = false;
		if (!rewrite_matching_records(slot, cutoff, 0, true, changed)) return false;
	}
	if (removed_segment && !rebuild_ring_state(header, true)) {
		// Every segment may have been removed; an absent ring is a valid empty state.
		remove_file(RING_INDEX_FILENAME);
	}
	sprinkler_log_invalidate_cache();
	return true;
}

bool sprinkler_log_remove_oldest_completed() {
	SprinklerLogRingHeader header = {};
	if (!load_ring_state(header, false)) return false;
	for (uint16_t offset = 1; offset < header.max_files; offset++) {
		cooperative_yield();
		uint16_t slot = (uint16_t)((header.active_slot + offset) % header.max_files);
		SprinklerLogDescriptor descriptor = {};
		if (!inspect_segment(slot, descriptor) ||
			!(descriptor.flags & SPRINKLER_DESCRIPTOR_FLAG_CLOSED)) continue;
		char filename[24];
		segment_filename(slot, filename, sizeof(filename));
		if (!remove_file(filename)) return false;
		if (!rebuild_ring_state(header, true)) remove_file(RING_INDEX_FILENAME);
		sprinkler_log_invalidate_cache();
		return true;
	}
	return false;
}

uint32_t sprinkler_log_allocated_bytes(uint32_t block_size) {
	uint32_t total = 0;
	char filename[24];
	for (uint16_t slot = 0; slot < SPRINKLER_LOG_MAX_FILES; slot++) {
		segment_filename(slot, filename, sizeof(filename));
		os_file_type file = file_open(filename, FileOpenMode::Read);
		if (file) {
			total += file_allocation(file_size(file), block_size);
			file_close(file);
		}
		cooperative_yield();
	}
	os_file_type index = file_open(RING_INDEX_FILENAME, FileOpenMode::Read);
	if (index) {
		total += file_allocation(file_size(index), block_size);
		file_close(index);
	}
	return total;
}

SprinklerLogCursor sprinkler_log_cursor_begin() {
	SprinklerLogCursor cursor = {};
	cursor.legacy_day = 0;
	return cursor;
}

bool sprinkler_log_cursor_parse(const char* value, SprinklerLogCursor& cursor) {
	if (!value || strlen(value) != 28) return false;
	for (uint8_t i = 0; i < 28; i++) {
		char c = value[i];
		if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
			(c >= 'A' && c <= 'F'))) return false;
	}
	unsigned long legacy_day = 0;
	unsigned long legacy_offset = 0;
	unsigned long ring_generation = 0;
	unsigned int ring_record = 0;
	if (sscanf(value, "%8lx%8lx%8lx%4x", &legacy_day, &legacy_offset,
		&ring_generation, &ring_record) != 4) return false;
	cursor.legacy_day = (uint32_t)legacy_day;
	cursor.legacy_offset = (uint32_t)legacy_offset;
	cursor.ring_generation = (uint32_t)ring_generation;
	cursor.ring_record = (uint16_t)ring_record;
	return true;
}

void sprinkler_log_cursor_format(const SprinklerLogCursor& cursor,
	char* output, size_t output_size) {
	if (!output || output_size == 0) return;
	snprintf(output, output_size, "%08lx%08lx%08lx%04x",
		(unsigned long)cursor.legacy_day, (unsigned long)cursor.legacy_offset,
		(unsigned long)cursor.ring_generation, cursor.ring_record);
}

bool sprinkler_log_query(uint32_t start_day, uint32_t end_day,
	const SprinklerLogCursor& cursor, uint32_t max_physical_slots,
	SprinklerLogVisitor visitor, void* context, SprinklerLogCursor& next_cursor,
	uint32_t& scanned_slots, bool& done) {
	if (start_day > end_day || !visitor || max_physical_slots == 0) return false;

	#if defined(ESP8266)
	// Forwarded OTC requests already have a deep WebSocket callback stack on
	// ESP8266. Keep the two filesystem iterators off that continuation stack.
	std::unique_ptr<LegacyIterator> legacy_owner(new (std::nothrow) LegacyIterator(
		start_day, end_day, cursor.legacy_day, cursor.legacy_offset));
	std::unique_ptr<RingIterator> ring_owner(new (std::nothrow) RingIterator(
		start_day, end_day, cursor.ring_generation, cursor.ring_record));
	if (!legacy_owner || !ring_owner) return false;
	LegacyIterator& legacy = *legacy_owner;
	RingIterator& ring = *ring_owner;
#else
	LegacyIterator legacy(start_day, end_day, cursor.legacy_day, cursor.legacy_offset);
	RingIterator ring(start_day, end_day, cursor.ring_generation, cursor.ring_record);
#endif
	SprinklerLogRecord legacy_record = {};
	SprinklerLogRecord ring_record = {};
	bool legacy_live = false;
	bool ring_live = false;
	bool has_legacy = legacy.peek(legacy_record, legacy_live);
	bool has_ring = ring.peek(ring_record, ring_live);
	scanned_slots = 0;

	while (scanned_slots < max_physical_slots && (has_legacy || has_ring)) {
		// Legacy wins timestamp ties, keeping a deterministic order during the
		// transition where old daily files and ring segments overlap.
		bool use_legacy = has_legacy &&
			(!has_ring || legacy_record.timestamp <= ring_record.timestamp);
		const SprinklerLogRecord& record = use_legacy ? legacy_record : ring_record;
		bool live = use_legacy ? legacy_live : ring_live;
		if (!visitor(record, live, context)) return false;
		if (use_legacy) {
			legacy.consume();
			has_legacy = legacy.peek(legacy_record, legacy_live);
		} else {
			ring.consume();
			has_ring = ring.peek(ring_record, ring_live);
		}
		scanned_slots++;
		#if defined(ARDUINO)
		if ((scanned_slots & 0x3f) == 0) yield();
		#endif
	}

	legacy.cursor(next_cursor.legacy_day, next_cursor.legacy_offset);
	ring.cursor(next_cursor.ring_generation, next_cursor.ring_record);
	done = !has_legacy && !has_ring;
	return true;
}
