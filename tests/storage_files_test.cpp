#include "defines.h"
#include "storage/files.h"
#include "storage/maintenance.h"

#include <cassert>
#include <cstring>

char tmp_buffer[TMP_BUFFER_ALLOC_SIZE];

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
	return 0;
}
