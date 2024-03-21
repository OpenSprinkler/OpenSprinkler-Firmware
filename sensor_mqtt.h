/* OpenSprinkler Unified (AVR/RPI/BBB/LINUX) Firmware
 * Copyright (C) 2015 by Ray Wang (ray@opensprinkler.com)
 *
 * sensors header file
 * 2024 @ OpenSprinklerShop
 * Stefan Schmaltz (info@opensprinklershop.de)
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

#ifndef _SENSOR_MQTT_H
#define _SENSOR_MQTT_H

#include "sensors.h"

#if defined(ARDUINO)
void sensor_mqtt_callback(char* mtopic, byte* payload, unsigned int length);
#else
static void sensor_mqtt_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg);
#endif

void sensor_mqtt_init();
int read_sensor_mqtt(Sensor_t *sensor);

void sensor_mqtt_subscribe(uint nr, uint type, const char *urlstr);
void sensor_mqtt_unsubscribe(uint nr, uint type, const char *urlstr);

#endif // _SENSOR_MQTT_H
