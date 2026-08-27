#include "defines.h"
#include "storage/files.h"
#include "storage/maintenance.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

char tmp_buffer[TMP_BUFFER_ALLOC_SIZE];

namespace {

uint32_t progress_calls = 0;
uint32_t progress_value = 0;
uint32_t progress_total = 0;

void migration_progress(uint32_t processed, uint32_t total) {
	progress_calls++;
	progress_value = processed;
	progress_total = total;
}

void write_test_file(const char* filename) {
	os_file_type file = file_open(filename, FileOpenMode::WriteTruncate);
	assert(file);
	const char value[] = "test";
	assert(file_write(file, value, sizeof(value)) == (int)sizeof(value));
	file_close(file);
}

void test_legacy_log_migration(const char* data_dir) {
	assert(ensure_log_dir());
	write_test_file(LOG_DIR "spr.hdr");
	write_test_file(LOG_DIR "spr.log000");
	write_test_file(LOG_DIR "sens.log000");
	write_test_file("config.dat");
	for (uint32_t day = 20000; day < 20070; day++) {
		char filename[32];
		snprintf(filename, sizeof(filename), "%s%lu.txt", LOG_DIR, (unsigned long)day);
		write_test_file(filename);
	}

	LegacySprinklerLogMigrationResult result =
		migrate_legacy_sprinkler_logs(migration_progress);
	assert(result.total == 70);
	assert(result.removed == 70);
	assert(result.remaining == 0);
	assert(result.complete);
	assert(progress_calls > 1);
	assert(progress_value == 70 && progress_total == 70);
	assert(embedded_storage_legacy_removed_files() == 70);
	assert(file_exists(LOG_DIR "spr.hdr"));
	assert(file_exists(LOG_DIR "spr.log000"));
	assert(file_exists(LOG_DIR "sens.log000"));
	assert(file_exists("config.dat"));

	// A matching directory simulates an undeletable entry. Migration must stop
	// without looping, then complete when the entry can be removed next time.
	char blocked_path[PATH_MAX];
	snprintf(blocked_path, sizeof(blocked_path), "%s%s99999.txt",
		data_dir, data_dir[strlen(data_dir) - 1] == '/' ? "logs/" : "/logs/");
	assert(mkdir(blocked_path, 0755) == 0);
	write_test_file(LOG_DIR "99999.txt/child");
	result = migrate_legacy_sprinkler_logs();
	assert(result.total == 1 && result.removed == 0 && result.remaining == 1);
	assert(!result.complete);
	assert(remove_file(LOG_DIR "99999.txt/child"));
	assert(rmdir(blocked_path) == 0);
	write_test_file(LOG_DIR "99999.txt");
	result = migrate_legacy_sprinkler_logs();
	assert(result.total == 1 && result.removed == 1 && result.remaining == 0);
	assert(result.complete);
}

} // namespace

int main(int argc, char** argv) {
	assert(argc == 2);
	set_data_dir(argv[1]);

	const char* filename = "storage-test.dat";
	const char initial[] = "abcdef";
	assert(file_write_block(filename, initial, 0, sizeof(initial)));
	assert(file_exists(filename));

	os_file_type file = file_open(filename, FileOpenMode::ReadWrite);
	assert(file);
	assert(file_size(file) == sizeof(initial));
	assert(file_seek(file, 2));
	const char replacement[] = "XY";
	assert(file_write(file, replacement, 2) == 2);
	assert(file_seek(file, 0));
	char result[sizeof(initial)] = {};
	assert(file_read(file, result, sizeof(result)) == (int)sizeof(result));
	file_close(file);
	assert(strcmp(result, "abXYef") == 0);

	char temporary[2];
	assert(file_copy_block(filename, 2, 4, 2, temporary));
	assert(file_read_block(filename, result, 0, sizeof(result)));
	assert(strcmp(result, "abXYXY") == 0);
	assert(file_cmp_block(filename, "abXYXY", 0) == 0);
	assert(file_cmp_block(filename, "abXYef", 0) != 0);
	assert(!file_read_block(filename, result, sizeof(result) + 1, 1));
	assert(!file_copy_block(filename, sizeof(result) + 1, 0, 1, temporary));

	assert(file_write_byte(filename, 1, 'Z'));
	assert(file_read_byte(filename, 1) == 'Z');
	assert(truncate_file(filename, 4));
	file = file_open(filename, FileOpenMode::Read);
	assert(file && file_size(file) == 4);
	file_close(file);
	assert(remove_file(filename));
	assert(remove_file(filename));
	assert(!file_exists(filename));
	assert(!file_write_block("missing/dir.dat", initial, 0, sizeof(initial)));
	assert(!file_copy_block(filename, 0, 1, 1, nullptr));

	EmbeddedStorageUsage usage = {2UL * 1024UL * 1024UL, 0, 0, 8192, true};
	assert(embedded_storage_reserve_bytes(usage) == 64UL * 1024UL);
	usage.total_bytes = 128UL * 1024UL;
	assert(embedded_storage_reserve_bytes(usage) == 32UL * 1024UL);
	usage.valid = false;
	assert(embedded_storage_reserve_bytes(usage) == 0);

	assert(is_sprinkler_log_filename("20690.txt"));
	assert(is_sprinkler_log_filename("/logs/20690.txt"));
	assert(is_sprinkler_log_filename("logs/0.txt"));
	assert(!is_sprinkler_log_filename("notes.txt"));
	assert(!is_sprinkler_log_filename("20690.csv"));
	assert(!is_sprinkler_log_filename(".txt"));
	assert(!is_sprinkler_log_filename("12345678901.txt"));

	test_legacy_log_migration(argv[1]);
	return 0;
}
