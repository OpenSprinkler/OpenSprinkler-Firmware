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
#else // headers for RPI/BBB
	#include <stdio.h>
	#include <limits.h>
	#include <sys/time.h>

#endif
#include "defines.h"
#include "utils.h"
#include <sys/stat.h>

//Sensor types:
#define SENSOR_NONE                       0   //None or deleted sensor
#define SENSOR_SMT100_MODBUS_RTU_MOIS     1   //Truebner SMT100 RS485 Modbus RTU over TCP, moisture mode
#define SENSOR_SMT100_MODBUS_RTU_TEMP     2   //Truebner SMT100 RS485 Modbus RTU over TCP, temperature mode

#define SENSOR_GROUP_MIN               1000   //Sensor group with min value
#define SENSOR_GROUP_MAX               1001   //Sensor group with max value
#define SENSOR_GROUP_AVG               1002   //Sensor group with avg value
#define SENSOR_GROUP_SUM               1003   //Sensor group with sum value

#define SENSOR_READ_TIMEOUT            3000   //ms

//Definition of a sensor
typedef struct Sensor {
	uint     nr;   //1..n sensor-nr, 0=deleted
	char     name[30];
	uint     type; //1..n type definition, 0=deleted
	uint     group;            // group assignment,0=no group
	uint32_t ip;
	uint     port;
	uint     id; //modbus id
	uint     read_interval; // seconds
	uint32_t last_native_data; // last native sensor data
	double   last_data;        // last converted sensor data
	byte     enable:1;
	byte     log:1;
	byte     data_ok:1;
	byte     undef[32]; //for later
	//unstored
	ulong    last_read; //millis
	Sensor   *next; 
} Sensor_t;
#define SENSOR_STORE_SIZE (sizeof(Sensor_t)-sizeof(Sensor_t*)-sizeof(ulong))

//Definition of a log data
typedef struct SensorLog {
	uint     nr; //sensor-nr
	ulong    time; 
	uint32_t native_data;
	double   data;
} SensorLog_t;
#define SENSORLOG_STORE_SIZE (sizeof(SensorLog_t))

//Sensor to program data
typedef struct ProgSensor {
	uint   sensor;
	double offset;
	double factor;
	double divider;
	byte   undef[32]; //for later
} ProgSensor_t;


//All sensors:
static Sensor_t *sensors = NULL;

//Program sensor data 
static ProgSensor_t *progSensor = NULL;

//Utils:
uint16_t CRC16 (byte buf[], int len);

//Sensor API functions:
int sensor_delete(uint nr);
int sensor_define(uint nr, char *name, uint type, uint group, uint32_t ip, uint port, uint id, uint ri, byte enabled, byte log);
void sensor_load();
void sensor_save();
uint sensor_count();
void sensor_update_groups();

void read_all_sensors();

Sensor_t *sensor_by_nr(uint nr);
Sensor_t *sensor_by_idx(uint idx);

int read_sensor(Sensor_t *sensor); //sensor value goes to last_native_data/last_data

//Sensorlog API functions:
void sensorlog_add(SensorLog_t *sensorlog);
void sensorlog_add(Sensor_t *sensor, ulong time);
void sensorlog_clear_all();
SensorLog_t *sensorlog_load(ulong pos);
SensorLog_t *sensorlog_load(ulong idx, SensorLog_t* sensorlog);
ulong sensorlog_size();

//Set Sensor Address
int set_sensor_address(Sensor_t *sensor, byte new_address);

#endif // _SENSORS_H
