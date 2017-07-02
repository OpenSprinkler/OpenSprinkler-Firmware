/* OpenSprinkler Unified (AVR/RPI/BBB/LINUX) Firmware
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

#if defined(ARDUINO)

#else
#include "etherport.h"
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
extern char ether_buffer[];
#endif

extern const char wtopts_filename[];

#include "OpenSprinkler.h"
#include "utils.h"
#include "server.h"
#include "weather.h"

extern OpenSprinkler os; // OpenSprinkler object
extern char tmp_buffer[];
byte findKeyVal (const char *str,char *strbuf, uint8_t maxlen,const char *key,bool key_in_pgm=false,uint8_t *keyfound=NULL);
void write_log(byte type, ulong curr_time);

// The weather function calls getweather.py on remote server to retrieve weather data
// the default script is WEATHER_SCRIPT_HOST/weather?.py
//static char website[] PROGMEM = DEFAULT_WEATHER_URL ;

static void getweather_callback(byte status, uint16_t off, uint16_t len) {
#if defined(ARDUINO)
  char *p = (char*)Ethernet::buffer + off;
#else
  char *p = ether_buffer;
#endif
  /* scan the buffer until the first & symbol */
  while(*p && *p!='&') {
    p++;
  }
  if (*p != '&')  return;
  int v;
  if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("sunrise"), true)) {
    v = atoi(tmp_buffer);
    if (v>=0 && v<=1440 && v != os.nvdata.sunrise_time) {
      os.nvdata.sunrise_time = v;
      os.nvdata_save();
      os.weather_update_flag |= WEATHER_UPDATE_SUNRISE;
    }
  }

  if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("sunset"), true)) {
    v = atoi(tmp_buffer);
    if (v>=0 && v<=1440 && v != os.nvdata.sunset_time) {
      os.nvdata.sunset_time = v;
      os.nvdata_save();
      os.weather_update_flag |= WEATHER_UPDATE_SUNSET;      
    }
  }

  if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("eip"), true)) {
    uint32_t l = atol(tmp_buffer);
    if(l != os.nvdata.external_ip) {
      os.nvdata.external_ip = atol(tmp_buffer);
      os.nvdata_save();
      os.weather_update_flag |= WEATHER_UPDATE_EIP;
    }
  }

  if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("scale"), true)) {
    v = atoi(tmp_buffer);
    if (v>=0 && v<=250 && v != os.options[OPTION_WATER_PERCENTAGE]) {
      // only save if the value has changed
      os.options[OPTION_WATER_PERCENTAGE] = v;
      os.options_save();
      os.weather_update_flag |= WEATHER_UPDATE_WL;      
    }
  }
  
  if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("tz"), true)) {
    v = atoi(tmp_buffer);
    if (v>=0 && v<= 108) {
      if (v != os.options[OPTION_TIMEZONE]) {
        // if timezone changed, save change and force ntp sync
        os.options[OPTION_TIMEZONE] = v;
        os.options_save();
        os.weather_update_flag |= WEATHER_UPDATE_TZ;
      }
    }
  }
  
  if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("rd"), true)) {
    v = atoi(tmp_buffer);
    if (v>0) {
      os.nvdata.rd_stop_time = os.now_tz() + (unsigned long) v * 3600;
      os.raindelay_start();
    } else if (v==0) {
      os.raindelay_stop();
    }
  }

  os.checkwt_success_lasttime = os.now_tz();
  write_log(LOGDATA_WATERLEVEL, os.checkwt_success_lasttime);
}

#if defined(ARDUINO)  // for AVR
void GetWeather() {
  // perform DNS lookup for every query
  nvm_read_block(tmp_buffer, (void*)ADDR_NVM_WEATHERURL, MAX_WEATHERURL);
  ether.dnsLookup(tmp_buffer, true);

  //bfill=ether.tcpOffset();
  char tmp[60];
  read_from_file(wtopts_filename, tmp, 60);
  BufferFiller bf = (uint8_t*)tmp_buffer;
  bf.emit_p(PSTR("$D.py?loc=$E&key=$E&fwv=$D&wto=$S"),
                (int) os.options[OPTION_USE_WEATHER],
                ADDR_NVM_LOCATION,
                ADDR_NVM_WEATHER_KEY,
                (int)os.options[OPTION_FW_VERSION],
                tmp);
  // copy string to tmp_buffer, replacing all spaces with _
  char *src=tmp_buffer+strlen(tmp_buffer);
  char *dst=tmp_buffer+TMP_BUFFER_SIZE-12;
  
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
  uint16_t _port = ether.hisport; // save current port number
  ether.hisport = 80;
  ether.browseUrl(PSTR("/weather"), dst, PSTR("*"), getweather_callback);
  ether.hisport = _port;
}

