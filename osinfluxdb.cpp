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
#include "OpenSprinkler.h"

extern char tmp_buffer[TMP_BUFFER_SIZE*2];
extern OpenSprinkler os;

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
    remove_file(INFLUX_CONFIG_FILE);
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
    remove_file(INFLUX_CONFIG_FILE);
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

void OSInfluxDB::get_influx_config(char *json) {
    json[0] = 0;
    if (file_exists(INFLUX_CONFIG_FILE))
    {
        ulong size = file_read_block(INFLUX_CONFIG_FILE, json, 0, TMP_BUFFER_SIZE_L);
        //DEBUG_PRINT(F("influx config size="));
        //DEBUG_PRINTLN(size);
        json[size] = 0;
        //DEBUG_PRINT(F("influx config="));
        //DEBUG_PRINTLN(tmp_buffer);
        if (size > 10 && (json[size-2] != '"' || json[size-1] != '}')) {
            strcpy(json, "{\"en\":0}");
            set_influx_config(json);
        }
    }
    if (json[0] != '{' || strchr(json, '}') != strrchr(json, '}')) {
        strcpy(json, "{\"en\":0}");
        set_influx_config(json);
    }
}

void OSInfluxDB::get_influx_config(ArduinoJson::JsonDocument &doc) {
    //DEBUG_PRINTLN("Load influx config");
    get_influx_config(tmp_buffer);
    ArduinoJson::DeserializationError error = ArduinoJson::deserializeJson(doc, tmp_buffer);
	if (error || doc.isNull() || !doc.containsKey("en")) {
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
        set_influx_config(doc);
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
        if (doc["en"] == 0)
            return;
        
        //InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
        String url = doc["url"];
        int port = doc["port"];
        if (port == 0)
            port = 8086;
        url = url + ":" + port;
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


#if defined(ESP8266) 
void OSInfluxDB::influxdb_send_state(const char *name, int state) {
	char tmp[TMP_BUFFER_SIZE];
    Point data("opensprinkler");
    os.sopt_load(SOPT_DEVICE_NAME, tmp);
    data.addTag("devicename", tmp);
	data.addTag("name", name);
	data.addField("state", state);
	write_influx_data(data);
}

void OSInfluxDB::influxdb_send_station(const char *name, uint32_t station, int state) {
    Point data("opensprinkler");
	char tmp[TMP_BUFFER_SIZE];
    os.sopt_load(SOPT_DEVICE_NAME, tmp);
    data.addTag("devicename", tmp);
	data.addTag("name", name);
	data.addField("station", station);
	data.addField("state", state);
	write_influx_data(data);
}

void OSInfluxDB::influxdb_send_program(const char *name, uint32_t nr, float level) {
    Point data("opensprinkler");
	char tmp[TMP_BUFFER_SIZE];
    os.sopt_load(SOPT_DEVICE_NAME, tmp);
    data.addTag("devicename", tmp);
	data.addTag("name", name);
	data.addField("program", nr);
	data.addField("level", level);
	write_influx_data(data);
}

void OSInfluxDB::influxdb_send_flowsensor(const char *name, uint32_t count, float volume) {
    Point data("opensprinkler");
	char tmp[TMP_BUFFER_SIZE];
    os.sopt_load(SOPT_DEVICE_NAME, tmp);
    data.addTag("devicename", tmp);
	data.addTag("name", name);
	data.addField("count", count);
	data.addField("volume", volume);
	write_influx_data(data);
}

void OSInfluxDB::influxdb_send_flowalert(const char *name, uint32_t station, int f1, int f2, int f3, int f4, int f5) {
    Point data("opensprinkler");
	char tmp[TMP_BUFFER_SIZE];
    os.sopt_load(SOPT_DEVICE_NAME, tmp);
    data.addTag("devicename", tmp);
	data.addTag("name", name);
	data.addField("station", station);
	data.addField("flowrate", (double)(f1)+(double)(f2)/100);
	data.addField("duration", f3);
	data.addField("alert_setpoint", (double)(f4)+(double)(f5)/100);
	write_influx_data(data);
}

void OSInfluxDB::influxdb_send_warning(const char *name, uint32_t level, float value) {
    Point data("opensprinkler");
	char tmp[TMP_BUFFER_SIZE];
    os.sopt_load(SOPT_DEVICE_NAME, tmp);
    data.addTag("devicename", tmp);
	data.addTag("warning", name);
	data.addField("level", (int)level);
	data.addField("currentvalue", value);
	write_influx_data(data);
}

#else

void OSInfluxDB::influxdb_send_state(const char *name, int state) {
  influxdb_cpp::server_info * client = get_client();
  if (!client)
    return;
  char tmp[TMP_BUFFER_SIZE];
  os.sopt_load(SOPT_DEVICE_NAME, tmp);
  influxdb_cpp::builder()
    .meas("opensprinkler")
    .tag("devicename", tmp)
    .tag("name", name)
    .field("state", state)
    .timestamp(millis())
    .post_http(*client);
}

void OSInfluxDB::influxdb_send_station(const char *name, uint32_t station, int state) {
  influxdb_cpp::server_info * client = get_client();
  if (!client)
    return;

  char tmp[TMP_BUFFER_SIZE];
  os.sopt_load(SOPT_DEVICE_NAME, tmp);
  influxdb_cpp::builder()
    .meas("opensprinkler")
    .tag("devicename", tmp)
    .tag("name", name)
    .field("station", (int)station)
    .field("state", state)
    .timestamp(millis())
    .post_http(*client);
}

void OSInfluxDB::influxdb_send_program(const char *name, uint32_t nr, float level) {
  influxdb_cpp::server_info * client = get_client();
  if (!client)
    return;

  char tmp[TMP_BUFFER_SIZE];
  os.sopt_load(SOPT_DEVICE_NAME, tmp);
  influxdb_cpp::builder()
    .meas("opensprinkler")
    .tag("devicename", tmp)
    .tag("name", name)
    .field("program", (int)nr)
    .field("level", level)
    .timestamp(millis())
    .post_http(*client);
}

void OSInfluxDB::influxdb_send_flowsensor(const char *name, uint32_t count, float volume) {
  influxdb_cpp::server_info * client = get_client();
  if (!client)
    return;

  char tmp[TMP_BUFFER_SIZE];
  os.sopt_load(SOPT_DEVICE_NAME, tmp);
  influxdb_cpp::builder()
    .meas("opensprinkler")
    .tag("devicename", tmp)
    .tag("name", name)
    .field("count", (int)count)
    .field("volume", volume)
    .timestamp(millis())
    .post_http(*client);
}

void OSInfluxDB::influxdb_send_flowalert(const char *name, uint32_t station, int f1, int f2, int f3, int f4, int f5) {
  influxdb_cpp::server_info * client = get_client();
  if (!client)
    return;

  char tmp[TMP_BUFFER_SIZE];
  os.sopt_load(SOPT_DEVICE_NAME, tmp);
  influxdb_cpp::builder()
    .meas("opensprinkler")
    .tag("devicename", tmp)
    .tag("name", name)
    .field("station", (int)(station))
    .field("flowrate", (double)(f1)+(double)(f2)/100)
	.field("duration", f3)
	.field("alert_setpoint", (double)(f4)+(double)(f5)/100)
    .timestamp(millis())
    .post_http(*client);
}

void OSInfluxDB::influxdb_send_warning(const char *name, uint32_t level, float value) {
  influxdb_cpp::server_info * client = get_client();
  if (!client)
    return;

  char tmp[TMP_BUFFER_SIZE];
  os.sopt_load(SOPT_DEVICE_NAME, tmp);
  influxdb_cpp::builder()
    .meas("opensprinkler")
    .tag("devicename", tmp)
    .tag("warning", name)
    .field("level", (int)level)
    .field("currentvalue", value)
    .timestamp(millis())
    .post_http(*client);
}
#endif

void OSInfluxDB::push_message(uint16_t type, uint32_t lval, float fval, const char* sval) {
    if (!isEnabled())
        return;

    uint32_t volume;

   	switch(type) {
		case  NOTIFY_STATION_ON:
			influxdb_send_station("station", lval, 1);
			break;

		case NOTIFY_FLOW_ALERT:{
			//influxdb_send_flowalert("flowalert", lval, f1, f2, f3, f4, f5);
			break;
		}

		case NOTIFY_STATION_OFF:
			influxdb_send_station("station", lval, 0);
			break;

		case NOTIFY_PROGRAM_SCHED:
			influxdb_send_program("program sched", lval, fval);
			break;

		case NOTIFY_SENSOR1:
			influxdb_send_state("sensor1", (int)fval);
			break;

		case NOTIFY_SENSOR2:
			influxdb_send_state("sensor2", (int)fval);
			break;

		case NOTIFY_RAINDELAY:
			influxdb_send_state("raindelay", (int)fval);
			break;

		case NOTIFY_FLOWSENSOR:
			volume = os.iopts[IOPT_PULSE_RATE_1];
			volume = (volume<<8)+os.iopts[IOPT_PULSE_RATE_0];
			volume = lval*volume;
			influxdb_send_flowsensor("flowsensor", lval, (float)volume/100);
			break;

		case NOTIFY_WEATHER_UPDATE:
			influxdb_send_state("waterlevel", (int)fval);
			break;

		case NOTIFY_REBOOT:
			break;

		case NOTIFY_MONITOR_LOW: 
		case NOTIFY_MONITOR_MID:
		case NOTIFY_MONITOR_HIGH:
			influxdb_send_flowsensor(sval, lval, fval);
			break;

	}
}