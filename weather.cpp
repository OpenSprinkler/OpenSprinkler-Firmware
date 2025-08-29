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
#include "OpenSprinkler.h"
#include "utils.h"
#include "opensprinkler_server.h"
#include "weather.h"
#include "main.h"
#include "types.h"
#include "ArduinoJson.hpp"

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

extern const char *user_agent_string;

unsigned char findKeyVal (const char *str,char *strbuf, uint16_t maxlen,const char *key,bool key_in_pgm=false,uint8_t *keyfound=NULL);

unsigned char parseMdScalesArray (const char* input);

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
			os.nvdata.rd_stop_time = tnow + (unsigned long) v * 3600;
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
	BufferFiller bf = BufferFiller(tmp_buffer, TMP_BUFFER_SIZE*2);
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

	strcat(ether_buffer, " HTTP/1.0\r\nHOST: ");
	strcat(ether_buffer, host);
	strcat(ether_buffer, "\r\nUser-Agent: ");
	strcat(ether_buffer, user_agent_string);
	strcat(ether_buffer, "\r\n\r\n");

	wt_errCode = HTTP_RQT_NOT_RECEIVED;
	DEBUG_PRINT(ether_buffer);
#if defined(OS_AVR)
	int ret = os.send_http_request(host, ether_buffer, getweather_callback_with_peel_header);
#else
	int ret = os.send_http_request(host, 443, ether_buffer, getweather_callback_with_peel_header, true);
#endif
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
		time_os_t ct = curr_time;
		struct tm *ti = gmtime(&ct);
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
		long val = strtol(p, &endptr, 10);   // parse number
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
