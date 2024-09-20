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

#include "osinfluxdb.h"
#include "utils.h"
#include "defines.h"

extern char tmp_buffer[TMP_BUFFER_SIZE*2];

#define INFLUX_CONFIG_FILE "influx.json"

void OSInfluxDB::set_influx_config(int enabled, char *url, uint16_t port, char *org, char *bucket, char *token) {
    ArduinoJson::JsonDocument doc;
    doc["en"] = enabled;
    doc["url"] = url;
    doc["port]"] = port;
    doc["org"] = org;
    doc["bucket"] = bucket;
    doc["token"] = token;
    set_influx_config(doc);
}

void OSInfluxDB::set_influx_config(ArduinoJson::JsonDocument &doc) {
    size_t size = ArduinoJson::serializeJson(doc, tmp_buffer);
    file_write_block(INFLUX_CONFIG_FILE, tmp_buffer, 0, size);
    if (client)
    {
        delete client;
        client = NULL;
    }
    enabled = doc["en"];
    initialized = true;
}

void OSInfluxDB::set_influx_config(const char *data) {
    size_t size = strlen(data);
    file_write_block(INFLUX_CONFIG_FILE, "{", 0, 1);
    file_write_block(INFLUX_CONFIG_FILE, data, 1, size);
    file_write_block(INFLUX_CONFIG_FILE, "}", size+1, 1);
    if (client)
    {
        delete client;
        client = NULL;
    }
    enabled = false;
    initialized = false;
}

void OSInfluxDB::get_influx_config(ArduinoJson::JsonDocument &doc) {
    //DEBUG_PRINTLN("Load influx config");
    if (file_exists(INFLUX_CONFIG_FILE))
    {
        ulong size = file_read_block(INFLUX_CONFIG_FILE, tmp_buffer, 0, TMP_BUFFER_SIZE*2);
        //DEBUG_PRINT(F("influx config size="));
        //DEBUG_PRINTLN(size);
        tmp_buffer[size] = 0;
        //DEBUG_PRINT(F("influx config="));
        //DEBUG_PRINTLN(tmp_buffer);
    }
    ArduinoJson::DeserializationError error = ArduinoJson::deserializeJson(doc, tmp_buffer);
	if (error || doc.isNull() || doc["en"] > 1) {
        if (error) {
            DEBUG_PRINT(F("influxdb: deserializeJson() failed: "));
		    DEBUG_PRINTLN(error.c_str());  
        }
        doc["en"] = 0;
		doc["url"] = "";
        doc["port"] = 8086;
		doc["org"] = "";
		doc["bucket"] = "";
		doc["token"] = "";
    }
    enabled = doc["en"];
    initialized = true; 
}

void OSInfluxDB::init() {
    ArduinoJson::JsonDocument doc;
    get_influx_config(doc);
    enabled = doc["en"];
    initialized = true; 
}

boolean OSInfluxDB::isEnabled() {
    if (!initialized) {
        init();
    }
    return enabled; 
}

#if defined(ESP8266)
OSInfluxDB::~OSInfluxDB() {
    if (client) delete client;
}

void OSInfluxDB::write_influx_data(Point &sensor_data) {
    if (!initialized) init();
    if (!enabled) 
        return;

    if (!client) {
        if (!file_exists(INFLUX_CONFIG_FILE))
            return;

        //Load influx config:
        ArduinoJson::JsonDocument doc; 
        get_influx_config(doc);
        if (doc[en] == 0)
            return;
        
        //InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
        String url = doc["url"];
        int port = doc["port"];
        if (port == 0)
            port = 8086;
        url += ":"+port;
        client = new InfluxDBClient(url, doc["org"], doc["bucket"], doc["token"], InfluxDbCloud2CACert);
    }

    if (client) {
       // Print what are we exactly writing
        DEBUG_PRINT("Writing: ");
        DEBUG_PRINTLN(sensor_data.toLineProtocol());
  
        // Write point
        if (!client->writePoint(sensor_data)) {
            DEBUG_PRINT("influxdb write failed: ");
            DEBUG_PRINTLN(client->getLastErrorMessage());
        }     
    }
}

#else
OSInfluxDB::~OSInfluxDB() {
    if (client) delete client;
}

influxdb_cpp::server_info *OSInfluxDB::get_client() {
    if (!initialized) init();
    if (!enabled) 
        return NULL;
    if (!client) {
        //Load influx config:
        ArduinoJson::JsonDocument doc; 
        get_influx_config(doc);
        if (doc["en"] == 0)
            return NULL;
        client = new influxdb_cpp::server_info(doc["url"], doc["port"], doc["bucket"], "", "", "ms", doc["token"]);
    }
    return client;
}

#endif