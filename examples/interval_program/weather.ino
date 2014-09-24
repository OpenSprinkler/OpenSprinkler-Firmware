// Weather.cpp
// This file manages the retrieval of Weather related information and adjustment of durations
//   from Weather Underground
// Author: Richard Zimmerman
// Copyright (c) 2013 Richard Zimmerman
//
// Sep 6, 2014
// Modified by Ray Wang to fit OpenSprinkler 

#include "OpenSprinklerGen2.h"
#include <string.h>
#include <stdlib.h>

extern OpenSprinkler os; // OpenSprinkler object
extern char tmp_buffer[];

// The weather function calls getweather.py on remote server to retrieve weather data
// the default script is WEATHER_SCRIPT_HOST/scripts/getweather.py
static char website[] PROGMEM = WEATHER_SCRIPT_HOST ;

void getweather_callback(byte status, word off, word len) {
  char *p = (char*)Ethernet::buffer + off;
  
  /* scan the buffer until the first & symbol */
  while(*p && *p!='&') {
    p++;
  }
  if (*p != '&')  return;
  int v;
  DEBUG_PRINTLN(p);
  if (ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, "sunrise")) {
    v = atoi(tmp_buffer);
    if (v>=0 && v<=1440) {
      os.nvdata.sunrise_time = v;
    }
  }
  if (ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, "sunset")) {
    v = atoi(tmp_buffer);
    if (v>=0 && v<=1440) {
      os.nvdata.sunset_time = v;
    }
  }
  os.nvdata_save(); // save non-volatile memory

  if (ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, "scale")) {
    v = atoi(tmp_buffer);
    if (v>=0 && v<=250) {
      os.options[OPTION_WATER_PERCENTAGE].value = v;
      os.options_save();
    }
  }
  
  if (ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, "tz")) {
    v = atoi(tmp_buffer);
    if (v>=0 && v<= 96) {
      os.options[OPTION_TIMEZONE].value = v;
      os.ntpsync_lasttime = 0;      
      os.options_save();
    }
  }
  os.status.wt_received = 1;
}

void GetWeather() {
  // check if we've already done dns lookup
  if(ether.hisip[0] == 0) {
    ether.dnsLookup(website);
  }

  bfill=ether.tcpOffset();
  bfill.emit_p(PSTR("$D.py?loc=$E&key=$E&fwv=$D"),
                (int) os.options[OPTION_USE_WEATHER].value,
                ADDR_EEPROM_LOCATION,
                ADDR_EEPROM_WEATHER_KEY,
                (int)os.options[OPTION_FW_VERSION].value);
  // copy string to tmp_buffer, replacing all spaces with _
  char *src = (char*)ether.tcpOffset();
  char *dst = tmp_buffer;
  char c;
  do {
    c = *src++;
    if (c==' ') {
      *dst++='%';
      *dst++='2';
      *dst++='0';
    } else {
      *dst++ = c;
    }
  } while(c);
  
  DEBUG_PRINTLN(tmp_buffer);
  os.status.wt_received = 0;
  ether.browseUrl(PSTR("/scripts/weather"), tmp_buffer, website, getweather_callback);
}






















