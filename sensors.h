/* OpenSprinkler Unified (AVR/RPI/BBB/LINUX) Firmware
 * Copyright (C) 2015 by Ray Wang (ray@opensprinkler.com)
 *
 * sensors header file
 * Sep 2022 @ OpenSprinklerShop
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

#ifndef _SENSORS_H
#define _SENSORS_H

#if defined(ARDUINO)
#include <Arduino.h>
#include <sys/stat.h>
#else  // headers for RPI/BBB
#include <stdio.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/ioctl.h>
extern "C" {
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
}
#endif
#include "defines.h"
#include "utils.h"
#if defined(ESP8266)
#include <ADS1X15.h>
#endif
#include "program.h"

// Files
#define SENSOR_FILENAME "sensor.dat"      // analog sensor filename
#define SENSOR_FILENAME_BAK "sensor.bak"  // analog sensor filename backup
#define PROG_SENSOR_FILENAME "progsensor.dat"  // sensor to program assign filename
#define SENSORLOG_FILENAME1 "sensorlog.dat"   // analog sensor log filename
#define SENSORLOG_FILENAME2 "sensorlog2.dat"  // analog sensor log filename2
#define MONITOR_FILENAME "monitors.dat"

#define SENSORLOG_FILENAME_WEEK1 \
  "sensorlogW1.dat"  // analog sensor log filename for  week average
#define SENSORLOG_FILENAME_WEEK2 \
  "sensorlogW2.dat"  // analog sensor log filename2 for week average
#define SENSORLOG_FILENAME_MONTH1 \
  "sensorlogM1.dat"  // analog sensor log filename for month average
#define SENSORLOG_FILENAME_MONTH2 \
  "sensorlogM2.dat"  // analog sensor log filename2 for month average

#define SENSORURL_FILENAME "sensorurl.dat"  // long urls filename

// MaxLogSize
#define MAX_LOG_SIZE 8000

// Sensor types:
#define SENSOR_NONE                     0   // None or deleted sensor
#define SENSOR_SMT100_MOIS              1   // Truebner SMT100 RS485, moisture mode
#define SENSOR_SMT100_TEMP              2   // Truebner SMT100 RS485, temperature mode
#define SENSOR_SMT100_PMTY              3   // Truebner SMT100 RS485, permittivity mode
#define SENSOR_TH100_MOIS               4   // Truebner TH100 RS485,  humidity mode
#define SENSOR_TH100_TEMP               5   // Truebner TH100 RS485,  temperature mode
#define SENSOR_ANALOG_EXTENSION_BOARD   10  // New OpenSprinkler analog extension board x8 - voltage mode 0..4V
#define SENSOR_ANALOG_EXTENSION_BOARD_P 11  // New OpenSprinkler analog extension board x8 - percent 0..3.3V to 0..100%
#define SENSOR_SMT50_MOIS               15  // New OpenSprinkler analog extension board x8 - SMT50 VWC [%] = (U * 50) : 3
#define SENSOR_SMT50_TEMP               16  // New OpenSprinkler analog extension board x8 - SMT50 T [°C] = (U – 0,5) * 100
#define SENSOR_SMT100_ANALOG_MOIS       17  // New OpenSprinkler analog extension board x8 - SMT100 VWC [%] = (U * 100) : 3
#define SENSOR_SMT100_ANALOG_TEMP       18  // New OpenSprinkler analog extension board x8 - SMT50 T [°C] = (U * 100) : 3 - 40

#define SENSOR_VH400                    30  // New OpenSprinkler analog extension board x8 - Vegetronix VH400
#define SENSOR_THERM200                 31  // New OpenSprinkler analog extension board x8 - Vegetronix THERM200
#define SENSOR_AQUAPLUMB                32  // New OpenSprinkler analog extension board x8 - Vegetronix Aquaplumb

#define SENSOR_USERDEF                  49  // New OpenSprinkler analog extension board x8 - User defined sensor

#define SENSOR_OSPI_ANALOG              50  // Old OSPi analog input - voltage mode 0..3.3V
#define SENSOR_OSPI_ANALOG_P            51  // Old OSPi analog input - percent 0..3.3V to 0...100%
#define SENSOR_OSPI_ANALOG_SMT50_MOIS   52  // Old OSPi analog input - SMT50 VWC [%] = (U * 50) : 3
#define SENSOR_OSPI_ANALOG_SMT50_TEMP   53  // Old OSPi analog input - SMT50 T [°C] = (U – 0,5) * 100
#define SENSOR_OSPI_INTERNAL_TEMP       54  // Internal OSPI Temperature
 
#define SENSOR_MQTT                     90  // subscribe to a MQTT server and query a value

#define SENSOR_REMOTE                   100 // Remote sensor of an remote opensprinkler
#define SENSOR_WEATHER_TEMP_F           101 // Weather service - temperature (Fahrenheit)
#define SENSOR_WEATHER_TEMP_C           102 // Weather service - temperature (Celcius)
#define SENSOR_WEATHER_HUM              103 // Weather service - humidity (%)
#define SENSOR_WEATHER_PRECIP_IN        105 // Weather service - precip (inch)
#define SENSOR_WEATHER_PRECIP_MM        106 // Weather service - precip (mm)
#define SENSOR_WEATHER_WIND_MPH         107 // Weather service - wind (mph)
#define SENSOR_WEATHER_WIND_KMH         108 // Weather service - wind (kmh)

#define SENSOR_GROUP_MIN                1000 // Sensor group with min value
#define SENSOR_GROUP_MAX                1001 // Sensor group with max value
#define SENSOR_GROUP_AVG                1002 // Sensor group with avg value
#define SENSOR_GROUP_SUM                1003 // Sensor group with sum value

//Diagnostic
#define SENSOR_FREE_MEMORY              10000 //Free memory
#define SENSOR_FREE_STORE               10001 //Free storage

#define SENSOR_READ_TIMEOUT 3000  // ms

#define MIN_DISK_FREE 8192  // 8Kb min

#define MAX_SENSOR_REPEAT_READ 32000  // max reads for calculating avg
#define MAX_SENSOR_READ_TIME 1        // second for reading sensors

// detected Analog Sensor Boards:
#define ASB_BOARD1 0x0001
#define ASB_BOARD2 0x0002
#define OSPI_PCF8591 0x0004
#define OSPI_ADS1115 0x0008
#define RS485_TRUEBNER1 0x0020
#define RS485_TRUEBNER2 0x0040
#define RS485_TRUEBNER3 0x0080
#define RS485_TRUEBNER4 0x0100
#define OSPI_USB_RS485 0x0200

typedef struct SensorFlags {
  uint enable : 1;   // enabled
  uint log : 1;      // log data enabled
  uint data_ok : 1;  // last data is ok
  uint show : 1;     // show on mainpage
} SensorFlags_t;

// Definition of a sensor
typedef struct Sensor {
  uint nr;                    // 1..n sensor-nr, 0=deleted
  char name[30];              // name
  uint type;                  // 1..n type definition, 0=deleted
  uint group;                 // group assignment,0=no group
  uint32_t ip;                // tcp-ip
  uint port;                  // tcp-port    / ADC: I2C Address 0x48/0x49 or 0/1
  uint id;                    // modbus id   / ADC: channel
  uint read_interval;         // seconds
  uint32_t last_native_data;  // last native sensor data
  double last_data;           // last converted sensor data
  SensorFlags_t flags;        // Flags see obove
  int16_t factor;             // faktor  - for custom sensor
  int16_t divider;            // divider - for custom sensor
  char userdef_unit[8];       // unit    - for custom sensor
  int16_t offset_mv;          // offset millivolt - for custom sensor (before)
  int16_t offset2;  // offset unit value 1/100 - for custom sensor (after):
                    //   sensorvalue = (read_value-offset_mv/1000) * factor /
                    //   divider + offset2/100
  unsigned char assigned_unitid;  // unitid for userdef and mqtt sensors
  unsigned char undef[15];        // for later
  // unstored:
  bool mqtt_init : 1;
  bool mqtt_push : 1;
  unsigned char unitid;
  uint32_t repeat_read;
  double repeat_data;
  uint64_t repeat_native;
  ulong last_read;  // millis
  double last_logged_data;   // last logged sensor data
  ulong last_logged_time;    // last logged timestamp        /
  Sensor *next;
} Sensor_t;
#define SENSOR_STORE_SIZE 111

// Definition of a log data
typedef struct SensorLog {
  uint nr;  // sensor-nr
  ulong time;
  uint32_t native_data;
  double data;
} SensorLog_t;
#define SENSORLOG_STORE_SIZE (sizeof(SensorLog_t))

// Sensor to program data
// Adjustment is formula
//    min max  factor1 factor2
//    10..90 -> 5..1 factor1 > factor2
//     a   b    c  d
//    (b-sensorData) / (b-a) * (c-d) + d
//
//    10..90 -> 1..5 factor1 < factor2
//     a   b    c  d
//    (sensorData-a) / (b-a) * (d-c) + c

#define PROG_DELETE 0          // deleted
#define PROG_LINEAR 1          // formula see above
#define PROG_DIGITAL_MIN 2     // under or equal min : factor1 else factor2
#define PROG_DIGITAL_MAX 3     // over or equal max  : factor2 else factor1
#define PROG_DIGITAL_MINMAX 4  // under min or over max : factor1 else factor2
#define PROG_NONE 99           // No adjustment

typedef struct ProgSensorAdjust {
  uint nr;      // adjust-nr 1..x
  uint type;    // PROG_XYZ type=0 -->delete
  uint sensor;  // sensor-nr
  uint prog;    // program-nr=pid
  double factor1;
  double factor2;
  double min;
  double max;
  char name[30];
  unsigned char undef[2];  // for later
  ProgSensorAdjust *next;
} ProgSensorAdjust_t;
#define PROGSENSOR_STORE_SIZE \
  (sizeof(ProgSensorAdjust_t) - sizeof(ProgSensorAdjust_t *))

#define SENSORURL_TYPE_URL 0     // URL for Host/Path
#define SENSORURL_TYPE_TOPIC 1   // TOPIC for MQTT
#define SENSORURL_TYPE_FILTER 2  // JSON Filter for MQTT

typedef struct SensorUrl {
  uint nr;
  uint type;  // see SENSORURL_TYPE
  uint length;
  char *urlstr;
  SensorUrl *next;
} SensorUrl_t;
#define SENSORURL_STORE_SIZE \
  (sizeof(SensorUrl_t) - sizeof(char *) - sizeof(SensorUrl_t *))

#define MONITOR_DELETE 0
#define MONITOR_MIN 1
#define MONITOR_MAX 2
#define MONITOR_SENSOR12 3 // Read Digital OS Sensors Rain/Soil Moisture 
#define MONITOR_SET_SENSOR12 4 // Write Digital OS Sensors Rain/Soil Moisture 
#define MONITOR_AND 10
#define MONITOR_OR 11
#define MONITOR_XOR 12
#define MONITOR_NOT 13
#define MONITOR_TIME 14
#define MONITOR_REMOTE 100

typedef struct Monitor_MINMAX { // type = 1+2
  double value1;  // MIN/MAX
  double value2; // Secondary
} Monitor_MINMAX_t;

typedef struct Monitor_SENSOR12 { // type = 3
  uint16_t sensor12;
  bool invers : 1;
} Monitor_SENSOR12_t;

typedef struct Monitor_SET_SENSOR12 { // type = 4
  uint16_t monitor;
  uint16_t sensor12;
} Monitor_SET_SENSOR12_t;

typedef struct Monitor_ANDORXOR { // type = 10+11+12
  uint16_t monitor1;
  uint16_t monitor2;
  uint16_t monitor3;
  uint16_t monitor4;
  bool invers1 : 1;
  bool invers2 : 1;
  bool invers3 : 1;
  bool invers4 : 1;
} Monitor_ANDORXOR_t;

typedef struct Monitor_NOT { // type = 13
  uint16_t monitor;
} Monitor_NOT_t;

typedef struct Monitor_TIME { // type = 14
  uint16_t time_from; //Format: HHMM
  uint16_t time_to;   //Format: HHMM
  uint8_t weekdays;  //bit 0=monday
} Monitor_TIME_t;

typedef struct Monitor_REMOTE { // type = 100
  uint16_t rmonitor;
  uint32_t ip;
  uint16_t port;
} Monitor_REMOTE_t;

typedef union Monitor_Union {
    Monitor_MINMAX_t minmax;     // type = 1+2
    Monitor_SENSOR12_t sensor12; // type = 3
    Monitor_SET_SENSOR12_t set_sensor12; // type = 4
    Monitor_ANDORXOR_t andorxor; // type = 10+11+12
    Monitor_NOT_t mnot; // type = 13
    Monitor_TIME_t mtime; // type = 14
    Monitor_REMOTE_t remote; //type = 100
} Monitor_Union_t;

typedef struct Monitor {
  uint nr;
  uint type;     // MONITOR_TYPES
  uint sensor;   // sensor-nr
  uint prog;     // program-nr=pid
  uint zone;     // Zone
  Monitor_Union_t m;
  boolean active;
  ulong time;
  char name[30];
  ulong maxRuntime;
  uint8_t prio;
  unsigned char undef[20];  // for later
  Monitor *next;
} Monitor_t;
#define MONITOR_STORE_SIZE (sizeof(Monitor_t) - sizeof(char *) - sizeof(Monitor_t *))

#define UNIT_NONE 0
#define UNIT_PERCENT 1
#define UNIT_DEGREE 2
#define UNIT_FAHRENHEIT 3
#define UNIT_VOLT 4
#define UNIT_HUM_PERCENT 5
#define UNIT_INCH 6
#define UNIT_MM 7
#define UNIT_MPH 8
#define UNIT_KMH 9
#define UNIT_LEVEL 10
#define UNIT_DK 11 //Permitivität
#define UNIT_LM 12 //Lumen
#define UNIT_LX 13 //Lux
#define UNIT_USERDEF 99

// Unitnames
//  extern const char* sensor_unitNames[];

#define ASB_BOARD_ADDR1a 0x48
#define ASB_BOARD_ADDR1b 0x49
#define ASB_BOARD_ADDR2a 0x4A
#define ASB_BOARD_ADDR2b 0x4B
#define RS485_TRUEBNER1_ADDR 0x38
#define RS485_TRUEBNER2_ADDR 0x39
#define RS485_TRUEBNER3_ADDR 0x3A
#define RS485_TRUEBNER4_ADDR 0x3B

void sensor_api_init(boolean detect_boards);
uint16_t get_asb_detected_boards();
void sensor_save_all();
void sensor_api_free();

Sensor_t *getSensors();
const char *getSensorUnit(int unitid);
const char *getSensorUnit(Sensor_t *sensor);
unsigned char getSensorUnitId(int type);
unsigned char getSensorUnitId(Sensor_t *sensor);

extern char ether_buffer[];
extern char tmp_buffer[];
extern ProgramData pd;

// Utils:
uint16_t CRC16(unsigned char buf[], int len);

// Sensor API functions:
int sensor_delete(uint nr);
int sensor_define(uint nr, const char *name, uint type, uint group, uint32_t ip,
                  uint port, uint id, uint ri, int16_t factor, int16_t divider,
                  const char *userdef_unit, int16_t offset_mv, int16_t offset2,
                  SensorFlags_t flags, int16_t assigned_unitid);
int sensor_define_userdef(uint nr, int16_t factor, int16_t divider,
                          const char *userdef_unit, int16_t offset_mv,
                          int16_t offset2, int16_t sensor_define_userdef);
void sensor_load();
void sensor_save();
uint sensor_count();
boolean sensor_isgroup(Sensor_t *sensor);
void sensor_update_groups();

void read_all_sensors(boolean online);

Sensor_t *sensor_by_nr(uint nr);
Sensor_t *sensor_by_idx(uint idx);

int read_sensor(Sensor_t *sensor,
                ulong time);  // sensor value goes to last_native_data/last_data

// Sensorlog API functions:
#define LOG_STD   0
#define LOG_WEEK  1
#define LOG_MONTH 2
bool sensorlog_add(uint8_t log, SensorLog_t *sensorlog);
bool sensorlog_add(uint8_t log, Sensor_t *sensor, ulong time);
void sensorlog_clear_all();
void sensorlog_clear(bool std, bool week, bool month);
ulong sensorlog_clear_sensor(uint sensorNr, uint8_t log, bool use_under,
                             double under, bool use_over, double over, time_t before, time_t after);
SensorLog_t *sensorlog_load(uint8_t log, ulong pos);
SensorLog_t *sensorlog_load(uint8_t log, ulong idx, SensorLog_t *sensorlog);
int sensorlog_load2(uint8_t log, ulong idx, int count, SensorLog_t *sensorlog);
ulong sensorlog_filesize(uint8_t log);
ulong sensorlog_size(uint8_t log);
ulong findLogPosition(uint8_t log, ulong after);
const char *getlogfile(uint8_t log);
const char *getlogfile2(uint8_t log);
void checkLogSwitch(uint8_t log);
void checkLogSwitchAfterWrite(uint8_t log);

//influxdb
void add_influx_data(Sensor_t *sensor);

// Set Sensor Address for SMT100:
int set_sensor_address(Sensor_t *sensor, uint8_t new_address);

// Calc watering adjustment:
int prog_adjust_define(uint nr, uint type, uint sensor, uint prog,
                       double factor1, double factor2, double min, double max, char * name);
int prog_adjust_delete(uint nr);
void prog_adjust_save();
void prog_adjust_load();
uint prog_adjust_count();
ProgSensorAdjust_t *prog_adjust_by_nr(uint nr);
ProgSensorAdjust_t *prog_adjust_by_idx(uint idx);
double calc_sensor_watering(uint prog);
double calc_sensor_watering_by_nr(uint nr);
double calc_sensor_watering_int(ProgSensorAdjust_t *p, double sensorData);

void GetSensorWeather();
// PUSH Message to MQTT and others:
void push_message(Sensor_t *sensor);

// Web URLS Host/Path and MQTT topics:
void SensorUrl_load();
void SensorUrl_save();
bool SensorUrl_delete(uint nr, uint type);
bool SensorUrl_add(uint nr, uint type, const char *urlstr);
char *SensorUrl_get(uint nr, uint type);

void detect_asb_board();

//Value Monitoring
void monitor_load();
void monitor_save();
int monitor_count();
int monitor_delete(uint nr);
bool monitor_define(uint nr, uint type, uint sensor, uint prog, uint zone, const Monitor_Union_t m, char * name, ulong maxRuntime, uint8_t prio);
Monitor_t * monitor_by_nr(uint nr);
Monitor_t * monitor_by_idx(uint idx);
void check_monitors();

#if defined(ESP8266)
ulong diskFree();
bool checkDiskFree();  // true: disk space Ok, false: Out of disk space
#endif

#endif  // _SENSORS_H
