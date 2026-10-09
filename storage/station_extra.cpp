#include "storage/station_extra.h"

#include "defines.h"
#include "storage/files.h"

#include <cstring>

namespace {

constexpr uint16_t ENABLED_FLAG = 1;

uint16_t read_u16(const uint8_t* data) {
	return uint16_t(data[0]) | (uint16_t(data[1]) << 8);
}

void write_u16(uint8_t* data, uint16_t value) {
	data[0] = value & 0xff;
	data[1] = value >> 8;
}

bool valid_count(uint16_t count) {
	return count > 0 && count <= MAX_NUM_STATIONS;
}

StationExtraResult check_header(FileReader& file, uint16_t count) {
	uint8_t header[STATION_EXTRA_HEADER_SIZE];
	const uint32_t expected = STATION_EXTRA_HEADER_SIZE + uint32_t(count) * STATION_EXTRA_RECORD_SIZE;
	if (file.size() != expected) return StationExtraResult::Malformed;
	if (!file.read_block(header, 0, sizeof(header))) {
		return StationExtraResult::IoError;
	}
	if (memcmp(header, "SNX1", 4) || header[4] != 1 ||
		header[5] != STATION_EXTRA_HEADER_SIZE ||
		read_u16(header + 6) != STATION_EXTRA_RECORD_SIZE || read_u16(header + 8) != count) {
		return StationExtraResult::Malformed;
	}
	return StationExtraResult::Ok;
}

StationExtraResult read_record(uint16_t sid, uint16_t count, uint8_t* record) {
	if (!valid_count(count) || sid >= count) return StationExtraResult::InvalidArgument;
	if (!file_exists(STATION_EXTRA_FILENAME)) return StationExtraResult::Missing;
	FileReader file(STATION_EXTRA_FILENAME);
	if (!file) return StationExtraResult::IoError;
	StationExtraResult result = check_header(file, count);
	if (result == StationExtraResult::Ok &&
		!file.read_block(record, STATION_EXTRA_HEADER_SIZE + uint32_t(sid) * STATION_EXTRA_RECORD_SIZE,
			STATION_EXTRA_RECORD_SIZE)) {
		result = StationExtraResult::IoError;
	}
	return result;
}

StationExtraResult decode_record(const uint8_t* record, CycleSoakPolicy& policy) {
	CycleSoakPolicy loaded;
	loaded.enabled = (read_u16(record) & ENABLED_FLAG) != 0;
	loaded.cycle_seconds = read_u16(record + 2);
	loaded.soak_seconds = read_u16(record + 4);
	if (!cycle_soak_policy_valid(loaded)) return StationExtraResult::Malformed;
	policy = loaded;
	return StationExtraResult::Ok;
}

} // namespace

StationExtraReader::StationExtraReader(uint16_t station_count)
	: file_(valid_count(station_count) ? STATION_EXTRA_FILENAME : nullptr),
	  count_(station_count), status_(StationExtraResult::InvalidArgument) {
	if (!valid_count(count_)) return;
	if (!file_) {
		status_ = file_exists(STATION_EXTRA_FILENAME) ? StationExtraResult::IoError : StationExtraResult::Missing;
		return;
	}
	status_ = check_header(file_, count_);
}

StationExtraResult StationExtraReader::load(uint16_t sid, CycleSoakPolicy& policy) {
	policy = {};
	if (!valid_count(count_) || sid >= count_) return StationExtraResult::InvalidArgument;
	if (status_ != StationExtraResult::Ok) return status_;
	uint8_t record[STATION_EXTRA_RECORD_SIZE];
	if (!file_.read_block(record, STATION_EXTRA_HEADER_SIZE + uint32_t(sid) * STATION_EXTRA_RECORD_SIZE,
		STATION_EXTRA_RECORD_SIZE)) return StationExtraResult::IoError;
	return decode_record(record, policy);
}

StationExtraResult station_extra_load(uint16_t sid, uint16_t station_count,
	CycleSoakPolicy& policy) {
	policy = {};
	uint8_t record[STATION_EXTRA_RECORD_SIZE];
	const auto result = read_record(sid, station_count, record);
	return result == StationExtraResult::Ok ? decode_record(record, policy) : result;
}

StationExtraResult station_extra_initialize(uint16_t station_count) {
	if (!valid_count(station_count)) return StationExtraResult::InvalidArgument;
	if (file_exists(STATION_EXTRA_FILENAME)) {
		uint8_t record[STATION_EXTRA_RECORD_SIZE];
		return read_record(0, station_count, record);
	}
	uint8_t header[STATION_EXTRA_HEADER_SIZE] = {'S', 'N', 'X', '1', 1, STATION_EXTRA_HEADER_SIZE};
	write_u16(header + 6, STATION_EXTRA_RECORD_SIZE);
	write_u16(header + 8, station_count);
	os_file_type file = file_open(STATION_EXTRA_FILENAME, FileOpenMode::WriteTruncate);
	if (!file) return StationExtraResult::IoError;
	bool ok = file_write(file, header, sizeof(header)) == sizeof(header);
	const uint8_t record[STATION_EXTRA_RECORD_SIZE] = {};
	for (uint16_t sid = 0; ok && sid < station_count; sid++) {
		ok = file_write(file, record, sizeof(record)) == sizeof(record);
	}
	file_close(file);
	// An interrupted/short creation remains visible as malformed, not defaults.
	if (!ok) return StationExtraResult::IoError;
	uint8_t check[STATION_EXTRA_RECORD_SIZE];
	return read_record(station_count - 1, station_count, check);
}

StationExtraResult station_extra_save(uint16_t sid, uint16_t station_count,
	const CycleSoakPolicy& policy) {
	if (!cycle_soak_policy_valid(policy)) return StationExtraResult::InvalidArgument;
	uint8_t record[STATION_EXTRA_RECORD_SIZE];
	const StationExtraResult result = read_record(sid, station_count, record);
	if (result != StationExtraResult::Ok) return result;
	const uint16_t flags = (read_u16(record) & ~ENABLED_FLAG) | (policy.enabled ? ENABLED_FLAG : 0);
	if (read_u16(record) == flags && read_u16(record + 2) == policy.cycle_seconds &&
		read_u16(record + 4) == policy.soak_seconds) return StationExtraResult::Ok;
	write_u16(record, flags);
	write_u16(record + 2, policy.cycle_seconds);
	write_u16(record + 4, policy.soak_seconds);
	const uint32_t offset = STATION_EXTRA_HEADER_SIZE + uint32_t(sid) * STATION_EXTRA_RECORD_SIZE;
	if (!file_write_block(STATION_EXTRA_FILENAME, record, offset, sizeof(record))) {
		return StationExtraResult::IoError;
	}
	uint8_t check[STATION_EXTRA_RECORD_SIZE];
	if (!file_read_block(STATION_EXTRA_FILENAME, check, offset, sizeof(check)) ||
		memcmp(check, record, sizeof(check))) return StationExtraResult::IoError;
	return StationExtraResult::Ok;
}
