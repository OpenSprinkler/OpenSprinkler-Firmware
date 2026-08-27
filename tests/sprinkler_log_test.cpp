#include "defines.h"
#include "storage/files.h"
#include "storage/sprinkler_log.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

char tmp_buffer[TMP_BUFFER_ALLOC_SIZE];

namespace {

SprinklerLogRecord make_record(uint32_t timestamp, uint32_t value = 30) {
	SprinklerLogRecord record = {};
	record.timestamp = timestamp;
	record.value = value;
	record.type = LOGDATA_STATION;
	record.program = 3;
	record.station = 7;
	return record;
}

bool collect_record(const SprinklerLogRecord& record, bool live, void* context) {
	if (live) static_cast<std::vector<SprinklerLogRecord>*>(context)->push_back(record);
	return true;
}

void write_legacy_file(uint32_t day, const char* contents) {
	assert(ensure_log_dir());
	char filename[32];
	snprintf(filename, sizeof(filename), "%s%lu.txt", LOG_DIR, (unsigned long)day);
	os_file_type file = file_open(filename, FileOpenMode::WriteTruncate);
	assert(file);
	assert(file_write(file, contents, strlen(contents)) == (int)strlen(contents));
	file_close(file);
}

void remove_legacy_file(uint32_t day) {
	char filename[32];
	snprintf(filename, sizeof(filename), "%s%lu.txt", LOG_DIR, (unsigned long)day);
	assert(remove_file(filename));
}

void test_codec() {
	SprinklerLogRecord input = make_record(0x04030201, 0x08070605);
	input.aux = 0x0c0b0a09;
	input.flags = SPRINKLER_LOG_FLAG_FLOW;
	uint8_t encoded[16] = {};
	sprinkler_log_encode_record(input, encoded);
	const uint8_t expected[16] = {
		1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
		LOGDATA_STATION, 3, 7, SPRINKLER_LOG_FLAG_FLOW,
	};
	assert(memcmp(encoded, expected, sizeof(expected)) == 0);
	SprinklerLogRecord output = {};
	assert(sprinkler_log_decode_record(encoded, output));
	assert(memcmp(&input, &output, sizeof(input)) == 0);
	encoded[15] = 0x40;
	assert(!sprinkler_log_decode_record(encoded, output));
	SprinklerLogCursor cursor = {};
	char invalid_cursor[] = "0000000000000000000000000000";
	invalid_cursor[27] = 'g';
	assert(!sprinkler_log_cursor_parse(invalid_cursor, cursor));
}

void test_append_rotate_and_recover() {
	assert(!sprinkler_log_exists());
	assert(sprinkler_log_append(make_record(1000)));
	assert(sprinkler_log_exists());
	assert(file_exists(LOG_DIR "spr.hdr"));
	assert(file_exists(LOG_DIR "spr.log000"));

	os_file_type first = file_open(LOG_DIR "spr.log000", FileOpenMode::Read);
	assert(first);
	assert(file_size(first) == sizeof(SprinklerLogSegmentHeader) + sizeof(SprinklerLogRecord));
	file_close(first);

	for (uint32_t i = 1; i < SPRINKLER_LOG_RECORDS_PER_FILE; i++) {
		assert(sprinkler_log_append(make_record(1000 + i)));
	}
	first = file_open(LOG_DIR "spr.log000", FileOpenMode::Read);
	assert(first);
	assert(file_size(first) == sizeof(SprinklerLogSegmentHeader) +
		(uint32_t)SPRINKLER_LOG_RECORDS_PER_FILE * sizeof(SprinklerLogRecord));
	file_close(first);

	assert(sprinkler_log_append(make_record(500000)));
	assert(file_exists(LOG_DIR "spr.log001"));

	// A missing central index is rebuilt from segment generations on append.
	assert(remove_file(LOG_DIR "spr.hdr"));
	sprinkler_log_invalidate_cache();
	assert(sprinkler_log_append(make_record(500001)));
	assert(file_exists(LOG_DIR "spr.hdr"));
}

void test_deletion_and_clear() {
	const uint32_t day = 12;
	assert(sprinkler_log_append(make_record(day * 86400UL + 10)));
	assert(sprinkler_log_delete_day(day));

	// Locate the newest segment record and verify the timestamp-preserving tombstone.
	os_file_type file = file_open(LOG_DIR "spr.log001", FileOpenMode::Read);
	assert(file);
	uint32_t count = (file_size(file) - sizeof(SprinklerLogSegmentHeader)) /
		sizeof(SprinklerLogRecord);
	assert(count >= 3);
	assert(file_seek(file, sizeof(SprinklerLogSegmentHeader) +
		(count - 1) * sizeof(SprinklerLogRecord)));
	uint8_t bytes[16];
	assert(file_read(file, bytes, sizeof(bytes)) == (int)sizeof(bytes));
	file_close(file);
	SprinklerLogRecord record = {};
	assert(sprinkler_log_decode_record(bytes, record));
	assert(record.timestamp == day * 86400UL + 10);
	assert(record.flags & SPRINKLER_LOG_FLAG_TOMBSTONE);

	assert(sprinkler_log_clear());
	assert(!sprinkler_log_exists());
	assert(sprinkler_log_clear());
}

void test_partial_tail_recovery() {
	assert(sprinkler_log_append(make_record(700000)));
	os_file_type file = file_open(LOG_DIR "spr.log000", FileOpenMode::Append);
	assert(file);
	const uint8_t partial[] = {1, 2, 3, 4, 5};
	assert(file_write(file, partial, sizeof(partial)) == (int)sizeof(partial));
	file_close(file);
	sprinkler_log_invalidate_cache();
	assert(sprinkler_log_append(make_record(700001)));
	file = file_open(LOG_DIR "spr.log000", FileOpenMode::Read);
	assert(file);
	assert(file_size(file) == sizeof(SprinklerLogSegmentHeader) +
		2 * sizeof(SprinklerLogRecord));
	file_close(file);
	assert(sprinkler_log_clear());
}

void test_merged_query_and_cursor() {
	const uint32_t day = 20;
	char legacy[160];
	snprintf(legacy, sizeof(legacy),
		"[2,1,10,%lu]\n[3,2,20,%lu]\n",
		(unsigned long)(day * 86400UL + 100),
		(unsigned long)(day * 86400UL + 300));
	write_legacy_file(day, legacy);
	assert(sprinkler_log_append(make_record(day * 86400UL + 200, 11)));
	assert(sprinkler_log_append(make_record(day * 86400UL + 400, 21)));

	SprinklerLogCursor cursor = sprinkler_log_cursor_begin();
	SprinklerLogCursor next = {};
	std::vector<SprinklerLogRecord> records;
	uint32_t scanned = 0;
	bool done = false;
	assert(sprinkler_log_query(day, day, cursor, 2, collect_record, &records,
		next, scanned, done));
	assert(scanned == 2 && !done && records.size() == 2);
	assert(records[0].timestamp == day * 86400UL + 100);
	assert(records[1].timestamp == day * 86400UL + 200);

	char encoded_cursor[32];
	sprinkler_log_cursor_format(next, encoded_cursor, sizeof(encoded_cursor));
	SprinklerLogCursor parsed = {};
	assert(sprinkler_log_cursor_parse(encoded_cursor, parsed));
	records.clear();
	assert(sprinkler_log_query(day, day, parsed, 2, collect_record, &records,
		next, scanned, done));
	assert(scanned == 2 && done && records.size() == 2);
	assert(records[0].timestamp == day * 86400UL + 300);
	assert(records[1].timestamp == day * 86400UL + 400);

	assert(sprinkler_log_clear());
	remove_legacy_file(day);

	// Advancing past EOF must continue with the next retained legacy day.
	write_legacy_file(day + 1, "[4,3,10,1814401]\n");
	write_legacy_file(day + 3, "[5,4,20,1987201]\n");
	cursor = sprinkler_log_cursor_begin();
	records.clear();
	assert(sprinkler_log_query(day + 1, day + 3, cursor, 4, collect_record, &records,
		next, scanned, done));
	assert(done && scanned == 2 && records.size() == 2);
	assert(records[0].timestamp == (day + 1) * 86400UL + 1);
	assert(records[1].timestamp == (day + 3) * 86400UL + 1);
	remove_legacy_file(day + 1);
	remove_legacy_file(day + 3);

	// A missing ring and no legacy files is a valid empty result.
	cursor = sprinkler_log_cursor_begin();
	records.clear();
	assert(sprinkler_log_query(day, day, cursor, 2, collect_record, &records,
		next, scanned, done));
	assert(done && scanned == 0 && records.empty());
}

void test_full_wrap_and_prune() {
	assert(sprinkler_log_clear());
	const uint32_t capacity = (uint32_t)SPRINKLER_LOG_MAX_FILES *
		SPRINKLER_LOG_RECORDS_PER_FILE;
	for (uint32_t i = 0; i <= capacity; i++) {
		assert(sprinkler_log_append(make_record(1000 + i, i)));
	}

	SprinklerLogCursor cursor = sprinkler_log_cursor_begin();
	SprinklerLogCursor next = {};
	std::vector<SprinklerLogRecord> records;
	uint32_t scanned = 0;
	bool done = false;
	assert(sprinkler_log_query(0, 0, cursor, capacity + 1, collect_record,
		&records, next, scanned, done));
	assert(done);
	assert(records.size() == capacity - SPRINKLER_LOG_RECORDS_PER_FILE + 1);
	assert(records.front().timestamp == 1000 + SPRINKLER_LOG_RECORDS_PER_FILE);
	assert(records.back().timestamp == 1000 + capacity);

	assert(sprinkler_log_remove_oldest_completed());
	records.clear();
	cursor = sprinkler_log_cursor_begin();
	assert(sprinkler_log_query(0, 0, cursor, capacity + 1, collect_record,
		&records, next, scanned, done));
	assert(records.size() == capacity - 2 * SPRINKLER_LOG_RECORDS_PER_FILE + 1);
	assert(records.front().timestamp == 1000 + 2 * SPRINKLER_LOG_RECORDS_PER_FILE);
	assert(sprinkler_log_clear());
}

void test_delete_before() {
	assert(sprinkler_log_clear());
	for (uint32_t day = 1; day <= 2; day++) {
		for (uint16_t i = 0; i < SPRINKLER_LOG_RECORDS_PER_FILE; i++) {
			assert(sprinkler_log_append(make_record(day * 86400UL + i)));
		}
	}
	assert(sprinkler_log_append(make_record(3 * 86400UL + 1)));
	assert(sprinkler_log_delete_before(3));

	SprinklerLogCursor cursor = sprinkler_log_cursor_begin();
	SprinklerLogCursor next = {};
	std::vector<SprinklerLogRecord> records;
	uint32_t scanned = 0;
	bool done = false;
	assert(sprinkler_log_query(0, 4, cursor, 32, collect_record, &records,
		next, scanned, done));
	assert(done && records.size() == 1);
	assert(records[0].timestamp == 3 * 86400UL + 1);
	assert(sprinkler_log_delete_day(3));
	records.clear();
	cursor = sprinkler_log_cursor_begin();
	assert(sprinkler_log_query(0, 4, cursor, 32, collect_record, &records,
		next, scanned, done));
	assert(done && scanned == 1 && records.empty());
	assert(sprinkler_log_clear());
}

} // namespace

int main(int argc, char** argv) {
	assert(argc == 2);
	set_data_dir(argv[1]);
	test_codec();
	test_append_rotate_and_recover();
	test_deletion_and_clear();
	test_partial_tail_recovery();
	test_merged_query_and_cursor();
	test_full_wrap_and_prune();
	test_delete_before();
	puts("sprinkler log tests passed");
	return 0;
}
