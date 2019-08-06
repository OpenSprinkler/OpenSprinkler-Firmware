/* ESPConnect functions
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

#include "espconnect.h"

const char html_ap_redirect[] PROGMEM = "<h3>WiFi config saved. Now switching to station mode.</h3>";

String scan_network() {
	WiFi.mode(WIFI_STA);
	WiFi.disconnect();
	byte n = WiFi.scanNetworks();
	String wirelessinfo;
	if (n>32) n = 32; // limit to 32 ssids max
	 //Maintain old format of wireless network JSON for mobile app compat
	 wirelessinfo = "{\"ssids\":["; 
	for(int i=0;i<n;i++) {
		wirelessinfo += "\"";
		wirelessinfo += WiFi.SSID(i);
		wirelessinfo += "\"";
		if(i<n-1) wirelessinfo += ",\r\n";
	}
	wirelessinfo += "],";
	wirelessinfo += "\"rssis\":["; 
	for(int i=0;i<n;i++) {
		wirelessinfo += "\"";
		wirelessinfo += WiFi.RSSI(i);
		wirelessinfo += "\"";
		if(i<n-1) wirelessinfo += ",\r\n";
	}
	wirelessinfo += "]}";
	return wirelessinfo;
}

void start_network_ap(const char *ssid, const char *pass) {
	if(!ssid) return;
	if(pass)
		WiFi.softAP(ssid, pass);
	else
		WiFi.softAP(ssid);
	WiFi.mode(WIFI_AP_STA); // start in AP_STA mode
	WiFi.disconnect();	// disconnect from router
}

void start_network_sta_with_ap(const char *ssid, const char *pass) {
	if(!ssid || !pass) return;
	WiFi.begin(ssid, pass);
}

void start_network_sta(const char *ssid, const char *pass) {
	if(!ssid || !pass) return;
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, pass);
}
#endif
