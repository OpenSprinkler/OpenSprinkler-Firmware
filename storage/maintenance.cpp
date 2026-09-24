#include "storage/maintenance.h"

#include "defines.h"
#include "sensors/sensor.h"
#include "storage/files.h"
#include "storage/sprinkler_log.h"

#include <cstdlib>
#include <cstring>

#if defined(ARDUINO)
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#else
#include <dirent.h>
#include <limits.h>
#endif

namespace {

uint32_t pruned_files = 0;
uint32_t legacy_removed_files = 0;
constexpr uint32_t MIN_RESERVE_BYTES = 64UL * 1024UL;
constexpr uint32_t SMALL_FS_THRESHOLD = 256UL * 1024UL;
constexpr uint8_t LEGACY_DELETE_BATCH_SIZE = 32;
constexpr uint8_t LEGACY_PROGRESS_INTERVAL = 4;

uint32_t scan_legacy_sprinkler_logs(uint32_t days[], uint8_t capacity) {
	uint32_t total = 0;
#if defined(ESP8266)
	Dir dir = LittleFS.openDir(LOG_DIR);
	while (dir.next()) {
		String name = dir.fileName();
		if (!is_sprinkler_log_filename(name.c_str())) continue;
		if (days && total < capacity)
			days[total] = (uint32_t)strtoul(name.c_str(), nullptr, 10);
		total++;
		if ((total & 0x0f) == 0) yield();
	}
#elif defined(ESP32)
	File dir = LittleFS.open(LOG_DIR);
	if (!dir || !dir.isDirectory()) return 0;
	for (File file = dir.openNextFile(); file; file = dir.openNextFile()) {
		String name = file.name();
		file.close();
		if (!is_sprinkler_log_filename(name.c_str())) continue;
		const char* base = strrchr(name.c_str(), '/');
		base = base ? base + 1 : name.c_str();
		if (days && total < capacity)
			days[total] = (uint32_t)strtoul(base, nullptr, 10);
		total++;
		if ((total & 0x0f) == 0) yield();
	}
	dir.close();
#else
	char directory_name[PATH_MAX];
	strncpy(directory_name, get_filename_fullpath(LOG_DIR), sizeof(directory_name) - 1);
	directory_name[sizeof(directory_name) - 1] = 0;
	DIR* dir = opendir(directory_name);
	if (!dir) return 0;
	struct dirent* entry;
	while ((entry = readdir(dir)) != nullptr) {
		if (!is_sprinkler_log_filename(entry->d_name)) continue;
		if (days && total < capacity)
			days[total] = (uint32_t)strtoul(entry->d_name, nullptr, 10);
		total++;
	}
	closedir(dir);
#endif
	return total;
}

bool remove_legacy_sprinkler_log(uint32_t day) {
	char filename[24];
	snprintf(filename, sizeof(filename), "%s%lu.txt", LOG_DIR, (unsigned long)day);
	return remove_file(filename);
}

#if defined(ARDUINO)

constexpr uint8_t LOG_CHECK_INTERVAL = 32;
constexpr uint8_t SPRINKLER_PRUNE_BATCH_SIZE = 8;

struct SprinklerLogCandidate {
	uint32_t day;
	uint32_t size;
	char filename[24];
};

uint32_t pruned_sprinkler_files = 0;
uint32_t pruned_sensor_files = 0;
uint8_t log_check_counter = 0;

uint32_t allocated_file_size(uint32_t size, uint32_t block_size) {
	if (size == 0 || block_size == 0) return 0;
	if (size <= block_size) return block_size;
	return ((size + 4 + block_size - 1) / block_size) * block_size;
}

bool candidate_precedes(uint32_t day, const char* filename,
	const SprinklerLogCandidate& candidate) {
	return day < candidate.day ||
		(day == candidate.day && strcmp(filename, candidate.filename) < 0);
}

void insert_candidate(SprinklerLogCandidate candidates[], uint8_t& count,
	uint32_t day, uint32_t size, const char* filename) {
	uint8_t position = 0;
	while (position < count && !candidate_precedes(day, filename, candidates[position])) position++;
	if (position >= SPRINKLER_PRUNE_BATCH_SIZE) return;

	uint8_t new_count = count < SPRINKLER_PRUNE_BATCH_SIZE ? (uint8_t)(count + 1) : count;
	for (uint8_t i = new_count - 1; i > position; i--) candidates[i] = candidates[i - 1];
	candidates[position].day = day;
	candidates[position].size = size;
	strncpy(candidates[position].filename, filename, sizeof(candidates[position].filename) - 1);
	candidates[position].filename[sizeof(candidates[position].filename) - 1] = 0;
	count = new_count;
}

uint8_t collect_oldest_sprinkler_logs(SprinklerLogCandidate candidates[], uint32_t block_size,
	uint32_t* total_size) {
	uint8_t count = 0;
	uint32_t total = 0;
#if defined(ESP8266)
	Dir dir = LittleFS.openDir(LOG_DIR);
	while (dir.next()) {
		String name = dir.fileName();
		if (!is_sprinkler_log_filename(name.c_str())) continue;
		char filename[24];
		snprintf(filename, sizeof(filename), "%s%s", LOG_DIR, name.c_str());
		uint32_t size = allocated_file_size(dir.fileSize(), block_size);
		total += size;
		insert_candidate(candidates, count,
			(uint32_t)strtoul(name.c_str(), nullptr, 10), size, filename);
	}
#else
	File dir = LittleFS.open(LOG_DIR);
	if (!dir || !dir.isDirectory()) {
		if (total_size) *total_size = 0;
		return 0;
	}
	for (File file = dir.openNextFile(); file; file = dir.openNextFile()) {
		String name = file.name();
		uint32_t size = allocated_file_size(file.size(), block_size);
		file.close();
		if (!is_sprinkler_log_filename(name.c_str())) continue;
		const char* base = strrchr(name.c_str(), '/');
		base = base ? base + 1 : name.c_str();
		char filename[24];
		if (name.length() > 0 && name[0] == '/') {
			snprintf(filename, sizeof(filename), "%s", name.c_str());
		} else if (strncmp(name.c_str(), "logs/", 5) == 0) {
			snprintf(filename, sizeof(filename), "/%s", name.c_str());
		} else {
			snprintf(filename, sizeof(filename), "%s%s", LOG_DIR, name.c_str());
		}
		total += size;
		insert_candidate(candidates, count, (uint32_t)strtoul(base, nullptr, 10), size, filename);
	}
	dir.close();
#endif
	if (total_size) *total_size = total;
	return count;
}

bool remove_oldest_completed_sensor_log() {
	SensorLogHeader header = {};
	os_file_type file = open_sensor_log_header(FileOpenMode::Read);
	if (!file) return false;
	bool valid = file_read(file, &header, sizeof(header)) == (int)sizeof(header);
	file_close(file);
	valid = valid && header.magic == SENSOR_LOG_MAGIC &&
		header.version == SENSOR_LOG_VERSION && header.max_files > 0 &&
		header.max_files <= SENSOR_LOG_MAX_FILES && header.cur_file < header.max_files;
	if (!valid) return false;

	const uint16_t total_files = header.wrapped ? header.max_files : (uint16_t)(header.cur_file + 1);
	const uint16_t completed_files = total_files > 0 ? (uint16_t)(total_files - 1) : 0;
	const uint16_t first_file = header.wrapped ?
		(uint16_t)((header.cur_file + 1) % header.max_files) : 0;
	char filename[24];
	for (uint16_t i = 0; i < completed_files; i++) {
		uint16_t file_no = (uint16_t)((first_file + i) % header.max_files);
		get_sensor_log_filename(filename, file_no);
		if (!LittleFS.exists(filename)) continue;
		if (!LittleFS.remove(filename)) continue;
		pruned_files++;
		pruned_sensor_files++;
		return true;
	}
	return false;
}

#endif

} // namespace

