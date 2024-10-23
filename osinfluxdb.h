/* OpenSprinkler Unified (AVR/RPI/BBB/LINUX/ESP8266) Firmware
 * Copyright (C) 2024 by Stefan Schmaltz (info@opensprinklershop.de)
 *
 * OpenSprinkler library header file
 * Sep 2024 @ OpenSprinklerShop.de
 *
 * This file is part of the OpenSprinkler library
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

#ifndef _OSINFLUX_H
#define _OSINFLUX_H
#include "ArduinoJson.hpp"
#if defined(ESP8266) 
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#else
#include "influxdb.hpp"

#endif

class OSInfluxDB {
private:
    #if defined(ESP8266) 
    InfluxDBClient * client;
    #else
    influxdb_cpp::server_info * client;
    #endif
    bool enabled;
    bool initialized;
    void init();
    void influxdb_send_state(const char *name, int state);
    void influxdb_send_station(const char *name, uint32_t station, int state);
    void influxdb_send_program(const char *name, uint32_t nr, float level);
    void influxdb_send_flowsensor(const char *name, uint32_t count, float volume);
    void influxdb_send_flowalert(const char *name, uint32_t station, int f1, int f2, int f3, int f4, int f5);
    void influxdb_send_warning(const char *name, uint32_t level, float value);

public:
    ~OSInfluxDB();
    void set_influx_config(int enabled, char *url, uint16_t port, char *org, char *bucket, char *token);
    void set_influx_config(ArduinoJson::JsonDocument &doc);
    void set_influx_config(const char *json);
    void get_influx_config(ArduinoJson::JsonDocument &doc);
    void get_influx_config(char *json);
    bool isEnabled();
    #if defined(ESP8266) 
    void write_influx_data(Point &sensor_data);
    #else
    influxdb_cpp::server_info * get_client();
    #endif
    void push_message(uint16_t type, uint32_t lval, float fval, const char* sval);
};
#endif