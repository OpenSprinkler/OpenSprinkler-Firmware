#include "storage/files.h"

#include <cassert>
#include <cstring>

int main(int argc, char** argv) {
	assert(argc == 2);
	set_data_dir(argv[1]);

	const char* filename = "storage-test.dat";
	const char initial[] = "abcdef";
	file_write_block(filename, initial, 0, sizeof(initial));
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
	file_copy_block(filename, 2, 4, 2, temporary);
	file_read_block(filename, result, 0, sizeof(result));
	assert(strcmp(result, "abXYXY") == 0);
	assert(file_cmp_block(filename, "abXYXY", 0) == 0);
	assert(file_cmp_block(filename, "abXYef", 0) != 0);

	file_write_byte(filename, 1, 'Z');
	assert(file_read_byte(filename, 1) == 'Z');
	remove_file(filename);
	assert(!file_exists(filename));
	return 0;
}