bool is_sprinkler_log_filename(const char* name) {
	if (!name) return false;
	const char* base = strrchr(name, '/');
	base = base ? base + 1 : name;
	const size_t length = strlen(base);
	if (length <= 4 || length > 14 || strcmp(base + length - 4, ".txt") != 0) return false;
	for (size_t i = 0; i < length - 4; i++) {
		if (base[i] < '0' || base[i] > '9') return false;
	}
	return true;
}

EmbeddedStorageUsage get_embedded_storage_usage() {
	EmbeddedStorageUsage usage = {};
#if defined(ESP8266)
	FSInfo info;
	if (!LittleFS.info(info)) return usage;
	usage.total_bytes = info.totalBytes;
	usage.used_bytes = info.usedBytes;
	usage.block_size = info.blockSize;
	usage.valid = usage.total_bytes >= usage.used_bytes && usage.total_bytes > 0;
#elif defined(ESP32)
	usage.total_bytes = (uint32_t)LittleFS.totalBytes();
	usage.used_bytes = (uint32_t)LittleFS.usedBytes();
	// Arduino-ESP32 LittleFS uses 4 KiB erase blocks for the current OS4 partition.
	usage.block_size = 4096;
	usage.valid = usage.total_bytes >= usage.used_bytes && usage.total_bytes > 0;
#endif
	usage.free_bytes = usage.valid ? usage.total_bytes - usage.used_bytes : 0;
	return usage;
}

