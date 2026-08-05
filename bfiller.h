#pragma once

#include "utils.h"

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
	char *start; //!< Pointer to start of buffer
	char *ptr; //!< Pointer to cursor position
	size_t len;
	bfill_flush_fn flush_fn = nullptr;

	void mid_flush() {
		if (flush_fn && ptr > start) {
			flush_fn(start, (size_t)(ptr - start));
			ptr = start;
			*ptr = 0;
		}
	}
public:
	BufferFiller () {}
	BufferFiller (char *buf, size_t buffer_len) {
		start = buf;
		ptr = buf;
		len = buffer_len;
		flush_fn = nullptr;
	}

	char* buffer () const { return start; }
	size_t length () const { return len; }
	unsigned int position () const { return ptr - start; }
	void set_flush(bfill_flush_fn fn) { flush_fn = fn; }

	void emit_p(PGM_P fmt, ...) {
		va_list ap;
		va_start(ap, fmt);
		for (;;) {
			char c = pgm_read_byte(fmt++);
			if (c == 0)
				break;
			if (c != '$') {
				if ((size_t)(ptr - start) >= len - 1) mid_flush();
				*ptr++ = c;
				continue;
			}
			c = pgm_read_byte(fmt++);
			switch (c) {
			case 'D':
				// itoa(va_arg(ap, int), (char*) ptr, 10);  // ray
				snprintf((char*) ptr, len - position(),  "%d", va_arg(ap, int));
				break;
			case 'E': //Double
				snprintf((char*) ptr, len - position(), "%g", va_arg(ap, double));
				break;
			case 'L':
				// ultoa(va_arg(ap, uint32_t), (char*) ptr, 10);
				// TODO: check if there is a way to print uint32_t
				snprintf((char*) ptr, len - position(), "%" PRIu32, va_arg(ap, uint32_t));
				break;
			case 'S': {
				const char *s = va_arg(ap, const char*);
				size_t slen = strlen(s);
				if (flush_fn && (size_t)(ptr - start) + slen + 1 > len) {
					mid_flush();
					if (slen >= len) {
						// string larger than entire buffer: stream directly
						flush_fn(s, slen);
						*ptr = 0;
						break;
					}
				}
				strcpy((char*) ptr, s);
			}
				break;
			case 'X': {
				char d = va_arg(ap, int);
				*ptr++ = dec2hexchar((d >> 4) & 0x0F);
				*ptr++ = dec2hexchar(d & 0x0F);
			}
				continue;
			case 'F': {
				PGM_P s = va_arg(ap, PGM_P);
				char d;
				while ((d = pgm_read_byte(s++)) != 0) {
					if ((size_t)(ptr - start) >= len - 1) mid_flush();
					*ptr++ = d;
				}
				continue;
			}
			case 'O': {
				uint16_t oid = va_arg(ap, int);
				if (flush_fn && (size_t)(ptr - start) + MAX_SOPTS_SIZE + 1 > len)
					mid_flush();
				file_read_block(SOPTS_FILENAME, (char*) ptr, oid*MAX_SOPTS_SIZE, MAX_SOPTS_SIZE);
			}
				break;
			default:
				*ptr++ = c;
				continue;
			}
			ptr += strlen((char*) ptr);
		}
		*(ptr)=0;
		va_end(ap);
		mid_flush();
	}
};
