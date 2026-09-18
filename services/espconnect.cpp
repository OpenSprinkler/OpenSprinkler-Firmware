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
#if defined(ARDUINO)

#include "espconnect.h"

String scan_network() {
	WiFi.mode(WIFI_STA);
	WiFi.disconnect();
	unsigned char n = WiFi.scanNetworks();
	String json;
	if (n>40) n = 40; // limit to 40 ssids max
	// maintain old format of wireless network JSON for mobile app compat
	json = "{\"ssids\":[";
	for(int i=0;i<n;i++) {
		json += "\"";
		json += WiFi.SSID(i);
		json += "\"";
		if(i<n-1) json += ",";
	}
	json += "],";
	// scanned contains complete wireless info including bssid and channel
	json += "\"scanned\":[";
	for(int i=0;i<n;i++) {
		json += "[\"" + WiFi.SSID(i) + "\",";
		json += "\"" + WiFi.BSSIDstr(i) + "\",";
		json += String(WiFi.RSSI(i))+",",
		json += String(WiFi.channel(i))+"]";
		if(i<n-1) json += ",";
	}
	json += "]}";
	return json;
}

void start_network_ap(const char *ssid, const char *pass) {
	if(!ssid) return;
	WiFi.mode(WIFI_AP_STA); // start in AP_STA mode
	#if defined(ESP32)
	WiFi.disconnect(false, false);
	WiFi.setSleep(false);
	#else
	WiFi.disconnect();	// disconnect from router
	#endif
	if(pass) WiFi.softAP(ssid, pass);
	else WiFi.softAP(ssid);
}

void start_network_sta_with_ap(const char *ssid, const char *pass, int32_t channel, const unsigned char *bssid) {
	if(!ssid || !pass) return;
	if(WiFi.getMode()!=WIFI_AP_STA) WiFi.mode(WIFI_AP_STA);
	#if defined(ESP32)
	WiFi.setSleep(false);
	WiFi.setAutoReconnect(true);
	#endif
	WiFi.begin(ssid, pass, channel, bssid);
}

void start_network_sta(const char *ssid, const char *pass, int32_t channel, const unsigned char *bssid) {
	if(!ssid || !pass) return;
	#if defined(ESP32)
	WiFi.disconnect(false, false);
	delay(100);
	#endif
	if(WiFi.getMode()!=WIFI_STA) WiFi.mode(WIFI_STA);
	WiFi.setSleep(false); // work-around for ARP issue: disable sleep mode
	#if defined(ESP8266)
	WiFi.setOutputPower(20.5);
	#endif
	WiFi.setAutoReconnect(true); // enable auto reconnect
	WiFi.begin(ssid, pass, channel, bssid);
}
#endif
