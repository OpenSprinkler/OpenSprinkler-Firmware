/* OpenSprinkler Unified Firmware
 * Copyright (C) 2015 by Ray Wang (ray@opensprinkler.com)
 *
 * Program data structures and functions
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

#include <limits.h>
#include "program.h"
#include "main.h"

#if !defined(SECS_PER_DAY)
#define SECS_PER_MIN  (60UL)
#define SECS_PER_HOUR (3600UL)
#define SECS_PER_DAY  (SECS_PER_HOUR * 24UL)
#endif

// Declare static data members
unsigned char ProgramData::nprograms = 0;
unsigned char ProgramData::nqueue = 0;
RuntimeQueueStruct ProgramData::queue[RUNTIME_QUEUE_SIZE];
unsigned char ProgramData::station_qid[MAX_NUM_STATIONS];
LogStruct ProgramData::lastrun;
time_os_t ProgramData::last_seq_stop_times[NUM_SEQ_GROUPS];

extern char tmp_buffer[];

void ProgramData::init() {
	reset_runtime();
	load_count();
}

void ProgramData::reset_runtime() {
	memset(station_qid, 0xFF, MAX_NUM_STATIONS);  // reset station qid to 0xFF
	nqueue = 0;
	memset(last_seq_stop_times, 0, sizeof(last_seq_stop_times));
}

/** Insert a new element to the queue
 * This function returns pointer to the next available element in the queue
 * and returns NULL if the queue is full
 */
RuntimeQueueStruct* ProgramData::enqueue() {
	if (nqueue < RUNTIME_QUEUE_SIZE) {
		nqueue ++;
		return queue + (nqueue-1);
	} else {
		return NULL;
	}
}

/** Remove an element from the queue
 * This function copies the last element of
 * the queue to overwrite the requested
 * element, therefore removing the requested element.
 */
// this removes an element from the queue
void ProgramData::dequeue(unsigned char qid) {
	if (qid>=nqueue)	return;
	if (qid<nqueue-1) {
		queue[qid] = queue[nqueue-1]; // copy the last element to the dequeud element to fill the space
		if(station_qid[queue[qid].sid] == nqueue-1) // fix queue index if necessary
			station_qid[queue[qid].sid] = qid;
	}
	nqueue--;
}

/** Load program count from program file */
void ProgramData::load_count() {
	nprograms = file_read_byte(PROG_FILENAME, 0);
}

/** Save program count to program file */
void ProgramData::save_count() {
	file_write_byte(PROG_FILENAME, 0, nprograms);
}

/** Erase all program data */
void ProgramData::eraseall() {
	nprograms = 0;
	save_count();
}

/** Read a program from program file*/
void ProgramData::read(unsigned char pid, ProgramStruct *buf) {
	if (pid >= nprograms) return;
	// first unsigned char is program counter, so 1+
	file_read_block(PROG_FILENAME, buf, 1+(ulong)pid*PROGRAMSTRUCT_SIZE, PROGRAMSTRUCT_SIZE);
}

/** Add a program */
unsigned char ProgramData::add(ProgramStruct *buf) {
	if (nprograms >= MAX_NUM_PROGRAMS)	return 0;
	file_write_block(PROG_FILENAME, buf, 1+(ulong)nprograms*PROGRAMSTRUCT_SIZE, PROGRAMSTRUCT_SIZE);
	nprograms ++;
	save_count();
	return 1;
}

/** Move a program up (i.e. swap a program with the one above it) */
void ProgramData::moveup(unsigned char pid) {
	if(pid >= nprograms || pid == 0) return;
	// swap program pid-1 and pid
	ulong pos = 1+(ulong)(pid-1)*PROGRAMSTRUCT_SIZE;
	ulong next = pos+PROGRAMSTRUCT_SIZE;
	char buf2[PROGRAMSTRUCT_SIZE];
	file_read_block(PROG_FILENAME, tmp_buffer, pos, PROGRAMSTRUCT_SIZE);
	file_read_block(PROG_FILENAME, buf2, next, PROGRAMSTRUCT_SIZE);
	file_write_block(PROG_FILENAME, tmp_buffer, next, PROGRAMSTRUCT_SIZE);
	file_write_block(PROG_FILENAME, buf2, pos, PROGRAMSTRUCT_SIZE);
}

