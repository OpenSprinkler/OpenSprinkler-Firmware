#pragma once

#include <cstddef>
#include <cstdint>

constexpr uint16_t SPRINKLER_LOG_MAGIC = 0x534c;
constexpr uint8_t SPRINKLER_LOG_VERSION = 1;
#if defined(SPRINKLER_LOG_TEST_SMALL_GEOMETRY)
constexpr uint16_t SPRINKLER_LOG_MAX_FILES = 4;
constexpr uint16_t SPRINKLER_LOG_RECORDS_PER_FILE = 3;
#else
constexpr uint16_t SPRINKLER_LOG_MAX_FILES = 100;
#if defined(ARDUINO)
constexpr uint16_t SPRINKLER_LOG_RECORDS_PER_FILE = 510;
#else
constexpr uint16_t SPRINKLER_LOG_RECORDS_PER_FILE = 16383;
#endif
#endif

constexpr uint8_t SPRINKLER_LOG_FLAG_FLOW = 0x01;
constexpr uint8_t SPRINKLER_LOG_FLAG_TOMBSTONE = 0x80;
constexpr uint8_t SPRINKLER_SEGMENT_FLAG_CLOSED = 0x01;
constexpr uint8_t SPRINKLER_DESCRIPTOR_FLAG_VALID = 0x01;
constexpr uint8_t SPRINKLER_DESCRIPTOR_FLAG_CLOSED = 0x02;

struct __attribute__((packed)) SprinklerLogRecord {
	uint32_t timestamp;
	uint32_t value;
	uint32_t aux;
	uint8_t type;
	uint8_t program;
	uint8_t station;
	uint8_t flags;
};

struct __attribute__((packed)) SprinklerLogSegmentHeader {
	uint16_t magic;
	uint8_t version;
	uint8_t flags;
	uint32_t generation;
	uint32_t min_timestamp;
	uint32_t max_timestamp;
};

struct __attribute__((packed)) SprinklerLogRingHeader {
	uint16_t magic;
	uint8_t version;
	uint8_t flags;
	uint16_t max_files;
	uint16_t records_per_file;
	uint16_t active_slot;
	uint16_t reserved;
	uint32_t next_generation;
};

struct __attribute__((packed)) SprinklerLogDescriptor {
	uint32_t generation;
	uint32_t min_timestamp;
	uint32_t max_timestamp;
	uint16_t record_count;
	uint8_t flags;
	uint8_t reserved;
};

static_assert(sizeof(SprinklerLogRecord) == 16, "sprinkler log record size");
static_assert(sizeof(SprinklerLogSegmentHeader) == 16, "sprinkler segment header size");
static_assert(sizeof(SprinklerLogRingHeader) == 16, "sprinkler ring header size");
static_assert(sizeof(SprinklerLogDescriptor) == 16, "sprinkler descriptor size");

using SprinklerLogVisitor = bool (*)(const SprinklerLogRecord& record, bool live, void* context);

struct SprinklerLogCursor {
	uint32_t ring_generation;
	uint16_t ring_record;
};

void sprinkler_log_encode_record(const SprinklerLogRecord& record, uint8_t output[16]);
bool sprinkler_log_decode_record(const uint8_t input[16], SprinklerLogRecord& record);

bool sprinkler_log_append(const SprinklerLogRecord& record);
bool sprinkler_log_clear();
bool sprinkler_log_delete_day(uint32_t day);
bool sprinkler_log_delete_before(uint32_t day);
bool sprinkler_log_remove_oldest_completed();
uint32_t sprinkler_log_allocated_bytes(uint32_t block_size);
bool sprinkler_log_exists();

SprinklerLogCursor sprinkler_log_cursor_begin();
bool sprinkler_log_cursor_parse(const char* value, SprinklerLogCursor& cursor);
void sprinkler_log_cursor_format(const SprinklerLogCursor& cursor, char* output, size_t output_size);

// Query callbacks and appends execute synchronously from do_loop(). If either
// path moves to another task, this iterator requires synchronization or an
// immutable per-request snapshot before concurrent access is safe.
bool sprinkler_log_query(uint32_t start_day, uint32_t end_day,
	const SprinklerLogCursor& cursor, uint32_t max_physical_slots,
	SprinklerLogVisitor visitor, void* context, SprinklerLogCursor& next_cursor,
	uint32_t& scanned_slots, bool& done);

void sprinkler_log_invalidate_cache();
