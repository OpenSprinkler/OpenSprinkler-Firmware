#include "storage/files.h"

#include "defines.h"
#include "storage/maintenance.h"

#include <cerrno>

#if defined(ARDUINO)

#include <FS.h>
#include <LittleFS.h>
#if defined(ESP32)
#include <unistd.h>
#endif

#else

#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>

namespace {

const char* data_dir = nullptr;

} // namespace

char* get_runtime_path() {
	static char path[PATH_MAX];
	static bool queried = false;

#ifdef __APPLE__
	strcpy(path, "./");
	return path;
#endif

	if (!queried) {
		if (readlink("/proc/self/exe", path, PATH_MAX) <= 0) return nullptr;
		char* path_end = strrchr(path, '/');
		if (!path_end) return nullptr;
		path_end++;
		*path_end = 0;
		queried = true;
	}
	return path;
}

const char* get_data_dir() {
	return data_dir ? data_dir : get_runtime_path();
}

void set_data_dir(const char* data_directory) {
	data_dir = data_directory;
}

char* get_filename_fullpath(const char* filename) {
	static char fullpath[PATH_MAX];
	strcpy(fullpath, get_data_dir());
	if ('/' != fullpath[strlen(fullpath) - 1]) strcat(fullpath, "/");
	strcat(fullpath, filename);
	return fullpath;
}

#endif

bool remove_file(const char* filename) {
	#if defined(ARDUINO)
	if (!LittleFS.exists(filename)) return true;
	return LittleFS.remove(filename);
#else
	if (remove(get_filename_fullpath(filename)) == 0) return true;
	return errno == ENOENT;
#endif
}

bool rename_file(const char* source, const char* destination) {
	#if defined(ARDUINO)
	return LittleFS.rename(source, destination);
	#else
	char source_path[PATH_MAX];
	char destination_path[PATH_MAX];
	strncpy(source_path, get_filename_fullpath(source), sizeof(source_path) - 1);
	source_path[sizeof(source_path) - 1] = 0;
	strncpy(destination_path, get_filename_fullpath(destination), sizeof(destination_path) - 1);
	destination_path[sizeof(destination_path) - 1] = 0;
	return rename(source_path, destination_path) == 0;
	#endif
}

bool truncate_file(const char* filename, uint32_t length) {
	#if defined(ESP8266)
	File file = LittleFS.open(filename, "r+");
	if (!file) return false;
	bool ok = file.truncate(length);
	file.close();
	return ok;
	#elif defined(ESP32)
	char path[64];
	int written = snprintf(path, sizeof(path), "/littlefs%s", filename);
	return written > 0 && (size_t)written < sizeof(path) &&
		truncate(path, (off_t)length) == 0;
	#else
	return truncate(get_filename_fullpath(filename), (off_t)length) == 0;
	#endif
}

bool ensure_log_dir() {
	#if defined(ESP32)
	if (LittleFS.exists(LOG_DIR)) return true;
	return LittleFS.mkdir(LOG_DIR);
	#elif !defined(ARDUINO)
		const char* directory = get_filename_fullpath(LOG_DIR);
		struct stat status;
		if (stat(directory, &status) == 0) return S_ISDIR(status.st_mode);
		if (errno != ENOENT) return false;
		return mkdir(directory,
			S_IRUSR | S_IWUSR | S_IXUSR |
			S_IRGRP | S_IWGRP | S_IXGRP |
			S_IROTH | S_IWOTH | S_IXOTH) == 0 || errno == EEXIST;
	#else
	return true; // ESP8266 LittleFS creates parent directories on create-open.
	#endif
}

bool file_exists(const char* filename) {
	#if defined(ARDUINO)
	return LittleFS.exists(filename);
#else
	FILE* file = fopen(get_filename_fullpath(filename), "rb");
	if (!file) return false;
	fclose(file);
	return true;
#endif
}

os_file_type file_open(const char* filename, FileOpenMode mode) {
	#if defined(ARDUINO)
	switch (mode) {
	default:
	case FileOpenMode::Read:
		return LittleFS.open(filename, "r");
	case FileOpenMode::ReadWrite:
		if (!LittleFS.exists(filename)) {
			File file = LittleFS.open(filename, "w");
			if (!file) return file;
			file.close();
		}
		return LittleFS.open(filename, "r+");
	case FileOpenMode::WriteTruncate:
		return LittleFS.open(filename, "w");
	case FileOpenMode::ReadWriteTruncate:
		return LittleFS.open(filename, "w+");
	case FileOpenMode::Append:
		return LittleFS.open(filename, "a");
	case FileOpenMode::ReadAppend:
		return LittleFS.open(filename, "a+");
	}
#else
	char* full_file = get_filename_fullpath(filename);
	switch (mode) {
	default:
	case FileOpenMode::Read:
		return fopen(full_file, "rb");
	case FileOpenMode::ReadWrite: {
		int descriptor = open(full_file, O_RDWR | O_CREAT, 0644);
		if (descriptor == -1) return nullptr;
		FILE* file = fdopen(descriptor, "rb+");
		if (!file) close(descriptor);
		return file;
	}
	case FileOpenMode::WriteTruncate:
		return fopen(full_file, "wb");
	case FileOpenMode::ReadWriteTruncate:
		return fopen(full_file, "wb+");
	case FileOpenMode::Append:
		return fopen(full_file, "ab");
	case FileOpenMode::ReadAppend:
		return fopen(full_file, "ab+");
	}
#endif
}