void ProgramData::toggle_pause(ulong delay) {
	if (os.status.pause_state) { // was paused
		resume_stations();
	} else {
		//os.clear_all_station_bits(); // TODO: this will affect logging
		os.pause_timer = delay;
		set_pause();
	}
	os.status.pause_state = !os.status.pause_state;
}

void ProgramData::set_pause() {
	RuntimeQueueStruct *q = queue;
	time_os_t curr_t = os.now_tz();

	for (; q < queue + nqueue; q++) {

		turn_off_station(q->sid, curr_t);
		if (curr_t>=q->st+q->dur) { // already finished running
			continue;
		} else if (curr_t>=q->st) { // currently running
			q->dur -= (curr_t - q->st); // adjust remaining run time
			q->st = curr_t + os.pause_timer;     // push back start time
		} else { // scheduled but not running yet
			q->st += os.pause_timer;
		}
		q->deque_time += os.pause_timer;
		unsigned char gid = os.get_station_gid(q->sid);
		if (q->st + q->dur > last_seq_stop_times[gid]) {
			last_seq_stop_times[gid] = q->st + q->dur; // update last_seq_stop_times of the corresponding group
		}
	}
}

void ProgramData::resume_stations() {
	RuntimeQueueStruct *q = queue;
	for (; q < queue + nqueue; q++) {
		q->st -= os.pause_timer;
		q->deque_time -= os.pause_timer;
		q->st += 1; // adjust by 1 second to give time for scheduler
		q->deque_time += 1;
	}
	clear_pause();
}

void ProgramData::clear_pause() {
	os.status.pause_state = 0;
	os.pause_timer = 0;
	memset(last_seq_stop_times, 0, sizeof(last_seq_stop_times));
}

/** Modify a program */
unsigned char ProgramData::modify(unsigned char pid, ProgramStruct *buf) {
	if (pid >= nprograms)  return 0;
	ulong pos = 1+(ulong)pid*PROGRAMSTRUCT_SIZE;
	file_write_block(PROG_FILENAME, buf, pos, PROGRAMSTRUCT_SIZE);
	return 1;
}

/** Delete program(s) */
unsigned char ProgramData::del(unsigned char pid) {
	if (pid >= nprograms)  return 0;
	if (nprograms == 0) return 0;
	ulong pos = 1+(ulong)(pid+1)*PROGRAMSTRUCT_SIZE;
	// erase by copying backward
	for (; pos < 1+(ulong)nprograms*PROGRAMSTRUCT_SIZE; pos+=PROGRAMSTRUCT_SIZE) {
		file_copy_block(PROG_FILENAME, pos, pos-PROGRAMSTRUCT_SIZE, PROGRAMSTRUCT_SIZE, tmp_buffer);
	}
	nprograms --;
	save_count();
	return 1;
}

// set the enable bit
unsigned char ProgramData::set_flagbit(unsigned char pid, unsigned char bid, unsigned char value) {
	if (pid >= nprograms)  return 0;
	unsigned char flag = file_read_byte(PROG_FILENAME, 1+(ulong)pid*PROGRAMSTRUCT_SIZE);
	if(value) flag|=(1<<bid);
	else flag&=(~(1<<bid));
	file_write_byte(PROG_FILENAME, 1+(ulong)pid*PROGRAMSTRUCT_SIZE, flag);
	return 1;
}

/** Decode a sunrise/sunset start time to actual start time */
int16_t ProgramStruct::starttime_decode(int16_t t) {
	if((t>>15)&1) return -1;
	int16_t offset = t&0x7ff;
	if((t>>STARTTIME_SIGN_BIT)&1) offset = -offset;
	if((t>>STARTTIME_SUNRISE_BIT)&1) { // sunrise time
		t = os.nvdata.sunrise_time + offset;
		if (t<0) t=0; // clamp it to 0 if less than 0
	} else if((t>>STARTTIME_SUNSET_BIT)&1) {
		t = os.nvdata.sunset_time + offset;
		if (t>=1440) t=1439; // clamp it to 1440 if larger than 1440
	}
	return t;
}

