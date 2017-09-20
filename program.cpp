/* OpenSprinkler Unified (AVR/RPI/BBB/LINUX) Firmware
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

#if !defined(SECS_PER_DAY)
#define SECS_PER_MIN  (60UL)
#define SECS_PER_HOUR (3600UL)
#define SECS_PER_DAY  (SECS_PER_HOUR * 24UL)
#endif

// Declare static data members
byte ProgramData::nprograms = 0;
byte ProgramData::nqueue = 0;
byte ProgramData::pgm_nqueue = 0;
RuntimeQueueStruct ProgramData::queue[RUNTIME_QUEUE_SIZE];
RuntimePgmStruct ProgramData::pgm_queue[RUNTIME_QUEUE_SIZE];
byte ProgramData::station_qid[MAX_NUM_STATIONS];
byte ProgramData::program_qid[256]; // FIX ME Size should be MAX_PROGRAM + 2 (i.e. for Manual (99) and Run-Once (254)
LogStruct ProgramData::lastrun;
ulong ProgramData::last_seq_stop_time;

void ProgramData::init() {
       reset_runtime();
  load_count();
}

void ProgramData::reset_runtime() {
  memset(station_qid, 0xFF, MAX_NUM_STATIONS);  // reset station qid to 0xFF
  memset(program_qid, 0xFF, sizeof(program_qid));  // reset program qid to 0xFF
  nqueue = 0;
  pgm_nqueue = 0;
  last_seq_stop_time = 0;
}

/** Add a new element to the queue
 */
boolean ProgramData::queue_full(void) {
  return (nqueue < RUNTIME_QUEUE_SIZE ? 0 : 1);
}

/** Insert a new element to the queue
 * This function returns pointer to the next available element in the queue
 * and returns NULL if the queue is full
 */
RuntimeQueueStruct* ProgramData::enqueue(RuntimeQueueStruct* q) {
  RuntimePgmStruct * pgm = pgm_queue;

  if (nqueue >= RUNTIME_QUEUE_SIZE) return NULL;

  // Keep a record of the number of stations associated with a program
  for (pgm = pgm_queue; pgm < pgm_queue + pgm_nqueue; pgm++) {
    if (pgm->pid == q->pid && pgm->timestamp == q->timestamp) {
      pgm->count++;
      break;
    }
  }

  // If this is a new program then create a record on the program queue
  if (pgm == pgm_queue + pgm_nqueue) {
    pgm->timestamp = q->timestamp;
    pgm->pid = q->pid;
    pgm->st = -1;
    pgm->et = 0;
    pgm->volume = 0;
	pgm->running = false;
    pgm->count = 1;
	pgm_nqueue++;
  }

  // Store the station run on the scheduler queue
  queue[nqueue] = *q;
  queue[nqueue].pgm = pgm;
  nqueue++;

  return &(queue[nqueue - 1]);
}

/** Remove an element from the queue
 * This function copies the last element of
 * the queue to overwrite the requested
 * element, therefore removing the requested element.
 */
