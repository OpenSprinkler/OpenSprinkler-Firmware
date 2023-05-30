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
#else // headers for RPI/BBB
	#include <stdio.h>
	#include <limits.h>
	#include <sys/time.h>

#endif
#include "defines.h"
#include "utils.h"
#if defined(ESP8266)
#include <ADS1X15.h>
#endif

//Files
#define SENSOR_FILENAME       "sensor.dat"  // analog sensor filename
#define PROG_SENSOR_FILENAME  "progsensor.dat"  // sensor to program assign filename
#define SENSORLOG_FILENAME1   "sensorlog.dat"   // analog sensor log filename
#define SENSORLOG_FILENAME2   "sensorlog2.dat"  // analog sensor log filename2

#define SENSORLOG_FILENAME_WEEK1 "sensorlogW1.dat"    // analog sensor log filename for  week average
#define SENSORLOG_FILENAME_WEEK2 "sensorlogW2.dat"    // analog sensor log filename2 for week average
#define SENSORLOG_FILENAME_MONTH1 "sensorlogM1.dat"   // analog sensor log filename for month average
#define SENSORLOG_FILENAME_MONTH2 "sensorlogM2.dat"   // analog sensor log filename2 for month average

//MaxLogSize
#define MAX_LOG_SIZE 8000
#define MAX_LOG_SIZE_WEEK 2000
#define MAX_LOG_SIZE_MONTH 1000

//Sensor types:
#define SENSOR_NONE                       0   //None or deleted sensor
#define SENSOR_SMT100_MODBUS_RTU_MOIS     1   //Truebner SMT100 RS485 Modbus RTU over TCP, moisture mode
#define SENSOR_SMT100_MODBUS_RTU_TEMP     2   //Truebner SMT100 RS485 Modbus RTU over TCP, temperature mode
#if defined(ESP8266)
#define SENSOR_ANALOG_EXTENSION_BOARD     10  //New OpenSprinkler analog extension board x8 - voltage mode 0..4V
#define SENSOR_ANALOG_EXTENSION_BOARD_P   11  //New OpenSprinkler analog extension board x8 - percent 0..3.3V to 0..100%
#define SENSOR_SMT50_MOIS                 15  //New OpenSprinkler analog extension board x8 - SMT50 VWC [%] = (U * 50) : 3
#define SENSOR_SMT50_TEMP                 16  //New OpenSprinkler analog extension board x8 - SMT50 T [°C] = (U – 0,5) * 100
#define SENSOR_SMT100_ANALOG_MOIS         17  //New OpenSprinkler analog extension board x8 - SMT100 VWC [%] = (U * 100) : 3
#define SENSOR_SMT100_ANALOG_TEMP         18  //New OpenSprinkler analog extension board x8 - SMT50 T [°C] = (U * 100) : 3 - 40

#define SENSOR_VH400                      30  //New OpenSprinkler analog extension board x8 - Vegetronix VH400
#define SENSOR_THERM200                   31  //New OpenSprinkler analog extension board x8 - Vegetronix THERM200
#define SENSOR_AQUAPLUMB                  32  //New OpenSprinkler analog extension board x8 - Vegetronix Aquaplumb

#endif
#define SENSOR_OSPI_ANALOG_INPUTS         50  //Old OSPi analog input
#define SENSOR_REMOTE                     100 //Remote sensor of an remote opensprinkler
#define SENSOR_WEATHER_TEMP_F             101 //Weather service - temperature (Fahrenheit)
#define SENSOR_WEATHER_TEMP_C             102 //Weather service - temperature (Celcius)
#define SENSOR_WEATHER_HUM                103 //Weather service - humidity (%)
#define SENSOR_WEATHER_PRECIP_IN          105 //Weather service - precip (inch)
#define SENSOR_WEATHER_PRECIP_MM          106 //Weather service - precip (mm)
#define SENSOR_WEATHER_WIND_MPH           107 //Weather service - wind (mph)
#define SENSOR_WEATHER_WIND_KMH           108 //Weather service - wind (kmh)

#define SENSOR_GROUP_MIN               1000   //Sensor group with min value
#define SENSOR_GROUP_MAX               1001   //Sensor group with max value
#define SENSOR_GROUP_AVG               1002   //Sensor group with avg value
#define SENSOR_GROUP_SUM               1003   //Sensor group with sum value

#define SENSOR_READ_TIMEOUT            3000   //ms

#define MIN_DISK_FREE                  8192   //8Kb min 

typedef struct SensorFlags {
	uint     enable:1;
	uint     log:1;
	uint     data_ok:1;
	uint     show:1;
} SensorFlags_t;

