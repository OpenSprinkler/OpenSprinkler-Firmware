#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>

#include "bfiller.h"

static char option_value[MAX_SOPTS_SIZE + 1];
static std::string streamed;

char dec2hexchar(unsigned char value) {
	return value < 10 ? static_cast<char>('0' + value) : static_cast<char>('A' + value - 10);
}

static void capture(const char *buffer, size_t length) {
	streamed.append(buffer, length);
}

int main(int argc, char** argv) {
	assert(argc == 2);
	set_data_dir(argv[1]);
	memset(option_value, 0, sizeof(option_value));
	strcpy(option_value, "secret");
	assert(file_write_block(SOPTS_FILENAME, option_value, 0, sizeof(option_value)));

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
	assert(file_write_block(SOPTS_FILENAME, option_value, 0, sizeof(option_value)));
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

	streamed.clear();
	BufferFiller flash_streaming(stream_buffer, sizeof(stream_buffer));
	flash_streaming.set_flush(capture);
	flash_streaming.emit_p(PSTR("start:$F:end"),
		PSTR("flash string longer than the streaming buffer"));
	assert(!flash_streaming.overflowed());
	assert(streamed == "start:flash string longer than the streaming buffer:end");

	char option_stream_buffer[MAX_SOPTS_SIZE + 16];
	streamed.clear();
	{
		BufferFiller option_streaming(option_stream_buffer, sizeof(option_stream_buffer));
		option_streaming.set_flush(capture);
		BufferFiller::OptionReadScope options(option_streaming);
		option_streaming.emit_p(PSTR("prefix:$O/$O:suffix"), 0, 0);
		assert(!option_streaming.overflowed());
		assert(streamed == "prefix:" + std::string(MAX_SOPTS_SIZE, 'x') + "/" +
			std::string(MAX_SOPTS_SIZE, 'x') + ":suffix");
	}

	assert(file_write_block(SOPTS_FILENAME, "first", 0, 6));
	assert(file_write_block(SOPTS_FILENAME, "second", MAX_SOPTS_SIZE, 7));
	char output[1024];
	BufferFiller reusable(output, sizeof(output));
	{
		BufferFiller::OptionReadScope options(reusable);
		reusable.emit_p(PSTR("$O/$O"), 1, 0);
		assert(!strcmp(output, "second/first"));
		char slot[32];
		options.read_string(1, slot, sizeof(slot) - 1);
		assert(!strcmp(slot, "second"));
		{
			BufferFiller::OptionReadScope nested(reusable);
			reusable.emit_p(PSTR("/$O"), 1);
		}
		reusable.emit_p(PSTR("/$O"), 0); // outer reader restored
		assert(!strcmp(output, "second/first/second/first"));
	}
	assert(file_write_block(SOPTS_FILENAME, "new", 0, 4)); // readers closed before write
	reusable.emit_p(PSTR("/$O"), 0); // no dangling handle after scope
	assert(!strcmp(output, "second/first/second/first/new"));
	assert(remove_file(SOPTS_FILENAME));
	{
		BufferFiller missing(output, sizeof(output));
		BufferFiller::OptionReadScope options(missing);
		missing.emit_p(PSTR("<$O>"), 0);
		assert(file_write_block(SOPTS_FILENAME, "later", 0, 6));
		missing.emit_p(PSTR("<$O>"), 0);
		assert(!strcmp(output, "<><>")); // failed open remembered for this scope
	}
	{
		BufferFiller next(output, sizeof(output));
		BufferFiller::OptionReadScope options(next);
		next.emit_p(PSTR("$O"), 0);
		assert(!strcmp(output, "later"));
	}
	return 0;
}