/*
#define FIND_QUOTE1     0
#define PARSING_KEY     1
#define FIND_QUOTE2     2
#define PARSING_VALUE   3
#define PARSING_QVALUE  4
#define ERROR           5


int Weather::GetScale(const IPAddress & ip, const char * key, uint32_t zip, const char * pws, bool usePws) const
{
	ReturnVals vals = GetVals(ip, key, zip, pws, usePws);
	return GetScale(vals);
}

int Weather::GetScale(const ReturnVals & vals) const
{
	if (!vals.valid)
		return 100;
	const int humid_factor = 30 - (vals.maxhumidity + vals.minhumidity) / 2;
	const int temp_factor = (vals.meantempi - 70) * 4;
	const int rain_factor = (vals.precipi + vals.precip_today) * -2;
	const int adj = min(max(0, 100+humid_factor+temp_factor+rain_factor), 200);
	trace(F("Adjusting H(%d)T(%d)R(%d):%d\n"), humid_factor, temp_factor, rain_factor, adj);
	return adj;
}

Weather::ReturnVals Weather::GetVals(const IPAddress & ip, const char * key, uint32_t zip, const char * pws, bool usePws) const
{
	ReturnVals vals = {0};
	EthernetClient client;
	if (client.connect(ip, 80))
	{
		char getstring[90];
		trace(F("Connected\n"));
		if (usePws)
			snprintf(getstring, sizeof(getstring), "GET /api/%s/yesterday/conditions/q/pws:%s.json HTTP/1.0\n\n", key, pws);
		else
			snprintf(getstring, sizeof(getstring), "GET /api/%s/yesterday/conditions/q/%ld.json HTTP/1.0\n\n", key, (long) zip);
		//trace(getstring);
		client.write((uint8_t*) getstring, strlen(getstring));

		ParseResponse(client, &vals);
		client.stop();
		if (!vals.valid)
		{
			if (vals.keynotfound)
				trace("Invalid WUnderground Key\n");
			else
				trace("Bad WUnderground Response\n");
		}
	}
	else
	{
		trace(F("connection failed\n"));
		client.stop();
	}
	return vals;
}
*/
/*
static byte current_state;
Weather::ReturnVals ret;




void my_callback(byte status, word off, word len) {
  char *p = (char*)Ethernet::buffer + off;

  static char key[30], val[30];
  static char * keyptr = key;
  static char * valptr = val;
  char c;
  char *end = (char*)Ethernet::buffer + off + len;
  
  while (p<end && *p) {

    c = *(p++);
    switch (current_state)
    {
    case FIND_QUOTE1:
      if (c == '"') {
        current_state = PARSING_KEY;
        keyptr = key;
      }
      break;
    case PARSING_KEY:
      if (c == '"') {
				current_state = FIND_QUOTE2;
				*keyptr = 0;
			} else {
				if ((keyptr - key) < (long)(sizeof(key) - 1)) {
					*keyptr = c;
					keyptr++;
				}
			}
			break;
		case FIND_QUOTE2:
			if (c == '"') {
				current_state = PARSING_QVALUE;
				valptr = val;
			} else if (c == '{') {
				current_state = FIND_QUOTE1;
			} else if ((c >= '0') && (c <= '9')) {
				current_state = PARSING_VALUE;
				valptr = val;
				*valptr = c;
				valptr++;
			}
			break;
		case PARSING_VALUE:
			if (((c >= '0') && (c <= '9')) || (c == '.')) {
				*valptr = c;
				valptr++;
			} else {
				current_state = FIND_QUOTE1;
				*valptr = 0;
				if (strcmp(key, "sunrise") == 0) {
				  ret.valid = true;
				  ret.keynotfound = false;
				  ret.sunrise = ((atol(val) + (int32_t)3600/4*(int32_t)(os.options[OPTION_TIMEZONE].value-48)) % 86400L) / 60;
				  DEBUG_PRINTLN(ret.sunrise);
				} else if (strcmp(key, "sunset") == 0) {
				  ret.sunset = ((atol(val) + (int32_t)3600/4*(int32_t)(os.options[OPTION_TIMEZONE].value-48)) % 86400L) / 60;
				  DEBUG_PRINTLN(ret.sunset);
				}				
			}
			break;
		case PARSING_QVALUE:
			if (c == '"') {
				current_state = FIND_QUOTE1;
				*valptr = 0;
				
				if (strcmp(key, "maxhumidity") == 0)
				{
					ret.valid = true;
					ret.keynotfound = false;
					ret.maxhumidity = atoi(val);
				}
				else if (strcmp(key, "minhumidity") == 0)
				{
					ret.minhumidity = atoi(val);
				}
				else if (strcmp(key, "meantempi") == 0)
				{
					ret.meantempi = atoi(val);
				}
				else if (strcmp(key, "precip_today_in") == 0)
				{
					ret.precip_today = (atof(val) * 100.0);
				}
				else if (strcmp(key, "precipi") == 0)
				{
					ret.precipi = (atof(val) * 100.0);
				}
				//else if (strcmp(key, "UV") == 0)
				//{
				//	ret.UV = (atof(val) * 10.0);
				//}
				else if (strcmp(key, "meanwindspdi") == 0)
				{
					ret.windmph = (atof(val) * 10.0);
				}
				else if (strcmp(key, "type") == 0)
				{
					if (strcmp(val, "keynotfound") == 0)
						ret.keynotfound = true;
				}
				
			}
			else
			{
				if ((valptr - val) < (long)(sizeof(val) - 1)) {
					*valptr = c;
					valptr++;
				}
			}
			break;
		case ERROR:
			break;
		} // case
	} // while (true)
  if(len<512) {
    ether.persistTcpConnection(false);
  }
}
*/
