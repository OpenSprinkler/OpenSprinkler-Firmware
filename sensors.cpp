/* OpenSprinkler Unified (AVR/RPI/BBB/LINUX) Firmware
 * Copyright (C) 2015 by Ray Wang (ray@opensprinkler.com)
 *
 * Utility functions
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

#include <stdlib.h>
#include "defines.h"
#include "utils.h"
#include "program.h"
#include "OpenSprinkler.h"
#include "opensprinkler_server.h"
#include "sensors.h"
#include "weather.h"
#include "mqtt.h"

byte findKeyVal (const char *str,char *strbuf, uint16_t maxlen,const char *key,bool key_in_pgm=false,uint8_t *keyfound=NULL);

//All sensors:
static Sensor_t *sensors = NULL;

//Program sensor data 
static ProgSensorAdjust_t *progSensorAdjusts = NULL;

const char*   sensor_unitNames[] {
	"", "%", "°C", "°F", "V", "%", "in", "mm", "mph", "kmh", "%"
//   0   1     2     3    4    5     6      7      8      9   10
//   0=Nothing
//   1=Soil moisture
//   2=degree celsius temperature
//   3=degree fahrenheit temperature
//   4=Volt V
//   5=Humidity %
//   6=Rain inch
//   7=Rain mm
//   8=Wind mph
//   9=Wind kmh
//  10=Level %
};
byte logFileSwitch[3] = {0,0,0}; //0=use smaller File, 1=LOG1, 2=LOG2

//Weather
time_t last_weather_time = 0;
bool current_weather_ok = false;
double current_temp = 0.0;
double current_humidity = 0.0;
double current_precip = 0.0;
double current_wind = 0.0;

uint16_t CRC16 (byte buf[], int len) {
	uint16_t crc = 0xFFFF;

	for (int pos = 0; pos < len; pos++) {
	    crc ^= (uint16_t)buf[pos];          // XOR byte into least sig. byte of crc
		for (int i = 8; i != 0; i--) {    // Loop over each bit
      		if ((crc & 0x0001) != 0) {      // If the LSB is set
        		crc >>= 1;                    // Shift right and XOR 0xA001
        		crc ^= 0xA001;
      		}
      		else                            // Else LSB is not set
        		crc >>= 1;                    // Just shift right
    	}
  	}
  	// Note, this number has low and high bytes swapped, so use it accordingly (or swap bytes)
  	return crc;  
} // End: CRC16

/**
 * @brief delete a sensor
 * 
 * @param nr 
 */
int sensor_delete(uint nr) {

	Sensor_t *sensor = sensors;
	Sensor_t *last = NULL;
	while (sensor) {
		if (sensor->nr == nr) {
			if (last == NULL)
				sensors = sensor->next;
			else
				last->next = sensor->next;
			delete sensor;
			sensor_save();
			return HTTP_RQT_SUCCESS;
		}
		last = sensor;
		sensor = sensor->next;
	}
	return HTTP_RQT_NOT_RECEIVED;
}

/**
 * @brief define or insert a sensor
 * 
 * @param nr  > 0
 * @param type > 0 add/modify, 0=delete
 * @param group group assignment
 * @param ip 
 * @param port 
 * @param id 
 */
int sensor_define(uint nr, char *name, uint type, uint group, uint32_t ip, uint port, uint id, uint ri, SensorFlags_t flags) {

	if (nr == 0 || type == 0)
		return HTTP_RQT_NOT_RECEIVED;
	if (ri < 10)
		ri = 10;

	Sensor_t *sensor = sensors;
	Sensor_t *last = NULL;
	while (sensor) {
		if (sensor->nr == nr) { //Modify existing
			sensor->type = type;
			sensor->group = group;
			strncpy(sensor->name, name, sizeof(sensor->name)-1);
			sensor->ip = ip;
			sensor->port = port;
			sensor->id = id;
			sensor->read_interval = ri;
			sensor->flags = flags;
			sensor_save();
			return HTTP_RQT_SUCCESS;
		}

		if (sensor->nr > nr)
			break;

		last = sensor;
		sensor = sensor->next;
	}
	//Insert new
	Sensor_t *new_sensor = new Sensor_t;
	memset(new_sensor, 0, sizeof(Sensor_t));
	new_sensor->nr = nr;
	new_sensor->type = type;
	new_sensor->group = group;
	strncpy(new_sensor->name, name, sizeof(new_sensor->name)-1);
	new_sensor->ip = ip;
	new_sensor->port = port;
	new_sensor->id = id;
	new_sensor->read_interval = ri;
	new_sensor->flags = flags;
	if (last) {
		new_sensor->next = last->next;
		last->next = new_sensor;
	} else {
		new_sensor->next = sensors;
		sensors = new_sensor;
	}
	sensor_save();
	return HTTP_RQT_SUCCESS;
}

/**
 * @brief initial load stored sensor definitions
 * 
 */
void sensor_load() {
	//DEBUG_PRINTLN(F("sensor_load"));
	sensors = NULL;
	if (!file_exists(SENSOR_FILENAME))
		return;

	ulong pos = 0;
	Sensor_t *last = NULL;
	while (true) {
		Sensor_t *sensor = new Sensor_t;
		memset(sensor, 0, sizeof(Sensor_t));
		file_read_block (SENSOR_FILENAME, sensor, pos, SENSOR_STORE_SIZE);
		if (!sensor->nr || !sensor->type) {
			delete sensor;
			break;
		}
		if (!last) sensors = sensor;
		else last->next = sensor;
		last = sensor;
		sensor->next = NULL;
		pos += SENSOR_STORE_SIZE;
	}
}

/**
 * @brief Store sensor data
 * 
 */
void sensor_save() {
	//DEBUG_PRINTLN(F("sensor_save"));
	if (file_exists(SENSOR_FILENAME))
		remove_file(SENSOR_FILENAME);
	
	ulong pos = 0;
	Sensor_t *sensor = sensors;
	while (sensor) {
		file_write_block(SENSOR_FILENAME, sensor, pos, SENSOR_STORE_SIZE);
		sensor = sensor->next;
		pos += SENSOR_STORE_SIZE; 
	}
}

uint sensor_count() {
	//DEBUG_PRINTLN(F("sensor_count"));
	Sensor_t *sensor = sensors;
	uint count = 0;
	while (sensor) {
		count++;
		sensor = sensor->next;
	}
	return count;
}

Sensor_t *sensor_by_nr(uint nr) {
	//DEBUG_PRINTLN(F("sensor_by_nr"));
	Sensor_t *sensor = sensors;
	while (sensor) {
		if (sensor->nr == nr) 
			return sensor;
		sensor = sensor->next;
	}
	return NULL;
}

Sensor_t *sensor_by_idx(uint idx) {
	//DEBUG_PRINTLN(F("sensor_by_idx"));
	Sensor_t *sensor = sensors;
	uint count = 0;
	while (sensor) {
		if (count == idx)
			return sensor;
		count++;
		sensor = sensor->next;
	}
	return NULL;
}

// LOGGING METHODS:

/**
 * @brief getlogfile name
 * 
 * @param log 
 * @return const char* 
 */
