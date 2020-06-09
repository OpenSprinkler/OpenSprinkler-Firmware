/* OpenSprinkler Unified (AVR/RPI/BBB/LINUX) Firmware
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
 
#ifndef _SERVER_H
#define _SERVER_H

#if !defined(ARDUINO)
#include <stdarg.h>
#endif

char dec2hexchar(byte dec);

class BufferFiller {
	char *start; //!< Pointer to start of buffer
	char *ptr; //!< Pointer to cursor position
public:
	BufferFiller () {}
	BufferFiller (char *buf) : start (buf), ptr (buf) {}

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
				//wtoa(va_arg(ap, uint16_t), (char*) ptr);
				itoa(va_arg(ap, int), (char*) ptr, 10);  // ray
				break;
			case 'L':
				//ltoa(va_arg(ap, long), (char*) ptr, 10);
				ultoa(va_arg(ap, long), (char*) ptr, 10); // ray
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

	char* buffer () const { return start; }
	unsigned int position () const { return ptr - start; }
};


#endif // _SERVER_H