// this removes an element from the queue
void ProgramData::dequeue(RuntimeQueueStruct * q) {
  RuntimePgmStruct * pgm = q->pgm;
  byte sid = q->sid;
  byte pid = q->pid;

  if (q >= queue + nqueue)  return;

  // Remove the entry from the queue and fix up station_qid to match
  if (q < queue + nqueue - 1) {
    *q = queue[nqueue - 1]; // copy the last element to the dequeued element to fill the space
    if (station_qid[q->sid] == nqueue - 1) // fix queue index if necessary
      station_qid[q->sid] = q - queue;
  }
  nqueue--;

  station_qid[sid] = 0xFF;

  // Check if there is an earlier scheduled run for the sid in the queue
  ulong first = -1;
  for (q = queue; q < queue + nqueue; q++) {
    if (q->sid == sid && q->st < first) {
      station_qid[sid] = q - queue;
      first = q->st;
    }
  }

  // Decrement the remaining stations associated with a scheduled program 
  pgm->count--;

  if (pgm->count == 0) {
    // Remove the program from the queue if all stations have been dequeued
    if (pgm < pgm_queue + pgm_nqueue - 1) {
      *pgm = pgm_queue[pgm_nqueue - 1];
      if (program_qid[pgm->pid] == pgm_nqueue - 1)
        program_qid[pgm->pid] = pgm - pgm_queue;
    }
    pgm_nqueue--;
    program_qid[pid] = 0xFF;
  } else if (pgm->et == q->st + q->dur) {
    // Fix the end-time of the program
    q->pgm->et = 0;
    for (q = queue; q < queue + nqueue; q++) {
      if (q->pgm == pgm && q->st + q->dur > pgm->et)
        pgm->et = q->st + q->dur;
    }
  }

  // Check if there is another scheduled run for the pid in the queue
  if (program_qid[pid] == 0xFF) {
    ulong first = -1;
    for (pgm = pgm_queue; pgm < pgm_queue + pgm_nqueue; pgm++) {
      if (pgm->pid == pid && pgm->st < first) {
        program_qid[pid] = pgm - pgm_queue;
        first = pgm->st;
      }
    }
  }
}

// Set the start time of the queue item and update the associated program
void ProgramData::schedule(RuntimeQueueStruct * q, ulong st) {
  RuntimePgmStruct * pgm = q->pgm;

  if (q >= queue + nqueue)  return;
  q->st = st;

  if (q->st < q->pgm->st) q->pgm->st = q->st;
  if (q->st + q->dur > q->pgm->et) q->pgm->et = q->st + q->dur;

  if (station_qid[q->sid] == 0xFF || q->st < queue[station_qid[q->sid]].st)
    station_qid[q->sid] = q - queue;
  if (program_qid[pgm->pid] == 0xFF || pgm->st < pgm_queue[program_qid[pgm->pid]].st)
    program_qid[pgm->pid] = pgm - pgm_queue;
}

// Reset queue duration to force dequeue on next loop
void ProgramData::cancel(RuntimeQueueStruct * q) {
  RuntimePgmStruct * pgm = q->pgm;

  if (q >= queue + nqueue) return;

  q->dur = os.now_tz() - q->st;

  // Fix the end-time of the program
  pgm->et = 0;
  for (q = queue; q < queue + nqueue; q++) {
    if (q->pgm == pgm && q->st + q->dur > pgm->et)
      pgm->et = q->st + q->dur;
  }
}

void ProgramData::print_queue() {
  RuntimeQueueStruct * q;
  RuntimePgmStruct * p;

  DEBUG_PRINT("Scheduler - Queue:");
  for (q = queue; q < queue + nqueue; q++) {
    DEBUG_PRINT("[sid:");
    DEBUG_PRINT(q->sid);
    DEBUG_PRINT(", pid:");
    DEBUG_PRINT(q->pid);
    DEBUG_PRINT(", pgm:");
    DEBUG_PRINT(q->pgm - pgm_queue);
    DEBUG_PRINT(", st:");
    DEBUG_PRINT(q->st);
    DEBUG_PRINT(", dur:");
    DEBUG_PRINT(q->dur);
    DEBUG_PRINT("],");
  }
  DEBUG_PRINT("Count:");
  DEBUG_PRINTLN(nqueue);
  DEBUG_PRINT("Scheduler - Pgm:  ");
  for (p = pgm_queue; p < pgm_queue + pgm_nqueue; p++) {
    DEBUG_PRINT("[pid:");
    DEBUG_PRINT(p->pid);
    DEBUG_PRINT(", st:");
    DEBUG_PRINT(p->st);
    DEBUG_PRINT(", et:");
    DEBUG_PRINT(p->et);
    DEBUG_PRINT(", count:");
    DEBUG_PRINT(p->count);
    DEBUG_PRINT("],");
  }
  DEBUG_PRINT("Count:");
  DEBUG_PRINTLN(pgm_nqueue);
}

