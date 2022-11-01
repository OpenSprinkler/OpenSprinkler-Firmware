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
#include "sensors.h"
#include "program.h"
#include "OpenSprinkler.h"

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
int sensor_define(uint nr, char *name, uint type, uint group, uint32_t ip, uint port, uint id, uint ri, byte enable, byte log) {

	if (nr == 0 || type == 0)
		return HTTP_RQT_NOT_RECEIVED;

	Sensor_t *sensor = sensors;
	Sensor_t *last = NULL;
	while (sensor) {
		if (sensor->nr == nr) { //Modify existing
			sensor->type = type;
			sensor->group = group;
			strlcpy(sensor->name, name, sizeof(sensor->name)-1);
			sensor->ip = ip;
			sensor->port = port;
			sensor->id = id;
			sensor->read_interval = ri;
			sensor->enable = enable;
			sensor->log = log;
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
	strlcpy(new_sensor->name, name, sizeof(new_sensor->name)-1);
	new_sensor->ip = ip;
	new_sensor->port = port;
	new_sensor->id = id;
	new_sensor->read_interval = ri;
	new_sensor->enable = enable;
	new_sensor->log = log;
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
	DEBUG_PRINTLN("sensor_load");
	sensors = NULL;
	if (!file_exists(SENSOR_FILENAME))
		return;

	DEBUG_PRINTLN("sensor_load2");

	ulong pos = 0;
	Sensor_t *last = NULL;
	while (true) {
		Sensor_t *sensor = new Sensor_t;
		memset(sensor, 0, sizeof(Sensor_t));
		file_read_block (SENSOR_FILENAME, sensor, pos, SENSOR_STORE_SIZE);
		if (!sensor->nr || !sensor->type) {
			DEBUG_PRINTLN("sensor_load3");

			delete sensor;
			break;
		}
		if (!last) sensors = sensor;
		else last->next = sensor;
		last = sensor;
		sensor->next = NULL;
		pos += SENSOR_STORE_SIZE;
		DEBUG_PRINTLN("sensor_load4");
	}
	DEBUG_PRINTLN("sensor_load5");
}

/**
 * @brief Store sensor data
 * 
 */
void sensor_save() {
	DEBUG_PRINTLN("sensor_save1");
	if (!sensors && file_exists(SENSOR_FILENAME))
		remove_file(SENSOR_FILENAME);
	
	DEBUG_PRINTLN("sensor_save2");

	ulong pos = 0;
	Sensor_t *sensor = sensors;
	while (sensor) {
		DEBUG_PRINTLN("sensor_save3");
		file_write_block(SENSOR_FILENAME, sensor, pos, SENSOR_STORE_SIZE);
		sensor = sensor->next;
		pos += SENSOR_STORE_SIZE; 
		DEBUG_PRINTLN("sensor_save4");
	}
	DEBUG_PRINTLN("sensor_save5");
}

uint sensor_count() {
	Sensor_t *sensor = sensors;
	uint count = 0;
	while (sensor) {
		count++;
		sensor = sensor->next;
	}
	return count;
}

Sensor_t *sensor_by_nr(uint nr) {
	Sensor_t *sensor = sensors;
	while (sensor) {
		if (sensor->nr == nr) 
			return sensor;
		sensor = sensor->next;
	}
	return NULL;
}

Sensor_t *sensor_by_idx(uint idx) {
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

void sensorlog_add(SensorLog_t *sensorlog) {
	file_write_block(SENSORLOG_FILENAME, sensorlog, file_size(SENSORLOG_FILENAME), SENSORLOG_STORE_SIZE);
}

void sensorlog_add(Sensor_t *sensor, ulong time) {
	if (sensor->data_ok) {
		SensorLog_t sensorlog;
		sensorlog.nr = sensor->nr;
		sensorlog.time = time;
		sensorlog.native_data = sensor->last_native_data;
		sensorlog.data = sensor->last_data;
		sensorlog_add(&sensorlog);
	}
}


ulong sensorlog_size() {
	return file_size(SENSORLOG_FILENAME) / SENSORLOG_STORE_SIZE;
}

void sensorlog_clear_all() {
	remove_file(SENSORLOG_FILENAME);
}

SensorLog_t *sensorlog_load(ulong idx) {
	SensorLog_t *sensorlog = new SensorLog_t;
	return sensorlog_load(idx, sensorlog);
}

SensorLog_t *sensorlog_load(ulong idx, SensorLog_t* sensorlog) {
	file_read_block(SENSORLOG_FILENAME, sensorlog, idx * SENSORLOG_STORE_SIZE, SENSORLOG_STORE_SIZE);
	return sensorlog;
}

void read_all_sensors() {
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

int read_sensor(Sensor_t *sensor) {

	if (!sensor || !sensor->enable)
		return HTTP_RQT_NOT_RECEIVED;

	sensor->last_read = os.now_tz();

	DEBUG_PRINT(F("Reading sensor "));
	DEBUG_PRINTLN(sensor->name);

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

    IPAddress _ip(sensor->ip);
	byte ip[4] = {_ip[0], _ip[1], _ip[2], _ip[3]};

	if(!client->connect(ip, sensor->port)) {
		DEBUG_PRINT(F("Cannot connect to "));
		DEBUG_PRINT(_ip[0]); DEBUG_PRINT(".");
		DEBUG_PRINT(_ip[1]); DEBUG_PRINT(".");
		DEBUG_PRINT(_ip[2]); DEBUG_PRINT(".");
		DEBUG_PRINT(_ip[3]); DEBUG_PRINT(":");
		DEBUG_PRINTLN(sensor->port);
		client->stop();
		return HTTP_RQT_CONNECT_ERR;
	}

	uint8_t buffer[10];
	int len = 0;
	boolean addCrc16 = true;
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
			break;
 
		case SENSOR_SMT100_MODBUS_RTU_TEMP:
			buffer[0] = sensor->id;
			buffer[1] = 0x03; //Read Holding Registers
			buffer[2] = 0x00; 
			buffer[3] = 0x00; //temperature is at address 0x0000 
			buffer[4] = 0x00; 
			buffer[5] = 0x01; //Number of registers to read (soil moisture value is one 16-bit register)
			len = 6;
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
	client->flush();
	
	while (true) {
		if (client->available())
			break;
		if (os.now_tz() > sensor->last_read+SENSOR_READ_TIMEOUT) {
			client->stop();
			DEBUG_PRINT(F("Sensor "));
			DEBUG_PRINT(sensor->nr);
			DEBUG_PRINT(F(" timeout read!"));
			return HTTP_RQT_TIMEOUT;
		}
		delay(5);
	}

	//Read result:
	switch(sensor->type)
	{
		case SENSOR_SMT100_MODBUS_RTU_MOIS:
		case SENSOR_SMT100_MODBUS_RTU_TEMP:	
			int n = client->read(buffer, 7);
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
					sensor->data_ok = true;
					DEBUG_PRINT(F(" soil moisture %: "));
					break;
				case SENSOR_SMT100_MODBUS_RTU_TEMP:	//Equation: temperature [°C]= (16Bit_temperature_value / 100)-100
					sensor->last_data = ((double)sensor->last_native_data / 100) - 100;
					sensor->data_ok = true;
					DEBUG_PRINT(F(" temperature °C: "));
					break;
			}
			DEBUG_PRINTLN(sensor->last_data);
			return HTTP_RQT_SUCCESS;
	}

	return HTTP_RQT_NOT_RECEIVED;
}

void sensor_update_groups() {
	Sensor_t *sensor = sensors;	

	while (sensor) {
		switch(sensor->type) {
			case SENSOR_GROUP_MIN:
			case SENSOR_GROUP_MAX:
			case SENSOR_GROUP_AVG:
			case SENSOR_GROUP_SUM: {
				int nr = sensor->nr;
				Sensor_t *group = sensors;
				double value = 0;
				int n = 0;
				while (group) {
					if (group->nr != nr && group->group == nr && group->enable && group->data_ok) {
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
				sensor->data_ok = n>0;
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

 			IPAddress _ip(sensor->ip);
			byte ip[4] = {_ip[0], _ip[1], _ip[2], _ip[3]};

			if(!client->connect(ip, sensor->port)) {
				DEBUG_PRINT(F("Cannot connect to "));
				DEBUG_PRINT(_ip[0]); DEBUG_PRINT(".");
				DEBUG_PRINT(_ip[1]); DEBUG_PRINT(".");
				DEBUG_PRINT(_ip[2]); DEBUG_PRINT(".");
				DEBUG_PRINT(_ip[3]); DEBUG_PRINT(":");
				DEBUG_PRINTLN(sensor->port);
				client->stop();
				return HTTP_RQT_CONNECT_ERR;
			}

			client->write(buffer, len);
			client->flush();

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
			if (sensor && sensor->enable && sensor->data_ok) {

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

}

void prog_adjust_load() {

}

void prog_adjust_count() {
	
}
