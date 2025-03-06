/* OpenSprinkler Unified Firmware
 * Copyright (C) 2015 by Ray Wang (ray@opensprinkler.com)
 *
 * Notifier data structures and functions header file
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


#ifndef _NOTIFIER_H
#define _NOTIFIER_H

#define NOTIF_QUEUE_MAXSIZE 32

#include "OpenSprinkler.h"
#include "types.h"

/** Notifier Node data structure */
struct NotifNodeStruct {
	uint16_t type;
	uint32_t lval;
	float fval;
	uint8_t bval;
	NotifNodeStruct *next;
	NotifNodeStruct(uint16_t t, uint32_t l=0, float f=0.f, uint8_t b=0) : type(t), lval(l), fval(f), bval(b), next(NULL)
	{ }
};

/** Notifier Queue data structure */
class NotifQueue {
public:
	// Insert a new notification element
	static bool add(uint16_t t, uint32_t l=0, float f=0.f, uint8_t b=0);
	// Clear all elements (i.e. empty the queue)
	static void clear();
	// Run/Process elements. By default process 1 at a time. If n<=0, process all.
	static bool run(int n=1);
protected:
	static NotifNodeStruct* head;
	static NotifNodeStruct* tail;
	static unsigned char nqueue;
};

#endif  // _NOTIFIER_H