#else // for RPI/BBB/LINUX

void peel_http_header() { // remove the HTTP header
  int i=0;
  bool eol=true;
  while(i<ETHER_BUFFER_SIZE) {
    char c = ether_buffer[i];
    if(c==0)  return;
    if(c=='\n' && eol) {
      // copy
      i++;
      int j=0;
      while(i<ETHER_BUFFER_SIZE) {
        ether_buffer[j]=ether_buffer[i];
        if(ether_buffer[j]==0)  break;
        i++;
        j++;
      }
      return;
    }
    if(c=='\n') {
      eol=true;
    } else if (c!='\r') {
      eol=false;
    }
    i++;
  }
}

void GetWeather() {
  EthernetClient client;
  uint16_t port = 80;
  char * delim;
  struct hostent *server;
  
  nvm_read_block(tmp_buffer, (void*)ADDR_NVM_WEATHERURL, MAX_WEATHERURL);

  // Check to see if url specifies a port number to use
  delim = strchr(tmp_buffer, ':');
  if (delim != NULL) {
        *delim = 0;
        port = atoi(delim+1);
  }

  server = gethostbyname(tmp_buffer);
  if (!server) {
    DEBUG_PRINT("can't resolve weather server - ");
    DEBUG_PRINTLN(tmp_buffer);
    return;
  }
  DEBUG_PRINT("weather server ip:port - ");
  DEBUG_PRINT(((uint8_t*)server->h_addr)[0]);
  DEBUG_PRINT(".");
  DEBUG_PRINT(((uint8_t*)server->h_addr)[1]);
  DEBUG_PRINT(".");
  DEBUG_PRINT(((uint8_t*)server->h_addr)[2]);
  DEBUG_PRINT(".");
  DEBUG_PRINT(((uint8_t*)server->h_addr)[3]);
  DEBUG_PRINT(":");
  DEBUG_PRINTLN(port);

  if (!client.connect((uint8_t*)server->h_addr, port)) {
    client.stop();
    return;
  }

  BufferFiller bf = tmp_buffer;
  char tmp[100];
  read_from_file(wtopts_filename, tmp, 100);
  bf.emit_p(PSTR("$D.py?loc=$E&key=$E&fwv=$D&wto=$S"),
                (int) os.options[OPTION_USE_WEATHER],
                ADDR_NVM_LOCATION,
                ADDR_NVM_WEATHER_KEY,
                (int)os.options[OPTION_FW_VERSION],
                tmp);    

  char *src=tmp_buffer+strlen(tmp_buffer);
  char *dst=tmp_buffer+TMP_BUFFER_SIZE-12;
  
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

  char urlBuffer[255];
  strcpy(urlBuffer, "GET /weather");
  strcat(urlBuffer, dst);
  strcat(urlBuffer, " HTTP/1.0\r\nHOST: ");
  strcat(urlBuffer, server->h_name);
  strcat(urlBuffer, "\r\n\r\n");
  
  DEBUG_PRINTLN(urlBuffer);
  client.write((uint8_t *)urlBuffer, strlen(urlBuffer));
  
  bzero(ether_buffer, ETHER_BUFFER_SIZE);
  
  time_t timeout = os.now_tz() + 5; // 5 seconds timeout
  while(os.now_tz() < timeout) {
    int len=client.read((uint8_t *)ether_buffer, ETHER_BUFFER_SIZE);
    if (len<=0) {
      if(!client.connected())
        break;
      else 
        continue;
    }
    peel_http_header();
    getweather_callback(0, 0, ETHER_BUFFER_SIZE);
    break;
  }
  client.stop();
}
#endif // GetWeather()