//Definition of a sensor
typedef struct Sensor {
	uint     nr;               // 1..n sensor-nr, 0=deleted
	char     name[30];         // name
	uint     type;             // 1..n type definition, 0=deleted
	uint     group;            // group assignment,0=no group
	uint32_t ip;               // tcp-ip
	uint     port;             // tcp-port    / ADC: I2C Address 0x48/0x49 or 0/1
	uint     id;               // modbus id   / ADC: channel
	uint     read_interval;    // seconds
	uint32_t last_native_data; // last native sensor data
	double   last_data;        // last converted sensor data
	SensorFlags_t flags;       // Flags see obove
	byte     undef[32];        //for later
	//unstored
	byte     unitid;
	ulong    last_read; //millis
	Sensor   *next; 
} Sensor_t;
#define SENSOR_STORE_SIZE (sizeof(Sensor_t)-sizeof(Sensor_t*)-sizeof(ulong)-sizeof(byte))

//Definition of a log data
typedef struct SensorLog {
	uint     nr; //sensor-nr
	ulong    time; 
	uint32_t native_data;
	double   data;
} SensorLog_t;
#define SENSORLOG_STORE_SIZE (sizeof(SensorLog_t))

//Sensor to program data
//Adjustment is formula 
//   min max  factor1 factor2
//   10..90 -> 5..1 factor1 > factor2
//    a   b    c  d
//   (b-sensorData) / (b-a) * (c-d) + d
//
//   10..90 -> 1..5 factor1 < factor2
//    a   b    c  d
//   (sensorData-a) / (b-a) * (d-c) + c

#define PROG_NONE        0 //No adjustment (delete)
#define PROG_LINEAR      1 //formula see above
#define PROG_DIGITAL_MIN 2 //1=under or equal min, 0=above
#define PROG_DIGITAL_MAX 3 //1=over or equal max, 0=below

typedef struct ProgSensorAdjust {
	uint   nr;       //adjust-nr 1..x
	uint   type;     //PROG_XYZ type=0 -->delete
	uint   sensor;   //sensor-nr
	uint   prog;     //program-nr=pid
	double factor1;
	double factor2;
	double min;
	double max;
	byte   undef[32]; //for later
	ProgSensorAdjust *next;
} ProgSensorAdjust_t;
#define PROGSENSOR_STORE_SIZE (sizeof(ProgSensorAdjust_t)-sizeof(ProgSensorAdjust_t*))

#define UNIT_NONE        0
#define UNIT_PERCENT     1
#define UNIT_DEGREE      2
#define UNIT_FAHRENHEIT  3
#define UNIT_VOLT        4
#define UNIT_HUM_PERCENT 5
#define UNIT_INCH        6
#define UNIT_MM          7
#define UNIT_MPH         8
#define UNIT_KMH         9
#define UNIT_LEVEL      10

//Unitnames
extern const char* sensor_unitNames[];

const char* getSensorUnit(Sensor_t *sensor);
byte getSensorUnitId(int type);
byte getSensorUnitId(Sensor_t *sensor);

extern char ether_buffer[];
extern char tmp_buffer[];

//Utils:
uint16_t CRC16 (byte buf[], int len);

//Sensor API functions:
int sensor_delete(uint nr);
int sensor_define(uint nr, char *name, uint type, uint group, uint32_t ip, uint port, uint id, uint ri, SensorFlags_t flags);
void sensor_load();
void sensor_save();
uint sensor_count();
boolean sensor_isgroup(Sensor_t *sensor);
void sensor_update_groups();

void read_all_sensors();

Sensor_t *sensor_by_nr(uint nr);
Sensor_t *sensor_by_idx(uint idx);

int read_sensor(Sensor_t *sensor); //sensor value goes to last_native_data/last_data

//Sensorlog API functions:
#define LOG_STD   0
#define LOG_WEEK  1
#define LOG_MONTH 2
bool sensorlog_add(uint8_t log, SensorLog_t *sensorlog);
bool sensorlog_add(uint8_t log, Sensor_t *sensor, ulong time);
void sensorlog_clear_all();
void sensorlog_clear(bool std, bool week, bool month);
SensorLog_t *sensorlog_load(uint8_t log, ulong pos);
SensorLog_t *sensorlog_load(uint8_t log, ulong idx, SensorLog_t* sensorlog);
int sensorlog_load2(uint8_t log, ulong idx, int count, SensorLog_t* sensorlog);
ulong sensorlog_filesize(uint8_t log);
ulong sensorlog_size(uint8_t log);
ulong findLogPosition(uint8_t log, ulong after);

//Set Sensor Address for SMT100:
int set_sensor_address(Sensor_t *sensor, byte new_address);

//Calc watering adjustment:
int prog_adjust_define(uint nr, uint type, uint sensor, uint prog, double factor1, double factor2, double min, double max);
int prog_adjust_delete(uint nr);
void prog_adjust_save();
void prog_adjust_load();
uint prog_adjust_count();
ProgSensorAdjust_t *prog_adjust_by_nr(uint nr);
ProgSensorAdjust_t *prog_adjust_by_idx(uint idx);
double calc_sensor_watering(uint prog);
double calc_sensor_watering_by_nr(uint nr);

void GetSensorWeather();

#if defined(ESP8266)
ulong diskFree();
bool checkDiskFree(); //true: disk space Ok, false: Out of disk space
#endif

#endif // _SENSORS_H
