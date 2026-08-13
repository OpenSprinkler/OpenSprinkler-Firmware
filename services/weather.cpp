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
static uint8_t weather_sensor_stale_groups = 0;
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

class WeatherRequestWriter {
public:
	WeatherRequestWriter(char *buffer, size_t capacity) :
		buffer(buffer), capacity(capacity), used(0), good(buffer && capacity) {
		if (good) buffer[0] = 0;
	}

	bool append(const char *text) {
		if (!text) return fail();
		while (*text) {
			if (!append_char(*text++)) return false;
		}
		return true;
	}

	bool append_url_encoded(const char *text) {
		if (!text) return fail();
		static const char hex[] = "0123456789ABCDEF";
		while (*text) {
			uint8_t c = static_cast<uint8_t>(*text++);
			if (c == ' ' || c == '"' || c == '\'' || c == '<' || c == '>' || c > 127) {
				if (!append_char('%') || !append_char(hex[(c >> 4) & 0x0F]) ||
					!append_char(hex[c & 0x0F])) return false;
			} else if (!append_char(static_cast<char>(c))) {
				return false;
			}
		}
		return true;
	}

	bool ok() const { return good; }

private:
	bool append_char(char c) {
		if (!good || used + 1 >= capacity) return fail();
		buffer[used++] = c;
		buffer[used] = 0;
		return true;
	}

	bool fail() {
		good = false;
		if (buffer && capacity) buffer[capacity - 1] = 0;
		return false;
	}

	char *buffer;
	size_t capacity;
	size_t used;
	bool good;
};

static void load_weather_option(uint8_t oid, char *buffer, uint16_t maxlen) {
	os.sopt_load(oid, buffer, maxlen);
}

bool weather_build_http_request(char *output, size_t output_size,
	char *scratch, size_t scratch_size, const char *endpoint,
	const char *extra_query, const char *user_agent,
	weather_option_loader_t option_loader, WeatherHttpTarget *target) {
	if (!output || !scratch || scratch_size < 2 || !endpoint || !extra_query ||
		!user_agent || !option_loader || !target) return false;

	WeatherRequestWriter writer(output, output_size);
	writer.append("GET /");
	writer.append(endpoint);
	writer.append("?loc=");

	uint16_t option_capacity = static_cast<uint16_t>(scratch_size - 1);
	if (option_capacity > MAX_SOPTS_SIZE) option_capacity = MAX_SOPTS_SIZE;
	option_loader(SOPT_LOCATION, scratch, option_capacity);
	scratch[option_capacity] = 0;
	writer.append_url_encoded(scratch);

	writer.append("&wto=");
	option_loader(SOPT_WEATHER_OPTS, scratch, option_capacity);
	scratch[option_capacity] = 0;
	writer.append_url_encoded(scratch);
	writer.append(extra_query);

	option_loader(SOPT_WEATHERURL, scratch, option_capacity);
	scratch[option_capacity] = 0;
	target->host = scratch;
	target->use_ssl = true;
	target->port = 443;
	if (strncmp_P(scratch, PSTR("http://"), 7) == 0) {
		target->host = scratch + 7;
		target->use_ssl = false;
		target->port = 80;
	} else if (strncmp_P(scratch, PSTR("https://"), 8) == 0) {
		target->host = scratch + 8;
	}

	char *colon = strchr(target->host, ':');
	if (colon) {
		*colon = 0;
		long parsed_port = strtol(colon + 1, nullptr, 10);
		if (parsed_port < 1 || parsed_port > 65535) return false;
		target->port = static_cast<uint16_t>(parsed_port);
	}
	if (!target->host[0]) return false;

	writer.append(" HTTP/1.0\r\nHOST: ");
	writer.append(target->host);
	writer.append("\r\nUser-Agent: ");
	writer.append(user_agent);
	writer.append("\r\nConnection: close\r\n\r\n");
	return writer.ok();
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
	weather_sensor_stale_groups &= ~group;
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

uint8_t weather_sensor_expire_groups() {
	uint8_t newly_stale = 0;
	const uint8_t groups[] = {
		WEATHER_SENSOR_GROUP_CURRENT,
		WEATHER_SENSOR_GROUP_FORECAST,
		WEATHER_SENSOR_GROUP_HISTORICAL,
	};
	uint32_t now_ms = millis();
	for (uint8_t i = 0; i < sizeof(groups); i++) {
		uint8_t group = groups[i];
		uint32_t updated = weather_sensor_group_updated[i];
		if (updated && (uint32_t)(now_ms - updated) > WEATHER_SENSOR_STALE_MS &&
			!(weather_sensor_stale_groups & group)) {
			weather_sensor_stale_groups |= group;
			newly_stale |= group;
		}
	}
	return newly_stale;
}

void MaintainWeatherSensors() {
	uint8_t newly_stale = weather_sensor_expire_groups();
	if (!newly_stale) return;

	for (uint8_t i = 0; i < os.nsensors; i++) {
		if (!(os.sensors[i].flag & (1 << SENSOR_FLAG_ENABLE))) continue;
		Sensor *sensor = Sensor::get(i);
		if (!sensor || sensor->get_sensor_type() != SensorType::Weather) continue;
		WeatherAction action = static_cast<WeatherSensor *>(sensor)->action;
		if (action >= WeatherAction::MAX_VALUE || !(weather_action_group(action) & newly_stale)) continue;
		os.sensors[i].status =
			(os.sensors[i].status & SENSOR_STATUS_VALID) | SENSOR_STATUS_STALE;
	}
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
	}
	return groups;
}

