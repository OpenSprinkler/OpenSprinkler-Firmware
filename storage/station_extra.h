#pragma once

#include "core/cycle_soak.h"
#include "storage/files.h"

#include <cstdint>

constexpr uint16_t STATION_EXTRA_HEADER_SIZE = 16;
constexpr uint16_t STATION_EXTRA_RECORD_SIZE = 32;

enum class StationExtraResult {
	Ok,
	Missing,
	Malformed,
	IoError,
	InvalidArgument,
};

// Validate the header once and reuse the handle for a read-only policy batch.
class StationExtraReader {
public:
	explicit StationExtraReader(uint16_t station_count);
	StationExtraResult load(uint16_t sid, CycleSoakPolicy& policy);

private:
	FileReader file_;
	uint16_t count_;
	StationExtraResult status_;
};

// Loading never creates/repairs a file. Non-Ok reads return a disabled policy;
// callers retain the result to report a warning (except ordinary Missing).
StationExtraResult station_extra_load(uint16_t sid, uint16_t station_count,
	CycleSoakPolicy& policy);

// Initialization is explicit and never replaces an existing file. Save updates
// one record in place, preserving reserved fields and unknown flag bits.
StationExtraResult station_extra_initialize(uint16_t station_count);
StationExtraResult station_extra_save(uint16_t sid, uint16_t station_count,
	const CycleSoakPolicy& policy);