const char *getlogfile(uint8_t log) {
	bool sw = logFileSwitch[log];
	switch (log) {
		case 0: return sw?SENSORLOG_FILENAME1:SENSORLOG_FILENAME2;
		case 1: return sw?SENSORLOG_FILENAME_WEEK1:SENSORLOG_FILENAME_WEEK2;
		case 2: return sw?SENSORLOG_FILENAME_MONTH1:SENSORLOG_FILENAME_MONTH2;
	}
	return "";
}

/**
 * @brief getlogfile name2 (opposite file)
 * 
 * @param log 
 * @return const char* 
 */
const char *getlogfile2(uint8_t log) {
	bool sw = logFileSwitch[log];
	switch (log) {
		case 0: return sw?SENSORLOG_FILENAME2:SENSORLOG_FILENAME1;
		case 1: return sw?SENSORLOG_FILENAME_WEEK2:SENSORLOG_FILENAME_WEEK1;
		case 2: return sw?SENSORLOG_FILENAME_MONTH2:SENSORLOG_FILENAME_MONTH1;
	}
	return "";
}


void checkLogSwitch(uint8_t log) {
	if (logFileSwitch[log] == 0) { // Check file size, use smallest
		ulong logFileSize1 = file_size(getlogfile(log));
		ulong logFileSize2 = file_size(getlogfile2(log));
		if (logFileSize1 < logFileSize2)
			logFileSwitch[log] = 1; 
		else
			logFileSwitch[log] = 2;
	}
}

void checkLogSwitchAfterWrite(uint8_t log) {
	ulong size = file_size(getlogfile(log));
	if ((size / SENSORLOG_STORE_SIZE) >= MAX_LOG_SIZE) { // switch logs if max reached
		if (logFileSwitch[log] == 1)
			logFileSwitch[log] = 2;
		else
			logFileSwitch[log] = 1;
		remove_file(getlogfile(log));
	}
}

bool sensorlog_add(uint8_t log, SensorLog_t *sensorlog) {
#if defined(ESP8266)
	if (checkDiskFree()) {
#endif
		DEBUG_PRINT(F("sensorlog_add "));
		DEBUG_PRINT(log);
		checkLogSwitch(log);
		file_append_block(getlogfile(log), sensorlog, SENSORLOG_STORE_SIZE);
		checkLogSwitchAfterWrite(log);
		DEBUG_PRINT(F("="));
		DEBUG_PRINTLN(sensorlog_filesize(log));
		return true;
#if defined(ESP8266)		
	}
	return false;
#endif
}

bool sensorlog_add(uint8_t log, Sensor_t *sensor, ulong time) {
	if (sensor->flags.data_ok && sensor->flags.log && time > 1000) {
		SensorLog_t sensorlog;
		sensorlog.nr = sensor->nr;
		sensorlog.time = time;
		sensorlog.native_data = sensor->last_native_data;
		sensorlog.data = sensor->last_data;
		if (!sensorlog_add(log, &sensorlog)) {
			sensor->flags.log = 0;
			return false;
		}
		return true;
	}
	return false;
}

ulong sensorlog_filesize(uint8_t log) {
	//DEBUG_PRINT(F("sensorlog_filesize "));
	checkLogSwitch(log);
	ulong size = file_size(getlogfile(log))+file_size(getlogfile2(log));
	//DEBUG_PRINTLN(size);
	return size;
}

ulong sensorlog_size(uint8_t log) {
	//DEBUG_PRINT(F("sensorlog_size "));
	ulong size = sensorlog_filesize(log) / SENSORLOG_STORE_SIZE;
	//DEBUG_PRINTLN(size);
	return size;
}

void sensorlog_clear_all() {
	sensorlog_clear(true, true, true);
}

void sensorlog_clear(bool std, bool week, bool month) {
	DEBUG_PRINTLN(F("sensorlog_clear "));
	DEBUG_PRINT(std);
	DEBUG_PRINT(week);
	DEBUG_PRINT(month);
	if (std) {
		remove_file(SENSORLOG_FILENAME1);
		remove_file(SENSORLOG_FILENAME2);
		logFileSwitch[LOG_STD] = 1;
	}
	if (week) {
		remove_file(SENSORLOG_FILENAME_WEEK1);
		remove_file(SENSORLOG_FILENAME_WEEK2);
		logFileSwitch[LOG_WEEK] = 1;
	}
	if (month) {
		remove_file(SENSORLOG_FILENAME_MONTH1);
		remove_file(SENSORLOG_FILENAME_MONTH2);
		logFileSwitch[LOG_MONTH] = 1;
	}
}

SensorLog_t *sensorlog_load(uint8_t log, ulong idx) {
	SensorLog_t *sensorlog = new SensorLog_t;
	return sensorlog_load(log, idx, sensorlog);
}

SensorLog_t *sensorlog_load(uint8_t log, ulong idx, SensorLog_t* sensorlog) {
	//DEBUG_PRINTLN(F("sensorlog_load"));

	//Map lower idx to the other log file
	checkLogSwitch(log);
	const char *flast = getlogfile2(log);
	const char *fcur = getlogfile(log);
	ulong size = file_size(flast) / SENSORLOG_STORE_SIZE;
	const char *f;
	if (idx >= size) {
		idx -= size;
		f = fcur;
	} else {
		f = flast;
	}

	file_read_block(f, sensorlog, idx * SENSORLOG_STORE_SIZE, SENSORLOG_STORE_SIZE);
	return sensorlog;
}

int sensorlog_load2(uint8_t log, ulong idx, int count, SensorLog_t* sensorlog) {
	//DEBUG_PRINTLN(F("sensorlog_load"));

	//Map lower idx to the other log file
	checkLogSwitch(log);
	const char *flast = getlogfile2(log);
	const char *fcur = getlogfile(log);
	ulong size = file_size(flast) / SENSORLOG_STORE_SIZE;
	const char *f;
	if (idx >= size) {
		idx -= size;
		f = fcur;
		size = file_size(f) / SENSORLOG_STORE_SIZE;
	} else {
		f = flast;
	}

	if (idx+count > size)
		count = size-idx;
	if (count <= 0)
		return 0;
	file_read_block(f, sensorlog, idx * SENSORLOG_STORE_SIZE, count * SENSORLOG_STORE_SIZE);
	return count; 
}

ulong findLogPosition(uint8_t log, ulong after) {
	ulong log_size = sensorlog_size(log);
	ulong a = 0;
	ulong b = log_size-1;
	ulong lastIdx = 0;
	SensorLog_t sensorlog;
	while (true) {
		ulong idx = (b-a)/2+a;
		sensorlog_load(log, idx, &sensorlog);
		if (sensorlog.time < after) {
			a = idx;
		} else if (sensorlog.time > after) {
			b = idx;
		}
		if (a >= b || idx == lastIdx) return idx;
		lastIdx = idx;
	}
	return 0;
}

// 1/4 of a day: 6*60*60
#define BLOCKSIZE 64
#define CALCRANGE_WEEK 21600
#define CALCRANGE_MONTH 172800
static ulong next_week_calc = 0;
static ulong next_month_calc = 0;

