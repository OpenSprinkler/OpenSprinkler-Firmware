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

//All sensors:
static Sensor_t *sensors = NULL;

//Program sensor data 
static ProgSensorAdjust_t *progSensorAdjusts = NULL;

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
	DEBUG_PRINTLN(F("sensor_load"));
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
	DEBUG_PRINTLN(F("sensor_save"));
	if (!sensors && file_exists(SENSOR_FILENAME))
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
	DEBUG_PRINTLN(F("sensor_count"));
	Sensor_t *sensor = sensors;
	uint count = 0;
	while (sensor) {
		count++;
		sensor = sensor->next;
	}
	return count;
}

Sensor_t *sensor_by_nr(uint nr) {
	DEBUG_PRINTLN(F("sensor_by_nr"));
	Sensor_t *sensor = sensors;
	while (sensor) {
		if (sensor->nr == nr) 
			return sensor;
		sensor = sensor->next;
	}
	return NULL;
}

Sensor_t *sensor_by_idx(uint idx) {
	DEBUG_PRINTLN(F("sensor_by_idx"));
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

bool sensorlog_add(SensorLog_t *sensorlog) {
#if defined(ESP8266)
	if (checkDiskFree()) {
#endif
		DEBUG_PRINT(F("sensorlog_add "));
		file_append_block(SENSORLOG_FILENAME, sensorlog, SENSORLOG_STORE_SIZE);
		DEBUG_PRINT(sensorlog_filesize());
		return true;
#if defined(ESP8266)		
	}
	return false;
#endif
}

bool sensorlog_add(Sensor_t *sensor, ulong time) {
	if (sensor->flags.data_ok && sensor->flags.log) {
		SensorLog_t sensorlog;
		sensorlog.nr = sensor->nr;
		sensorlog.time = time;
		sensorlog.native_data = sensor->last_native_data;
		sensorlog.data = sensor->last_data;
		if (!sensorlog_add(&sensorlog)) {
			sensor->flags.log = 0;
			return false;
		}
		return true;
	}
	return false;
}

ulong sensorlog_filesize() {
	DEBUG_PRINT(F("sensorlog_filesize "));
	ulong size = file_size(SENSORLOG_FILENAME);
	DEBUG_PRINTLN(size);
	return size;
}

ulong sensorlog_size() {
	DEBUG_PRINT(F("sensorlog_size "));
	ulong size = file_size(SENSORLOG_FILENAME) / SENSORLOG_STORE_SIZE;
	DEBUG_PRINTLN(size);
	return size;
}

void sensorlog_clear_all() {
	DEBUG_PRINTLN(F("sensorlog_clear_all"));
	remove_file(SENSORLOG_FILENAME);
}

SensorLog_t *sensorlog_load(ulong idx) {
	SensorLog_t *sensorlog = new SensorLog_t;
	return sensorlog_load(idx, sensorlog);
}

SensorLog_t *sensorlog_load(ulong idx, SensorLog_t* sensorlog) {
	DEBUG_PRINTLN(F("sensorlog_load"));
	file_read_block(SENSORLOG_FILENAME, sensorlog, idx * SENSORLOG_STORE_SIZE, SENSORLOG_STORE_SIZE);
	return sensorlog;
}

void read_all_sensors() {
	DEBUG_PRINTLN(F("read_all_sensors"));
	if (!sensors || os.status.network_fails>0 || os.iopts[IOPT_REMOTE_EXT_MODE]) return;

	ulong time = os.now_tz();
	Sensor_t *sensor = sensors;

	while (sensor) {
		if (time >= sensor->last_read + sensor->read_interval) {
			if (read_sensor(sensor) == HTTP_RQT_SUCCESS) {
				sensorlog_add(sensor, time);
			}
		}
		sensor = sensor->next;
	}
	sensor_update_groups();
}

#if defined(ESP8266)
/**
 * Read ADS1015 sensors
*/
int read_sensor_adc(Sensor_t *sensor) {
	DEBUG_PRINTLN(F("read_sensor_adc"));
	if (!sensor || !sensor->flags.enable) return HTTP_RQT_NOT_RECEIVED;
	if (sensor->id >= 8) return HTTP_RQT_NOT_RECEIVED;
	//Init + Detect:

	int port = sensor->id < 4? 72 : 73;
	int id = sensor->id % 4;

	ADS1015 adc(port);
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

	switch(sensor->type) {
		case SENSOR_SMT50_MOIS:  // SMT50 VWC [%] = (U * 50) : 3
			sensor->last_data = (sensor->last_data * 50.0) / 3.0;
			break;
		case SENSOR_SMT50_TEMP:  // SMT50 T [°C] = (U – 0,5) * 100
			sensor->last_data = (sensor->last_data - 0.5) * 100.0;
			break;
		case SENSOR_ANALOG_EXTENSION_BOARD_P: // 0..3,3V -> 0..100%
			sensor->last_data = sensor->last_data * 100.0 / 3.3;
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

extern byte findKeyVal (const char *str,char *strbuf, uint16_t maxlen,const char *key,bool key_in_pgm=false,uint8_t *keyfound=NULL);

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

	char *p = tmp_buffer;
	BufferFiller bf = p;

	bf.emit_p(PSTR("GET /sg?pw=$O&nr=$D"),
		SOPT_PASSWORD, sensor->id);
	bf.emit_p(PSTR(" HTTP/1.0\r\nHOST: $D.$D.$D.$D\r\n\r\n"),
		ip[0],ip[1],ip[2],ip[3]);

	if (os.send_http_request(sensor->ip, sensor->port, p) == HTTP_RQT_SUCCESS) {
					
		if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("nativedata"), true)) {
			sensor->last_native_data = strtoul(tmp_buffer, NULL, 0);
		}
		if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("data"), true)) {
			sensor->last_data = atof(tmp_buffer);
			sensor->flags.data_ok = true;
		}
		if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("unitid"), true)) {
			sensor->unitid = strtoul(tmp_buffer, NULL, 0);
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
					sensor->flags.data_ok = true;
					DEBUG_PRINT(F(" soil moisture %: "));
					break;
				case SENSOR_SMT100_MODBUS_RTU_TEMP:	//Equation: temperature [°C]= (16Bit_temperature_value / 100)-100
					sensor->last_data = ((double)sensor->last_native_data / 100) - 100;
					sensor->flags.data_ok = true;
					DEBUG_PRINT(F(" temperature °C: "));
					break;
			}
			DEBUG_PRINTLN(sensor->last_data);
			return HTTP_RQT_SUCCESS;
	}

	return HTTP_RQT_NOT_RECEIVED;
}