/** Check if a given time matches the program's start day */
unsigned char ProgramStruct::check_day_match(time_os_t t) {

#if defined(ARDUINO)  // get current time from Arduino
	unsigned char weekday_t = weekday(t);  // weekday ranges from [0,6] within Sunday being 1
	unsigned char day_t = day(t);
	unsigned char month_t = month(t);
#else // get current time from RPI/LINUX
	time_os_t ct = t;
	struct tm *ti = gmtime(&ct);
	unsigned char weekday_t = (ti->tm_wday+1)%7;  // tm_wday ranges from [0,6] with Sunday being 0
	unsigned char day_t = ti->tm_mday;
	unsigned char month_t = ti->tm_mon+1;  // tm_mon ranges from [0,11]
#endif // get current time

	int epoch_t = (t / 86400);
	unsigned char wd = (weekday_t+5)%7;
	unsigned char dt = day_t;

	if(en_daterange) { // check date range if enabled
		int16_t currdate = date_encode(month_t, day_t);
		// depending on whether daterange[0] or [1] is smaller:
		if(daterange[0]<=daterange[1]) {
			if(currdate<daterange[0]||currdate>daterange[1]) return 0;
		} else {
			// this is the case where the defined range crosses the end of the year
			if(currdate>daterange[1] && currdate<daterange[0]) return 0;
		}
	}

	// check day match
	switch(type) {
		case PROGRAM_TYPE_WEEKLY:
			// weekday match
			if (!(days[0] & (1<<wd)))
				return 0;
		break;

		case PROGRAM_TYPE_SINGLERUN:
			// check match of exact day
			if(((days[0] << 8) + days[1]) != epoch_t)
				return 0;
		break;

		case PROGRAM_TYPE_MONTHLY:
			if ((days[0]&0b11111) == 0) {
				if(!isLastDayofMonth(month_t, dt))
					return 0;
			} else if (dt != (days[0]&0b11111)){
				return 0;
			}
				
		break;

		case PROGRAM_TYPE_INTERVAL:
			// this is an inverval program
			if (((t/SECS_PER_DAY)%days[1]) != days[0])	return 0;
		break;
	}

	// check odd/even day restriction
	if (!oddeven) { }
	else if (oddeven == 2) {
		// even day restriction
		if((dt%2)!=0)  return 0;
	} else if (oddeven == 1) {
		// odd day restriction
		// skip 31st and Feb 29
		if(dt==31)	return 0;
		else if (dt==29 && month_t==2)	return 0;
		else if ((dt%2)!=1)  return 0;
	}
	return 1;
}

// Check if a given time matches program's start time
// this also checks for programs that started the previous
// day and ran over night
// Return value: 0 if no match; otherwise return the n-th count of the match.
// For example, if this is the first-run of the day, return 1 etc.
unsigned char ProgramStruct::check_match(time_os_t t, bool *to_delete) {

	// check program enable status
	if (!enabled) return 0;

	int16_t start = starttime_decode(starttimes[0]);
	int16_t repeat = starttimes[1];
	int16_t interval = starttimes[2];
	int16_t current_minute = (t%86400L)/60;

	// first assume program starts today
	if (check_day_match(t)) {
		// t matches the program's start day

		if (starttime_type) {
			// given start time type
			unsigned char maxStartTime = -1;
			for(unsigned char i=0;i<MAX_NUM_STARTTIMES;i++) {
				if (starttime_decode(starttimes[i]) > maxStartTime){
					maxStartTime = starttime_decode(starttimes[i]);
				}
			}
			for(unsigned char i=0;i<MAX_NUM_STARTTIMES;i++) {
				//if curr = largest start time and the program is run once --> delete
				if (current_minute == starttime_decode(starttimes[i])){
					if(maxStartTime == current_minute && type == PROGRAM_TYPE_SINGLERUN){
						*to_delete = true;
					}else{
						*to_delete = false;
					}
					return (i+1); // if curren_minute matches any of the given start time, return matched index + 1
				}
			}
			return 0; // otherwise return 0
		} else {
			// repeating type
			// if current_minute matches start time, return 1
			// if also no interval and run once --> delete
			if (current_minute == start){
				if(!interval && type == PROGRAM_TYPE_SINGLERUN){
					*to_delete = true;
				}else{
					*to_delete = false;
				}
				return 1;
			}

			// otherwise, current_minute must be larger than start time, and interval must be non-zero
			if (current_minute > start && interval) {
				// check if we are on any interval match
				int16_t c = (current_minute - start) / interval;
				if ((c * interval == (current_minute - start)) && c <= repeat) {
					//if c == repeat (final repeat) and program is run-once --> delete
					if(c == repeat && type == PROGRAM_TYPE_SINGLERUN){
						*to_delete = true;
					}else{
						*to_delete = false;
					}
					return (c+1);  // return match count n
				}
			}
		}
	}
	// to proceed, program has to be repeating type, and interval and repeat must be non-zero
	if (starttime_type || !interval)	return 0;

	// next, assume program started the previous day and ran over night
	if (check_day_match(t-86400L)) {
		// t-86400L matches the program's start day
		int16_t c = (current_minute - start + 1440) / interval;
		if ((c * interval == (current_minute - start + 1440)) && c <= repeat) {
			//if c == repeat (final repeat) and program is run-once --> delete
			if(c == repeat && type == PROGRAM_TYPE_SINGLERUN){
				*to_delete = true;
			}else{
				*to_delete = false;
			}
			return (c+1);  // return the match count n
		}
	}
	return 0;
}