/**
Calculate week+month Data
We store only the average value of 6 hours utc
**/
void calc_sensorlogs()
{
	if (!sensors || timeStatus() != timeSet)
		return;

	ulong log_size = sensorlog_size(LOG_STD);
	if (log_size == 0)
		return;

	SensorLog_t *sensorlog = NULL;

	time_t time = os.now_tz();
	time_t last_day = time;

	if (time >= next_week_calc) {
		DEBUG_PRINTLN(F("calc_sensorlogs WEEK start"));
		sensorlog = (SensorLog_t*)malloc(sizeof(SensorLog_t)*BLOCKSIZE);
		ulong size = sensorlog_size(LOG_WEEK);
		if (size == 0) {
			sensorlog_load(LOG_STD, 0, sensorlog);
			last_day = sensorlog->time;
		}
		else
		{
			sensorlog_load(LOG_WEEK, size - 1, sensorlog); // last record
			last_day = sensorlog->time + CALCRANGE_WEEK;			// Skip last Range
		}
		time_t fromdate = (last_day / CALCRANGE_WEEK) * CALCRANGE_WEEK;
		time_t todate = fromdate + CALCRANGE_WEEK;

		// 4 blocks per day

		while (todate < time) {
			ulong startidx = findLogPosition(LOG_STD, fromdate);
			Sensor_t *sensor = sensors;
			while (sensor) {
				if (sensor->flags.enable && sensor->flags.log) {
					ulong idx = startidx;
					double data = 0;
					ulong n = 0;
					bool done = false;
					while (!done) {
						int sn = sensorlog_load2(LOG_STD, idx, BLOCKSIZE, sensorlog);
						if (sn <= 0) break;
						for (int i = 0; i < sn; i++) {
							idx++;
							if (sensorlog[i].time >= todate) {
								done = true;
								break;
							}
							if (sensorlog[i].nr == sensor->nr) {
								data += sensorlog[i].data;
								n++;
							}
						}
					}
					if (n > 0)
					{
						sensorlog->nr = sensor->nr;
						sensorlog->time = fromdate;
						sensorlog->data = data / (double)n;
						sensorlog->native_data = 0;
						sensorlog_add(LOG_WEEK, sensorlog);
					}
				}
				sensor = sensor->next;
			}
			fromdate += CALCRANGE_WEEK;
			todate += CALCRANGE_WEEK;
		}
		next_week_calc = todate;
		DEBUG_PRINTLN(F("calc_sensorlogs WEEK end"));
	}

	if (time >= next_month_calc) {

		if (!sensorlog)
			sensorlog = (SensorLog_t*)malloc(sizeof(SensorLog_t)*BLOCKSIZE);
		
		log_size = sensorlog_size(LOG_WEEK);
		if (log_size <= 0)
			return;

		DEBUG_PRINTLN(F("calc_sensorlogs MONTH start"));
		ulong size = sensorlog_size(LOG_MONTH);
		if (size == 0)
		{
			sensorlog_load(LOG_WEEK, 0, sensorlog);
			last_day = sensorlog->time;
		}
		else
		{
			sensorlog_load(LOG_MONTH, size - 1, sensorlog); // last record
			last_day = sensorlog->time + CALCRANGE_MONTH;			// Skip last Range
		}
		time_t fromdate = (last_day / CALCRANGE_MONTH) * CALCRANGE_MONTH;
		time_t todate = fromdate + CALCRANGE_MONTH;
		// 4 blocks per day

		while (todate < time)
		{
			ulong startidx = findLogPosition(LOG_WEEK, fromdate);
			Sensor_t *sensor = sensors;
			while (sensor)
			{
				if (sensor->flags.enable && sensor->flags.log)
				{
					ulong idx = startidx;
					double data = 0;
					ulong n = 0;
					bool done = false;
					while (!done) {
						int sn = sensorlog_load2(LOG_WEEK, idx, BLOCKSIZE, sensorlog);
						if (sn <= 0) break;
						for (int i = 0; i < sn; i++) {
							idx++;
							if (sensorlog[i].time >= todate) {
								done = true;
								break;
							}
							if (sensorlog[i].nr == sensor->nr) {
								data += sensorlog[i].data;
								n++;
							}
						}
					}
					if (n > 0)
					{
						sensorlog->nr = sensor->nr;
						sensorlog->time = fromdate;
						sensorlog->data = data / (double)n;
						sensorlog->native_data = 0;
						sensorlog_add(LOG_MONTH, sensorlog);
					}
				}
				sensor = sensor->next;
			}
			fromdate += CALCRANGE_MONTH;
			todate += CALCRANGE_MONTH;
		}
		next_month_calc = todate;
		DEBUG_PRINTLN(F("calc_sensorlogs MONTH end"));
	}
	if (sensorlog)
		free(sensorlog);
}

void sensor_remote_http_callback(char*) {
//unused
}

void push_message(Sensor_t *sensor) {
	if (!sensor || !sensor->last_read)
		return;
		
	static char topic[TMP_BUFFER_SIZE];
	static char payload[TMP_BUFFER_SIZE];
	char* postval = tmp_buffer;

	if (os.mqtt.enabled()) {
		strcpy_P(topic, PSTR("opensprinkler/analogsensor/"));
		strncat(topic, sensor->name, sizeof(topic)-1);
		sprintf_P(payload, PSTR("{\"nr\":%u,\"type\":%u,\"data_ok\":%u,\"time\":%u,\"value\":%d.%02d,\"unit\":\"%s\"}"), 
			sensor->nr, sensor->type, sensor->flags.data_ok, sensor->last_read, (int)sensor->last_data, (int)(sensor->last_data*100)%100, getSensorUnit(sensor));

		os.mqtt.publish(topic, payload);
	}
	if (os.iopts[IOPT_IFTTT_ENABLE]) {
		strcpy_P(postval, PSTR("{\"value1\":\"On site ["));
		os.sopt_load(SOPT_DEVICE_NAME, postval+strlen(postval));
		strcat_P(postval, PSTR("], "));
	
		strcat_P(postval, PSTR("analogsensor "));
		sprintf_P(postval+strlen(postval), PSTR("nr: %u, type: %u, data_ok: %u, time: %u, value: %d.%02d, unit: %s"), 
			sensor->nr, sensor->type, sensor->flags.data_ok, sensor->last_read, (int)sensor->last_data, (int)(sensor->last_data*100)%100, getSensorUnit(sensor));
		strcat_P(postval, PSTR("\"}"));

		//char postBuffer[1500];
		BufferFiller bf = ether_buffer;
		bf.emit_p(PSTR("POST /trigger/sprinkler/with/key/$O HTTP/1.0\r\n"
						"Host: $S\r\n"
						"Accept: */*\r\n"
						"Content-Length: $D\r\n"
						"Content-Type: application/json\r\n\r\n$S"),
						SOPT_IFTTT_KEY, DEFAULT_IFTTT_URL, strlen(postval), postval);

		os.send_http_request(DEFAULT_IFTTT_URL, 80, ether_buffer, sensor_remote_http_callback);
	}			
}

void read_all_sensors() {
	//DEBUG_PRINTLN(F("read_all_sensors"));
	if (!sensors || os.status.network_fails>0 || os.iopts[IOPT_REMOTE_EXT_MODE]) return;

	ulong time = os.now_tz();

	if (time < os.powerup_lasttime+30) return; //wait 30s before first sensor read

	Sensor_t *sensor = sensors;

	while (sensor) {
		if (time >= sensor->last_read + sensor->read_interval) {
			if (read_sensor(sensor) == HTTP_RQT_SUCCESS) {
				sensorlog_add(LOG_STD, sensor, time);
			}
			push_message(sensor);
		}
		sensor = sensor->next;
	}
	sensor_update_groups();

	calc_sensorlogs();
}

