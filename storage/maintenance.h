#pragma once

#include <cstdint>

struct EmbeddedStorageUsage {
	uint32_t total_bytes;
	uint32_t used_bytes;
	uint32_t free_bytes;
	uint32_t block_size;
	bool valid;
};

struct LegacySprinklerLogMigrationResult {
	uint32_t total;
	uint32_t removed;
	uint32_t remaining;
	bool complete;
};

using LegacySprinklerLogMigrationProgress = void (*)(uint32_t processed, uint32_t total);

bool is_sprinkler_log_filename(const char* name);
EmbeddedStorageUsage get_embedded_storage_usage();
uint32_t embedded_storage_reserve_bytes(const EmbeddedStorageUsage& usage);
uint32_t embedded_storage_pruned_files();
uint32_t embedded_storage_legacy_removed_files();

// Delete legacy numeric daily sprinkler logs. Embedded startup invokes this
// before storage maintenance; native tests may invoke it explicitly, but
// Linux/OSPi startup intentionally leaves legacy files untouched.
LegacySprinklerLogMigrationResult migrate_legacy_sprinkler_logs(
	LegacySprinklerLogMigrationProgress progress = nullptr);

// Enforce both the sprinkler-log size cap and the physical free-space reserve.
bool maintain_embedded_storage(uint32_t additional_free_bytes = 0);

// Log writes use a throttled check unless force is true (new-file and rotation paths).
bool prepare_log_write(bool force = false);
bool embedded_storage_is_low();