static void schedule_weather_sensor_poll(uint8_t groups) {
	if (!groups) return;
	for (uint8_t i = 0; i < os.nsensors; i++) {
		if (!(os.sensors[i].flag & (1 << SENSOR_FLAG_ENABLE))) continue;
		Sensor *sensor = Sensor::get(i);
		if (!sensor || sensor->get_sensor_type() != SensorType::Weather) continue;
		WeatherAction action = static_cast<WeatherSensor *>(sensor)->action;
		if (action < WeatherAction::MAX_VALUE && (weather_action_group(action) & groups)) {
			os.sensors[i].next_update = 0;
		}
	}
}

void weather_sensor_reset_cache() {
	weather_sensor_valid = 0;
	weather_sensor_requested_actions = 0;
	weather_sensor_requested_groups = 0;
	weather_sensor_received_groups = 0;
	weather_sensor_stale_groups = 0;
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

	char extra_query[12];
	snprintf(extra_query, sizeof(extra_query), "&scope=%s", scope);
	WeatherHttpTarget target;
	if (!weather_build_http_request(ether_buffer, ETHER_BUFFER_SIZE,
		tmp_buffer, TMP_BUFFER_ALLOC_SIZE, "weatherSensorData", extra_query,
		user_agent_string, load_weather_option, &target)) {
		DEBUG_PRINTLN(F("weather sensor: request too large or invalid"));
		weather_sensor_next_request = millis() + WEATHER_SENSOR_RETRY_MS;
		return;
	}

	weather_sensor_received_groups = 0;
	int ret = os.send_http_request(target.host, target.port, ether_buffer,
		weather_sensor_callback, target.use_ssl);
	// A successful group refresh should be visible on the next sensor poll,
	// even when that sensor normally uses a long interval.
	schedule_weather_sensor_poll(weather_sensor_received_groups);
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
	int method = os.iopts[IOPT_USE_WEATHER];
	// use manual adjustment call for monthly adjustment -- a bit ugly, but does not involve weather server changes
	if(method==WEATHER_METHOD_MONTHLY) method=WEATHER_METHOD_MANUAL;
	wt_errCode = HTTP_RQT_NOT_RECEIVED;
	char endpoint[4];
	snprintf(endpoint, sizeof(endpoint), "%d", method);
	char extra_query[16];
	snprintf(extra_query, sizeof(extra_query), "&fwv=%d", (int)os.iopts[IOPT_FW_VERSION]);
	WeatherHttpTarget target;
	if (!weather_build_http_request(ether_buffer, ETHER_BUFFER_SIZE,
		tmp_buffer, TMP_BUFFER_ALLOC_SIZE, endpoint, extra_query,
		user_agent_string, load_weather_option, &target)) {
		DEBUG_PRINTLN(F("weather: request too large or invalid"));
		wt_errCode = HTTP_RQT_CONNECT_ERR;
		return;
	}
	DEBUG_PRINT(ether_buffer);
	int ret = os.send_http_request(target.host, target.port, ether_buffer,
		getweather_callback_with_peel_header, target.use_ssl);
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
