#include "storage/station_extra.h"
#include "storage/files.h"
#include "defines.h"
#include <cassert>
#include <cstdio>
#include <cstring>

char tmp_buffer[TMP_BUFFER_ALLOC_SIZE];

int main(int argc, char** argv) {
	assert(argc == 2);
	set_data_dir(argv[1]);
	constexpr uint16_t count = 72;
	CycleSoakPolicy p;
	p.enabled = true;
	assert(station_extra_load(0, count, p) == StationExtraResult::Missing);
	assert(!p.enabled && !file_exists(STATION_EXTRA_FILENAME));
	{
		StationExtraReader missing(count);
		for (uint16_t sid = 0; sid < count; sid++) {
			assert(missing.load(sid, p) == StationExtraResult::Missing && !p.enabled);
		}
		assert(station_extra_initialize(count) == StationExtraResult::Ok);
		assert(missing.load(0, p) == StationExtraResult::Missing); // no per-record reopen
	}
	assert(station_extra_initialize(count) == StationExtraResult::Ok);
	os_file_type file = file_open(STATION_EXTRA_FILENAME, FileOpenMode::Read);
	assert(file && file_size(file) == 2320);
	file_close(file);
	for (uint16_t sid = 0; sid < count; sid++) {
		assert(station_extra_load(sid, count, p) == StationExtraResult::Ok);
		assert(!p.enabled && !p.cycle_seconds && !p.soak_seconds);
	}
	const uint32_t offset = 16 + 71 * 32;
	uint8_t record[32] = {};
	record[1] = 0x80; record[6] = 0x5a; record[31] = 0xa5;
	assert(file_write_block(STATION_EXTRA_FILENAME, record, offset, sizeof(record)));
	p.enabled = true; p.cycle_seconds = 300; p.soak_seconds = 600;
	assert(station_extra_save(71, count, p) == StationExtraResult::Ok);
	assert(file_read_block(STATION_EXTRA_FILENAME, record, offset, sizeof(record)));
	assert(record[0] == 1 && record[1] == 0x80); // unknown flags preserved
	assert(record[2] == 0x2c && record[3] == 1); // little endian
	assert(record[4] == 0x58 && record[5] == 2);
	assert(record[6] == 0x5a && record[31] == 0xa5);
	assert(station_extra_initialize(count) == StationExtraResult::Ok); // never wipe existing values
	assert(station_extra_load(71, count, p) == StationExtraResult::Ok && p.enabled);
	{
		StationExtraReader reader(count);
		for (uint16_t sid = 0; sid < count; sid++) {
			CycleSoakPolicy single;
			assert(reader.load(sid, p) == station_extra_load(sid, count, single));
			assert(p.enabled == single.enabled && p.cycle_seconds == single.cycle_seconds &&
				p.soak_seconds == single.soak_seconds);
		}
		assert(reader.load(count, p) == StationExtraResult::InvalidArgument && !p.enabled);
	}
	assert(station_extra_load(71, count, p) == StationExtraResult::Ok && p.enabled);
	assert(station_extra_save(71, count, p) == StationExtraResult::Ok); // no change
	p.enabled = false;
	assert(station_extra_save(71, count, p) == StationExtraResult::Ok);
	assert(station_extra_load(71, count, p) == StationExtraResult::Ok && !p.enabled);
	assert(p.cycle_seconds == 300 && p.soak_seconds == 600);
	p.enabled = true; p.cycle_seconds = 1;
	assert(station_extra_save(71, count, p) == StationExtraResult::InvalidArgument);
	assert(station_extra_load(71, count, p) == StationExtraResult::Ok && p.cycle_seconds == 300);
	assert(station_extra_load(72, count, p) == StationExtraResult::InvalidArgument);
	assert(station_extra_load(0, 0, p) == StationExtraResult::InvalidArgument);
	assert(station_extra_load(0, count - 1, p) == StationExtraResult::Malformed);
	const uint8_t invalid[] = {1, 0, 1, 0, 60, 0};
	assert(file_write_block(STATION_EXTRA_FILENAME, invalid, 16, sizeof(invalid)));
	assert(station_extra_load(0, count, p) == StationExtraResult::Malformed && !p.enabled);
	assert(station_extra_load(71, count, p) == StationExtraResult::Ok);
	{
		StationExtraReader reader(count);
		assert(reader.load(0, p) == StationExtraResult::Malformed && !p.enabled);
		assert(reader.load(71, p) == StationExtraResult::Ok && p.cycle_seconds == 300);
	}
	uint8_t check[6];
	assert(file_read_block(STATION_EXTRA_FILENAME, check, 16, sizeof(check)));
	assert(!memcmp(check, invalid, sizeof(check))); // malformed record untouched
	assert(file_write_byte(STATION_EXTRA_FILENAME, 0, 'X'));
	assert(station_extra_initialize(count) == StationExtraResult::Malformed);
	assert(station_extra_load(71, count, p) == StationExtraResult::Malformed && !p.enabled);
	assert(station_extra_save(71, count, p) == StationExtraResult::Malformed);
	{
		StationExtraReader bad_header(count);
		assert(bad_header.load(71, p) == StationExtraResult::Malformed && !p.enabled);
		StationExtraReader bad_count(0);
		assert(bad_count.load(0, p) == StationExtraResult::InvalidArgument);
	}
	assert(file_read_byte(STATION_EXTRA_FILENAME, 0) == 'X');
	assert(truncate_file(STATION_EXTRA_FILENAME, 17));
	assert(station_extra_initialize(count) == StationExtraResult::Malformed);
	assert(remove_file(STATION_EXTRA_FILENAME));
	assert(station_extra_initialize(MAX_NUM_STATIONS) == StationExtraResult::Ok);
	assert(station_extra_load(MAX_NUM_STATIONS - 1, MAX_NUM_STATIONS, p) == StationExtraResult::Ok);
	puts("Station extra storage PASS");
}
