// Example code for OpenSprinkler Generation 2

/* Program Data Structures and Functions
   Creative Commons Attribution-ShareAlike 3.0 license
   Sep 2014 @ Rayshobby.net
*/

#ifndef PROGRAM_STRUCT_H
#define PROGRAM_STRUCT_H

#define MAX_NUM_STARTTIMES  4
#define PROGRAM_NAME_SIZE   12
#include <OpenSprinklerGen2.h>

// Log data structure
struct LogStruct {
  byte station;
  byte program;
  unsigned int duration;
  unsigned long endtime;
};

#define PROGRAM_TYPE_WEEKLY   0
#define PROGRAM_TYPE_BIWEEKLY 1
#define PROGRAM_TYPE_MONTHLY  2
#define PROGRAM_TYPE_INTERVAL 3
// Program data structure
class ProgramStruct {
public:
  byte enabled  :1;  // HIGH means the program is enabled
  
  // weather data
  byte use_weather: 1;
  
  // odd/even restriction:
  // 0->none, 1->odd day (except 31st and Feb 29th)
  // 2->even day, 3->N/A
  byte oddeven   :2;
  
  // schedule type:
  // 0: weekly, 1->biweekly, 2->monthly, 3->interval
  byte type      :2;  
  
  // starttime type:
  // 0: repeating (give start time, repeat every, number of repeats)
  // 1: fixed start time (give arbitrary start times up to MAX_NUM_STARTTIMEs)
  byte starttime_type: 1;

  // misc. data
  byte dummy1: 1;
  
  // weekly:   days[0][0..6] correspond to Monday till Sunday
  // bi-weekly:days[0][0..6] and [1][0..6] store two weeks
  // monthly:  days[0][0..5] stores the day of the month (32 means last day of month)
  // interval: days[0] stores the interval (0 to 255), days[1] stores the starting day remainder (0 to 254)
  byte days[2];  
  
  // When the program is a fixed start time type:
  //   up to MAX_NUM_STARTTIMES fixed start times
  // When the program is a repeating type:
  //   starttimes[0]: start time
  //   starttimes[1]: repeat count
  //   starttimes[2]: repeat every
  // Start time structure:
  //   if bit 15 = 1: negative, undefined
  //   if bit 14 = 1: sunrise time +/- offset (by lowest 12 bits)
  //   if bit 13 = 1: sunset  time +/- offset (by lowest 12 bits)
  //      bit 12: unused, undefined
  // else: standard start time (value between 0 to 1440, by lowest 12 bits)
  int16_t starttimes[MAX_NUM_STARTTIMES];

  uint8_t durations[MAX_NUM_STATIONS];  // duration / water time of each station
  
  char name[PROGRAM_NAME_SIZE];

  byte check_match(time_t t);
};

// program structure size
#define PROGRAMSTRUCT_SIZE         (sizeof(ProgramStruct))
#define ADDR_PROGRAMTYPEVERSION     ADDR_EEPROM_PROGRAMS
#define ADDR_PROGRAMCOUNTER        (ADDR_EEPROM_PROGRAMS+1)
#define ADDR_PROGRAMDATA           (ADDR_EEPROM_PROGRAMS+2)
// maximum number of programs, restricted by internal EEPROM size
#define MAX_NUMBER_PROGRAMS  ((MAX_PROGRAMDATA-2)/PROGRAMSTRUCT_SIZE)

extern OpenSprinkler os;

#define PROGRAM_TYPE_VERSION  11

class ProgramData {
public:  
  static unsigned long scheduled_start_time[];// scheduled start time for each station
  static unsigned long scheduled_stop_time[]; // scheduled stop time for each station
  static byte scheduled_program_index[]; // scheduled program index
  static byte  nprograms;     // number of programs
  static LogStruct lastrun;
  static unsigned long last_seq_stop_time;  // the last stop time of a sequential station
  
  static void init();
  static void reset_runtime();
  static void eraseall();
  static void read(byte pid, ProgramStruct *buf);
  static byte add(ProgramStruct *buf);
  static byte modify(byte pid, ProgramStruct *buf);
  static void moveup(byte pid);  
  static byte del(byte pid);
  static void drem_to_relative(byte days[2]); // absolute to relative reminder conversion
  static void drem_to_absolute(byte days[2]);
private:  
  static void load_count();
  static void save_count();
};

#endif
