/* OpenSprinkler AVR/RPI/BBB Library
 * Copyright (C) 2014 by Ray Wang (ray@opensprinkler.com)
 *
 * Weather functions
 * Sep 2014 @ Rayshobby.net
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
#include <string.h>
#include <stdlib.h>
extern char ether_buffer[];
#endif

#include "OpenSprinkler.h"
#include "utils.h"
#include "server.h"

extern OpenSprinkler os; // OpenSprinkler object
extern char tmp_buffer[];
byte findKeyVal (const char *str,char *strbuf, uint8_t maxlen,const char *key,bool key_in_pgm=false,uint8_t *keyfound=NULL);

// The weather function calls getweather.py on remote server to retrieve weather data
// the default script is WEATHER_SCRIPT_HOST/weather?.py
static char website[] PROGMEM = WEATHER_SCRIPT_HOST ;

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
    if (v>=0 && v<=1440) {
      os.nvdata.sunrise_time = v;
    }
  }

  if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("sunset"), true)) {
    v = atoi(tmp_buffer);
    if (v>=0 && v<=1440) {
      os.nvdata.sunset_time = v;
    }
  }
  os.nvdata_save(); // save non-volatile memory

  if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("scale"), true)) {
    v = atoi(tmp_buffer);
    if (v>=0 && v<=250) {
      os.options[OPTION_WATER_PERCENTAGE].value = v;
      os.options_save();
    }
  }
  
  if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("tz"), true)) {
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
  
  if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("eip"), true)) {
    os.external_ip = atol(tmp_buffer);
  }
  os.status.wt_received = 1;
}

#if defined(ARDUINO)
void GetWeather() {
  // check if we've already done dns lookup
  if(ether.hisip[0] == 0) {
    ether.dnsLookup(website);
  }

  //bfill=ether.tcpOffset();
  BufferFiller bf = (uint8_t*)tmp_buffer;
  bf.emit_p(PSTR("$D.py?loc=$E&key=$E&fwv=$D"),
                (int) os.options[OPTION_USE_WEATHER].value,
                ADDR_NVM_LOCATION,
                ADDR_NVM_WEATHER_KEY,
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
  
  os.status.wt_received = 0;
  uint16_t _port = ether.hisport; // save current port number
  ether.hisport = 80;
  ether.browseUrl(PSTR("/weather"), dst, website, getweather_callback);
  ether.hisport = _port;
}
#else // GetWeather() for RPI/BBB

void peel_http_header() {
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
  int sockfd = -1;
  struct sockaddr_in serv_addr;
  struct hostent *server;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if(sockfd < 0) {
    DEBUG_PRINTLN("can't establish socket.");
    return;
  }
  strcpy(tmp_buffer, WEATHER_SCRIPT_HOST);
  server = gethostbyname(tmp_buffer);
  if(server == NULL) {
    DEBUG_PRINTLN("can't resolve weather server.");
    close(sockfd);
    return;
  }
  bzero((char*)&serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char*)server->h_addr, (char*)&serv_addr.sin_addr.s_addr, server->h_length);
  serv_addr.sin_port = htons(80);
 
  int ret=connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
  if ( ret < 0) {
    DEBUG_PRINTLN("failed to connect.");
    close(sockfd);
    return;
  }

  BufferFiller bf = tmp_buffer;  
  bf.emit_p(PSTR("$D.py?loc=$E&key=$E&fwv=$D"),
                (int) os.options[OPTION_USE_WEATHER].value,
                ADDR_NVM_LOCATION,
                ADDR_NVM_WEATHER_KEY,
                (int)os.options[OPTION_FW_VERSION].value);    

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
  
  char urlBuffer[255];
  strcpy(urlBuffer, "GET /weather");
  strcat(urlBuffer, dst);
  strcat(urlBuffer, " HTTP/1.0\r\nHOST: weather.opensprinkler.com\r\n\r\n");
  int n=write(sockfd,urlBuffer,strlen(urlBuffer));
  if(n<0) {
    DEBUG_PRINTLN("error sending data to weather server.");
    return;
  }
  bzero(tmp_buffer, TMP_BUFFER_SIZE);
  
  time_t timeout = os.now_tz() + 5;
  n=-1;
  os.status.wt_received = 0;
  while(os.now_tz() < timeout) {
    n = read(sockfd, ether_buffer, ETHER_BUFFER_SIZE);
    if(n>0) {
      peel_http_header();
      getweather_callback(0, 0, ETHER_BUFFER_SIZE);
      close(sockfd);
      return;
    }
  }
  if(n<0) {
    DEBUG_PRINTLN("error reading data from weather server.");
  }
  close(sockfd);
}
#endif // GetWeather()