void file_close(os_file_type file) {
	#if defined(ARDUINO)
	file.close();
#else
	fclose(file);
#endif
}

bool file_seek(os_file_type file, uint32_t position, FileSeekMode mode) {
	#if defined(ARDUINO)
	switch (mode) {
	case FileSeekMode::Set:
		return file.seek(position, fs::SeekMode::SeekSet);
	case FileSeekMode::Current:
		return file.seek(position, fs::SeekMode::SeekCur);
	case FileSeekMode::End:
		return file.seek(position, fs::SeekMode::SeekEnd);
	}
#else
	switch (mode) {
	case FileSeekMode::Set:
		return fseek(file, position, SEEK_SET) == 0;
	case FileSeekMode::Current:
		return fseek(file, position, SEEK_CUR) == 0;
	case FileSeekMode::End:
		return fseek(file, position, SEEK_END) == 0;
	}
#endif
	return false;
}

bool file_seek(os_file_type file, uint32_t position) {
	return file_seek(file, position, FileSeekMode::Set);
}

int file_read(os_file_type file, void* target, uint32_t length) {
	#if defined(ARDUINO)
	return file.read((uint8_t*)target, length);
#else
	return fread(target, 1, length, file);
#endif
}

int file_write(os_file_type file, const void* source, uint32_t length) {
	#if defined(ARDUINO)
	return file.write((const uint8_t*)source, length);
#else
	return fwrite(source, 1, length, file);
#endif
}

uint32_t file_size(os_file_type file) {
#if defined(ARDUINO)
	return file.size();
#else
	long current = ftell(file);
	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fseek(file, current, SEEK_SET);
	return (uint32_t)(size >= 0 ? size : 0);
#endif
}

bool file_read_block(const char* filename, void* destination, uint32_t position, uint32_t length) {
	os_file_type file = file_open(filename, FileOpenMode::Read);
	if (!file) return false;
	bool ok = file_seek(file, position) && file_read(file, destination, length) == (int)length;
	file_close(file);
	return ok;
}

static bool file_write_block_once(const char* filename, const void* source, uint32_t position, uint32_t length) {
	os_file_type file = file_open(filename, FileOpenMode::ReadWrite);
	if (!file) return false;
	bool ok = file_seek(file, position) && file_write(file, source, length) == (int)length;
	file_close(file);
	return ok;
}

bool file_write_block(const char* filename, const void* source, uint32_t position, uint32_t length) {
	if (file_write_block_once(filename, source, position, length)) return true;
#if defined(ARDUINO)
	if (embedded_storage_is_low()) {
		maintain_embedded_storage();
		return file_write_block_once(filename, source, position, length);
	}
#endif
	return false;
}

static bool file_copy_block_once(const char* filename, uint32_t from, uint32_t to, uint32_t length,
	void* temporary_buffer) {
	os_file_type file = file_open(filename, FileOpenMode::ReadWrite);
	if (!file) return false;
	bool ok = file_seek(file, from) && file_read(file, temporary_buffer, length) == (int)length &&
		file_seek(file, to) && file_write(file, temporary_buffer, length) == (int)length;
	file_close(file);
	return ok;
}

bool file_copy_block(const char* filename, uint32_t from, uint32_t to, uint32_t length,
	void* temporary_buffer) {
	if (!temporary_buffer) return false;
	if (file_copy_block_once(filename, from, to, length, temporary_buffer)) return true;
#if defined(ARDUINO)
	if (embedded_storage_is_low()) {
		maintain_embedded_storage();
		return file_copy_block_once(filename, from, to, length, temporary_buffer);
	}
#endif
	return false;
}

unsigned char file_cmp_block(const char* filename, const char* value, uint32_t position) {
	os_file_type file = file_open(filename, FileOpenMode::Read);
	if (!file) return 1;
	file_seek(file, position);
	char current;
	if (file_read(file, &current, 1) != 1) current = (char)-1;
	while (*value && current == *value) {
		value++;
		if (file_read(file, &current, 1) != 1) current = (char)-1;
	}
	file_close(file);
	return (*value == current) ? 0 : 1;
}

unsigned char file_read_byte(const char* filename, uint32_t position) {
	unsigned char value = 0;
	file_read_block(filename, &value, position, 1);
	return value;
}

bool file_write_byte(const char* filename, uint32_t position, unsigned char value) {
	return file_write_block(filename, &value, position, 1);
}