/** Load program count from NVM */
void ProgramData::load_count() {
  nprograms = nvm_read_byte((byte *) ADDR_PROGRAMCOUNTER);
}

/** Save program count to NVM */
void ProgramData::save_count() {
  nvm_write_byte((byte *) ADDR_PROGRAMCOUNTER, nprograms);
}

/** Erase all program data */
void ProgramData::eraseall() {
  nprograms = 0;
  save_count();
}

/** Read a program from NVM*/
void ProgramData::read(byte pid, ProgramStruct *buf) {
  if (pid >= nprograms) return;
  if (0) {
    // todo: handle SD card
  } else {
    unsigned int addr = ADDR_PROGRAMDATA + (unsigned int)pid * PROGRAMSTRUCT_SIZE;
    nvm_read_block((void*)buf, (const void *)addr, PROGRAMSTRUCT_SIZE);  
  }
}

/** Add a program */
byte ProgramData::add(ProgramStruct *buf) {
  if (0) {
    // todo: handle SD card
  } else {
    if (nprograms >= MAX_NUMBER_PROGRAMS)  return 0;
    unsigned int addr = ADDR_PROGRAMDATA + (unsigned int)nprograms * PROGRAMSTRUCT_SIZE;
    nvm_write_block((const void*)buf, (void *)addr, PROGRAMSTRUCT_SIZE);
    nprograms ++;
    save_count();
  }
  return 1;
}

/** Move a program up (i.e. swap a program with the one above it) */
void ProgramData::moveup(byte pid) {
  if(pid >= nprograms || pid == 0) return;

  if(0) {
    // todo: handle SD card
  } else {
    // swap program pid-1 and pid
    unsigned int src = ADDR_PROGRAMDATA + (unsigned int)(pid-1) * PROGRAMSTRUCT_SIZE;
    unsigned int dst = src + PROGRAMSTRUCT_SIZE;
#if defined(ARDUINO) // NVM write for Arduino
    byte tmp;
    for(int i=0;i<PROGRAMSTRUCT_SIZE;i++,src++,dst++) {
      tmp = nvm_read_byte((byte *)src);
      nvm_write_byte((byte *)src, nvm_read_byte((byte *)dst));
      nvm_write_byte((byte *)dst, tmp);
    }
#else // NVM write for RPI/BBB
    ProgramStruct tmp1, tmp2;
    nvm_read_block(&tmp1, (void *)src, PROGRAMSTRUCT_SIZE);
    nvm_read_block(&tmp2, (void *)dst, PROGRAMSTRUCT_SIZE);
    nvm_write_block(&tmp1, (void *)dst, PROGRAMSTRUCT_SIZE);
    nvm_write_block(&tmp2, (void *)src, PROGRAMSTRUCT_SIZE);
#endif // NVM write
  }
}

/** Modify a program */
byte ProgramData::modify(byte pid, ProgramStruct *buf) {
  if (pid >= nprograms)  return 0;
  if (0) {
    // handle SD card
  } else {
    unsigned int addr = ADDR_PROGRAMDATA + (unsigned int)pid * PROGRAMSTRUCT_SIZE;
    nvm_write_block((const void*)buf, (void *)addr, PROGRAMSTRUCT_SIZE);
  }
  return 1;
}