/**
 * read a sensor
*/
int read_sensor(Sensor_t *sensor) {

	if (!sensor || !sensor->flags.enable)
		return HTTP_RQT_NOT_RECEIVED;

	sensor->last_read = os.now_tz();

	DEBUG_PRINT(F("Reading sensor "));
	DEBUG_PRINTLN(sensor->name);

	switch(sensor->type)
	{
		case SENSOR_SMT100_MODBUS_RTU_MOIS:
		case SENSOR_SMT100_MODBUS_RTU_TEMP:
			return read_sensor_ip(sensor);

#if defined(ESP8266)
		case SENSOR_ANALOG_EXTENSION_BOARD:
		case SENSOR_ANALOG_EXTENSION_BOARD_P:
		case SENSOR_SMT50_MOIS: //SMT50 VWC [%] = (U * 50) : 3
		case SENSOR_SMT50_TEMP: //SMT50 T [°C] = (U – 0,5) * 100
			return read_sensor_adc(sensor);
#endif

		//case SENSOR_OSPI_ANALOG_INPUTS:
		//	return read_sensor_ospi(sensor);
		case SENSOR_REMOTE:
			return read_sensor_http(sensor);

		default: return HTTP_RQT_NOT_RECEIVED;
	}
}

/**
 * @brief Update group values
 * 
 */
void sensor_update_groups() {
	Sensor_t *sensor = sensors;	

	while (sensor) {
		switch(sensor->type) {
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
						switch(sensor->type) {
							case SENSOR_GROUP_MIN:
								if (n++ == 0) value = group->last_data;
								else if (group->last_data < value) value = group->last_data;
								break;
							case SENSOR_GROUP_MAX:
								if (n++ == 0) value = group->last_data;
								else if (group->last_data > value) value = group->last_data;
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
				if (sensor->type == SENSOR_GROUP_AVG && n>0) {
					value = value / n;
				}
				sensor->last_data = value;
				sensor->last_native_data = 0;
				sensor->last_read = os.now_tz();
				sensor->flags.data_ok = n>0;
				break;
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
		if (p->prog == prog) {
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
	if (!progSensorAdjusts && file_exists(PROG_SENSOR_FILENAME))
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
#endif
		case SENSOR_OSPI_ANALOG_INPUTS: 	  return UNIT_VOLT;

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
		case SENSOR_ANALOG_EXTENSION_BOARD_P: return UNIT_PERCENT;
		case SENSOR_SMT50_MOIS: 			  return UNIT_PERCENT;
		case SENSOR_SMT50_TEMP: 			  return UNIT_DEGREE;
#endif
		case SENSOR_OSPI_ANALOG_INPUTS: 	  return UNIT_VOLT;
		case SENSOR_REMOTE:                	  return sensor->unitid;

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