uint32_t embedded_storage_reserve_bytes(const EmbeddedStorageUsage& usage) {
	if (!usage.valid) return 0;
	uint32_t reserve = usage.block_size * 8UL;
	if (reserve < MIN_RESERVE_BYTES) reserve = MIN_RESERVE_BYTES;
	if (usage.total_bytes < SMALL_FS_THRESHOLD && reserve > usage.total_bytes / 4) {
		reserve = usage.total_bytes / 4;
	}
	return reserve;
}

uint32_t embedded_storage_pruned_files() {
	return pruned_files;
}

uint32_t embedded_storage_legacy_removed_files() {
	return legacy_removed_files;
}

bool maintain_embedded_storage(uint32_t additional_free_bytes) {
#if !defined(ARDUINO)
	(void)additional_free_bytes;
	return true;
#else
	EmbeddedStorageUsage before = get_embedded_storage_usage();
	if (!before.valid) return false;
	const uint32_t reserve = embedded_storage_reserve_bytes(before);
	const uint32_t target_free = UINT32_MAX - reserve < additional_free_bytes ?
		UINT32_MAX : reserve + additional_free_bytes;
	SprinklerLogCandidate candidates[SPRINKLER_PRUNE_BATCH_SIZE];
	uint32_t sprinkler_size = 0;
	uint8_t candidate_count = collect_oldest_sprinkler_logs(
		candidates, before.block_size, &sprinkler_size);
	sprinkler_size += sprinkler_log_allocated_bytes(before.block_size);
	const uint32_t sprinkler_limit = (uint32_t)LOG_SPRINKLER_MAX_KB * 1024UL;
	const uint32_t sprinkler_removed_before = pruned_sprinkler_files;
	const uint32_t sensor_removed_before = pruned_sensor_files;
	EmbeddedStorageUsage usage = before;

	while (candidate_count > 0 &&
		(sprinkler_size >= sprinkler_limit || usage.free_bytes < target_free)) {
		bool removed_any = false;
		uint32_t estimated_free = usage.free_bytes;
		for (uint8_t i = 0; i < candidate_count; i++) {
			if (sprinkler_size < sprinkler_limit && estimated_free >= target_free) break;
			if (!LittleFS.remove(candidates[i].filename)) continue;
			removed_any = true;
			pruned_files++;
			pruned_sprinkler_files++;
			sprinkler_size = candidates[i].size <= sprinkler_size ?
				sprinkler_size - candidates[i].size : 0;
			estimated_free = UINT32_MAX - estimated_free < candidates[i].size ?
				UINT32_MAX : estimated_free + candidates[i].size;
			yield();
		}
		if (!removed_any) break;
		usage = get_embedded_storage_usage();
		if (!usage.valid) break;
		if (sprinkler_size >= sprinkler_limit || usage.free_bytes < target_free) {
			candidate_count = collect_oldest_sprinkler_logs(
				candidates, usage.block_size, nullptr);
		}
	}

	// Preserve the active segment. Under pressure, retire completed binary
	// segments only after all legacy daily files have been pruned.
	while (usage.valid &&
		(sprinkler_size >= sprinkler_limit || usage.free_bytes < target_free)) {
		uint32_t previous_free = usage.free_bytes;
		if (!sprinkler_log_remove_oldest_completed()) break;
		pruned_files++;
		pruned_sprinkler_files++;
		yield();
		usage = get_embedded_storage_usage();
		if (!usage.valid || usage.free_bytes <= previous_free) break;
		sprinkler_size = sprinkler_log_allocated_bytes(usage.block_size);
	}

	while (usage.valid && usage.free_bytes < target_free) {
		uint32_t previous_free = usage.free_bytes;
		if (!remove_oldest_completed_sensor_log()) break;
		yield();
		usage = get_embedded_storage_usage();
		if (!usage.valid || usage.free_bytes <= previous_free) break;
	}

	if (pruned_sprinkler_files != sprinkler_removed_before ||
		pruned_sensor_files != sensor_removed_before ||
		(usage.valid && usage.free_bytes < target_free)) {
		DEBUG_PRINTF("storage: total=%lu used=%lu free=%lu target=%lu sprinkler=%lu sensor=%lu\n",
			(unsigned long)usage.total_bytes, (unsigned long)usage.used_bytes,
			(unsigned long)usage.free_bytes, (unsigned long)target_free,
			(unsigned long)(pruned_sprinkler_files - sprinkler_removed_before),
			(unsigned long)(pruned_sensor_files - sensor_removed_before));
	}
	return usage.valid && usage.free_bytes >= target_free;
#endif
}