/** Delete program(s) */
byte ProgramData::del(byte pid) {
  if (pid >= nprograms)  return 0;
  if (nprograms == 0) return 0;
  if (0) {
    // handle SD card
  } else {
    ProgramStruct copy;
    unsigned int addr = ADDR_PROGRAMDATA + (unsigned int)(pid+1) * PROGRAMSTRUCT_SIZE;
    // erase by copying backward
    for (; addr < ADDR_PROGRAMDATA + nprograms * PROGRAMSTRUCT_SIZE; addr += PROGRAMSTRUCT_SIZE) {
      nvm_read_block((void*)&copy, (const void *)addr, PROGRAMSTRUCT_SIZE);  
      nvm_write_block((const void*)&copy, (void *)(addr-PROGRAMSTRUCT_SIZE), PROGRAMSTRUCT_SIZE);
    }
    nprograms --;
    save_count();
  }
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
byte ProgramStruct::check_day_match(time_t t) {

#if defined(ARDUINO) // get current time from Arduino
  byte weekday_t = weekday(t);        // weekday ranges from [0,6] within Sunday being 1
  byte day_t = day(t);
  byte month_t = month(t);
#else // get current time from RPI/BBB
  time_t ct = t;
  struct tm *ti = gmtime(&ct);
  byte weekday_t = (ti->tm_wday+1)%7;  // tm_wday ranges from [0,6] with Sunday being 0
  byte day_t = ti->tm_mday;
  byte month_t = ti->tm_mon+1;   // tm_mon ranges from [0,11]
#endif // get current time

  byte wd = (weekday_t+5)%7;
  byte dt = day_t;

  // check day match
  switch(type) {
    case PROGRAM_TYPE_WEEKLY:
      // weekday match
      if (!(days[0] & (1<<wd)))
        return 0;
    break;

    case PROGRAM_TYPE_BIWEEKLY:
      // todo
    break;

    case PROGRAM_TYPE_MONTHLY:
      if (dt != (days[0]&0b11111))
        return 0;
    break;

    case PROGRAM_TYPE_INTERVAL:
      // this is an inverval program
      if (((t/SECS_PER_DAY)%days[1]) != days[0])  return 0;
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
    if(dt==31)  return 0;
    else if (dt==29 && month_t==2)  return 0;
    else if ((dt%2)!=1)  return 0;
  }
  return 1;
}

// Check if a given time matches program's start time
// this also checks for programs that started the previous
// day and ran over night
byte ProgramStruct::check_match(time_t t) {

  // check program enable status
  if (!enabled) return 0;

  int16_t start = starttime_decode(starttimes[0]);
  int16_t repeat = starttimes[1];
  int16_t interval = starttimes[2];
  unsigned int current_minute = (t%86400L)/60;

  // first assume program starts today
  if (check_day_match(t)) {
    // t matches the program's start day

    if (starttime_type) {
      // given start time type
      for(byte i=0;i<MAX_NUM_STARTTIMES;i++) {
        if (current_minute == starttime_decode(starttimes[i]))  return 1; // if curren_minute matches any of the given start time, return 1
      }
      return 0; // otherwise return 0
    } else {
      // repeating type
      // if current_minute matches start time, return 1
      if (current_minute == start) return 1;

      // otherwise, current_minute must be larger than start time, and interval must be non-zero
      if (current_minute > start && interval) {
        // check if we are on any interval match
        int16_t c = (current_minute - start) / interval;
        if ((c * interval == (current_minute - start)) && c <= repeat) {
          return 1;
        }
      }
    }
  }
  // to proceed, program has to be repeating type, and interval and repeat must be non-zero
  if (starttime_type || !interval)  return 0;

  // next, assume program started the previous day and ran over night
  if (check_day_match(t-86400L)) {
    // t-86400L matches the program's start day
    int16_t c = (current_minute - start + 1440) / interval;
    if ((c * interval == (current_minute - start + 1440)) && c <= repeat) {
      return 1;
    }
  }
  return 0;
}

// convert absolute remainder (reference time 1970 01-01) to relative remainder (reference time today)
// absolute remainder is stored in nvm, relative remainder is presented to web
void ProgramData::drem_to_relative(byte days[2]) {
  byte rem_abs=days[0];
  byte inv=days[1];
  // todo: use now_tz()?
  days[0] = (byte)((rem_abs + inv - (os.now_tz()/SECS_PER_DAY) % inv) % inv);
}

// relative remainder -> absolute remainder
void ProgramData::drem_to_absolute(byte days[2]) {
  byte rem_rel=days[0];
  byte inv=days[1];
  // todo: use now_tz()?
  days[0] = (byte)(((os.now_tz()/SECS_PER_DAY) + rem_rel) % inv);
}


