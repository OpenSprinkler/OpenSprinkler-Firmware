// Example code for OpenSprinkler Generation 2

/* Program Data Structures and Functions
   Creative Commons Attribution-ShareAlike 3.0 license
   Sep 2014 @ Rayshobby.net
*/

#include <limits.h>
#include "program.h"

// Declare static data members
byte ProgramData::nprograms = 0;
unsigned long ProgramData::scheduled_start_time[(MAX_EXT_BOARDS+1)*8];
unsigned long ProgramData::scheduled_stop_time[(MAX_EXT_BOARDS+1)*8];
byte ProgramData::scheduled_program_index[(MAX_EXT_BOARDS+1)*8];
LogStruct ProgramData::lastrun;
unsigned long ProgramData::last_seq_stop_time;

void ProgramData::init() {
	reset_runtime();
  load_count();
}

void ProgramData::reset_runtime() {
  for (byte i=0; i<MAX_NUM_STATIONS; i++) {
    scheduled_start_time[i] = 0;
    scheduled_stop_time[i] = 0;
    scheduled_program_index[i] = 0;
  }
  last_seq_stop_time = 0;
}

// load program count from EEPROM
void ProgramData::load_count() {
  nprograms = eeprom_read_byte((unsigned char *) ADDR_PROGRAMCOUNTER);
}

// save program count to EEPROM
void ProgramData::save_count() {
  eeprom_write_byte((unsigned char *) ADDR_PROGRAMCOUNTER, nprograms);
}

// erase all program data
void ProgramData::eraseall() {
  nprograms = 0;
  save_count();
}

// read a program
void ProgramData::read(byte pid, ProgramStruct *buf) {
  if (pid >= nprograms) return;
  if (0) {
    // todo: handle SD card
  } else {
    unsigned int addr = ADDR_PROGRAMDATA + (unsigned int)pid * PROGRAMSTRUCT_SIZE;
    eeprom_read_block((void*)buf, (const void *)addr, PROGRAMSTRUCT_SIZE);  
  }
}

// add a program
byte ProgramData::add(ProgramStruct *buf) {
  if (0) {
    // todo: handle SD card
  } else {
    if (nprograms >= MAX_NUMBER_PROGRAMS)  return 0;
    unsigned int addr = ADDR_PROGRAMDATA + (unsigned int)nprograms * PROGRAMSTRUCT_SIZE;
    eeprom_write_block((const void*)buf, (void *)addr, PROGRAMSTRUCT_SIZE);
    nprograms ++;
    save_count();
  }
  return 1;
}

// move a program up (i.e. swap a program with the one above it)
void ProgramData::moveup(byte pid) {
  if(pid >= nprograms || pid == 0) return;

  if(0) {
    // todo: handle SD card
  } else {
    // swap program pid-1 and pid
    unsigned int src = ADDR_PROGRAMDATA + (unsigned int)(pid-1) * PROGRAMSTRUCT_SIZE;
    unsigned int dst = src + PROGRAMSTRUCT_SIZE;
    byte tmp;
    for(int i=0;i<PROGRAMSTRUCT_SIZE;i++,src++,dst++) {
      tmp = eeprom_read_byte((byte *)src);
      eeprom_write_byte((byte *)src, eeprom_read_byte((byte *)dst));
      eeprom_write_byte((byte *)dst, tmp);
    }
  }
}

// modify a program
byte ProgramData::modify(byte pid, ProgramStruct *buf) {
  if (pid >= nprograms)  return 0;
  if (0) {
    // handle SD card
  } else {
    unsigned int addr = ADDR_PROGRAMDATA + (unsigned int)pid * PROGRAMSTRUCT_SIZE;
    eeprom_write_block((const void*)buf, (void *)addr, PROGRAMSTRUCT_SIZE);
  }
  return 1;
}

// delete program(s)
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
      eeprom_read_block((void*)&copy, (const void *)addr, PROGRAMSTRUCT_SIZE);  
      eeprom_write_block((const void*)&copy, (void *)(addr-PROGRAMSTRUCT_SIZE), PROGRAMSTRUCT_SIZE);
    }
    nprograms --;
    save_count();
  }
  return 1;
}

// Check if a given time matches program schedule
byte ProgramStruct::check_match(time_t t) {

  unsigned int current_minute = (unsigned int)hour(t)*60+(unsigned int)minute(t);
  
  // check program enable status
  if (!enabled) return 0;
 
  byte wd = ((byte)weekday(t)+5)%7;
  byte dt = day(t);
  byte i;
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
    else if (dt==29 && month(t)==2)  return 0;
    else if ((dt%2)!=1)  return 0;
  }
  
  // check start time match
  if (!starttime_type) {
    // repeating type
    int16_t start = starttimes[0];
    int16_t repeat = starttimes[1];
    int16_t interval = starttimes[2];
    // if current time is prior to start time, return false
    if (current_minute < start)
      return 0;
      
    // if this is a single run program
    if (!repeat) {
      return (current_minute == start) ? 1 : 0;
    }
    
    // if this is a multiple-run program
    // first ensure the interval is non-zero
    if (!interval) {
      return 0;
    }

    // check if we are on any interval match
    int16_t c = (current_minute - start) / interval;
    if ((c * interval == (current_minute - start)) && c <= repeat) {
      return 1;
    }
    
    // check previous day in case the repeating start times went over night
    // this needs to be fixed because we have to check if yesterday
    // is a valid starting day
    /*c = (current_minute - start + 1440) / interval;
    if ((c * interval == (current_minute - start + 1440)) && c <= repeat) {
      return 1;
    }*/
    
  } else {
    // given start time type
    for(i=0;i<MAX_NUM_STARTTIMES;i++) {
      if (current_minute == starttimes[i])  return 1;
    }
  }
  return 0;
}

// convert absolute remainder (reference time 1970 01-01) to relative remainder (reference time today)
// absolute remainder is stored in eeprom, relative remainder is presented to web
void ProgramData::drem_to_relative(byte days[2]) {
  byte rem_abs=days[0];
  byte inv=days[1];
  // todo: use now_tz()?
  days[0] = (byte)((rem_abs + inv - (now()/SECS_PER_DAY) % inv) % inv);
}

// relative remainder -> absolute remainder
void ProgramData::drem_to_absolute(byte days[2]) {
  byte rem_rel=days[0];
  byte inv=days[1];
  // todo: use now_tz()?
  days[0] = (byte)(((now()/SECS_PER_DAY) + rem_rel) % inv);
}
