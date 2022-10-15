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
#include "OpenSprinkler.h"

uint16_t CRC16 (const uint8_t *nData, uint16_t wLength)
{
    static const uint16_t wCRCTable[] = {
    0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
    0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
    0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
    0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
    0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
    0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
    0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
    0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
    0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
    0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
    0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
    0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
    0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
    0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
    0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
    0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
    0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
    0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
    0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
    0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
    0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
    0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
    0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
    0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
    0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
    0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
    0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
    0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
    0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
    0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
    0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
    0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040 };

    uint8_t nTemp;
    uint16_t wCRCWord = 0xFFFF;

    while (wLength--)
    {
        nTemp = *nData++ ^ wCRCWord;
        wCRCWord >>= 8;
        wCRCWord  ^= wCRCTable[(nTemp & 0xFF)];
    }
    return wCRCWord;
} // End: CRC16

/**
 * @brief delete a sensor
 * 
 * @param nr 
 */
void sensor_delete(uint nr) {

	Sensor_t *sensor = sensors;
	Sensor_t *last = NULL;
	while (sensor) {
		if (sensor->nr == nr) {
			if (last == NULL)
				sensors = sensor->next;
			else
				last->next = sensor->next;
			delete sensor;
			return;
		}
		last = sensor;
		sensor = sensor->next;
	}

	sensor_save();
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
void sensor_define(uint nr, char *name, uint type, uint group, uint32_t ip, uint port, uint id, uint ri, byte enable, byte log) {

	if (nr == 0 || type == 0)
		return;

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
			return;
		}

		if (sensor->nr > nr)
			break;

		last = sensor;
		sensor = sensor->next;
	}

	//Insert new
	Sensor_t *new_sensor = new Sensor_t;
	memset(new_sensor, 0, sizeof(Sensor_t));
	new_sensor->type = type;
	new_sensor->group = group;
	strlcpy(sensor->name, name, sizeof(sensor->name)-1);
	new_sensor->ip = ip;
	new_sensor->port = port;
	new_sensor->id = id;
	new_sensor->read_interval = ri;
	new_sensor->enable = enable;
	new_sensor->log = log;
	if (last == NULL) {
		sensors = new_sensor;
		new_sensor->next = sensor;
	} else {
		new_sensor->next = last->next;
		last->next = new_sensor;
	}

	sensor_save();
}

/**
 * @brief initial load stored sensor definitions
 * 
 */
void sensor_load() {

	sensors = NULL;
	if (!file_exists(SENSOR_FILENAME))
		return;

	ulong pos = 0;
	Sensor_t *last = NULL;
	while (true) {
		Sensor_t *sensor = new Sensor_t;
		memset(sensor, 0, sizeof(Sensor_t));
		file_read_block (SENSOR_FILENAME, sensor, pos, SENSOR_STORE_SIZE);
		if (sensor->nr == 0) {
			delete sensor;
			return;
		}
		if (last == NULL) {
			sensors = sensor;
			last = sensor;
		}
		else {
			last->next = sensor;
			last = sensor;
		}
		sensor->next = NULL;
		pos += SENSOR_STORE_SIZE;
	}

}

/**
 * @brief Store sensor data
 * 
 */
void sensor_save() {
	if (sensors == NULL && file_exists(SENSOR_FILENAME))
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

uint64_t sensorlog_size() {
	return file_size(SENSORLOG_FILENAME) / SENSORLOG_STORE_SIZE;
}

void sensorlog_clear_all() {
	remove_file(SENSORLOG_FILENAME);
}

SensorLog_t *sensorlog_load(ulong idx) {
	SensorLog_t *sensorlog = new SensorLog_t;
	file_read_block(SENSORLOG_FILENAME, sensorlog, idx * SENSORLOG_STORE_SIZE, SENSORLOG_STORE_SIZE);
	return sensorlog;
}

int read_sensor(Sensor_t *sensor, uint32_t *result) {

	if (!sensor || !sensor->enable)
		return HTTP_RQT_NOT_RECEIVED;

	sensor->last_read = millis();

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

	uint8_t buffer[50];
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
		buffer[len+0] = (0xFF00&crc)>>8;
		buffer[len+1] = (0x00FF&crc);
		len += 2;
	}

	client->write(buffer, len);
	client->flush();

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
			uint16_t crc = (buffer[5] << 8) | buffer[6];
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
					sensor->last_data = (sensor->last_native_data / 100);
					DEBUG_PRINT(F(" soil moisture %: "));
					break;
				case SENSOR_SMT100_MODBUS_RTU_TEMP:	//Equation: temperature [°C]= (16Bit_temperature_value / 100)-100
					sensor->last_data = (sensor->last_native_data / 100) - 100;
					DEBUG_PRINT(F(" temperature °C: "));
					break;
			}
			DEBUG_PRINT(sensor->last_data);
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
					if (group->nr != nr && group->group == nr && group->enable) {
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
				sensor->last_read = millis();
				break;
			}
		}

		sensor = sensor->next;
	}
}
