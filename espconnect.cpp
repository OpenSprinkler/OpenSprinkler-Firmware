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

String scan_network() {
	WiFi.setOutputPower(20.5);
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
	WiFi.setOutputPower(20.5);
	if(pass) WiFi.softAP(ssid, pass);
	else WiFi.softAP(ssid);
	WiFi.mode(WIFI_AP_STA); // start in AP_STA mode
	WiFi.disconnect();	// disconnect from router
}

void start_network_sta_with_ap(const char *ssid, const char *pass, int32_t channel, const byte *bssid) {
	if(!ssid || !pass) return;
	WiFi.setOutputPower(20.5);
	if(WiFi.getMode()!=WIFI_AP_STA) WiFi.mode(WIFI_AP_STA);
	WiFi.begin(ssid, pass, channel, bssid);
}

void start_network_sta(const char *ssid, const char *pass, int32_t channel, const byte *bssid) {
	if(!ssid || !pass) return;
	WiFi.setOutputPower(20.5);
	if(WiFi.getMode()!=WIFI_STA) WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, pass, channel, bssid);
}
#endif
