// Weather.cpp
// This file manages the retrieval of Weather related information and adjustment of durations
//   from Weather Underground
// Author: Richard Zimmerman
// Copyright (c) 2013 Richard Zimmerman
//
// Sep, 2014
// Modified by Ray Wang to work with OpenSprinkler 

#include "OpenSprinklerGen2.h"
#include <string.h>
#include <stdlib.h>

extern OpenSprinkler os; // OpenSprinkler object
extern char tmp_buffer[];

// The weather function calls getweather.py on remote server to retrieve weather data
// the default script is WEATHER_SCRIPT_HOST/weather?.py
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
  if (ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("sunrise"), true)) {
    v = atoi(tmp_buffer);
    if (v>=0 && v<=1440) {
      os.nvdata.sunrise_time = v;
    }
  }

  if (ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("sunset"), true)) {
    v = atoi(tmp_buffer);
    if (v>=0 && v<=1440) {
      os.nvdata.sunset_time = v;
    }
  }
  os.nvdata_save(); // save non-volatile memory

  if (ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("scale"), true)) {
    v = atoi(tmp_buffer);
    if (v>=0 && v<=250) {
      os.options[OPTION_WATER_PERCENTAGE].value = v;
      os.options_save();
    }
  }
  
  if (ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("tz"), true)) {
    v = atoi(tmp_buffer);
    if (v>=0 && v<= 96) {
      if (v != os.options[OPTION_TIMEZONE].value) {
        // if timezone changed, save change and force ntp sync
        os.options[OPTION_TIMEZONE].value = v;
        os.ntpsync_lasttime = 0;      
        os.options_save();
      }
    }
  }
  os.status.wt_received = 1;
}

void GetWeather() {
  // check if we've already done dns lookup
  if(ether.hisip[0] == 0) {
    ether.dnsLookup(website);
  }

  //bfill=ether.tcpOffset();
  BufferFiller bf = (uint8_t*)tmp_buffer;
  bf.emit_p(PSTR("$D.py?loc=$E&key=$E&fwv=$D"),
                (int) os.options[OPTION_USE_WEATHER].value,
                ADDR_EEPROM_LOCATION,
                ADDR_EEPROM_WEATHER_KEY,
                (int)os.options[OPTION_FW_VERSION].value);
  // copy string to tmp_buffer, replacing all spaces with _
  char *src=tmp_buffer+strlen(tmp_buffer);
  char *dst=tmp_buffer+TMP_BUFFER_SIZE-1;
  
  char c;
  // url encode. convert SPACE to %20
  // copy reversely from the end because we are potentially expanding
  // the string size 
  while(src!=tmp_buffer) {
    c = *src--;
    if(c==' ') {
      *dst-- = '0';
      *dst-- = '2';
      *dst-- = '%';
    } else {
      *dst-- = c;
    }
  };
  *dst = *src;
  
  DEBUG_PRINTLN(dst);
  os.status.wt_received = 0;
  uint16_t _port = ether.hisport; // save current port number
  ether.hisport = 80;
  ether.browseUrl(PSTR("/weather"), dst, website, getweather_callback);
  ether.hisport = _port;
}

