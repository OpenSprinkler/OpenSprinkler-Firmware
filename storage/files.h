#pragma once

#include <cstdint>

#if defined(ESP8266)
#include <FS.h>
#include <LittleFS.h>
using os_file_type = File;
#else
#include <cstdio>
using os_file_type = FILE*;
#endif

enum class FileOpenMode {
	Read,
	ReadWrite,
	WriteTruncate,
	ReadWriteTruncate,
	Append,
	ReadAppend,
};

enum class FileSeekMode {
	Set,
	Current,
	End,
};

void remove_file(const char* filename);
bool file_exists(const char* filename);
void ensure_log_dir();

os_file_type file_open(const char* filename, FileOpenMode mode);
void file_close(os_file_type file);
bool file_seek(os_file_type file, uint32_t position, FileSeekMode mode);
bool file_seek(os_file_type file, uint32_t position);
int file_read(os_file_type file, void* target, uint32_t length);
int file_write(os_file_type file, const void* source, uint32_t length);
uint32_t file_size(os_file_type file);

void file_read_block(const char* filename, void* destination, uint32_t position, uint32_t length);
void file_write_block(const char* filename, const void* source, uint32_t position, uint32_t length);
void file_copy_block(const char* filename, uint32_t from, uint32_t to, uint32_t length,
	void* temporary_buffer = nullptr);
unsigned char file_read_byte(const char* filename, uint32_t position);
void file_write_byte(const char* filename, uint32_t position, unsigned char value);
unsigned char file_cmp_block(const char* filename, const char* value, uint32_t position);

#if !defined(ESP8266)
const char* get_data_dir();
void set_data_dir(const char* data_directory);
char* get_filename_fullpath(const char* filename);
char* get_runtime_path();
#endif
