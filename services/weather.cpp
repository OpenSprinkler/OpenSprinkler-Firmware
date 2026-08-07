/* OpenSprinkler Unified Firmware
 * Copyright (C) 2015 by Ray Wang (ray@opensprinkler.com)
 *
 * Weather functions
 * Feb 2015 @ OpenSprinkler.com
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

#include <stdlib.h>
#include "../OpenSprinkler.h"
#include "util/utils.h"
#include "../api/server.h"
#include "weather.h"
#include "../core/scheduler.h"
#include "../storage/logging.h"
#include "../types.h"
#include "external/ArduinoJson.hpp"

extern OpenSprinkler os; // OpenSprinkler object
extern char tmp_buffer[];
extern char ether_buffer[];
unsigned char md_scales[MAX_N_MD_SCALES];
unsigned char md_N = 0;
unsigned char mda = 0;
char wt_rawData[TMP_BUFFER_SIZE];
int wt_errCode = HTTP_RQT_NOT_RECEIVED;
unsigned char wt_monthly[12] = {100,100,100,100,100,100,100,100,100,100,100,100};
unsigned char wt_restricted = 0;

static float weather_sensor_values[static_cast<uint8_t>(WeatherAction::MAX_VALUE)] = {};
static uint16_t weather_sensor_valid = 0;
static uint16_t weather_sensor_requested_actions = 0;
static uint8_t weather_sensor_requested_groups = 0;
static uint8_t weather_sensor_received_groups = 0;
static uint32_t weather_sensor_group_updated[3] = {};
static uint32_t weather_sensor_next_request = 0;

extern const char *user_agent_string;

unsigned char parseMdScalesArray (const char* input);

static uint8_t weather_action_group(WeatherAction action) {
	if (action <= WeatherAction::CurrentRaining) return WEATHER_SENSOR_GROUP_CURRENT;
	if (action <= WeatherAction::ForecastPrecipitation) return WEATHER_SENSOR_GROUP_FORECAST;
	return WEATHER_SENSOR_GROUP_HISTORICAL;
}

static uint8_t weather_group_index(uint8_t group) {
	if (group == WEATHER_SENSOR_GROUP_CURRENT) return 0;
	if (group == WEATHER_SENSOR_GROUP_FORECAST) return 1;
	return 2;
}

static uint16_t weather_group_action_mask(uint8_t group) {
	uint16_t mask = 0;
	for (uint8_t i = 0; i < static_cast<uint8_t>(WeatherAction::MAX_VALUE); i++) {
		if (weather_action_group(static_cast<WeatherAction>(i)) == group) mask |= 1U << i;
	}
	return mask;
}

static void weather_sensor_store(ArduinoJson::JsonObject group, const char *key, WeatherAction action) {
	ArduinoJson::JsonVariant value = group[key];
	if (!value.is<float>()) return;
	uint8_t index = static_cast<uint8_t>(action);
	weather_sensor_values[index] = value.as<float>();
	weather_sensor_valid |= static_cast<uint16_t>(1U << index);
}

static bool weather_sensor_parse_group(ArduinoJson::JsonDocument &doc, const char *key, uint8_t group) {
	ArduinoJson::JsonObject values = doc[key].as<ArduinoJson::JsonObject>();
	if (values.isNull()) return false;

	weather_sensor_valid &= ~weather_group_action_mask(group);
	switch (group) {
		case WEATHER_SENSOR_GROUP_CURRENT:
			weather_sensor_store(values, "t", WeatherAction::CurrentTemperature);
			weather_sensor_store(values, "h", WeatherAction::CurrentHumidity);
			weather_sensor_store(values, "w", WeatherAction::CurrentWindSpeed);
			weather_sensor_store(values, "r", WeatherAction::CurrentRaining);
			break;
		case WEATHER_SENSOR_GROUP_FORECAST:
			weather_sensor_store(values, "lo", WeatherAction::ForecastLowTemperature);
			weather_sensor_store(values, "hi", WeatherAction::ForecastHighTemperature);
			weather_sensor_store(values, "p", WeatherAction::ForecastPrecipitation);
			break;
		case WEATHER_SENSOR_GROUP_HISTORICAL:
			weather_sensor_store(values, "t", WeatherAction::HistoricalTemperature);
			weather_sensor_store(values, "h", WeatherAction::HistoricalHumidity);
			weather_sensor_store(values, "p", WeatherAction::HistoricalPrecipitation);
			weather_sensor_store(values, "w", WeatherAction::HistoricalWindSpeed);
			weather_sensor_store(values, "sr", WeatherAction::HistoricalSolarRadiation);
			weather_sensor_store(values, "eto", WeatherAction::HistoricalETo);
			break;
	}
	weather_sensor_group_updated[weather_group_index(group)] = millis();
	weather_sensor_received_groups |= group;
	return true;
}

bool weather_sensor_parse_response(const char *buffer, uint8_t requested_groups,
	uint16_t requested_actions) {
	ArduinoJson::JsonDocument doc;
	ArduinoJson::DeserializationError error = ArduinoJson::deserializeJson(doc, buffer);
	if (error || doc["v"].as<uint8_t>() != 1 || strcmp(doc["u"] | "", "us") != 0) {
		DEBUG_PRINTLN(F("weather sensor: invalid response"));
		return false;
	}

	weather_sensor_received_groups = 0;
	if (requested_groups & WEATHER_SENSOR_GROUP_CURRENT) {
		weather_sensor_parse_group(doc, "c", WEATHER_SENSOR_GROUP_CURRENT);
	}
	if (requested_groups & WEATHER_SENSOR_GROUP_FORECAST) {
		weather_sensor_parse_group(doc, "f", WEATHER_SENSOR_GROUP_FORECAST);
	}
	if (requested_groups & WEATHER_SENSOR_GROUP_HISTORICAL) {
		weather_sensor_parse_group(doc, "h", WEATHER_SENSOR_GROUP_HISTORICAL);
	}
	bool groups_complete =
		(weather_sensor_received_groups & requested_groups) == requested_groups;
	bool actions_complete = !requested_actions ||
		(weather_sensor_valid & requested_actions) == requested_actions;
	return groups_complete && actions_complete;
}

static void weather_sensor_callback(char *buffer) {
	peel_http_header(buffer);
	weather_sensor_parse_response(buffer, weather_sensor_requested_groups,
		weather_sensor_requested_actions);
}

static uint8_t find_weather_sensor_groups() {
	uint8_t groups = 0;
	weather_sensor_requested_actions = 0;
	for (uint8_t i = 0; i < os.nsensors; i++) {
		if (!(os.sensors[i].flag & (1 << SENSOR_FLAG_ENABLE))) continue;
		Sensor *sensor = Sensor::get(i);
		if (!sensor || sensor->get_sensor_type() != SensorType::Weather) continue;
		WeatherAction action = static_cast<WeatherSensor *>(sensor)->action;
		if (action >= WeatherAction::MAX_VALUE) continue;
		groups |= weather_action_group(action);
		weather_sensor_requested_actions |=
			static_cast<uint16_t>(1U << static_cast<uint8_t>(action));
		// Make a newly configured sensor, or one whose shared data became stale,
		// retry on the next one-second poll after this service request.
		if (isnan(weather_sensor_get_value(action))) os.sensors[i].next_update = 0;
	}
	return groups;
}

void weather_sensor_reset_cache() {
	weather_sensor_valid = 0;
	weather_sensor_requested_actions = 0;
	weather_sensor_requested_groups = 0;
	weather_sensor_received_groups = 0;
	weather_sensor_next_request = 0;
	memset(weather_sensor_group_updated, 0, sizeof(weather_sensor_group_updated));
}

void weather_sensor_schedule_refresh() {
	weather_sensor_next_request = 0;
}

float weather_sensor_get_value(WeatherAction action) {
	if (action >= WeatherAction::MAX_VALUE) return NAN;
	uint8_t index = static_cast<uint8_t>(action);
	if (!(weather_sensor_valid & (1U << index))) return NAN;
	uint8_t group_index = weather_group_index(weather_action_group(action));
	uint32_t updated = weather_sensor_group_updated[group_index];
	if (!updated || (uint32_t)(millis() - updated) > WEATHER_SENSOR_STALE_MS) return NAN;
	return weather_sensor_values[index];
}

void CheckWeatherSensors() {
	uint32_t now_ms = millis();
	if (weather_sensor_next_request && (int32_t)(now_ms - weather_sensor_next_request) < 0) return;

	weather_sensor_requested_groups = find_weather_sensor_groups();
	if (!weather_sensor_requested_groups) {
		weather_sensor_next_request = now_ms + WEATHER_SENSOR_REFRESH_MS;
		return;
	}

	char scope[4];
	uint8_t scope_len = 0;
	if (weather_sensor_requested_groups & WEATHER_SENSOR_GROUP_CURRENT) scope[scope_len++] = 'c';
	if (weather_sensor_requested_groups & WEATHER_SENSOR_GROUP_FORECAST) scope[scope_len++] = 'f';
	if (weather_sensor_requested_groups & WEATHER_SENSOR_GROUP_HISTORICAL) scope[scope_len++] = 'h';
	scope[scope_len] = 0;

	// urlEncode() can expand each input byte to three bytes. Reserve enough
	// capacity for that worst case plus the HTTP headers appended below.
	BufferFiller bf = BufferFiller(ether_buffer, (ETHER_BUFFER_SIZE - 256) / 3);
	bf.emit_p(PSTR("GET /weatherSensorData?loc=$O&wto=$O&scope=$S"),
		SOPT_LOCATION, SOPT_WEATHER_OPTS, scope);
	urlEncode(ether_buffer);

	char *host = tmp_buffer;
	os.sopt_load(SOPT_WEATHERURL, host);
	char *host_start = host;
	bool use_ssl = true;
	uint16_t port = 443;
	if (strncmp_P(host, PSTR("http://"), 7) == 0) {
		use_ssl = false;
		port = 80;
		host_start = host + 7;
	} else if (strncmp_P(host, PSTR("https://"), 8) == 0) {
		host_start = host + 8;
	}
	char *colon = strchr(host_start, ':');
	if (colon) {
		*colon = 0;
		port = static_cast<uint16_t>(atoi(colon + 1));
	}

	size_t used = strlen(ether_buffer);
	snprintf(ether_buffer + used, ETHER_BUFFER_SIZE - used,
		" HTTP/1.0\r\nHOST: %s\r\nUser-Agent: %s\r\nConnection: close\r\n\r\n",
		host_start, user_agent_string);

	weather_sensor_received_groups = 0;
	int ret = os.send_http_request(host_start, port, ether_buffer, weather_sensor_callback, use_ssl);
	bool complete = ret == HTTP_RQT_SUCCESS &&
		(weather_sensor_received_groups & weather_sensor_requested_groups) == weather_sensor_requested_groups &&
		(weather_sensor_valid & weather_sensor_requested_actions) == weather_sensor_requested_actions;
	weather_sensor_next_request = millis() + (complete ? WEATHER_SENSOR_REFRESH_MS : WEATHER_SENSOR_RETRY_MS);
}

// The weather function calls getweather.py on remote server to retrieve weather data
// the default script is WEATHER_SCRIPT_HOST/weather?.py
//static char website[] PROGMEM = DEFAULT_WEATHER_URL ;

static void getweather_callback(char* buffer) {
	char *p = buffer;
	DEBUG_PRINTLN(p);
	/* scan the buffer until the first & symbol */
	while(*p && *p!='&') {
		p++;
	}
	if (*p != '&')	return;
	int v;
	bool save_nvdata = false;
	time_os_t tnow = os.now_tz();
	// first check errCode, only update lswc timestamp if errCode is 0
	if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("errCode"), true)) {
		wt_errCode = atoi(tmp_buffer);
		if(wt_errCode==0) os.checkwt_success_lasttime = tnow;
	}

	// then only parse scale if errCode is 0
	if (wt_errCode==0 && findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("scale"), true)) {
		v = atoi(tmp_buffer);
		if (v>=0 && v<=250 && v != os.iopts[IOPT_WATER_PERCENTAGE]) {
			// only save if the value has changed
			os.iopts[IOPT_WATER_PERCENTAGE] = v;
			os.iopts_save();
			os.weather_update_flag |= WEATHER_UPDATE_WL;
		}
	}

	if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("restricted"), true)) {
		wt_restricted = atoi(tmp_buffer);
	} else {
		wt_restricted = 0;
	}

	if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("sunrise"), true)) {
		v = atoi(tmp_buffer);
		if (v>=0 && v<=1440 && (uint16_t)v != os.nvdata.sunrise_time) {
			os.nvdata.sunrise_time = v;
			save_nvdata = true;
			os.weather_update_flag |= WEATHER_UPDATE_SUNRISE;
		}
	}

	if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("sunset"), true)) {
		v = atoi(tmp_buffer);
		if (v>=0 && v<=1440 && (uint16_t)v != os.nvdata.sunset_time) {
			os.nvdata.sunset_time = v;
			save_nvdata = true;
			os.weather_update_flag |= WEATHER_UPDATE_SUNSET;
		}
	}

	if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("eip"), true)) {
		uint32_t l = strtoul(tmp_buffer, NULL, 0);
		if(l != os.nvdata.external_ip) {
			os.nvdata.external_ip = l;
			save_nvdata = true;
			os.weather_update_flag |= WEATHER_UPDATE_EIP;
		}
	}

	if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("tz"), true)) {
		v = atoi(tmp_buffer);
		if (v>=0 && v<= 108) {
			if (v != os.iopts[IOPT_TIMEZONE]) {
				// if timezone changed, save change and force ntp sync
				os.iopts[IOPT_TIMEZONE] = v;
				os.iopts_save();
				os.weather_update_flag |= WEATHER_UPDATE_TZ;
			}
		}
	}

	if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("rd"), true)) {
		v = atoi(tmp_buffer);
		if (v>0) {
			os.nvdata.rd_stop_time = tnow + (uint32_t) v * 3600;
			os.raindelay_start();
		} else if (v==0) {
			os.raindelay_stop();
		}
	}

	if (findKeyVal(p, wt_rawData, TMP_BUFFER_SIZE, PSTR("rawData"), true)) {
		wt_rawData[TMP_BUFFER_SIZE-1]=0;  // make sure the buffer ends properly
	}

	#define _STR_SCALES_SIZE (MAX_N_MD_SCALES*4+4)
	char _str_scales[_STR_SCALES_SIZE];
	if (wt_errCode==0) {
		if (findKeyVal(p, _str_scales, _STR_SCALES_SIZE, PSTR("scales"), true)) {
			parseMdScalesArray(_str_scales);
		} else {
			md_N = 0; // clear the wt_scales array
		}
	}

	if(save_nvdata) os.nvdata_save();
	write_log(LOGDATA_WATERLEVEL, os.checkwt_success_lasttime);
}

