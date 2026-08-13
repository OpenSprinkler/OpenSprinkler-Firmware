#pragma once

#include "util/utils.h"

#if defined(ARDUINO)
#include <Arduino.h>
#else
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <inttypes.h>
#endif

typedef void (*bfill_flush_fn)(const char *buf, size_t len);

class BufferFiller {
	char *start = nullptr; //!< Pointer to start of buffer
	char *ptr = nullptr; //!< Pointer to cursor position
	size_t len = 0;
	bfill_flush_fn flush_fn = nullptr;
	bool overflow = false;

	void mid_flush() {
		if (flush_fn && start && ptr && ptr > start) {
			flush_fn(start, static_cast<size_t>(ptr - start));
			ptr = start;
			*ptr = 0;
		}
	}

	bool fail() {
		overflow = true;
		if (start && len) start[len - 1] = 0;
		return false;
	}

	bool ensure_space(size_t count) {
		if (overflow) return false;
		if (position() + count < len) return true;
		if (flush_fn) {
			mid_flush();
			if (count < len) return true;
		}
		return fail();
	}

	bool append_char(char c) {
		if (!ensure_space(1)) return false;
		*ptr++ = c;
		*ptr = 0;
		return true;
	}

	bool append_string(const char *s) {
		if (!s || overflow) return fail();
		size_t slen = strlen(s);
		if (flush_fn && position() + slen >= len) {
			mid_flush();
			if (slen >= len) {
				flush_fn(s, slen);
				return true;
			}
		}
		if (!ensure_space(slen)) return false;
		memcpy(ptr, s, slen + 1);
		ptr += slen;
		return true;
	}

public:
	BufferFiller() {}

	BufferFiller(char *buf, size_t buffer_len) :
		start(buf), ptr(buf), len(buffer_len), overflow(!(buf && buffer_len)) {
		if (!overflow) *ptr = 0;
	}

	char *buffer() const { return start; }
	size_t length() const { return len; }
	unsigned int position() const {
		return start && ptr ? static_cast<unsigned int>(ptr - start) : 0;
	}
	void set_flush(bfill_flush_fn fn) { flush_fn = fn; }
	bool overflowed() const { return overflow; }

	void emit_p(PGM_P fmt, ...) {
		va_list ap;
		va_start(ap, fmt);
		for (;;) {
			char c = pgm_read_byte(fmt++);
			if (c == 0) break;
			if (c != '$') {
				append_char(c);
				continue;
			}

			c = pgm_read_byte(fmt++);
			switch (c) {
			case 'D': {
				char value[16];
				snprintf(value, sizeof(value), "%d", va_arg(ap, int));
				append_string(value);
				break;
			}
			case 'E': {
				char value[32];
				snprintf(value, sizeof(value), "%g", va_arg(ap, double));
				append_string(value);
				break;
			}
			case 'L': {
				char value[16];
				snprintf(value, sizeof(value), "%" PRIu32, va_arg(ap, uint32_t));
				append_string(value);
				break;
			}
			case 'S':
				append_string(va_arg(ap, const char*));
				break;
			case 'X': {
				char value = va_arg(ap, int);
				append_char(dec2hexchar((value >> 4) & 0x0F));
				append_char(dec2hexchar(value & 0x0F));
				break;
			}
			case 'F': {
				PGM_P value = va_arg(ap, PGM_P);
				char byte;
				while ((byte = pgm_read_byte(value++)) != 0) {
					if (!append_char(byte)) break;
				}
				break;
			}
			case 'O': {
				uint16_t oid = va_arg(ap, int);
				if (flush_fn && position() + MAX_SOPTS_SIZE >= len) mid_flush();
				if (overflow || !start || position() >= len) break;

				size_t available = len - position() - 1;
				size_t read_length = available < MAX_SOPTS_SIZE ? available : MAX_SOPTS_SIZE;
				if (!read_length) {
					fail();
					break;
				}
				memset(ptr, 0, read_length + 1);
				file_read_block(SOPTS_FILENAME, ptr, oid * MAX_SOPTS_SIZE, read_length);
				char *terminator = static_cast<char*>(memchr(ptr, 0, read_length));
				if (!terminator) {
					if (read_length < MAX_SOPTS_SIZE) {
						ptr[read_length] = 0;
						fail();
						break;
					}
					terminator = ptr + read_length;
					*terminator = 0;
				}
				ptr = terminator;
				break;
			}
			default:
				append_char(c);
				break;
			}
		}
		if (!overflow && ptr) *ptr = 0;
		va_end(ap);
		mid_flush();
	}
};
