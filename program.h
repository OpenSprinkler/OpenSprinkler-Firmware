/* OpenSprinkler Unified Firmware
 * Copyright (C) 2015 by Ray Wang (ray@opensprinkler.com)
 *
 * Program data structures and functions header file
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


#ifndef _PROGRAM_H
#define _PROGRAM_H

#define MAX_NUM_PROGRAMS    40  // maximum number of programs
#define MAX_NUM_STARTTIMES  4
#define PROGRAM_NAME_SIZE   32
#define RUNTIME_QUEUE_SIZE  MAX_NUM_STATIONS
#define PROGRAMSTRUCT_SIZE  sizeof(ProgramStruct)
#include "OpenSprinkler.h"
#include "types.h"

/** Log data structure */
struct LogStruct {
	unsigned char station;
	unsigned char program;
	uint16_t duration;
	uint32_t endtime;
};

#define PROGRAM_TYPE_WEEKLY    0
#define PROGRAM_TYPE_SINGLERUN 1
#define PROGRAM_TYPE_MONTHLY   2
#define PROGRAM_TYPE_INTERVAL  3

#define STARTTIME_SUNRISE_BIT 14
#define STARTTIME_SUNSET_BIT  13
#define STARTTIME_SIGN_BIT    12

#define PROGRAMSTRUCT_EN_BIT   0
#define PROGRAMSTRUCT_UWT_BIT  1

/** Program data structure */
class ProgramStruct {
public:
	unsigned char enabled:1;  // HIGH means the program is enabled

	// weather data
	unsigned char use_weather:1;

	// odd/even restriction:
	// 0->none, 1->odd day (except 31st and Feb 29th)
	// 2->even day, 3->N/A
	unsigned char oddeven:2;

	// schedule type:
	// 0: weekly, 1->single-run, 2->monthly, 3->interval
	unsigned char type:2;

	// starttime type:
	// 0: repeating (give start time, repeat every, number of repeats)
	// 1: fixed start time (give arbitrary start times up to MAX_NUM_STARTTIMEs)
	unsigned char starttime_type:1;

	// enable date range
	unsigned char en_daterange:1;

	// weekly:    days[0][0..6] correspond to Monday till Sunday
	// single-run:days[0] and [1] store the epoch time in days of the start 
	// monthly:   days[0][0..5] stores the day of the month (32 means last day of month)
	// interval:  days[1] stores the interval (0 to 255), days[0] stores the starting day remainder (0 to 254)
	unsigned char days[2];

	// When the program is a fixed start time type:
	//   up to MAX_NUM_STARTTIMES fixed start times
	// When the program is a repeating type:
	//   starttimes[0]: start time
	//   starttimes[1]: repeat count
	//   starttimes[2]: repeat every
	// Start time structure:
	//   bit 15         : not used, reserved
	//   if bit 14 == 1 : sunrise time +/- offset (by lowest 12 bits)
	//   or bit 13 == 1 : sunset	time +/- offset (by lowest 12 bits)
	//      bit 12      : sign, 0 is positive, 1 is negative
	//      bit 11      : not used, reserved
	//   else: standard start time (value between 0 to 1440, by bits 0 to 10)
	int16_t starttimes[MAX_NUM_STARTTIMES];

	uint16_t durations[MAX_NUM_STATIONS];  // duration / water time of each station

	char name[PROGRAM_NAME_SIZE];

	int16_t daterange[2] = {MIN_ENCODED_DATE, MAX_ENCODED_DATE}; // date range: start date, end date
	unsigned char check_match(time_os_t t, bool *to_delete);
	void gen_station_runorder(uint16_t runcount, unsigned char *order);
	int16_t starttime_decode(int16_t t);

protected:

	unsigned char check_day_match(time_os_t t);

};

extern OpenSprinkler os;

class RuntimeQueueStruct {
public:
	time_os_t   st;  // start time
	uint16_t dur; // water time
	unsigned char  sid;
	unsigned char  pid;
	time_os_t   deque_time; // deque time, which can be larger than st+dur to allow positive master off adjustment time
};

class ProgramData {
public:
	static RuntimeQueueStruct queue[];
	static unsigned char nqueue;  // number of queue elements
	static unsigned char station_qid[];  // this array stores the queue element index for each scheduled station
	static unsigned char nprograms;  // number of programs
	static LogStruct lastrun;
	static time_os_t last_seq_stop_times[]; // the last stop time of a sequential station (for each sequential group respectively)

	static void toggle_pause(ulong delay);
	static void set_pause();
	static void resume_stations();
	static void clear_pause();

	static void reset_runtime();
	static RuntimeQueueStruct* enqueue(); // this returns a pointer to the next available slot in the queue
	static void dequeue(unsigned char qid);  // this removes an element from the queue

	static void init();
	static void eraseall();
	static void read(unsigned char pid, ProgramStruct *buf);
	static unsigned char add(ProgramStruct *buf);
	static unsigned char modify(unsigned char pid, ProgramStruct *buf);
	static unsigned char set_flagbit(unsigned char pid, unsigned char bid, unsigned char value);
	static void moveup(unsigned char pid);
	static unsigned char del(unsigned char pid);
	static void drem_to_relative(unsigned char days[2]); // absolute to relative reminder conversion
	static void drem_to_absolute(unsigned char days[2]);
private:
	static void load_count();
	static void save_count();
};

#endif  // _PROGRAM_H
