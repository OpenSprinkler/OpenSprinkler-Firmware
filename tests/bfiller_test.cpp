#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>

#include "bfiller.h"

static char option_value[MAX_SOPTS_SIZE + 1];
static std::string streamed;

void file_read_block(const char *, void *destination, uint32_t, uint32_t length) {
	memcpy(destination, option_value, length);
}

char dec2hexchar(unsigned char value) {
	return value < 10 ? static_cast<char>('0' + value) : static_cast<char>('A' + value - 10);
}

static void capture(const char *buffer, size_t length) {
	streamed.append(buffer, length);
}

int main() {
	memset(option_value, 0, sizeof(option_value));
	strcpy(option_value, "secret");

	char guarded[66];
	memset(guarded, 0x5A, sizeof(guarded));
	BufferFiller fixed(guarded + 1, 64);
	fixed.emit_p(PSTR("prefix:$O:suffix"), SOPT_PASSWORD);
	assert(!fixed.overflowed());
	assert(strcmp(guarded + 1, "prefix:secret:suffix") == 0);
	assert(static_cast<unsigned char>(guarded[0]) == 0x5A);
	assert(static_cast<unsigned char>(guarded[65]) == 0x5A);

	memset(option_value, 'x', sizeof(option_value));
	option_value[MAX_SOPTS_SIZE] = 0;
	memset(guarded, 0x5A, sizeof(guarded));
	BufferFiller too_long(guarded + 1, 64);
	too_long.emit_p(PSTR("prefix:$O"), SOPT_PASSWORD);
	assert(too_long.overflowed());
	assert(static_cast<unsigned char>(guarded[0]) == 0x5A);
	assert(static_cast<unsigned char>(guarded[65]) == 0x5A);

	memset(guarded, 0x5A, sizeof(guarded));
	BufferFiller long_string(guarded + 1, 64);
	std::string value(80, 'y');
	long_string.emit_p(PSTR("$S"), value.c_str());
	assert(long_string.overflowed());
	assert(static_cast<unsigned char>(guarded[0]) == 0x5A);
	assert(static_cast<unsigned char>(guarded[65]) == 0x5A);

	char stream_buffer[16];
	BufferFiller streaming(stream_buffer, sizeof(stream_buffer));
	streaming.set_flush(capture);
	streaming.emit_p(PSTR("start:$S:end"), value.c_str());
	assert(!streaming.overflowed());
	assert(streamed == "start:" + value + ":end");
	return 0;
}