bool prepare_log_write(bool force) {
#if defined(ARDUINO)
	if (!force) {
		log_check_counter++;
		if (log_check_counter < LOG_CHECK_INTERVAL) return true;
		EmbeddedStorageUsage usage = get_embedded_storage_usage();
		if (usage.valid && usage.free_bytes >=
			embedded_storage_reserve_bytes(usage) + usage.block_size) {
			log_check_counter = 0;
			return true;
		}
	}
	log_check_counter = 0;
	EmbeddedStorageUsage usage = get_embedded_storage_usage();
	if (!usage.valid) return false;
	return maintain_embedded_storage(usage.block_size);
#else
	(void)force;
	return true;
#endif
}

bool embedded_storage_is_low() {
#if defined(ARDUINO)
	EmbeddedStorageUsage usage = get_embedded_storage_usage();
	return usage.valid && usage.free_bytes < embedded_storage_reserve_bytes(usage);
#else
	return false;
#endif
}

LegacySprinklerLogMigrationResult migrate_legacy_sprinkler_logs(
	LegacySprinklerLogMigrationProgress progress) {
	LegacySprinklerLogMigrationResult result = {};
	uint32_t days[LEGACY_DELETE_BATCH_SIZE];
	uint32_t remaining = scan_legacy_sprinkler_logs(days, LEGACY_DELETE_BATCH_SIZE);
	result.total = remaining;
	if (remaining == 0) {
		result.complete = true;
		return result;
	}
	if (progress) progress(0, result.total);

#if defined(ARDUINO)
	bool maintenance_retried = false;
#endif
	uint32_t displayed = 0;
	while (remaining > 0) {
		const uint8_t batch_count = remaining < LEGACY_DELETE_BATCH_SIZE ?
			(uint8_t)remaining : LEGACY_DELETE_BATCH_SIZE;
		uint8_t removed_this_batch = 0;
		for (uint8_t i = 0; i < batch_count; i++) {
			if (!remove_legacy_sprinkler_log(days[i])) continue;
			removed_this_batch++;
			result.removed++;
			legacy_removed_files++;
			const uint32_t processed = result.total > remaining ? result.total - remaining : 0;
			const uint32_t current = processed + removed_this_batch;
			if (progress && (current == result.total || current - displayed >= LEGACY_PROGRESS_INTERVAL)) {
				progress(current, result.total);
				displayed = current;
			}
#if defined(ARDUINO)
			yield();
#endif
		}

		if (removed_this_batch == 0) {
#if defined(ARDUINO)
			if (!maintenance_retried && embedded_storage_is_low()) {
				maintenance_retried = true;
				maintain_embedded_storage();
				remaining = scan_legacy_sprinkler_logs(days, LEGACY_DELETE_BATCH_SIZE);
				continue;
			}
#endif
			break;
		}
		const uint32_t next_remaining =
			scan_legacy_sprinkler_logs(days, LEGACY_DELETE_BATCH_SIZE);
		const uint32_t actual_removed = remaining > next_remaining ?
			remaining - next_remaining : 0;
		if (removed_this_batch > actual_removed) {
			const uint32_t overcount = removed_this_batch - actual_removed;
			result.removed -= overcount;
			legacy_removed_files -= overcount;
		}
		if (next_remaining >= remaining) break;
		remaining = next_remaining;
	}

	result.remaining = scan_legacy_sprinkler_logs(nullptr, 0);
	result.complete = result.remaining == 0;
	const uint32_t processed = result.total >= result.remaining ?
		result.total - result.remaining : result.removed;
	if (progress && processed != displayed) progress(processed, result.total);
	DEBUG_PRINTF("sprinkler log migration: total=%lu removed=%lu remaining=%lu\n",
		(unsigned long)result.total, (unsigned long)result.removed,
		(unsigned long)result.remaining);
	return result;
}