static void getweather_callback_with_peel_header(char* buffer) {
	peel_http_header(buffer);
	getweather_callback(buffer);
}

void GetWeather() {
	if(!os.network_connected()) return;
	// use temp buffer to construct get command
	BufferFiller bf = BufferFiller(tmp_buffer, TMP_BUFFER_ALLOC_SIZE);
	int method = os.iopts[IOPT_USE_WEATHER];
	// use manual adjustment call for monthly adjustment -- a bit ugly, but does not involve weather server changes
	if(method==WEATHER_METHOD_MONTHLY) method=WEATHER_METHOD_MANUAL;
	bf.emit_p(PSTR("$D?loc=$O&wto=$O&fwv=$D"),
								method,
								SOPT_LOCATION,
								SOPT_WEATHER_OPTS,
								(int)os.iopts[IOPT_FW_VERSION]);


	urlEncode(tmp_buffer);

	strcpy(ether_buffer, "GET /");
	strcat(ether_buffer, tmp_buffer);
	// because we are using tmp_buffer both for encoding the string
	// and for loading weather url, we will load weather url AFTER
	// the encoded string has been copied into ether_buffer

	// load weather url to tmp_buffer
	char *host = tmp_buffer;
	os.sopt_load(SOPT_WEATHERURL, host);

	// Parse protocol and extract host/port
	char *host_start = host;

	bool use_ssl = true;  // default to https
	int port = 443;       // default to https port

	// Check for http:// or https://
	if (strncmp_P(host, PSTR("http://"), 7) == 0) {
		use_ssl = false;
		port = 80;
		host_start = host + 7;
	} else if (strncmp_P(host, PSTR("https://"), 8) == 0) {
		use_ssl = true;
		port = 443;
		host_start = host + 8;
	}

	// Check for explicit port number
	char *colon = strchr(host_start, ':');
	if (colon) {
		*colon = '\0';  // null-terminate hostname
		port = atoi(colon + 1);
	}

	strcat(ether_buffer, " HTTP/1.0\r\nHOST: ");
	strcat(ether_buffer, host_start);
	strcat(ether_buffer, "\r\nUser-Agent: ");
	strcat(ether_buffer, user_agent_string);
	strcat(ether_buffer, "\r\n\r\n");

	wt_errCode = HTTP_RQT_NOT_RECEIVED;
	DEBUG_PRINT(ether_buffer);
	int ret = os.send_http_request(host_start, port, ether_buffer, getweather_callback_with_peel_header, use_ssl);
	if(ret!=HTTP_RQT_SUCCESS) {
		if(wt_errCode < 0) wt_errCode = ret;
		// if wt_errCode > 0, the call is successful but weather script may return error
	}
}

