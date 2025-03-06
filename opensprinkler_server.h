/* OpenSprinkler Unified Firmware
 * Copyright (C) 2015 by Ray Wang (ray@opensprinkler.com)
 *
 * Server functions
 * Feb 2015 @ OpenSprinkler.com
 *
 * This file is part of the OpenSprinkler library
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef _OPENSPRINKLER_SERVER_H
#define _OPENSPRINKLER_SERVER_H

#if !defined(ARDUINO)
#include <stdarg.h>
#include <unistd.h>
#endif

char dec2hexchar(unsigned char dec);

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


#endif // _OPENSPRINKLER_SERVER_H