#if defined(ESP8266)
/**
 * Read ADS1115 sensors
*/
int read_sensor_adc(Sensor_t *sensor) {
	DEBUG_PRINTLN(F("read_sensor_adc"));
	if (!sensor || !sensor->flags.enable) return HTTP_RQT_NOT_RECEIVED;
	if (sensor->id >= 8) return HTTP_RQT_NOT_RECEIVED;
	//Init + Detect:

	int port = sensor->id < 4? 72 : 73;
	int id = sensor->id % 4;
	sensor->flags.data_ok = false;

	ADS1115 adc(port);
	bool active = adc.begin();
	if (active)
		adc.setGain(1);
	DEBUG_PRINT(F("adc sensor found="));
	DEBUG_PRINTLN(active);

	if (!active)
		return HTTP_RQT_NOT_RECEIVED;

	//Read values:
	sensor->last_native_data = adc.readADC(id);
	sensor->last_data = adc.toVoltage(sensor->last_native_data);
	double v = sensor->last_data;

	switch(sensor->type) {
		case SENSOR_SMT50_MOIS:  // SMT50 VWC [%] = (U * 50) : 3
			sensor->last_data = (v * 50.0) / 3.0;
			break;
		case SENSOR_SMT50_TEMP:  // SMT50 T [°C] = (U – 0,5) * 100
			sensor->last_data = (v - 0.5) * 100.0;
			break;
		case SENSOR_ANALOG_EXTENSION_BOARD_P: // 0..3,3V -> 0..100%
			sensor->last_data = v * 100.0 / 3.3;
			if (sensor->last_data < 0)
				sensor->last_data = 0;
			else if (sensor->last_data > 100)
				sensor->last_data = 100;
			break;
		case SENSOR_SMT100_ANALOG_MOIS: // 0..3V -> 0..100%
			sensor->last_data = v * 100.0 / 3;
			break;
		case SENSOR_SMT100_ANALOG_TEMP: // 0..3V -> -40°C..60°C
			sensor->last_data = v * 100.0 / 3 - 40;
			break;

		case SENSOR_VH400: //http://vegetronix.com/Products/VH400/VH400-Piecewise-Curve
			if (v <= 1.1)      // 0 to 1.1V         VWC= 10*V-1
				sensor->last_data = 10 * v - 1;
			else if (v < 1.3)  // 1.1V to 1.3V      VWC= 25*V- 17.5
				sensor->last_data = 25 * v - 17.5;
			else if (v < 1.82) // 1.3V to 1.82V     VWC= 48.08*V- 47.5
				sensor->last_data = 48.08 * v - 47.5;
			else if (v < 2.2)  // 1.82V to 2.2V     VWC= 26.32*V- 7.89
				sensor->last_data = 26.32 * v - 7.89;
			else               // 2.2V - 3.0V       VWC= 62.5*V - 87.5 
				sensor->last_data = 62.5 * v - 87.5;
			break;
		case SENSOR_THERM200: //http://vegetronix.com/Products/THERM200/
			sensor->last_data = v * 41.67 - 40;
			break;
		case SENSOR_AQUAPLUMB: //http://vegetronix.com/Products/AquaPlumb/
			sensor->last_data = v * 100.0 / 3.0; // 0..3V -> 0..100%
			if (sensor->last_data < 0)
				sensor->last_data = 0;
			else if (sensor->last_data > 100)
				sensor->last_data = 100;
			break;
	}

	sensor->flags.data_ok = true;

	DEBUG_PRINT(F("adc sensor values: "));
	DEBUG_PRINT(sensor->last_native_data);
	DEBUG_PRINT(",");
	DEBUG_PRINTLN(sensor->last_data);

	return HTTP_RQT_SUCCESS;
}
#endif

int read_sensor_ospi(Sensor_t *sensor) {
	DEBUG_PRINTLN(F("read_sensor_ospi"));
	if (!sensor || !sensor->flags.enable) return HTTP_RQT_NOT_RECEIVED;

	//currently not implemented 

	return HTTP_RQT_SUCCESS;
}

bool extract(char *s, char *buf, int maxlen) {
	s = strstr(s, ":");
	if (!s) return false;
	s++;
	while (*s == ' ') s++; //skip spaces
	char *e = strstr(s, ",");
	char *f = strstr(s, "}");
	if (!e && !f) return false;
	if (f && f < e) e = f;
	int l = e-s;
	if (l < 1 || l > maxlen)
	 	return false;
	strncpy(buf, s, l);
	buf[l] = 0;
	//DEBUG_PRINTLN(buf);
	return true;
}

