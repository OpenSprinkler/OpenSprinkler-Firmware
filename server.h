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

#if defined(ARDUINO)

#else
class BufferFiller {
    char *start; //!< Pointer to start of buffer
    char *ptr; //!< Pointer to cursor position
public:
    BufferFiller () {}

    BufferFiller (char *buf) : start (buf), ptr (buf) {}

    void emit_p (const char *fmt, ...);

    char* buffer () const { return start; }

    unsigned int position () const { return ptr - start; }
};
#endif

#endif // _SERVER_H
