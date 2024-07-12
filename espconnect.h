/* ESPConnect header file
 * December 2016 @ opensprinkler.com
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

#if defined(ESP8266)

#ifndef _ESP_CONNECT_H
#define _ESP_CONNECT_H

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include "time.h"
#include "defines.h"
#include "htmls.h"

String scan_network();
void start_network_ap(const char *ssid, const char *pass);
void start_network_sta(const char *ssid, const char *pass, int32_t channel=0, const unsigned char *mac=NULL);
void start_network_sta_with_ap(const char *ssid, const char *pass, int32_t channel=0, const unsigned char *mac=NULL);
#endif

#endif
