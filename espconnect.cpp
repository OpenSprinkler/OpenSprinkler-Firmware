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
#if defined(ESP8266) || defined(ESP32)

#include "espconnect.h"

#if  defined(ESP32)
#include "esp32.h"
#endif

String scan_network() {
	WiFi.mode(WIFI_STA);
	WiFi.disconnect();
	byte n = WiFi.scanNetworks();
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
	if(pass) WiFi.softAP(ssid, pass);
	else WiFi.softAP(ssid);
	DEBUG_PRINT(F("Starting AP with SSID "));
	DEBUG_PRINTLN(ssid);
	WiFi.mode(WIFI_AP_STA); // start in AP_STA mode
	WiFi.disconnect();	// disconnect from router
}

void start_network_sta_with_ap(const char *ssid, const char *pass, int32_t channel, const byte *bssid) {
	if(!ssid || !pass) return;
	if(WiFi.getMode()!=WIFI_AP_STA) WiFi.mode(WIFI_AP_STA);
	DEBUG_PRINT(F("Connecting in AP_STA to WiFi network "));
	DEBUG_PRINTLN(ssid);
	WiFi.begin(ssid, pass, channel, bssid);
}

void start_network_sta(const char *ssid, const char *pass, int32_t channel, const byte *bssid) {
	if(!ssid || !pass) return;
	if(WiFi.getMode()!=WIFI_STA) WiFi.mode(WIFI_STA);
#if defined(ESP32)
	//WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
	WiFi.setSleep(false);
	#if defined(MDNS_NAME)
	WiFi.setHostname(MDNS_NAME);
	#endif
#endif
	DEBUG_PRINT(F("Connecting in STA to WiFi network "));
	DEBUG_PRINTLN(ssid);
	WiFi.begin(ssid, pass, channel, bssid);
}
#endif
