#ifndef _BFILLER_H
#define _BFILLER_H

#include "utils.h"

#if defined(ARDUINO)
#include <Arduino.h>
#else
#include <stdio.h>
#include <string.h>
#endif

class BufferFiller {
	char *start; //!< Pointer to start of buffer
	char *ptr; //!< Pointer to cursor position
	size_t len;
public:
	BufferFiller () {}
	BufferFiller (char *buf, size_t buffer_len) {
		start = buf;
		ptr = buf;
		len = buffer_len;
	}

	char* buffer () const { return start; }
	size_t length () const { return len; }
	unsigned int position () const { return ptr - start; }

	void emit_p(PGM_P fmt, ...) {
		va_list ap;
		va_start(ap, fmt);
		for (;;) {
			char c = pgm_read_byte(fmt++);
			if (c == 0)
				break;
			if (c != '$') {
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
				sprintf((char*) ptr, "%10.6lf", va_arg(ap, double));
				break;		
			case 'L':
				// ultoa(va_arg(ap, uint32_t), (char*) ptr, 10);
				snprintf((char*) ptr, len - position(), "%lu", (unsigned long) va_arg(ap, uint32_t));
				break;
			case 'S':
				strcpy((char*) ptr, va_arg(ap, const char*));
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
				while ((d = pgm_read_byte(s++)) != 0)
						*ptr++ = d;
				continue;
			}
			case 'O': {
				uint16_t oid = va_arg(ap, int);
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
	}
};

#endif //_BFILLER_H