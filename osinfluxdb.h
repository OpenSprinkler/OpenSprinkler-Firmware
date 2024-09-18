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
public:
    ~OSInfluxDB();
    void set_influx_config(int enabled, char *url, uint16_t port, char *org, char *bucket, char *token);
    void set_influx_config(ArduinoJson::JsonDocument &doc);
    void get_influx_config(ArduinoJson::JsonDocument &doc);
    bool isEnabled();
    #if defined(ESP8266) 
    void write_influx_data(Point &sensor_data);
    #else
    influxdb_cpp::server_info * get_client();
    #endif
};
#endif