int read_sensor_http(Sensor_t *sensor) {
#if defined(ESP8266)
    IPAddress _ip(sensor->ip);
	byte ip[4] = {_ip[0], _ip[1], _ip[2], _ip[3]};
#else
    byte ip[4];
	ip[0] = (byte)((sensor->ip >> 24) &0xFF);
	ip[1] = (byte)((sensor->ip >> 16) &0xFF);
	ip[2] = (byte)((sensor->ip >> 8) &0xFF);
	ip[3] = (byte)((sensor->ip &0xFF));
#endif

	DEBUG_PRINTLN(F("read_sensor_http"));

	char *p = tmp_buffer;
	BufferFiller bf = p;

	bf.emit_p(PSTR("GET /sg?pw=$O&nr=$D"),
		SOPT_PASSWORD, sensor->id);
	bf.emit_p(PSTR(" HTTP/1.0\r\nHOST: $D.$D.$D.$D\r\n\r\n"),
		ip[0],ip[1],ip[2],ip[3]);

	DEBUG_PRINTLN(p);

	char server[20];
	sprintf(server, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

	if (os.send_http_request(server, sensor->port, p) == HTTP_RQT_SUCCESS) {
		DEBUG_PRINTLN("Send Ok");
		p = ether_buffer;
		DEBUG_PRINTLN(p);

		char buf[20];
		char *s = strstr(p, "\"nativedata\":");
		if (s && extract(s, buf, sizeof(buf))) {
			sensor->last_native_data = strtoul(buf, NULL, 0);
		}
		s = strstr(p, "\"data\":");
		if (s && extract(s, buf, sizeof(buf))) {
			sensor->last_data = strtod(buf, NULL);
			sensor->flags.data_ok = true;
		}
		s = strstr(p, "\"unitid\":");
		if (s && extract(s, buf, sizeof(buf))) {
			sensor->unitid = atoi(buf);
		}
	
		return HTTP_RQT_SUCCESS;
	}
	return HTTP_RQT_EMPTY_RETURN;
}

/**
 * Read ip connected sensors
*/
int read_sensor_ip(Sensor_t *sensor) {
#if defined(ARDUINO)

	Client *client;
	#if defined(ESP8266)
		WiFiClient wifiClient;
		client = &wifiClient;
	#else
		EthernetClient etherClient;
		client = &etherClient;
	#endif

#else
	EthernetClient etherClient;
	EthernetClient *client = &etherClient;
#endif

	sensor->flags.data_ok = false;
	if (!sensor->ip || !sensor->port) {
		sensor->flags.enable = false;				
		return HTTP_RQT_CONNECT_ERR;
	}

#if defined(ESP8266)
    IPAddress _ip(sensor->ip);
	byte ip[4] = {_ip[0], _ip[1], _ip[2], _ip[3]};
#else
    byte ip[4];
	ip[0] = (byte)((sensor->ip >> 24) &0xFF);
	ip[1] = (byte)((sensor->ip >> 16) &0xFF);
	ip[2] = (byte)((sensor->ip >> 8) &0xFF);
	ip[3] = (byte)((sensor->ip &0xFF));
#endif

	if(!client->connect(ip, sensor->port)) {
		DEBUG_PRINT(F("Cannot connect to "));
		DEBUG_PRINT(ip[0]); DEBUG_PRINT(".");
		DEBUG_PRINT(ip[1]); DEBUG_PRINT(".");
		DEBUG_PRINT(ip[2]); DEBUG_PRINT(".");
		DEBUG_PRINT(ip[3]); DEBUG_PRINT(":");
		DEBUG_PRINTLN(sensor->port);
		client->stop();
		return HTTP_RQT_CONNECT_ERR;
	}

	uint8_t buffer[10];
	int len = 0;
	boolean addCrc16 = false;
	switch(sensor->type)
	{
		case SENSOR_SMT100_MODBUS_RTU_MOIS:
			buffer[0] = sensor->id; //Modbus ID
			buffer[1] = 0x03; //Read Holding Registers
			buffer[2] = 0x00; 
			buffer[3] = 0x01; //soil moisture is at address 0x0001 
			buffer[4] = 0x00; 
			buffer[5] = 0x01; //Number of registers to read (soil moisture value is one 16-bit register)
			len = 6;
			addCrc16 = true;
			break;
 
		case SENSOR_SMT100_MODBUS_RTU_TEMP:
			buffer[0] = sensor->id;
			buffer[1] = 0x03; //Read Holding Registers
			buffer[2] = 0x00; 
			buffer[3] = 0x00; //temperature is at address 0x0000 
			buffer[4] = 0x00; 
			buffer[5] = 0x01; //Number of registers to read (soil moisture value is one 16-bit register)
			len = 6;
			addCrc16 = true;
			break;

		default:
			client->stop();
		 	return HTTP_RQT_NOT_RECEIVED;
	}
	if (addCrc16) {
		uint16_t crc = CRC16(buffer, len);
		buffer[len+0] = (0x00FF&crc);
		buffer[len+1] = (0xFF00&crc)>>8;
		len += 2;
	}

	client->write(buffer, len);
#if defined(ESP8266)
	client->flush();
#endif

	//Read result:
	switch(sensor->type)
	{
		case SENSOR_SMT100_MODBUS_RTU_MOIS:
		case SENSOR_SMT100_MODBUS_RTU_TEMP:	
			uint32_t stoptime = millis()+SENSOR_READ_TIMEOUT;
#if defined(ESP8266)
			while (true) {
				if (client->available())
					break;
				if (millis() >=  stoptime) {
					client->stop();
					DEBUG_PRINT(F("Sensor "));
					DEBUG_PRINT(sensor->nr);
					DEBUG_PRINT(F(" timeout read!"));
					return HTTP_RQT_TIMEOUT;
				}
				delay(5);
			}
			int n = client->read(buffer, 7);
#else
			int n = 0;
			while (true) {
				n = client->read(buffer, 7);
				if (n > 0)
					break;
				if (millis() >=  stoptime) {
					client->stop();
					DEBUG_PRINT(F("Sensor "));
					DEBUG_PRINT(sensor->nr);
					DEBUG_PRINT(F(" timeout read!"));
					return HTTP_RQT_TIMEOUT;
				}
				delay(5);
			}
#endif
			client->stop();
			DEBUG_PRINT(F("Sensor "));
			DEBUG_PRINT(sensor->nr);
			if (n != 7) {
				DEBUG_PRINT(F(" returned "));
				DEBUG_PRINT(n); 
				DEBUG_PRINT(F(" bytes??")); 	
				return n == 0 ? HTTP_RQT_EMPTY_RETURN:HTTP_RQT_TIMEOUT;
			}
			if ((buffer[0] != sensor->id && sensor->id != 253)) { //253 is broadcast
				DEBUG_PRINT(F(" returned sensor id "));
				DEBUG_PRINT((int)buffer[0]);
				return HTTP_RQT_NOT_RECEIVED;
			}
			uint16_t crc = (buffer[6] << 8) | buffer[5];
			if (crc != CRC16(buffer, 5)) {
				DEBUG_PRINT(F(" crc error!"));
				return HTTP_RQT_NOT_RECEIVED;
			}

			//Valid result:
			sensor->last_native_data = (buffer[3] << 8) | buffer[4];
			DEBUG_PRINT(F(" native: "));
			DEBUG_PRINT(sensor->last_native_data);

			//Convert to readable value:
			switch(sensor->type)
			{
				case SENSOR_SMT100_MODBUS_RTU_MOIS: //Equation: soil moisture [vol.%]= (16Bit_soil_moisture_value / 100)
					sensor->last_data = ((double)sensor->last_native_data / 100);
					sensor->flags.data_ok = sensor->last_native_data < 10000;
					DEBUG_PRINT(F(" soil moisture %: "));
					break;
				case SENSOR_SMT100_MODBUS_RTU_TEMP:	//Equation: temperature [°C]= (16Bit_temperature_value / 100)-100
					sensor->last_data = ((double)sensor->last_native_data / 100) - 100;
					sensor->flags.data_ok = sensor->last_native_data > 7000;
					DEBUG_PRINT(F(" temperature °C: "));
					break;
			}
			DEBUG_PRINTLN(sensor->last_data);
			return sensor->flags.data_ok?HTTP_RQT_SUCCESS:HTTP_RQT_NOT_RECEIVED;
	}

	return HTTP_RQT_NOT_RECEIVED;
}

/**
 * read a sensor
*/
int read_sensor(Sensor_t *sensor) {

	if (!sensor || !sensor->flags.enable)
		return HTTP_RQT_NOT_RECEIVED;

	DEBUG_PRINT(F("Reading sensor "));
	DEBUG_PRINTLN(sensor->name);

	ulong time = os.now_tz();

	switch(sensor->type)
	{
		case SENSOR_SMT100_MODBUS_RTU_MOIS:
		case SENSOR_SMT100_MODBUS_RTU_TEMP:
			sensor->last_read = time;
			return read_sensor_ip(sensor);

#if defined(ESP8266)
		case SENSOR_ANALOG_EXTENSION_BOARD:
		case SENSOR_ANALOG_EXTENSION_BOARD_P:
		case SENSOR_SMT50_MOIS: //SMT50 VWC [%] = (U * 50) : 3
		case SENSOR_SMT50_TEMP: //SMT50 T [°C] = (U – 0,5) * 100
		case SENSOR_SMT100_ANALOG_MOIS: //SMT100 Analog Moisture
		case SENSOR_SMT100_ANALOG_TEMP: //SMT100 Analog Temperature
		case SENSOR_VH400:
		case SENSOR_THERM200:
		case SENSOR_AQUAPLUMB:
			sensor->last_read = time;
			return read_sensor_adc(sensor);
#endif

		//case SENSOR_OSPI_ANALOG_INPUTS:
		//	return read_sensor_ospi(sensor);
		case SENSOR_REMOTE:
			sensor->last_read = time;
			return read_sensor_http(sensor);

		case SENSOR_WEATHER_TEMP_F:
		case SENSOR_WEATHER_TEMP_C: 
		case SENSOR_WEATHER_HUM:
		case SENSOR_WEATHER_PRECIP_IN:
		case SENSOR_WEATHER_PRECIP_MM:
		case SENSOR_WEATHER_WIND_MPH:
		case SENSOR_WEATHER_WIND_KMH:
		{
			GetSensorWeather();
			if (current_weather_ok) {
				sensor->last_read = time;
				sensor->last_native_data = 0;
				sensor->flags.data_ok = true;

				switch(sensor->type)
				{
					case SENSOR_WEATHER_TEMP_F: {
						sensor->last_data = current_temp;
						break;
					}
					case SENSOR_WEATHER_TEMP_C: {
						sensor->last_data = (current_temp - 32.0) / 1.8;
						break;
					}
					case SENSOR_WEATHER_HUM: {
						sensor->last_data = current_humidity;
						break;
					}
					case SENSOR_WEATHER_PRECIP_IN: {
						sensor->last_data = current_precip;
						break;
					}
					case SENSOR_WEATHER_PRECIP_MM: {
						sensor->last_data = current_precip * 25.4;
						break;
					}
					case SENSOR_WEATHER_WIND_MPH: {
						sensor->last_data = current_wind;
						break;
					}
					case SENSOR_WEATHER_WIND_KMH: {
						sensor->last_data = current_wind * 1.609344;
						break;
					}
				}				
				return HTTP_RQT_SUCCESS;
			}
			return HTTP_RQT_NOT_RECEIVED;
		}

		default: return HTTP_RQT_NOT_RECEIVED;
	}
}

/**
 * @brief Update group values
 * 
 */
void sensor_update_groups()
{
	Sensor_t *sensor = sensors;

	ulong time = os.now_tz();

	while (sensor)
	{
		if (time >= sensor->last_read + sensor->read_interval) {
			switch (sensor->type) {
				case SENSOR_GROUP_MIN:
				case SENSOR_GROUP_MAX:
				case SENSOR_GROUP_AVG:
				case SENSOR_GROUP_SUM: {
					uint nr = sensor->nr;
					Sensor_t *group = sensors;
					double value = 0;
					int n = 0;
					while (group) {
						if (group->nr != nr && group->group == nr && group->flags.enable && group->flags.data_ok) {
							switch (sensor->type) {
								case SENSOR_GROUP_MIN:
									if (n++ == 0)
										value = group->last_data;
									else if (group->last_data < value)
										value = group->last_data;
									break;
								case SENSOR_GROUP_MAX:
									if (n++ == 0)
										value = group->last_data;
									else if (group->last_data > value)
										value = group->last_data;
									break;
								case SENSOR_GROUP_AVG:
								case SENSOR_GROUP_SUM:
									n++;
									value += group->last_data;
									break;
							}
						}
						group = group->next;
					}
					if (sensor->type == SENSOR_GROUP_AVG && n > 0) {
						value = value / (double)n;
					}
					sensor->last_data = value;
					sensor->last_native_data = 0;
					sensor->last_read = time;
					sensor->flags.data_ok = n > 0;
					sensorlog_add(LOG_STD, sensor, time);
					break;
				}
			}
		}
		sensor = sensor->next;
	}
}

int set_sensor_address(Sensor_t *sensor, byte new_address) {
	if (!sensor)
		return HTTP_RQT_NOT_RECEIVED;

	switch(sensor->type)
	{
		case SENSOR_SMT100_MODBUS_RTU_MOIS:
		case SENSOR_SMT100_MODBUS_RTU_TEMP:
			uint8_t buffer[10];
			int len = 8;
			buffer[0] = sensor->id;
			buffer[1] = 0x06;
			buffer[2] = 0x00;
			buffer[3] = 0x04;
			buffer[4] = 0x00;
			buffer[5] = new_address;
			uint16_t crc = CRC16(buffer, 6);
			buffer[6] = (0x00FF&crc);
			buffer[7] = (0xFF00&crc)>>8;
			len = 8;

#if defined(ARDUINO)
			Client *client;
	#if defined(ESP8266)
			WiFiClient wifiClient;
			client = &wifiClient;
	#else
			EthernetClient etherClient;
			client = &etherClient;
	#endif
#else
			EthernetClient etherClient;
			EthernetClient *client = &etherClient;
#endif
			sensor->flags.data_ok = false;
			if (!sensor->ip || !sensor->port) {
				sensor->flags.enable = false;				
				return HTTP_RQT_CONNECT_ERR;
			}

#if defined(ESP8266)
		    IPAddress _ip(sensor->ip);
			byte ip[4] = {_ip[0], _ip[1], _ip[2], _ip[3]};
#else
    		byte ip[4];
			ip[0] = (byte)((sensor->ip >> 24) &0xFF);
			ip[1] = (byte)((sensor->ip >> 16) &0xFF);
			ip[2] = (byte)((sensor->ip >> 8) &0xFF);
			ip[3] = (byte)((sensor->ip &0xFF));
#endif		
			if(!client->connect(ip, sensor->port)) {
				DEBUG_PRINT(F("Cannot connect to "));
				DEBUG_PRINT(ip[0]); DEBUG_PRINT(".");
				DEBUG_PRINT(ip[1]); DEBUG_PRINT(".");
				DEBUG_PRINT(ip[2]); DEBUG_PRINT(".");
				DEBUG_PRINT(ip[3]); DEBUG_PRINT(":");
				DEBUG_PRINTLN(sensor->port);
				client->stop();
				return HTTP_RQT_CONNECT_ERR;
			}

			client->write(buffer, len);
#if defined(ESP8266)
			client->flush();
#endif

			//Read result:
			int n = client->read(buffer, 8);
			client->stop();
			DEBUG_PRINT(F("Sensor "));
			DEBUG_PRINT(sensor->nr);
			if (n != 8) {
				DEBUG_PRINT(F(" returned "));
				DEBUG_PRINT(n); 
				DEBUG_PRINT(F(" bytes??")); 	
				return n == 0 ? HTTP_RQT_EMPTY_RETURN:HTTP_RQT_TIMEOUT;
			}
			if ((buffer[0] != sensor->id && sensor->id != 253)) { //253 is broadcast
				DEBUG_PRINT(F(" returned sensor id "));
				DEBUG_PRINT((int)buffer[0]);
				return HTTP_RQT_NOT_RECEIVED;
			}
			crc = (buffer[6] | buffer[7] << 8);
			if (crc != CRC16(buffer, 6)) {
				DEBUG_PRINT(F(" crc error!"));
				return HTTP_RQT_NOT_RECEIVED;
			}
			//Read OK:
			sensor->id = new_address;
			sensor_save();
			return HTTP_RQT_SUCCESS;
	}
	return HTTP_RQT_NOT_RECEIVED;
}

double calc_linear(ProgSensorAdjust_t *p, Sensor_t *sensor) {
	
//   min max  factor1 factor2
//   10..90 -> 5..1 factor1 > factor2
//    a   b    c  d
//   (b-sensorData) / (b-a) * (c-d) + d
//
//   10..90 -> 1..5 factor1 < factor2
//    a   b    c  d
//   (sensorData-a) / (b-a) * (d-c) + c
				
	double sensorData = sensor->last_data;
	// Limit to min/max:
	if (sensorData < p->min) sensorData = p->min;
	if (sensorData > p->max) sensorData = p->max;

	//Calculate:
	if (p->factor1 > p->factor2) { // invers scaling factor:
		return (p->max - sensorData) / (p->max - p->min) * (p->factor1 - p->factor2) + p->factor2;
	} else { // upscaling factor:
		return (sensorData - p->min) / (p->max - p->min) * (p->factor2 - p->factor1) + p->factor1;
	}
}

double calc_digital_min(ProgSensorAdjust_t *p, Sensor_t *sensor) {
	return sensor->last_data <= p->min? 1:0;
}

double calc_digital_max(ProgSensorAdjust_t *p, Sensor_t *sensor) {
	return sensor->last_data >= p->max? 1:0;
}

/**
 * @brief calculate adjustment
 * 
 * @param prog 
 * @return double 
 */
double calc_sensor_watering(uint prog) {
	double result = 1;
	ProgSensorAdjust_t *p = progSensorAdjusts;

	while (p) {
		if (p->prog-1 == prog) {
			Sensor_t *sensor = sensor_by_nr(p->sensor);
			if (sensor && sensor->flags.enable && sensor->flags.data_ok) {

				double res = 0;
				switch(p->type) {
					case PROG_NONE:        res = 1; break;
					case PROG_LINEAR:      res = calc_linear(p, sensor); break;
					case PROG_DIGITAL_MIN: res = calc_digital_min(p, sensor); break;
					case PROG_DIGITAL_MAX: res = calc_digital_max(p, sensor); break;
					default:               res = 0; 
				}

				result = result * res;
			}
		}

		p = p->next;
	}

	return result;
}

/**
 * @brief calculate adjustment
 * 
 * @param nr 
 * @return double 
 */
double calc_sensor_watering_by_nr(uint nr) {
	double result = 1;
	ProgSensorAdjust_t *p = progSensorAdjusts;

	while (p) {
		if (p->nr == nr) {
			Sensor_t *sensor = sensor_by_nr(p->sensor);
			if (sensor && sensor->flags.enable && sensor->flags.data_ok) {

				double res = 0;
				switch(p->type) {
					case PROG_NONE:        res = 1; break;
					case PROG_LINEAR:      res = calc_linear(p, sensor); break;
					case PROG_DIGITAL_MIN: res = calc_digital_min(p, sensor); break;
					case PROG_DIGITAL_MAX: res = calc_digital_max(p, sensor); break;
					default:               res = 0; 
				}

				result = result * res;
			}
			break;
		}

		p = p->next;
	}

	return result;
}

int prog_adjust_define(uint nr, uint type, uint sensor, uint prog, double factor1, double factor2, double min, double max) {
	ProgSensorAdjust_t *p = progSensorAdjusts;

	ProgSensorAdjust_t *last = NULL;

	while (p) {
		if (p->nr == nr) {
			p->type = type;
			p->sensor = sensor;
			p->prog = prog;
			p->factor1 = factor1;
			p->factor2 = factor2;
			p->min = min;
			p->max = max;
			prog_adjust_save();
			return HTTP_RQT_SUCCESS;
		}

		if (p->nr > nr)
			break;

		last = p;
		p = p->next;
	}

	p = new ProgSensorAdjust_t;
	p->nr = nr;
	p->type = type;
	p->sensor = sensor;
	p->prog = prog;
	p->factor1 = factor1;
	p->factor2 = factor2;
	p->min = min;
	p->max = max;
	if (last) {
		p->next = last->next;
		last->next = p;
	} else {
		p->next = progSensorAdjusts;
		progSensorAdjusts = p;
	}

	prog_adjust_save();
	return HTTP_RQT_SUCCESS;
}

int prog_adjust_delete(uint nr) {
	ProgSensorAdjust_t *p = progSensorAdjusts;

	ProgSensorAdjust_t *last = NULL;

	while (p) {
		if (p->nr == nr) {
			if (last)
				last->next = p->next;
			else
				progSensorAdjusts = p->next;
			delete p;
			prog_adjust_save();
			return HTTP_RQT_SUCCESS;
		}
		last = p;
		p = p->next;
	}
	return HTTP_RQT_NOT_RECEIVED;
}

void prog_adjust_save() {
	if (file_exists(PROG_SENSOR_FILENAME))
		remove_file(PROG_SENSOR_FILENAME);

	ulong pos = 0;
	ProgSensorAdjust_t *pa = progSensorAdjusts;
	while (pa) {
		file_write_block(PROG_SENSOR_FILENAME, pa, pos, PROGSENSOR_STORE_SIZE);
		pa = pa->next;
		pos += PROGSENSOR_STORE_SIZE; 
	}
}

void prog_adjust_load() {
	DEBUG_PRINTLN(F("prog_adjust_load"));
	progSensorAdjusts = NULL;
	if (!file_exists(PROG_SENSOR_FILENAME))
		return;

	ulong pos = 0;
	ProgSensorAdjust_t *last = NULL;
	while (true) {
		ProgSensorAdjust_t *pa = new ProgSensorAdjust_t;
		memset(pa, 0, sizeof(ProgSensorAdjust_t));
		file_read_block (PROG_SENSOR_FILENAME, pa, pos, PROGSENSOR_STORE_SIZE);
		if (!pa->nr || !pa->type) {
			delete pa;
			break;
		}
		if (!last) progSensorAdjusts = pa;
		else last->next = pa;
		last = pa;
		pa->next = NULL;
		pos += PROGSENSOR_STORE_SIZE;
	}
}

uint prog_adjust_count() {
	uint count = 0;
	ProgSensorAdjust_t *pa = progSensorAdjusts;
	while (pa) {
		count++;
		pa = pa->next;
	}
	return count;
}

ProgSensorAdjust_t *prog_adjust_by_nr(uint nr) {
	ProgSensorAdjust_t *pa = progSensorAdjusts;
	while (pa) {
		if (pa->nr == nr) return pa;
		pa = pa->next;
	}
	return NULL;
}

ProgSensorAdjust_t *prog_adjust_by_idx(uint idx) {
	ProgSensorAdjust_t *pa = progSensorAdjusts;
	uint idxCounter = 0;
	while (pa) {
		if (idxCounter++ == idx) return pa;
		pa = pa->next;
	}
	return NULL;
}

#if defined(ESP8266)
ulong diskFree() {
	struct FSInfo fsinfo;
	LittleFS.info(fsinfo);
	return fsinfo.totalBytes-fsinfo.usedBytes;
}

bool checkDiskFree() {
	if (diskFree() < MIN_DISK_FREE) {
		DEBUG_PRINT(F("fs has low space!"));
		return false;
	}
	return true;
}
#endif

const char* getSensorUnit(Sensor_t *sensor) {
	if (!sensor)
		return sensor_unitNames[0];

	return sensor_unitNames[getSensorUnitId(sensor)];
}

boolean sensor_isgroup(Sensor_t *sensor) {
	if (!sensor)
		return false;

	switch(sensor->type) {
		case SENSOR_GROUP_MIN:
		case SENSOR_GROUP_MAX:             
		case SENSOR_GROUP_AVG:             
		case SENSOR_GROUP_SUM: return true;

		default: return false;
	}
}

byte getSensorUnitId(int type) {
	switch(type) {
		case SENSOR_SMT100_MODBUS_RTU_MOIS:   return UNIT_PERCENT; 
		case SENSOR_SMT100_MODBUS_RTU_TEMP:   return UNIT_DEGREE;
#if defined(ESP8266)
	    case SENSOR_ANALOG_EXTENSION_BOARD:   return UNIT_VOLT;
	    case SENSOR_ANALOG_EXTENSION_BOARD_P: return UNIT_PERCENT;
		case SENSOR_SMT50_MOIS: 			  return UNIT_PERCENT;
		case SENSOR_SMT50_TEMP: 			  return UNIT_DEGREE;
		case SENSOR_SMT100_ANALOG_MOIS:       return UNIT_PERCENT; 
		case SENSOR_SMT100_ANALOG_TEMP:       return UNIT_DEGREE;

		case SENSOR_VH400:                    return UNIT_PERCENT;
		case SENSOR_THERM200:                 return UNIT_DEGREE;
		case SENSOR_AQUAPLUMB:                return UNIT_PERCENT;
#endif
		case SENSOR_OSPI_ANALOG_INPUTS: 	  return UNIT_VOLT;

		case SENSOR_WEATHER_TEMP_F:           return UNIT_FAHRENHEIT;
		case SENSOR_WEATHER_TEMP_C:           return UNIT_DEGREE;
		case SENSOR_WEATHER_HUM:              return UNIT_HUM_PERCENT;
		case SENSOR_WEATHER_PRECIP_IN:        return UNIT_INCH;
		case SENSOR_WEATHER_PRECIP_MM:        return UNIT_MM;
		case SENSOR_WEATHER_WIND_MPH:         return UNIT_MPH;
		case SENSOR_WEATHER_WIND_KMH:         return UNIT_KMH;

		default: return UNIT_NONE;
	}
}

byte getSensorUnitId(Sensor_t *sensor) {
	if (!sensor)
		return 0;

	switch(sensor->type) {
		case SENSOR_SMT100_MODBUS_RTU_MOIS:   return UNIT_PERCENT; 
		case SENSOR_SMT100_MODBUS_RTU_TEMP:   return UNIT_DEGREE;
#if defined(ESP8266)		
	    case SENSOR_ANALOG_EXTENSION_BOARD:   return UNIT_VOLT;
		case SENSOR_ANALOG_EXTENSION_BOARD_P: return UNIT_LEVEL;
		case SENSOR_SMT50_MOIS: 			  return UNIT_PERCENT;
		case SENSOR_SMT50_TEMP: 			  return UNIT_DEGREE;
		case SENSOR_SMT100_ANALOG_MOIS:       return UNIT_PERCENT;
		case SENSOR_SMT100_ANALOG_TEMP:       return UNIT_DEGREE;

		case SENSOR_VH400:                    return UNIT_PERCENT;
		case SENSOR_THERM200:                 return UNIT_DEGREE;
		case SENSOR_AQUAPLUMB:                return UNIT_PERCENT;
#endif
		case SENSOR_OSPI_ANALOG_INPUTS: 	  return UNIT_VOLT;
		case SENSOR_REMOTE:                	  return sensor->unitid;

		case SENSOR_WEATHER_TEMP_F:           return UNIT_FAHRENHEIT;
		case SENSOR_WEATHER_TEMP_C:           return UNIT_DEGREE;
		case SENSOR_WEATHER_HUM:              return UNIT_HUM_PERCENT;
		case SENSOR_WEATHER_PRECIP_IN:        return UNIT_INCH;
		case SENSOR_WEATHER_PRECIP_MM:        return UNIT_MM;
		case SENSOR_WEATHER_WIND_MPH:         return UNIT_MPH;
		case SENSOR_WEATHER_WIND_KMH:         return UNIT_KMH;

		case SENSOR_GROUP_MIN:
		case SENSOR_GROUP_MAX:             
		case SENSOR_GROUP_AVG:             
		case SENSOR_GROUP_SUM: 

			for (int i = 0; i < 100; i++) {
				Sensor_t *sen = sensors;
				while (sen) {
					if (sen != sensor && sen->group > 0 && sen->group == sensor->nr) {
						if (!sensor_isgroup(sen)) 
							return getSensorUnitId(sen);
						sensor = sen;
						break;
					}
					sen = sen->next;
				}
			}

		default: return UNIT_NONE;
	}
}

void GetSensorWeather() {
#if defined(ESP8266)
	if (!useEth)
		if (os.state!=OS_STATE_CONNECTED || WiFi.status()!=WL_CONNECTED) return;
#endif
	time_t time = os.now_tz();
	if (last_weather_time == 0)
		last_weather_time = time - 59*60;

	if (time < last_weather_time + 60*60)
		return;

	// use temp buffer to construct get command
	BufferFiller bf = tmp_buffer;
	bf.emit_p(PSTR("weatherData?loc=$O"), SOPT_LOCATION);

	char *src=tmp_buffer+strlen(tmp_buffer);
	char *dst=tmp_buffer+TMP_BUFFER_SIZE-12;

	char c;
	// url encode. convert SPACE to %20
	// copy reversely from the end because we are potentially expanding
	// the string size
	while(src!=tmp_buffer) {
		c = *src--;
		if(c==' ') {
			*dst-- = '0';
			*dst-- = '2';
			*dst-- = '%';
		} else {
			*dst-- = c;
		}
	};
	*dst = *src;

	strcpy(ether_buffer, "GET /");
	strcat(ether_buffer, dst);
	// because dst is part of tmp_buffer,
	// must load weather url AFTER dst is copied to ether_buffer

	// load weather url to tmp_buffer
	char *host = tmp_buffer;
	os.sopt_load(SOPT_WEATHERURL, host);

	strcat(ether_buffer, " HTTP/1.0\r\nHOST: ");
	strcat(ether_buffer, host);
	strcat(ether_buffer, "\r\n\r\n");

	DEBUG_PRINTLN(F("GetSensorWeather"));
	DEBUG_PRINTLN(ether_buffer);

	int ret = os.send_http_request(host, ether_buffer, NULL);
	if(ret == HTTP_RQT_SUCCESS) {
		last_weather_time = time;
		DEBUG_PRINTLN(ether_buffer);

		char buf[20];
		char *s = strstr(ether_buffer, "\"temp\":");
		if (s && extract(s, buf, sizeof(buf))) {
			current_temp = atof(buf);
		}
		s = strstr(ether_buffer, "\"humidity\":");
		if (s && extract(s, buf, sizeof(buf))) {
			current_humidity = atof(buf);
		}
		s = strstr(ether_buffer, "\"precip\":");
		if (s && extract(s, buf, sizeof(buf))) {
			current_precip = atof(buf);
		}
		s = strstr(ether_buffer, "\"wind\":");
		if (s && extract(s, buf, sizeof(buf))) {
			current_wind = atof(buf);
		}
		char tmp[10];
		DEBUG_PRINT("temp: ");
		dtostrf(current_temp, 2, 2, tmp);
		DEBUG_PRINTLN(tmp)
		DEBUG_PRINT("humidity: ");
		dtostrf(current_humidity, 2, 2, tmp);
		DEBUG_PRINTLN(tmp)
		DEBUG_PRINT("precip: ");
		dtostrf(current_precip, 2, 2, tmp);
		DEBUG_PRINTLN(tmp)
		DEBUG_PRINT("wind: ");
		dtostrf(current_wind, 2, 2, tmp);
		DEBUG_PRINTLN(tmp)

		current_weather_ok = true;
	} else {
		current_weather_ok = false;
	}
}