struct StationNameSortElem {
	unsigned char idx;
	char *name;
};

int StationNameSortAscendCmp(const void *a, const void *b) {
	return strcmp(((StationNameSortElem*)a)->name,((StationNameSortElem*)b)->name);
}

int StationNameSortDescendCmp(const void *a, const void *b) {
	return StationNameSortAscendCmp(b, a);
}

// generate station runorder based on the annotation in program names
// alternating means on the odd numbered runs of the program, it uses one order; on the even runs, it uses the opposite order
void ProgramStruct::gen_station_runorder(uint16_t runcount, unsigned char *order) {
	unsigned char len = strlen(name);
	unsigned char ns = os.nstations;
	int16_t i;
	unsigned char temp;

	// default order: ascending by index
	for(i=0;i<ns;i++) {
		order[i] = i;
	}

	// check matches with program name annotation
	if(len>=2 && name[len-2]=='>') {
		char anno = name[len-1];
		switch(anno) {
			case 'I':	// descending by index
			case 'a': // alternating: odd-numbered runs ascending by index, even-numbered runs descending.
			case 'A': // odd-numbered runs descending by index, even-numbered runs ascending

			if(anno=='I' || (anno=='a' && (runcount%2==0)) || (anno=='A' && (runcount%2==1)))  {
				// reverse the order
				for(i=0;i<ns;i++) {
					order[i] = ns-1-i;
				}
			}
			break;

			case 'n': // ascending by name
			case 'N': // descending by name
			case 't': // alternating: odd-numbered runs ascending by name, even-numbered runs descending.
			case 'T': // odd-numbered runs descending by name, even-numbered runs ascending
			{
				StationNameSortElem elems[ns];
				for(i=0;i<ns;i++) {
					elems[i].idx=i;
					os.get_station_name(i,tmp_buffer);
					elems[i].name=strdup(tmp_buffer);
				}
				if(anno=='n' || (anno=='t' && (runcount%2==0)) || (anno='T' && (runcount%2==1))) {
					qsort(elems, ns, sizeof(StationNameSortElem), StationNameSortAscendCmp);
				} else {
					qsort(elems, ns, sizeof(StationNameSortElem), StationNameSortDescendCmp);
				}
				for(i=0;i<ns;i++) {
					order[i]=elems[i].idx;
					free(elems[i].name);
				}
			}
			break;

			case 'r': // random ordering
			case 'R': // random ordering

			{
				for(i=0;i<ns-1;i++) { // todo: need random seeding
					unsigned char sel = (rand()%(ns-i))+i;
					temp = order[i]; // swap order[i] with order[sel]
					order[i] = order[sel];
					order[sel] = temp;
				}
			}
			break;
		}
	}
	DEBUG_PRINT("station order:[");
	for(i=0;i<ns;i++) {
		DEBUG_PRINT(order[i]);
		DEBUG_PRINT(",");
	}
	DEBUG_PRINTLN("]");
}

// convert absolute remainder (reference time 1970 01-01) to relative remainder (reference time today)
// absolute remainder is stored in flash, relative remainder is presented to web
void ProgramData::drem_to_relative(unsigned char days[2]) {
	unsigned char rem_abs=days[0];
	unsigned char inv=days[1];
	// todo future: use now_tz()?
	days[0] = (unsigned char)((rem_abs + inv - (os.now_tz()/SECS_PER_DAY) % inv) % inv);
}

// relative remainder -> absolute remainder
void ProgramData::drem_to_absolute(unsigned char days[2]) {
	unsigned char rem_rel=days[0];
	unsigned char inv=days[1];
	// todo future: use now_tz()?
	days[0] = (unsigned char)(((os.now_tz()/SECS_PER_DAY) + rem_rel) % inv);
}
