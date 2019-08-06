/* OpenSprinkler Unified (AVR/RPI/BBB/LINUX) Firmware
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

#define MAX_NUM_PROGRAMS		40		// maximum number of programs
#define MAX_NUM_STARTTIMES	4
#define PROGRAM_NAME_SIZE		32
#define RUNTIME_QUEUE_SIZE	MAX_NUM_STATIONS
#define PROGRAMSTRUCT_SIZE	sizeof(ProgramStruct)
#include "OpenSprinkler.h"

/** Log data structure */
struct LogStruct {
	byte station;
	byte program;
	uint16_t duration;
	uint32_t endtime;
};

#define PROGRAM_TYPE_WEEKLY		0
#define PROGRAM_TYPE_BIWEEKLY 1
#define PROGRAM_TYPE_MONTHLY	2
#define PROGRAM_TYPE_INTERVAL 3

#define STARTTIME_SUNRISE_BIT 14
#define STARTTIME_SUNSET_BIT	13
#define STARTTIME_SIGN_BIT		12

#define PROGRAMSTRUCT_EN_BIT	 0
#define PROGRAMSTRUCT_UWT_BIT  1

/** Program data structure */
class ProgramStruct {
public:
	byte enabled	:1;  // HIGH means the program is enabled
	
	// weather data
	byte use_weather: 1;
	
	// odd/even restriction:
	// 0->none, 1->odd day (except 31st and Feb 29th)
	// 2->even day, 3->N/A
	byte oddeven	 :2;
	
	// schedule type:
	// 0: weekly, 1->biweekly, 2->monthly, 3->interval
	byte type			 :2;	
	
	// starttime type:
	// 0: repeating (give start time, repeat every, number of repeats)
	// 1: fixed start time (give arbitrary start times up to MAX_NUM_STARTTIMEs)
	byte starttime_type: 1;

	// misc. data
	byte dummy1: 1;
	
	// weekly:	 days[0][0..6] correspond to Monday till Sunday
	// bi-weekly:days[0][0..6] and [1][0..6] store two weeks
	// monthly:  days[0][0..5] stores the day of the month (32 means last day of month)
	// interval: days[0] stores the interval (0 to 255), days[1] stores the starting day remainder (0 to 254)
	byte days[2];  
	
	// When the program is a fixed start time type:
	//	 up to MAX_NUM_STARTTIMES fixed start times
	// When the program is a repeating type:
	//	 starttimes[0]: start time
	//	 starttimes[1]: repeat count
	//	 starttimes[2]: repeat every
	// Start time structure:
	//	 bit 15					: not used, reserved
	//	 if bit 14 == 1 : sunrise time +/- offset (by lowest 12 bits)
	//	 or bit 13 == 1 : sunset	time +/- offset (by lowest 12 bits)
	//			bit 12			: sign, 0 is positive, 1 is negative
	//			bit 11			: not used, reserved
	//	 else: standard start time (value between 0 to 1440, by bits 0 to 10)
	int16_t starttimes[MAX_NUM_STARTTIMES];

	uint16_t durations[MAX_NUM_STATIONS];  // duration / water time of each station
	
	char name[PROGRAM_NAME_SIZE];

	byte check_match(time_t t);
	int16_t starttime_decode(int16_t t);
	
protected:

	byte check_day_match(time_t t);

};

extern OpenSprinkler os;

class RuntimeQueueStruct {
public:
	ulong		 st;	// start time
	uint16_t dur; // water time
	byte	sid;
	byte	pid;
};

class ProgramData {
public:  
	static RuntimeQueueStruct queue[];
	static byte nqueue;					// number of queue elements
	static byte station_qid[];	// this array stores the queue element index for each scheduled station
	static byte nprograms;			// number of programs
	static LogStruct lastrun;
	static ulong last_seq_stop_time;	// the last stop time of a sequential station
	
	static void reset_runtime();
	static RuntimeQueueStruct* enqueue(); // this returns a pointer to the next available slot in the queue
	static void dequeue(byte qid);	// this removes an element from the queue

	static void init();
	static void eraseall();
	static void read(byte pid, ProgramStruct *buf);
	static byte add(ProgramStruct *buf);
	static byte modify(byte pid, ProgramStruct *buf);
	static byte set_flagbit(byte pid, byte bid, byte value);
	static void moveup(byte pid);  
	static byte del(byte pid);
	static void drem_to_relative(byte days[2]); // absolute to relative reminder conversion
	static void drem_to_absolute(byte days[2]);
private:	
	static void load_count();
	static void save_count();
};

#endif	// _PROGRAM_H