void parse_wto(char* wto) {
	// reset variables to default values before parsing
	mda = 0;
	if(*(wto+1)){
		// Wrap in curly braces
		wto[0] = '{';
		int len = strlen(wto);
		wto[len] = '}';
		wto[len+1] = 0;

		ArduinoJson::JsonDocument doc;
		ArduinoJson::DeserializationError error = ArduinoJson::deserializeJson(doc, wto);

		// Test and parse
		if (error) {
			DEBUG_PRINT(F("wto: deserializeJson() failed: "));
			DEBUG_PRINTLN(error.c_str());
		} else {
			if(doc.containsKey("scales")){
				for(unsigned char i=0;i<12;i++){
					int p=doc["scales"][i];
					p = (p<0) ? 0 : ((p>250) ? 250 : p); // clamp to [0, 250]
					wt_monthly[i]=p;
				}
			}
			if(doc.containsKey("mda")){
				mda = doc["mda"];
			}
		}
	}
}

void apply_monthly_adjustment(time_os_t curr_time) {
	// ====== Check monthly water percentage ======
	if(os.iopts[IOPT_USE_WEATHER]==WEATHER_METHOD_MONTHLY) {
	#if defined(ARDUINO)
		unsigned char m = month(curr_time)-1;
#else
		time_t _ct = curr_time;
		struct tm *ti = gmtime(&_ct);
		unsigned char m = ti->tm_mon;  // tm_mon ranges from [0,11]
#endif
		if(os.iopts[IOPT_WATER_PERCENTAGE]!=wt_monthly[m]) {
			os.iopts[IOPT_WATER_PERCENTAGE]=wt_monthly[m];
			os.iopts_save();
			os.weather_update_flag |= WEATHER_UPDATE_WL;
		}
	}
}

unsigned char parseMdScalesArray (const char* input) {
	if (!input) return 0;
	const char* p = input;
	if (*p != '[') return 0;   // must start with [
	++p;                       // skip '['

	DEBUG_PRINT("parseing md_scales:[");
	int count = 0;
	while (*p && *p != ']' && count < MAX_N_MD_SCALES) {
		char* endptr;
		int32_t val = (int32_t)strtol(p, &endptr, 10);   // parse number
		md_scales[count] = static_cast<unsigned char>(val);
		DEBUG_PRINT(md_scales[count]);
		DEBUG_PRINT(",");
		count++;
		p = endptr;
		if (*p == ',') {
			++p;  // skip comma
		}
	}
	md_N = count;
	DEBUG_PRINT("],total=");
	DEBUG_PRINTLN(md_N);
	return count;
}
