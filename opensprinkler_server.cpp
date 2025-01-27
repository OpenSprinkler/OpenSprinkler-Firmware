/* OpenSprinkler Unified Firmware
 * Copyright (C) 2015 by Ray Wang (ray@opensprinkler.com)
 *
 * Server functions
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

#include "types.h"
#include "OpenSprinkler.h"
#include "program.h"
#include "opensprinkler_server.h"
#include "weather.h"
#include "mqtt.h"
#include "main.h"
#include "sensors.h"
#include "osinfluxdb.h"

// External variables defined in main ion file
#if defined(USE_OTF)
	extern OTF::OpenThingsFramework *otf;
	#define OTF_PARAMS_DEF const OTF::Request &req,OTF::Response &res
	#define OTF_PARAMS req,res
	#define FKV_SOURCE req
	#define handle_return(x) {if(x==HTML_OK) res.writeBodyData(ether_buffer, strlen(ether_buffer)); else otf_send_result(req,res,x); return;}
#else
	extern EthernetClient *m_client;
	#define OTF_PARAMS_DEF
	#define OTF_PARAMS
	#define FKV_SOURCE p
	#define handle_return(x) {return_code=x; return;}
#endif

#if defined(ARDUINO)
	#if defined(ESP8266)
		#include <FS.h>
		#include <LittleFS.h>
		#include "espconnect.h"
		extern ESP8266WebServer *update_server;
		extern ENC28J60lwIP enc28j60;
		extern Wiznet5500lwIP w5500;
		extern lwipEth eth;
	#else
		#include "SdFat.h"
		extern SdFat sd;
	#endif
#else
	#include <stdarg.h>
	#include <stdlib.h>
	#include "etherport.h"
#endif

extern char ether_buffer[];
extern char tmp_buffer[];
extern OpenSprinkler os;
extern ProgramData pd;
extern ulong flow_count;

#if !defined(USE_OTF)
static unsigned char return_code;
static char* get_buffer = NULL;
#endif

BufferFiller bfill;

/* Check available space (number of bytes) in the Ethernet buffer */
int available_ether_buffer() {
	return ETHER_BUFFER_SIZE - (int)bfill.position();
}

// Define return error code
#define HTML_OK               0x00
#define HTML_SUCCESS          0x01
#define HTML_UNAUTHORIZED     0x02
#define HTML_MISMATCH         0x03
#define HTML_DATA_MISSING     0x10
#define HTML_DATA_OUTOFBOUND  0x11
#define HTML_DATA_FORMATERROR 0x12
#define HTML_RFCODE_ERROR     0x13
#define HTML_PAGE_NOT_FOUND   0x20
#define HTML_NOT_PERMITTED    0x30
#define HTML_UPLOAD_FAILED    0x40
#define HTML_REDIRECT_HOME    0xFF

#if !defined(USE_OTF)
static const char html200OK[] PROGMEM =
	"HTTP/1.1 200 OK\r\n"
;

static const char htmlNoCache[] PROGMEM =
	"Cache-Control: max-age=0, no-cache, no-store, must-revalidate\r\n"
;

static const char htmlContentJSON[] PROGMEM =
	"Content-Type: application/json\r\n"
	"Connection: close\r\n"
;

static const char htmlContentHTML[] PROGMEM =
	"Content-Type: text/html\r\n"
	"Connection: close\r\n"
;

static const char htmlAccessControl[] PROGMEM =
	"Access-Control-Allow-Origin: *\r\n"
;
#endif

static const char htmlMobileHeader[] PROGMEM =
	"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1.0,minimum-scale=1.0,user-scalable=no\">"
;

static const char htmlReturnHome[] PROGMEM =
	"<script>window.location=\"/\";</script>\n"
;

#if defined(USE_OTF)
unsigned char findKeyVal (const OTF::Request &req,char *strbuf, uint16_t maxlen,const char *key,bool key_in_pgm=false,uint8_t *keyfound=NULL) {
#if defined(ARDUINO)
	char* result = key_in_pgm ? req.getQueryParameter((const __FlashStringHelper *)key) : req.getQueryParameter(key);
#else
	char* result = req.getQueryParameter(key);
#endif
	if(result!=NULL) {
		strncpy(strbuf, result, maxlen);
		strbuf[maxlen-1]=0;
		if(keyfound) *keyfound=1;
		return strlen(strbuf);
	} else {
		if(keyfound) *keyfound=0;
	}
	return 0;
}
#endif
unsigned char findKeyVal (const char *str,char *strbuf, uint16_t maxlen,const char *key,bool key_in_pgm=false,uint8_t *keyfound=NULL) {
	uint8_t found=0;
	uint16_t i=0;
	const char *kp;
	if(str==NULL||strbuf==NULL||key==NULL) {return 0;}
	kp=key;
	if (key_in_pgm) {
		// key is in program memory space
		while(*str &&  *str!=' ' && *str!='\n' && found==0){
			if (*str == pgm_read_byte(kp)){
				kp++;
				if (pgm_read_byte(kp) == '\0'){
					str++;
					kp=key;
					if (*str == '='){
						found=1;
					}
				}
			} else {
				kp=key;
			}
			str++;
		}
	}	else {
		while(*str &&  *str!=' ' && *str!='\n' && found==0){
			if (*str == *kp){
				kp++;
				if (*kp == '\0'){
					str++;
					kp=key;
					if (*str == '='){
						found=1;
					}
				}
			} else {
				kp=key;
			}
			str++;
		}
	}
	if (found==1){
		// copy the value to a buffer and terminate it with '\0'
		while(*str &&  *str!=' ' && *str!='\n' && *str!='&' && i<maxlen-1){
			*strbuf=*str;
			i++;
			str++;
			strbuf++;
		}
		if (!(*str) || *str == ' ' || *str == '\n' || *str == '&') {
			*strbuf = '\0';
		} else {
			found = 0;	// Ignore partial values i.e. value length is larger than maxlen
			i = 0;
		}
	}
	// return the length of the value
	if (keyfound) *keyfound = found;
	return(i);
}

void rewind_ether_buffer() {
    bfill = BufferFiller(ether_buffer, ETHER_BUFFER_SIZE*2);
	ether_buffer[0] = 0;
}

#if defined(USE_OTF)
boolean buffer_available() {
	return bfill.position() < RESPONSE_BUFFER_SIZE/2 && available_ether_buffer() > 0;
}
#else
boolean buffer_available() {
	return available_ether_buffer() > 0;
}
#endif

void send_packet(OTF_PARAMS_DEF) {
#if defined(USE_OTF)
	res.writeBodyData(ether_buffer, strlen(ether_buffer));
#else
	m_client->write((const uint8_t *)ether_buffer, strlen(ether_buffer));
#endif

	rewind_ether_buffer();
}

char dec2hexchar(unsigned char dec) {
	if(dec<10) return '0'+dec;
	else return 'A'+(dec-10);
}

#if defined(USE_OTF)
void print_header(OTF_PARAMS_DEF, bool isJson=true, int len=0) {
	res.writeStatus(200, F("OK"));
	res.writeHeader(F("Content-Type"), isJson?F("application/json"):F("text/html"));
	if(len>0)
		res.writeHeader(F("Content-Length"), len);
	res.writeHeader(F("Access-Control-Allow-Origin"), F("*"));
	res.writeHeader(F("Cache-Control"), F("max-age=0, no-cache, no-store, must-revalidate"));
	res.writeHeader(F("Connection"), F("close"));
}
#else
void print_header(bool isJson=true)  {
	bfill.emit_p(PSTR("$F$F$F$F\r\n"), html200OK, isJson?htmlContentJSON:htmlContentHTML, htmlAccessControl, htmlNoCache);
}
#endif

#if defined(USE_OTF)
void print_header_download(OTF_PARAMS_DEF, int len=0) {
	res.writeStatus(200, F("OK"));
	res.writeHeader(F("Content-Type"), F("text/plain"));
	res.writeHeader(F("Content-Disposition"), F("attachment; filename=\"log.csv\";"));
	if(
	len>0)
		res.writeHeader(F("Content-Length"), len);
	res.writeHeader(F("Access-Control-Allow-Origin"), F("*"));
	res.writeHeader(F("Cache-Control"), F("max-age=0, no-cache, no-store, must-revalidate"));
	res.writeHeader(F("Connection"), F("close"));
}
#else
void print_header_download()  {
	bfill.emit_p(PSTR("$F$F$F$F$F\r\n"), html200OK, "Content-Type: text/plain", "Content-Disposition: attachment; filename=\"log.txt\";", htmlAccessControl, htmlNoCache);
}
#endif

#if defined(USE_OTF)
#if !defined(ARDUINO)
string two_digits(uint8_t x) {
	return std::to_string(x);
}
#else
String two_digits(uint8_t x) {
	return String(x/10) + (x%10);
}
#endif

String toHMS(ulong t) {
	return two_digits(t/3600)+":"+two_digits((t/60)%60)+":"+two_digits(t%60);
}

void otf_send_result(OTF_PARAMS_DEF, unsigned char code, const char *item = NULL) {
	String json = F("{\"result\":");
#if defined(ARDUINO)
	json += code;
#else
	json += std::to_string(code);
#endif
	if (!item) item = "";
	json += F(",\"item\":\"");
	json += item;
	json += F("\"");
	json += F("}");
	print_header(OTF_PARAMS, true, json.length());
	res.writeBodyChunk((char *)"%s",json.c_str());
}
#endif

#if defined(ESP8266)
void update_server_send_result(unsigned char code, const char* item = NULL) {
	String json = F("{\"result\":");
	json += code;
	if (!item) item = "";
	json += F(",\"item\":\"");
	json += item;
	json += F("\"");
	json += F("}");
	update_server->sendHeader("Access-Control-Allow-Origin", "*"); // from esp8266 2.4 this has to be sent explicitly
	update_server->send(200, "application/json", json);
}

String get_ap_ssid() {
	static String ap_ssid;
	if(!ap_ssid.length()) {
		unsigned char mac[6];
		WiFi.macAddress(mac);
		ap_ssid = "OS_";
		for(unsigned char i=3;i<6;i++) {
			ap_ssid += dec2hexchar((mac[i]>>4)&0x0F);
			ap_ssid += dec2hexchar(mac[i]&0x0F);
		}
	}
	return ap_ssid;
}

static String scanned_ssids;

void on_ap_home(OTF_PARAMS_DEF) {
	if(os.get_wifi_mode()!=WIFI_MODE_AP) return;
	print_header(OTF_PARAMS, false, strlen_P((char*)ap_home_html));
	res.writeBodyChunk((char *) "%s", ap_home_html);
}

void on_ap_scan(OTF_PARAMS_DEF) {
	if(os.get_wifi_mode()!=WIFI_MODE_AP) return;
	print_header(OTF_PARAMS, true, scanned_ssids.length());
	res.writeBodyChunk((char *)"%s",scanned_ssids.c_str());
}

void on_ap_change_config(OTF_PARAMS_DEF) {
	if(os.get_wifi_mode()!=WIFI_MODE_AP) return;
	char *ssid = req.getQueryParameter("ssid");
	if(ssid!=NULL&&strlen(ssid)!=0) {
		os.wifi_ssid = ssid;
		os.wifi_pass = req.getQueryParameter("pass");
		char *extra = req.getQueryParameter("extra");
		if(extra!=NULL) { // bssid and channel are in the format of xx:xx:xx:xx:xx:xx@ch
			char *mac = strchr(extra, '@'); // search for symbol @
			if(mac==NULL || !isValidMAC(extra)) { // if not found or if MAC is invalid
				otf_send_result(OTF_PARAMS, HTML_DATA_FORMATERROR, "bssid");
				return;
			}
			int chl = atoi(mac+1); // convert ch to integer
			if(!(chl>=0 && chl<=255)) { // chl must be less than 255
				otf_send_result(OTF_PARAMS, HTML_DATA_OUTOFBOUND, "channel");
				return;
			}
			os.sopt_save(SOPT_STA_BSSID_CHL, extra); // save string to flash first
			*mac=0; // terminate bssid string
			str2mac(extra, os.wifi_bssid); // update controller variables
			os.wifi_channel = chl;
		} else {
			os.sopt_save(SOPT_STA_BSSID_CHL, DEFAULT_EMPTY_STRING); // if extra is not present, write empty string
		}
		os.sopt_save(SOPT_STA_SSID, os.wifi_ssid.c_str());
		os.sopt_save(SOPT_STA_PASS, os.wifi_pass.c_str());
		otf_send_result(OTF_PARAMS, HTML_SUCCESS, nullptr);
		os.state = OS_STATE_TRY_CONNECT;
		os.lcd.setCursor(0, 2);
		os.lcd.print(F("Connecting..."));
	} else {
		otf_send_result(OTF_PARAMS, HTML_DATA_MISSING, "ssid");
	}
}

void reboot_in(uint32_t ms);

void on_ap_try_connect(OTF_PARAMS_DEF) {
	if(os.get_wifi_mode()!=WIFI_MODE_AP) return;
	String json = "{";
	json += F("\"ip\":");
	json += (WiFi.status() == WL_CONNECTED) ? (uint32_t)WiFi.localIP() : 0;
	json += F("}");
	print_header(OTF_PARAMS,true,json.length());
	res.writeBodyChunk((char *)"%s",json.c_str());
	if(WiFi.status() == WL_CONNECTED && WiFi.localIP()) {
		os.iopts[IOPT_WIFI_MODE] = WIFI_MODE_STA;
		os.iopts_save();
		DEBUG_PRINTLN(F("IP received by client, restart."));
		reboot_in(1000);
	}
}
#endif

/** Check and verify password */
#if defined(USE_OTF)
boolean check_password(char *p) {
	return true;
}
boolean process_password(OTF_PARAMS_DEF, boolean fwv_on_fail=false)
#else
boolean check_password(char *p)
#endif
{
#if defined(DEMO)
	return true;
#endif
	if (os.iopts[IOPT_IGNORE_PASSWORD])  return true;

#if !defined(USE_OTF)
	if (m_client && !p) {
		p = get_buffer;
	}
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("pw"), true)) {
		if (os.password_verify(tmp_buffer)) return true;
	}
#else
	/*if(req.isCloudRequest()){ // password is not required if this is coming from cloud connection
		return true;
	}*/
	const char *pw = req.getQueryParameter("pw");
	if(pw != NULL && os.password_verify(pw)) return true;

	/* if fwv_on_fail is true, output fwv if password check has failed */
	if(fwv_on_fail) {
		rewind_ether_buffer();
		bfill.emit_p(PSTR("{\"$F\":$D}"), iopt_json_names+0, os.iopts[0]);
		print_header(OTF_PARAMS,true,strlen(ether_buffer));
		res.writeBodyChunk((char *)"%s",ether_buffer);
	} else {
		otf_send_result(OTF_PARAMS, HTML_UNAUTHORIZED);
	}
#endif
	return false;
}

void server_json_board_attrib(const char* name, unsigned char *attrib)
{
	bfill.emit_p(PSTR("\"$F\":["), name);
	for(unsigned char i=0;i<os.nboards;i++) {
		bfill.emit_p(PSTR("$D"), attrib[i]);
		if(i!=os.nboards-1)
			bfill.emit_p(PSTR(","));
	}
	bfill.emit_p(PSTR("],"));
}

void server_json_stations_attrib(const char* name, unsigned char *attrib)
{
	bfill.emit_p(PSTR("\"$F\":["), name);
	for(unsigned char bid=0;bid<os.nboards;bid++) {
		for (unsigned char s = 0; s < 8; s++) {
			bfill.emit_p(PSTR("$D"), attrib[bid * 8 + s]);
			if(bid != os.nboards-1 || s < 7) {
				bfill.emit_p(PSTR(","));
			}
		}
	}
	bfill.emit_p(PSTR("],"));
}

void server_json_stations_attrib16(const char* name, uint16_t *attrib)
{
	bfill.emit_p(PSTR("\"$F\":["), name);
	for(unsigned char bid=0;bid<os.nboards;bid++) {
		for (unsigned char s = 0; s < 8; s++) {
			bfill.emit_p(PSTR("$D"), attrib[bid * 8 + s]);
			if(bid != os.nboards-1 || s < 7) {
				bfill.emit_p(PSTR(","));
			}
		}
	}
	bfill.emit_p(PSTR("],"));
}

void server_json_stations_main(OTF_PARAMS_DEF) {
	server_json_board_attrib(PSTR("masop"), os.attrib_mas);
	server_json_board_attrib(PSTR("masop2"), os.attrib_mas2);
	server_json_board_attrib(PSTR("ignore_rain"), os.attrib_igrd);
	server_json_board_attrib(PSTR("ignore_sn1"), os.attrib_igs);
	server_json_board_attrib(PSTR("ignore_sn2"), os.attrib_igs2);
	server_json_board_attrib(PSTR("stn_dis"), os.attrib_dis);
	server_json_board_attrib(PSTR("stn_spe"), os.attrib_spe);
	server_json_stations_attrib(PSTR("stn_grp"), os.attrib_grp);
	server_json_stations_attrib16(PSTR("stn_fas"), os.attrib_fas);
	server_json_stations_attrib16(PSTR("stn_favg"), os.attrib_favg);

	bfill.emit_p(PSTR("\"snames\":["));
	unsigned char sid;
	for(sid=0;sid<os.nstations;sid++) {
		os.get_station_name(sid, tmp_buffer);
		bfill.emit_p(PSTR("\"$S\""), tmp_buffer);
		if(sid!=os.nstations-1)
			bfill.emit_p(PSTR(","));
		if (!buffer_available() ) {
			send_packet(OTF_PARAMS);
		}
	}
	bfill.emit_p(PSTR("],\"maxlen\":$D}"), STATION_NAME_SIZE);
}

/** Output stations data */
void server_json_stations(OTF_PARAMS_DEF) {
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
	rewind_ether_buffer();
	print_header(OTF_PARAMS);
#else
	print_header();
#endif

	bfill.emit_p(PSTR("{"));
	server_json_stations_main(OTF_PARAMS);
	handle_return(HTML_OK);
}

/** Output station special attribute */
void server_json_station_special(OTF_PARAMS_DEF) {
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
	rewind_ether_buffer();
	print_header(OTF_PARAMS);
#else
	print_header();
#endif

	unsigned char sid;
	unsigned char comma=0;
	StationData *data = (StationData*)tmp_buffer;

	bfill.emit_p(PSTR("{"));
	for(sid=0;sid<os.nstations;sid++) {
		unsigned char bid=sid>>3,s=sid&0x07;
		if(os.attrib_spe[bid]&(1<<s)) { // check if this is a special station
			os.get_station_data(sid, data);
			if (comma) bfill.emit_p(PSTR(","));
			else {comma=1;}
			bfill.emit_p(PSTR("\"$D\":{\"st\":$D,\"sd\":\"$S\"}"), sid, data->type, data->sped);
		}
		if (!buffer_available() ) {
			send_packet(OTF_PARAMS);
		}
	}
	bfill.emit_p(PSTR("}"));
	handle_return(HTML_OK);
}

#if defined(USE_OTF)
void server_change_board_attrib(const OTF::Request &req, char header, unsigned char *attrib)
#else
void server_change_board_attrib(char *p, char header, unsigned char *attrib)
#endif
{
	char tbuf2[5] = {0, 0, 0, 0, 0};
	unsigned char bid;
	tbuf2[0]=header;
	for(bid=0;bid<os.nboards;bid++) {
		snprintf(tbuf2+1, 3, "%d", bid);
		if(findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, tbuf2)) {
			attrib[bid] = atoi(tmp_buffer);
		}
	}
}

#if defined(USE_OTF)
void server_change_stations_attrib(const OTF::Request &req, char header, unsigned char *attrib)
#else
void server_change_stations_attrib(char *p, char header, unsigned char *attrib)
#endif
{
	char tbuf2[6] = {0, 0, 0, 0, 0, 0};
	unsigned char bid, s, sid;
	tbuf2[0]=header;
	for(bid=0;bid<os.nboards;bid++) {
		for(s=0;s<8;s++) {
			sid=bid*8+s;
			snprintf(tbuf2+1, 3, "%d", sid);
			if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, tbuf2)) {
				attrib[sid] = atoi(tmp_buffer);
			}
		}
	}
}

#if defined(USE_OTF)
void server_change_stations_attrib16(const OTF::Request &req, char header, uint16_t *attrib)
#else
void server_change_stations_attrib16(char *p, char header, uint16_t *attrib)
#endif
{
	char tbuf2[6] = {0, 0, 0, 0, 0, 0};
	unsigned char bid, s, sid;
	tbuf2[0]=header;
	for(bid=0;bid<os.nboards;bid++) {
		for(s=0;s<8;s++) {
			sid=bid*8+s;
			snprintf(tbuf2+1, 3, "%d", sid);
			if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, tbuf2)) {
				attrib[sid] = atoi(tmp_buffer);
			}
		}
	}
}

/**Change Station Name and Attributes
 * Command: /cs?pw=xxx&s?=x&m?=x&i?=x&n?=x&d?=x
 *
 * pw: password
 * s?: station name (? is station index, starting from 0)
 * m?: master operation bit field (? is board index, starting from 0)
 * i?: ignore rain bit field
 * n?: master2 operation bit field
 * d?: disable sation bit field
 * q?: station sequential bit field
 * p?: station special flag bit field
 * g?: sequential group id
 * f?: flow alert setpoint value
 */
void server_change_stations(OTF_PARAMS_DEF) {
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
#else
	char* p = get_buffer;
#endif

	unsigned char sid;
	char tbuf2[5] = {'s', 0, 0, 0, 0};
	// process station names
	for(sid=0;sid<os.nstations;sid++) {
		snprintf(tbuf2+1, 3, "%d", sid);
		if(findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, tbuf2)) {
			#if !defined(USE_OTF)
			urlDecode(tmp_buffer);
			#endif
			strReplaceQuoteBackslash(tmp_buffer);
			os.set_station_name(sid, tmp_buffer);
		}
	}

	server_change_board_attrib(FKV_SOURCE, 'm', os.attrib_mas); // master1
	server_change_board_attrib(FKV_SOURCE, 'i', os.attrib_igrd); // ignore rain delay
	server_change_board_attrib(FKV_SOURCE, 'j', os.attrib_igs); // ignore sensor1
	server_change_board_attrib(FKV_SOURCE, 'k', os.attrib_igs2); // ignore sensor2
	server_change_board_attrib(FKV_SOURCE, 'n', os.attrib_mas2); // master2
	server_change_board_attrib(FKV_SOURCE, 'd', os.attrib_dis); // disable
	server_change_stations_attrib(FKV_SOURCE, 'g', os.attrib_grp); // sequential groups
	server_change_stations_attrib16(FKV_SOURCE, 'f', os.attrib_fas); // flow alert setpoint
	server_change_stations_attrib16(FKV_SOURCE, 'a', os.attrib_favg); // flow avg values
	/* handle special data */
	if(findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("sid"), true)) {
		sid = atoi(tmp_buffer);
		if(sid<0 || sid>os.nstations) handle_return(HTML_DATA_OUTOFBOUND);
		if(findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("st"), true) &&
			findKeyVal(FKV_SOURCE, tmp_buffer+1, TMP_BUFFER_SIZE-1, PSTR("sd"), true)) {

			tmp_buffer[0]-='0';
			tmp_buffer[STATION_SPECIAL_DATA_SIZE] = 0;

			if(tmp_buffer[0] == STN_TYPE_GPIO) {
				// check that pin does not clash with OSPi pins
				unsigned char gpio = (tmp_buffer[1] - '0') * 10 + tmp_buffer[2] - '0';
				unsigned char activeState = tmp_buffer[3] - '0';

				unsigned char gpioList[] = PIN_FREE_LIST;
				bool found = false;
				for (unsigned char i = 0; i < sizeof(gpioList) && found == false; i++) {
					if (gpioList[i] == gpio) found = true;
				}
				if (!found || activeState > 1) {
					handle_return(HTML_DATA_OUTOFBOUND);
				}
			} else if ((tmp_buffer[0] == STN_TYPE_HTTP) || (tmp_buffer[0] == STN_TYPE_HTTPS) || (tmp_buffer[0] == STN_TYPE_REMOTE_OTC)) {
				if (strlen(tmp_buffer+1) > sizeof(HTTPStationData)) {
					handle_return(HTML_DATA_OUTOFBOUND);
				}
			}
			// write spe data
			file_write_block(STATIONS_FILENAME, tmp_buffer,
				(uint32_t)sid*sizeof(StationData)+offsetof(StationData,type), STATION_SPECIAL_DATA_SIZE+1);

		} else {

			handle_return(HTML_DATA_MISSING);

		}
	}
	// handle special attribute after parameters have been processed
	server_change_board_attrib(FKV_SOURCE, 'p', os.attrib_spe);

	os.attribs_save();
	handle_return(HTML_SUCCESS);
}

/** Parse one number from a comma separate list */
uint16_t parse_listdata(char **p) {
	char* pv;
	int i=0;
	tmp_buffer[i]=0;
	// copy to tmp_buffer until a non-number is encountered
	for(pv=(*p);pv<(*p)+10;pv++) {
		if ((*pv)=='-' || (*pv)=='+' || ((*pv)>='0'&&(*pv)<='9'))
			tmp_buffer[i++] = (*pv);
		else
			break;
	}
	tmp_buffer[i]=0;
	*p = pv+1;
	return (uint16_t)atol(tmp_buffer);
}

void manual_start_program(unsigned char, unsigned char);
/** Manual start program
 * Command: /mp?pw=xxx&pid=xxx&uwt=xxx
 *
 * pw:	password
 * pid: program index (0 refers to the first program)
 * uwt: use weather (i.e. watering percentage)
 */
void server_manual_program(OTF_PARAMS_DEF) {
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
#else
	char *p = get_buffer;
#endif

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("pid"), true))
		handle_return(HTML_DATA_MISSING);

	int pid=atoi(tmp_buffer);
	if (pid < 0 || pid >= pd.nprograms) {
		handle_return(HTML_DATA_OUTOFBOUND);
	}

	unsigned char uwt = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("uwt"), true)) {
		if(tmp_buffer[0]=='1') uwt = 1;
	}

	// reset all stations and prepare to run one-time program
	reset_all_stations_immediate();

	manual_start_program(pid+1, uwt);

	handle_return(HTML_SUCCESS);
}

/**
 * Change run-once program
 * Command: /cr?pw=xxx&t=[x,x,x...]&cnt?=xxx&int?=xxx&uwt?=xxx
 *
 * pw: password
 * t:  station water time
 * cnt?: repeat count
 * int?: repeat interval
 * uwt?: use weather adjustment
 */
void server_change_runonce(OTF_PARAMS_DEF) {
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
	if(!findKeyVal(FKV_SOURCE,tmp_buffer,TMP_BUFFER_SIZE, "t", false)) handle_return(HTML_DATA_MISSING);
	char *pv = tmp_buffer+1;
#else
	char *p = get_buffer;

	// decode url first
	#if !defined(USE_OTF)
	if(p) urlDecode(p);
	#endif
	// search for the start of t=[
	char *pv;
	boolean found=false;
	for(pv=p;(*pv)!=0 && pv<p+100;pv++) {
		if(strncmp(pv, "t=[", 3)==0) {
			found=true;
			break;
		}
	}
	if(!found)	handle_return(HTML_DATA_MISSING);
	pv+=3;
#endif

	// reset all stations and prepare to run one-time program
	reset_all_stations_immediate();

	ProgramStruct prog;

	uint16_t dur;
	for(int i=0;i<os.nstations;i++) {
		dur = parse_listdata(&pv);
		prog.durations[i] = dur > 0 ? dur : 0;
	}

	//check if repeat count is defined and create program to perform the repetitions
	if(findKeyVal(FKV_SOURCE,tmp_buffer,TMP_BUFFER_SIZE,PSTR("cnt"),true)){
		prog.starttimes[1] = (uint16_t)atol(tmp_buffer) - 1;
		if(prog.starttimes[1] >= 0){
			if(findKeyVal(FKV_SOURCE,tmp_buffer,TMP_BUFFER_SIZE,PSTR("int"),true)){
				prog.starttimes[2] = (uint16_t)atol(tmp_buffer);
			}else{
				handle_return(HTML_DATA_MISSING);
			}
			//check for positive interval length
			if(prog.starttimes[2] < 1){
				handle_return(HTML_DATA_OUTOFBOUND);
			}
			unsigned long curr_time = os.now_tz();

			curr_time = (curr_time / 60) + prog.starttimes[2] + 1; //time in minutes for one interval past current time
			uint16_t epoch_t = curr_time / 1440;

			//if repeat count and interval are defined --> complete program
			prog.enabled = 1;
			prog.use_weather = 0;
			if(findKeyVal(FKV_SOURCE,tmp_buffer,TMP_BUFFER_SIZE,PSTR("uwt"),true)){
				if((uint16_t)atol(tmp_buffer)){
					prog.use_weather = 1;
				}
			}
			prog.oddeven = 0;
			prog.type = 1;
			prog.starttime_type = 0;
			prog.en_daterange = 0;
			prog.days[0] = (epoch_t >> 8) & 0b11111111; //one interval past current day in epoch time
			prog.days[1] = epoch_t & 0b11111111; //one interval past current day in epoch time
			prog.starttimes[0] = curr_time % 1440; //one interval past current time
			strcpy_P(prog.name, "Run Once (Repeat)"); //TODO: Change name

			//if no more repeats, remove interval to flag for deletion
			if(prog.starttimes[1] == 0){
				prog.starttimes[2] = 0;
			}

			if(!pd.add(&prog)){
				handle_return(HTML_DATA_OUTOFBOUND);
			}
		}
	}

	//No repeat count defined or first repeat --> use old API
	unsigned char sid, bid, s;
	boolean match_found = false;
	for(sid=0;sid<os.nstations;sid++) {
		dur=prog.durations[sid];
		bid=sid>>3;
		s=sid&0x07;
		// if non-zero duration is given
		// and if the station has not been disabled
		if (dur>0 && !(os.attrib_dis[bid]&(1<<s))) {
			RuntimeQueueStruct *q = pd.enqueue();
			if (q) {
				q->st = 0;
				q->dur = water_time_resolve(dur);
				q->pid = 254;
				q->sid = sid;
				match_found = true;
			}
		}
	}
	if(match_found) {
		schedule_all_stations(os.now_tz());
		handle_return(HTML_SUCCESS);
	}

	handle_return(HTML_DATA_MISSING);
}


/**
 * Delete a program
 * Command: /dp?pw=xxx&pid=xxx
 *
 * pw: password
 * pid:program index (-1 will delete all programs)
 */
void server_delete_program(OTF_PARAMS_DEF) {
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
#else
	char *p = get_buffer;
#endif
	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("pid"), true))
		handle_return(HTML_DATA_MISSING);

	int pid=atoi(tmp_buffer);
	if (pid == -1) {
		pd.eraseall();
	} else if (pid < pd.nprograms) {
		pd.del(pid);
	} else {
		handle_return(HTML_DATA_OUTOFBOUND);
	}

	handle_return(HTML_SUCCESS);
}

/**
 * Move up a program
 * Command: /up?pw=xxx&pid=xxx
 *
 * pw:	password
 * pid: program index (must be 1 or larger, because we can't move up program 0)
*/
void server_moveup_program(OTF_PARAMS_DEF) {
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
#else
	char *p = get_buffer;
#endif

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("pid"), true))
		handle_return(HTML_DATA_MISSING);

	int pid=atoi(tmp_buffer);
	if (!(pid>=1 && pid< pd.nprograms))
		handle_return(HTML_DATA_OUTOFBOUND);

	pd.moveup(pid);

	handle_return(HTML_SUCCESS);
}

/**
 * Change a program
 * Command: /cp?pw=xxx&pid=x&v=[flag,days0,days1,[start0,start1,start2,start3],[dur0,dur1,dur2..]]
 *              &name=x&from=x&to=x
 *
 * pw:		password
 * pid:		program index
 * flag:	program flag
 * start?:up to 4 start times
 * dur?:	station water time
 * name:	program name
 * from:  start date of the program: an integer that's (month*32+day)
 * to:    end date of the program, same format as from
*/
const char _str_program[] PROGMEM = "Program ";
void server_change_program(OTF_PARAMS_DEF) {
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
#else
	char *p = get_buffer;
#endif

	unsigned char i;

	ProgramStruct prog;

	// parse program index
	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("pid"), true)) handle_return(HTML_DATA_MISSING);

	int pid=atoi(tmp_buffer);
	if (!(pid>=-1 && pid< pd.nprograms)) handle_return(HTML_DATA_OUTOFBOUND);

	// check if "en" parameter is present
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("en"), true)) {
		if(pid<0) handle_return(HTML_DATA_OUTOFBOUND);
		pd.set_flagbit(pid, PROGRAMSTRUCT_EN_BIT, (tmp_buffer[0]=='0')?0:1);
		handle_return(HTML_SUCCESS);
	}

	// check if "uwt" parameter is present
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("uwt"), true)) {
		if(pid<0) handle_return(HTML_DATA_OUTOFBOUND);
		pd.set_flagbit(pid, PROGRAMSTRUCT_UWT_BIT, (tmp_buffer[0]=='0')?0:1);
		handle_return(HTML_SUCCESS);
	}

	// parse program name
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("name"), true)) {
		#if !defined(USE_OTF)
		urlDecode(tmp_buffer);
		#endif
		strReplaceQuoteBackslash(tmp_buffer);
		strncpy(prog.name, tmp_buffer, PROGRAM_NAME_SIZE);
	} else {
		strcpy_P(prog.name, _str_program);
		snprintf(prog.name+8, PROGRAM_NAME_SIZE - 8, "%d", (pid==-1)? (pd.nprograms+1): (pid+1));
	}

	// parse program start date and end date
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("from"), true)) {
		int16_t date = atoi(tmp_buffer);
		if(!isValidDate(date)) handle_return(HTML_DATA_OUTOFBOUND);
		prog.daterange[0] = date;
		if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("to"), true)) {
			date = atoi(tmp_buffer);
			if(!isValidDate(date)) handle_return(HTML_DATA_OUTOFBOUND);
			prog.daterange[1] = date;
		} else {
			handle_return(HTML_DATA_MISSING);
		}
	}


#if !defined(USE_OTF)
	if(p) urlDecode(p);
#endif


#if defined(USE_OTF)
	if(!findKeyVal(FKV_SOURCE,tmp_buffer,TMP_BUFFER_SIZE, "v",false)) handle_return(HTML_DATA_MISSING);
	char *pv = tmp_buffer+1;
#else
	// parse ad-hoc v=[...
	// search for the start of v=[
	char *pv;
	boolean found=false;

	for(pv=p;(*pv)!=0 && pv<p+100;pv++) {
		if(strncmp(pv, "v=[", 3)==0) {
			found=true;
			break;
		}
	}

	if(!found)	handle_return(HTML_DATA_MISSING);
	pv+=3;
#endif

	// parse headers
	*(char*)(&prog) = parse_listdata(&pv);
	prog.days[0]= parse_listdata(&pv);
	prog.days[1]= parse_listdata(&pv);
	
	if (prog.type == PROGRAM_TYPE_INTERVAL) {
		if (prog.days[1] == 0) handle_return(HTML_DATA_OUTOFBOUND)
		else if (prog.days[1] >= 1) {
			// process interval day remainder (relative-> absolute)
			pd.drem_to_absolute(prog.days);
		}
	}

	// parse start times
	pv++; // this should be a '['
	for (i=0;i<MAX_NUM_STARTTIMES;i++) {
		prog.starttimes[i] = parse_listdata(&pv);
	}
	pv++; // this should be a ','
	pv++; // this should be a '['
	for (i=0;i<os.nstations;i++) {
		uint16_t pre = parse_listdata(&pv);
		prog.durations[i] = pre;
	}
	pv++; // this should be a ']'
	pv++; // this should be a ']'
	// parse program name

	// i should be equal to os.nstations at this point
	for(;i<MAX_NUM_STATIONS;i++) {
		prog.durations[i] = 0;		 // clear unused field
	}

	if (pid==-1) {
		if(!pd.add(&prog)) handle_return(HTML_DATA_OUTOFBOUND);
	} else {
		if(!pd.modify(pid, &prog)) handle_return(HTML_DATA_OUTOFBOUND);
	}
	handle_return(HTML_SUCCESS);
}

void server_json_options_main() {
	unsigned char oid;
	for(oid=0;oid<NUM_IOPTS;oid++) {
		#if !defined(ARDUINO) // do not send the following parameters for non-Arduino platforms
		if (oid==IOPT_USE_NTP			|| oid==IOPT_USE_DHCP		 ||
				(oid>=IOPT_STATIC_IP1	&& oid<=IOPT_STATIC_IP4) ||
				(oid>=IOPT_GATEWAY_IP1 && oid<=IOPT_GATEWAY_IP4) ||
				(oid>=IOPT_DNS_IP1 && oid<=IOPT_DNS_IP4) ||
				(oid>=IOPT_SUBNET_MASK1 && oid<=IOPT_SUBNET_MASK4) ||
				(oid==IOPT_FORCE_WIRED))
				continue;
		#endif

		#if !(defined(ESP8266) || defined(PIN_SENSOR2))
		// only OS 3.x or controllers that have PIN_SENSOR2 defined support sensor 2 options
		if (oid==IOPT_SENSOR2_TYPE || oid==IOPT_SENSOR2_OPTION || oid==IOPT_SENSOR2_ON_DELAY || oid==IOPT_SENSOR2_OFF_DELAY)
			continue;
		#endif

		int32_t v=os.iopts[oid];
		if (oid==IOPT_MASTER_OFF_ADJ || oid==IOPT_MASTER_OFF_ADJ_2 ||
				oid==IOPT_MASTER_ON_ADJ  || oid==IOPT_MASTER_ON_ADJ_2 ||
				oid==IOPT_STATION_DELAY_TIME) {
			v=water_time_decode_signed(v);
		}

		#if defined(ARDUINO)
		if (oid==IOPT_BOOST_TIME) {
			if (os.hw_type==HW_TYPE_AC || os.hw_type==HW_TYPE_UNKNOWN) continue;
			else v<<=2;
		}
		if (oid==IOPT_LATCH_ON_VOLTAGE || oid==IOPT_LATCH_OFF_VOLTAGE) {
			if (os.hw_type!=HW_TYPE_LATCH) continue;
		}
		#else
		if (oid==IOPT_BOOST_TIME || oid==IOPT_LATCH_ON_VOLTAGE || oid==IOPT_LATCH_OFF_VOLTAGE) continue;
		#endif

		#if defined(ESP8266)
		if (oid==IOPT_HW_VERSION) {
			v+=os.hw_rev;	// for OS3.x, add hardware revision number
		}
		#endif

		if (oid==IOPT_SEQUENTIAL_RETIRED || oid==IOPT_URS_RETIRED || oid==IOPT_RSO_RETIRED) continue;

#if defined(ARDUINO)
		#if defined(ESP8266)
		// for SSD1306, we can't adjust contrast or backlight
		if(oid==IOPT_LCD_CONTRAST || oid==IOPT_LCD_BACKLIGHT) continue;
		#else
		if (os.lcd.type() == LCD_I2C) {
			// for I2C type LCD, we can't adjust contrast or backlight
			if(oid==IOPT_LCD_CONTRAST || oid==IOPT_LCD_BACKLIGHT) continue;
		}
		#endif
#else
		// for Linux-based platforms, there is no LCD currently
		if(oid==IOPT_LCD_CONTRAST || oid==IOPT_LCD_BACKLIGHT || oid==IOPT_LCD_DIMMING) continue;
#endif

		// each json name takes 5 characters
		strncpy_P0(tmp_buffer, iopt_json_names+oid*5, 5);
		bfill.emit_p(PSTR("\"$S\":$D"), tmp_buffer, v);
		if(oid!=NUM_IOPTS-1)
			bfill.emit_p(PSTR(","));
	}

	//feature flag Analog Sensor API
	bfill.emit_p(PSTR(",\"feature\":\"ASB\""));

	bfill.emit_p(PSTR(",\"dexp\":$D,\"mexp\":$D,\"hwt\":$D,"), os.detect_exp(), MAX_EXT_BOARDS, os.hw_type);
	// print master array
	unsigned char masid, optidx;
	bfill.emit_p(PSTR("\"ms\":["));
	for (masid = 0; masid < NUM_MASTER_ZONES; masid++) {
		for (optidx = 0; optidx < NUM_MASTER_OPTS; optidx++) {
			bfill.emit_p(PSTR("$D"), os.masters[masid][optidx]);
			bfill.emit_p((masid == NUM_MASTER_ZONES - 1 && optidx == NUM_MASTER_OPTS - 1) ? PSTR("]}") : PSTR(","));
		}
	}
}

/** Output Options */
void server_json_options(OTF_PARAMS_DEF) {
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS,true)) return;
	rewind_ether_buffer();
	print_header(OTF_PARAMS);
#else
	print_header();
#endif
	bfill.emit_p(PSTR("{"));
	server_json_options_main();
	handle_return(HTML_OK);
}

void server_json_programs_main(OTF_PARAMS_DEF) {

	bfill.emit_p(PSTR("\"nprogs\":$D,\"nboards\":$D,\"mnp\":$D,\"mnst\":$D,\"pnsize\":$D,\"pd\":["),
							 pd.nprograms, os.nboards, MAX_NUM_PROGRAMS, MAX_NUM_STARTTIMES, PROGRAM_NAME_SIZE);
	unsigned char pid, i;
	ProgramStruct prog;
	for(pid=0;pid<pd.nprograms;pid++) {
		pd.read(pid, &prog);
		if (prog.type == PROGRAM_TYPE_INTERVAL && prog.days[1] >= 1) {
			pd.drem_to_relative(prog.days);
		}

		unsigned char bytedata = *(char*)(&prog);
		bfill.emit_p(PSTR("[$D,$D,$D,["), bytedata, prog.days[0], prog.days[1]);
		// start times data
		for (i=0;i<(MAX_NUM_STARTTIMES-1);i++) {
			bfill.emit_p(PSTR("$D,"), prog.starttimes[i]);
		}
		bfill.emit_p(PSTR("$D],["), prog.starttimes[i]);	// this is the last element
		// station water time
		for (i=0; i<os.nstations-1; i++) {
			bfill.emit_p(PSTR("$L,"),(unsigned long)prog.durations[i]);
		}
		bfill.emit_p(PSTR("$L],\""),(unsigned long)prog.durations[i]); // this is the last element
		// program name
		strncpy(tmp_buffer, prog.name, PROGRAM_NAME_SIZE);
		tmp_buffer[PROGRAM_NAME_SIZE] = 0;	// make sure the string ends
		bfill.emit_p(PSTR("$S\",[$D,$D,$D]]"), tmp_buffer,prog.en_daterange,prog.daterange[0],prog.daterange[1]);
		if(pid!=pd.nprograms-1) {
			bfill.emit_p(PSTR(","));
		}
		// push out a packet if available
		// buffer size is getting small
		if (!buffer_available()) {
			send_packet(OTF_PARAMS);
		}
	}
	bfill.emit_p(PSTR("]}"));
}

/** Output program data */
void server_json_programs(OTF_PARAMS_DEF) {
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
	rewind_ether_buffer();
	print_header(OTF_PARAMS);
#else
	print_header();
#endif
	bfill.emit_p(PSTR("{"));
	server_json_programs_main(OTF_PARAMS);
	handle_return(HTML_OK);
}

/** Output script url form */
void server_view_scripturl(OTF_PARAMS_DEF) {
	rewind_ether_buffer();
#if defined(USE_OTF)
	print_header(OTF_PARAMS,false,strlen(ether_buffer));
#else
	print_header(false);
#endif
	//bfill.emit_p(PSTR("<form name=of action=cu method=get><table cellspacing=12><tr><td><b>JavaScript</b>:</td><td><input type=text size=40 maxlength=$D value='$O' name=jsp></td></tr><tr><td>Default:</td><td>$S</td></tr><tr><td><b>Weather</b>:</td><td><input type=text size=40 maxlength=$D value='$O' name=wsp></td></tr><tr><td>Default:</td><td>$S</td></tr><tr><td><b>Password</b>:</td><td><input type=password size=32 name=pw> <input type=submit value=Submit></td></tr></table></form><script src=https://ui.opensprinkler.com/js/hasher.js></script>"),
	bfill.emit_p(PSTR(R"(<form name=of action=cu method=get><table cellspacing=12>
<tr><td><b>UI Source</b>:</td><td><input type=text size=40 maxlength=$D value='$O' id=jsp name=jsp></td></tr>
<tr><td></td><td><button type=button onclick='rst_jsp()'>Reset UI Source</button></td></tr>
<tr><td><b>Weather</b>:</td><td><input type=text size=40 maxlength=$D value='$O' id=wsp name=wsp></td></tr>
<tr><td></td><td><button type=button onclick='rst_wsp()'>Reset Weather Server</button></td></tr>
<tr><td><b>Password</b>:</td><td><input type=password size=32 name=pw><input type=submit value=submit></tr>
</table></form>
<script src=https://ui.opensprinkler.com/js/hasher.js></script>
<script>function rst_jsp() {document.getElementById('jsp').value='$S';}
function rst_wsp() {document.getElementById('wsp').value='$S';}</script>)"),
	MAX_SOPTS_SIZE, SOPT_JAVASCRIPTURL, MAX_SOPTS_SIZE, SOPT_WEATHERURL, DEFAULT_JAVASCRIPT_URL, DEFAULT_WEATHER_URL);
	handle_return(HTML_OK);
}

void server_json_controller_main(OTF_PARAMS_DEF) {
	unsigned char bid, sid;
	time_os_t curr_time = os.now_tz();
	bfill.emit_p(PSTR("\"devt\":$L,\"nbrd\":$D,\"en\":$D,\"sn1\":$D,\"sn2\":$D,\"rd\":$D,\"rdst\":$L,"
										"\"sunrise\":$D,\"sunset\":$D,\"eip\":$L,\"lwc\":$L,\"lswc\":$L,"
										"\"lupt\":$L,\"lrbtc\":$D,\"lrun\":[$D,$D,$D,$L],\"pq\":$D,\"pt\":$L,\"nq\":$D,"),
							curr_time,
							os.nboards,
							os.status.enabled,
							os.status.sensor1_active,
							os.status.sensor2_active,
							os.status.rain_delayed,
							os.nvdata.rd_stop_time,
							os.nvdata.sunrise_time,
							os.nvdata.sunset_time,
							os.nvdata.external_ip,
							os.checkwt_lasttime,
							os.checkwt_success_lasttime,
							os.powerup_lasttime,
							os.last_reboot_cause,
							pd.lastrun.station,
							pd.lastrun.program,
							pd.lastrun.duration,
							pd.lastrun.endtime,
							os.status.pause_state,
							os.pause_timer,
							pd.nqueue);

#if defined(ESP8266)
	bfill.emit_p(PSTR("\"RSSI\":$D,"), (int16_t)WiFi.RSSI());
#endif

#if defined(USE_OTF)
	bfill.emit_p(PSTR("\"otc\":{$O},\"otcs\":$D,"), SOPT_OTC_OPTS, otf->getCloudStatus());
#endif

	unsigned char mac[6] = {0};
#if defined(ARDUINO)
	os.load_hardware_mac(mac, useEth);
#else
	os.load_hardware_mac(mac, true);
#endif
	char mqtt_opt[MAX_SOPTS_SIZE];
	os.sopt_load(SOPT_MQTT_OPTS, mqtt_opt);
	//DEBUG_PRINTLN(mqtt_opt);

	//Test for invalid mqtt options:
	int l = strlen(mqtt_opt);
	if (l > 0 && mqtt_opt[l-1] != '"') { //first+last char
		mqtt_opt[l] = '"';
		mqtt_opt[l+1] = 0;
	}
	//DEBUG_PRINTLN(mqtt_opt);

	bfill.emit_p(PSTR("\"mac\":\"$X:$X:$X:$X:$X:$X\","), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	bfill.emit_p(PSTR("\"loc\":\"$O\",\"jsp\":\"$O\",\"wsp\":\"$O\",\"wto\":{$O},\"ifkey\":\"$O\",\"mqtt\":{$S},\"wtdata\":$S,\"wterr\":$D,\"dname\":\"$O\","),
							 SOPT_LOCATION,
							 SOPT_JAVASCRIPTURL,
							 SOPT_WEATHERURL,
							 SOPT_WEATHER_OPTS,
							 SOPT_IFTTT_KEY,
							 mqtt_opt,
							 strlen(wt_rawData)==0?"{}":wt_rawData,
							 wt_errCode,
							 SOPT_DEVICE_NAME);

#if defined(SUPPORT_EMAIL)
	bfill.emit_p(PSTR("\"email\":{$O},"), SOPT_EMAIL_OPTS);
#endif

#if defined(ARDUINO)
	if(os.status.has_curr_sense) {
		uint16_t current = os.read_current();
		if((!os.status.program_busy) && (current<os.baseline_current)) current=0;
		bfill.emit_p(PSTR("\"curr\":$D,"), current);
	}
#endif
	if(os.iopts[IOPT_SENSOR1_TYPE]==SENSOR_TYPE_FLOW) {
		bfill.emit_p(PSTR("\"flcrt\":$L,\"flwrt\":$D,"), os.flowcount_rt, FLOWCOUNT_RT_WINDOW);
	}

	bfill.emit_p(PSTR("\"sbits\":["));
	// print sbits
	for(bid=0;bid<os.nboards;bid++)
		bfill.emit_p(PSTR("$D,"), os.station_bits[bid]);
	bfill.emit_p(PSTR("0],\"ps\":["));
	// print ps
	for(sid=0;sid<os.nstations;sid++) {
		// if available ether buffer is getting small
		// send out a packet
		if(!buffer_available()) {
			send_packet(OTF_PARAMS);
		}
		unsigned long rem = 0;
		unsigned char qid = pd.station_qid[sid];
		RuntimeQueueStruct *q = pd.queue + qid;
		if (qid<255) {
			rem = (curr_time >= q->st) ? (q->st+q->dur-curr_time) : q->dur;
			if(rem>65535) rem = 0;
		}
		bfill.emit_p(PSTR("[$D,$L,$L,$D]"),
		(qid<255)?q->pid:0, rem, (qid<255)?q->st:0, os.attrib_grp[sid]);
		bfill.emit_p((sid<os.nstations-1)?PSTR(","):PSTR("]"));
	}

	unsigned char gpioList[] = PIN_FREE_LIST;
	bfill.emit_p(PSTR(",\"gpio\":["));
	for (unsigned char i = 0; i < sizeof(gpioList); ++i)
	{
		if(i != sizeof(gpioList) - 1) {
			bfill.emit_p(PSTR("$D,"), gpioList[i]);
		} else {
			bfill.emit_p(PSTR("$D"), gpioList[i]);
		}
	}
	bfill.emit_p(PSTR("]"));

	//influxdb
	if(!buffer_available()) {
		send_packet(OTF_PARAMS);
	}
	bfill.emit_p(PSTR(",\"influxdb\":"));
	server_influx_get_main();
	//end influxdb


	bfill.emit_p(PSTR("}"));
}

/** Output controller variables in json */
void server_json_controller(OTF_PARAMS_DEF) {
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
	rewind_ether_buffer();
	print_header(OTF_PARAMS);
#else
	print_header();
#endif

	bfill.emit_p(PSTR("{"));
	server_json_controller_main(OTF_PARAMS);
	handle_return(HTML_OK);
}

/** Output homepage */
void server_home(OTF_PARAMS_DEF)
{
	rewind_ether_buffer();
#if defined(USE_OTF)
	print_header(OTF_PARAMS,false,strlen(ether_buffer));
#else
	print_header(false);
#endif
	bfill.emit_p(PSTR("<!DOCTYPE html><html><head>$F</head><body><script>"), htmlMobileHeader);
	// send server variables and javascript packets
	bfill.emit_p(PSTR("var ver=$D,ipas=$D;</script>"),
							 OS_FW_VERSION, os.iopts[IOPT_IGNORE_PASSWORD]);

	bfill.emit_p(PSTR("<script src=\"$O/home.js\"></script></body></html>"), SOPT_JAVASCRIPTURL);

	handle_return(HTML_OK);
}

/**
 * Change controller variables
 * Command: /cv?pw=xxx&rsn=x&rbt=x&en=x&rd=x&re=x&ap=x
 *
 * pw:	password
 * rsn: reset all stations (0 or 1)
 * rbt: reboot controller (0 or 1)
 * en:	enable (0 or 1)
 * rd:	rain delay hours (0 turns off rain delay)
 * re:	remote extension mode
 * ap:	reset to ap (ESP8266 only)
 * update: launch update script (for OSPi/Linux only)
 */
void server_change_values(OTF_PARAMS_DEF)
{
#if defined(USE_OTF)
	extern uint32_t reboot_timer;
	if(!process_password(OTF_PARAMS)) return;
#else
	char *p = get_buffer;
#endif
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("rsn"), true)) {
		reset_all_stations();
	}

#if !defined(ARDUINO)
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("update"), true) && atoi(tmp_buffer) > 0) {
		os.update_dev();
	}
#endif

	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("rbt"), true) && atoi(tmp_buffer) > 0) {
		#if defined(USE_OTF)
			os.status.safe_reboot = 0;
			reboot_timer = os.now_tz() + 1;
			handle_return(HTML_SUCCESS);
		#else
			print_header(false);
			//bfill.emit_p(PSTR("Rebooting..."));
			send_packet();
			m_client->stop();
			os.reboot_dev(REBOOT_CAUSE_WEB);
		#endif
	}

	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("en"), true)) {
		if (tmp_buffer[0]=='1' && !os.status.enabled)  os.enable();
		else if (tmp_buffer[0]=='0' &&	os.status.enabled)	os.disable();
	}

	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("rd"), true)) {
		int rd = atoi(tmp_buffer);
		if (rd>0) {
			os.nvdata.rd_stop_time = os.now_tz() + (unsigned long) rd * 3600;
			os.raindelay_start();
		} else if (rd==0){
			os.raindelay_stop();
		} else	handle_return(HTML_DATA_OUTOFBOUND);
	}

	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("re"), true)) {
		if (tmp_buffer[0]=='1' && !os.iopts[IOPT_REMOTE_EXT_MODE]) {
			os.iopts[IOPT_REMOTE_EXT_MODE] = 1;
			os.iopts_save();
		} else if(tmp_buffer[0]=='0' && os.iopts[IOPT_REMOTE_EXT_MODE]) {
			os.iopts[IOPT_REMOTE_EXT_MODE] = 0;
			os.iopts_save();
		}
	}

	#if defined(ESP8266)
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("ap"), true)) {
		os.reset_to_ap();
	}
	#endif
	handle_return(HTML_SUCCESS);
}

// remove spaces from a string
void string_remove_space(char *src) {
	char *dst = src;
	while(1) {
		if (*src != ' ') {
			*dst++ = *src;
		}
		if (*src == 0) break;
		src++;
	}
}

/**
 * Change script url
 * Command: /cu?pw=xxx&jsp=x
 *
 * pw:	password
 * jsp: Javascript path
 */
void server_change_scripturl(OTF_PARAMS_DEF) {
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
#else
	char *p = get_buffer;
#endif

#if defined(DEMO)
	handle_return(HTML_REDIRECT_HOME);
#endif
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("jsp"), true)) {
		tmp_buffer[TMP_BUFFER_SIZE-1]=0;	// make sure we don't exceed the maximum size
		#if !defined(USE_OTF)
		urlDecode(tmp_buffer);
		#endif
		// trim unwanted space characters
		string_remove_space(tmp_buffer);
		os.sopt_save(SOPT_JAVASCRIPTURL, tmp_buffer);
	}
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("wsp"), true)) {
		tmp_buffer[TMP_BUFFER_SIZE-1]=0;
		#if !defined(USE_OTF)
		urlDecode(tmp_buffer);
		#endif
		string_remove_space(tmp_buffer);
		os.sopt_save(SOPT_WEATHERURL, tmp_buffer);
	}
#if defined(USE_OTF)
	rewind_ether_buffer();
	print_header(OTF_PARAMS,false,strlen(ether_buffer));
	bfill.emit_p(PSTR("$F"), htmlReturnHome);
	handle_return(HTML_OK);
#else
	handle_return(HTML_REDIRECT_HOME);
#endif
}

/**
 * Change options
 * Command: /co?pw=xxx&o?=x&loc=x&ttt=x
 *
 * pw:	password
 * o?:	option name (? is option index)
 * loc: location
 * ttt: manual time (applicable only if ntp=0)
 */
void server_change_options(OTF_PARAMS_DEF)
{
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
#else
	char *p = get_buffer;
#endif

	// temporarily save some old options values
	bool time_change = false;
	bool weather_change = false;
	bool sensor_change = false;

	// !!! p and bfill share the same buffer, so don't write
	// to bfill before you are done analyzing the buffer !!!
	// process option values
	unsigned char err = 0;
	unsigned char prev_value;
	unsigned char max_value;
	for (unsigned char oid=0; oid<NUM_IOPTS; oid++) {

		// skip options that cannot be set through /co command
		if (oid==IOPT_FW_VERSION || oid==IOPT_HW_VERSION || oid==IOPT_SEQUENTIAL_RETIRED ||
				oid==IOPT_DEVICE_ENABLE || oid==IOPT_FW_MINOR || oid==IOPT_REMOTE_EXT_MODE ||
				oid==IOPT_RESET || oid==IOPT_WIFI_MODE || oid==IOPT_URS_RETIRED || oid==IOPT_RSO_RETIRED)
			continue;
		prev_value = os.iopts[oid];
		max_value = pgm_read_byte(iopt_max+oid);

		// will no longer support oxx option names
		// json name only
		char tbuf2[6];
		strncpy_P0(tbuf2, iopt_json_names+oid*5, 5);
		if(findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, tbuf2)) {
			int32_t v = atol(tmp_buffer);
			if (oid==IOPT_MASTER_OFF_ADJ || oid==IOPT_MASTER_OFF_ADJ_2 ||
					oid==IOPT_MASTER_ON_ADJ  || oid==IOPT_MASTER_ON_ADJ_2  ||
					oid==IOPT_STATION_DELAY_TIME) {
				v=water_time_encode_signed(v);
			} // encode station delay time
			if(oid==IOPT_BOOST_TIME) {
				 v>>=2;
			}
			if (oid==IOPT_NOTIF_ENABLE) {
				set_notif_enabled(v);
				continue;
			}

			if (v>=0 && v<=max_value) {
				os.iopts[oid] = v;
			} else {
				err = 1;
			}
		}

		if (os.iopts[oid] != prev_value) {	// if value has changed
			if (oid==IOPT_TIMEZONE || oid==IOPT_USE_NTP)		time_change = true;
			if (oid>=IOPT_NTP_IP1 && oid<=IOPT_NTP_IP4)			time_change = true;
			if (oid==IOPT_USE_WEATHER) weather_change = true;
			if (oid>=IOPT_SENSOR1_TYPE && oid<=IOPT_SENSOR2_OFF_DELAY) sensor_change = true;
		}
	}

	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("loc"), true)) {
		#if !defined(USE_OTF)
		urlDecode(tmp_buffer);
		#endif
		strReplaceQuoteBackslash(tmp_buffer);
		if (os.sopt_save(SOPT_LOCATION, tmp_buffer)) { // if location string has changed
			weather_change = true;
		}
	}
	uint8_t keyfound = 0;
	if(findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("wto"), true)) {
		#if !defined(USE_OTF)
		urlDecode(tmp_buffer);
		#endif
		if (os.sopt_save(SOPT_WEATHER_OPTS, tmp_buffer)) {
			if(os.iopts[IOPT_USE_WEATHER]==WEATHER_METHOD_MONTHLY) {
				load_wt_monthly(tmp_buffer);
				apply_monthly_adjustment(os.now_tz());
			} else {
				weather_change = true;	// if wto has changed
			}
		}
	}

	keyfound = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("ifkey"), true, &keyfound)) {
		#if !defined(USE_OTF)
		urlDecode(tmp_buffer);
		#endif
		os.sopt_save(SOPT_IFTTT_KEY, tmp_buffer);
	} else if (keyfound) {
		tmp_buffer[0]=0;
		os.sopt_save(SOPT_IFTTT_KEY, tmp_buffer);
	}

	keyfound = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("otc"), true, &keyfound)) {
		#if !defined(USE_OTF)
		urlDecode(tmp_buffer);
		#endif
		os.sopt_save(SOPT_OTC_OPTS, tmp_buffer);
	} else if (keyfound) {
		tmp_buffer[0]=0;
		os.sopt_save(SOPT_OTC_OPTS, tmp_buffer);
	}

	keyfound = 0;
	if(findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("mqtt"), true, &keyfound)) {
		#if !defined(USE_OTF)
		urlDecode(tmp_buffer);
		#endif
		DEBUG_PRINTLN("MQTT:");
		DEBUG_PRINTLN(tmp_buffer);
		os.sopt_save(SOPT_MQTT_OPTS, tmp_buffer);
		os.status.req_mqtt_restart = true;
	} else if (keyfound) {
		tmp_buffer[0]=0;
		os.sopt_save(SOPT_MQTT_OPTS, tmp_buffer);
		os.status.req_mqtt_restart = true;
	}

	//influxdb set
	keyfound = 0;
	if(findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("influxdb"), true, &keyfound)) {
		#if !defined(USE_OTF)
		urlDecode(tmp_buffer);
		#endif
		os.influxdb.set_influx_config(tmp_buffer);
	} else if (keyfound) {
		tmp_buffer[0]=0;
		os.influxdb.set_influx_config(tmp_buffer);
	}
	//end influxdb set

	keyfound = 0;
	if(findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("email"), true, &keyfound)) {
		#if !defined(USE_OTF)
		urlDecode(tmp_buffer);
		#endif
		os.sopt_save(SOPT_EMAIL_OPTS, tmp_buffer);
	} else if (keyfound) {
		tmp_buffer[0]=0;
		os.sopt_save(SOPT_EMAIL_OPTS, tmp_buffer);
	}

	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("dname"), true)) {
		#if !defined(USE_OTF)
		urlDecode(tmp_buffer);
		#endif
		strReplaceQuoteBackslash(tmp_buffer);
		os.sopt_save(SOPT_DEVICE_NAME, tmp_buffer);
	}

	// if not using NTP and manually setting time
	if (!os.iopts[IOPT_USE_NTP] && findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("ttt"), true)) {
#if defined(ARDUINO)
		unsigned long t;
		t = strtoul(tmp_buffer, NULL, 0);
#endif
		// before chaging time, reset all stations to avoid messing up with timing
		reset_all_stations_immediate();
#if defined(ARDUINO)
		setTime(t);
		RTC.set(t);
#endif
	}
	if (err)	handle_return(HTML_DATA_OUTOFBOUND);

	os.iopts_save();
	os.populate_master();

	if(time_change) {
		os.status.req_ntpsync = 1;
	}

	if(weather_change) {
		os.iopts[IOPT_WATER_PERCENTAGE] = 100;  // reset watering percentage to 100%
		wt_rawData[0] = 0;  // reset wt_rawData and errCode
		wt_errCode = HTTP_RQT_NOT_RECEIVED;
		os.checkwt_lasttime = 0;  // force weather update
	}

	if(sensor_change) {
		os.sensor_resetall();
	}

	handle_return(HTML_SUCCESS);
}

/**
 * Change password
 * Command: /sp?pw=xxx&npw=x&cpw=x
 *
 * pw:	password
 * npw: new password
 * cpw: confirm new password
 */
void server_change_password(OTF_PARAMS_DEF) {
#if defined(DEMO)
	handle_return(HTML_SUCCESS);  // do not allow chnaging password for demo
	return;
#endif

#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
#else
	char* p = get_buffer;
#endif
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("npw"), true)) {
		const int pwBufferSize = TMP_BUFFER_SIZE/2;
		char *tbuf2 = tmp_buffer + pwBufferSize;	// use the second half of tmp_buffer 
		if (findKeyVal(FKV_SOURCE, tbuf2, pwBufferSize, PSTR("cpw"), true) && strncmp(tmp_buffer, tbuf2, pwBufferSize) == 0) {
			os.sopt_save(SOPT_PASSWORD, tmp_buffer);
			handle_return(HTML_SUCCESS);
		} else {
			handle_return(HTML_MISMATCH);
		}
	}
	handle_return(HTML_DATA_MISSING);
}

void server_json_status_main() {
	bfill.emit_p(PSTR("\"sn\":["));
	unsigned char sid;

	for (sid=0;sid<os.nstations;sid++) {
		bfill.emit_p(PSTR("$D"), (os.station_bits[(sid>>3)]>>(sid&0x07))&1);
		if(sid!=os.nstations-1) bfill.emit_p(PSTR(","));
	}
	bfill.emit_p(PSTR("],\"nstations\":$D}"), os.nstations);
}

/** Output station status */
void server_json_status(OTF_PARAMS_DEF)
{
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
	rewind_ether_buffer();
	print_header(OTF_PARAMS);
#else
	print_header();
#endif

	bfill.emit_p(PSTR("{"));
	server_json_status_main();
	handle_return(HTML_OK);
}

/**
 * Test station (previously manual operation)
 * Command: /cm?pw=xxx&sid=x&en=x&t=x&ssta=x
 *
 * pw: password
 * sid:station index (starting from 0)
 * en: enable (0 or 1)
 * t:  timer (required if en=1)
 * ssta: shift remaining stations
 */
void server_change_manual(OTF_PARAMS_DEF) {
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
#else
	char *p = get_buffer;
#endif

	int sid=-1;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("sid"), true)) {
		sid=atoi(tmp_buffer);
		if (sid<0 || sid>=os.nstations) handle_return(HTML_DATA_OUTOFBOUND);
	} else {
		handle_return(HTML_DATA_MISSING);
	}

	unsigned char en=0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("en"), true)) {
		en=atoi(tmp_buffer);
	} else {
		handle_return(HTML_DATA_MISSING);
	}

	uint16_t timer=0;
	unsigned long curr_time = os.now_tz();
	if (en) { // if turning on a station, must provide timer
		if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("t"), true)) {
			timer=(uint16_t)atol(tmp_buffer);
			if (timer==0 || timer>64800) {
				handle_return(HTML_DATA_OUTOFBOUND);
			}
			// schedule manual station
			// skip if the station is a master station
			// (because master cannot be scheduled independently)
			if ((os.status.mas==sid+1) || (os.status.mas2==sid+1))
				handle_return(HTML_NOT_PERMITTED);

			RuntimeQueueStruct *q = NULL;
			unsigned char sqi = pd.station_qid[sid];
			// check if the station already has a schedule
			if (sqi!=0xFF) {  // if so, we will overwrite the schedule
				q = pd.queue+sqi;
			} else {  // otherwise create a new queue element
				q = pd.enqueue();
			}
			// if the queue is not full
			if (q) {
				q->st = 0;
				q->dur = timer;
				q->sid = sid;
				q->pid = 99;  // testing stations are assigned program index 99
				schedule_all_stations(curr_time);
			} else {
				handle_return(HTML_NOT_PERMITTED);
			}
		} else {
			handle_return(HTML_DATA_MISSING);
		}
	} else {	// turn off station
		unsigned char ssta = 0;
		if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("ssta"), true)) {
			ssta = atoi(tmp_buffer);
		}
		// mark station for removal
		RuntimeQueueStruct *q = pd.queue + pd.station_qid[sid];
		q->deque_time = curr_time;
		turn_off_station(sid, curr_time, ssta);
	}
	handle_return(HTML_SUCCESS);
}


#if defined(ESP8266)
int file_fgets(File file, char* buf, int maxsize) {
	int index=0;
	while(index<maxsize) {
		int c = file.read();
		if(c<0||c=='\n') break;
		if(c=='\r') continue; // skip \r
		*buf++ = (char)c;
		index++;
	}
	return index;
}
#endif

/**
 * Get log data
 * Command: /jl?start=x&end=x&hist=x&type=x
 *
 * hist:  history (past n days)
 *        when hist is speceified, the start
 *        and end parameters below will be ignored
 * start: start time (epoch time)
 * end:   end time (epoch time)
 * type:  type of log records (optional)
 *        rs, rd, wl
 *        if unspecified, output all records
 */
void server_json_log(OTF_PARAMS_DEF) {

#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
#else
	char *p = get_buffer;
#endif

	unsigned int start, end;

	// past n day history
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("hist"), true)) {
		int hist = atoi(tmp_buffer);
		if (hist< 0 || hist > 365) handle_return(HTML_DATA_OUTOFBOUND);
		end = os.now_tz() / 86400L;
		start = end - hist;
	}
	else
	{
		if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("start"), true)) handle_return(HTML_DATA_MISSING);

		start = strtoul(tmp_buffer, NULL, 0) / 86400L;

		if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("end"), true)) handle_return(HTML_DATA_MISSING);

		end = strtoul(tmp_buffer, NULL, 0) / 86400L;

		// start must be prior to end, and can't retrieve more than 365 days of data
		if ((start>end) || (end-start)>365)  handle_return(HTML_DATA_OUTOFBOUND);
	}

	// extract the type parameter
	char type[4] = {0};
	bool type_specified = false;
	if (findKeyVal(FKV_SOURCE, type, 4, PSTR("type"), true))
		type_specified = true;

#if defined(USE_OTF)
	// as the log data can be large, we will use ESP8266's sendContent function to
	// send multiple packets of data, instead of the standard way of using send().
	rewind_ether_buffer();
	print_header(OTF_PARAMS);
#else
	print_header();
#endif

	bfill.emit_p(PSTR("["));

	bool comma = 0;
	for(unsigned int i=start;i<=end;i++) {
		snprintf(tmp_buffer, TMP_BUFFER_SIZE*2 , "%d", i);
		make_logfile_name(tmp_buffer);

#if defined(ESP8266)
		File file = LittleFS.open(tmp_buffer, "r");
		if(!file) continue;
#elif defined(ARDUINO)
		if (!sd.exists(tmp_buffer)) continue;
		SdFile file;
		file.open(tmp_buffer, O_READ);
#else // prepare to open log file for Linux
		FILE *file = fopen(get_filename_fullpath(tmp_buffer), "rb");
		if(!file) continue;
#endif // prepare to open log file
		int result;
		while(true) {
		#if defined(ESP8266)
			// do not use file.read_byte or read_byteUntil because it's very slow
			result = file_fgets(file, tmp_buffer, TMP_BUFFER_SIZE);
			if (result <= 0) {
				file.close();
				break;
			}
			tmp_buffer[result]=0;
		#elif defined(ARDUINO)
			result = file.fgets(tmp_buffer, TMP_BUFFER_SIZE);
			if (result <= 0) {
				file.close();
				break;
			}
		#else
			if(fgets(tmp_buffer, TMP_BUFFER_SIZE, file)) {
				result = strlen(tmp_buffer);
			} else {
				result = 0;
			}
			if (result <= 0) {
				fclose(file);
				break;
			}
		#endif
			// check record type
			// records are all in the form of [x,"xx",...]
			// where x is program index (>0) if this is a station record
			// and "xx" is the type name if this is a special record (e.g. wl, fl, rs)

			// search string until we find the first comma
			char *ptype = tmp_buffer;
			tmp_buffer[TMP_BUFFER_SIZE-1]=0; // make sure the search will end
			while(*ptype && *ptype != ',') ptype++;
			if(*ptype != ',') continue; // didn't find comma, move on
			ptype++;  // move past comma

			if (type_specified && strncmp(type, ptype+1, 2))
				continue;
			// if type is not specified, output everything except "wl" and "fl" records
			if (!type_specified && (!strncmp("wl", ptype+1, 2) || !strncmp("fl", ptype+1, 2)))
				continue;
			// if this is the first record, do not print comma
			if (comma)	bfill.emit_p(PSTR(","));
			else {comma=1;}
			bfill.emit_p(PSTR("$S"), tmp_buffer);
			// if the available ether buffer size is getting small
			// push out a packet
			if (!buffer_available()) {
				send_packet(OTF_PARAMS);
			}
		}
	}

	bfill.emit_p(PSTR("]"));
	handle_return(HTML_OK);
}
/**
 * Delete log
 * Command: /dl?pw=xxx&day=xxx
 *          /dl?pw=xxx&day=all
 *
 * pw: password
 * day:day (epoch time / 86400)
 * if day=all: delete all log files)
 */
void server_delete_log(OTF_PARAMS_DEF) {
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
#else
	char *p = get_buffer;
#endif

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("day"), true))
		handle_return(HTML_DATA_MISSING);

	delete_log(tmp_buffer);

	handle_return(HTML_SUCCESS);
}

/**
 * Command: "/pq?pw=x&dur=x&repl=x"
 * dur: duration (in units of seconds)
 * repl: replace (in units of seconds) (New UI allows for replace, extend, and pause using this)
 */
void server_pause_queue(OTF_PARAMS_DEF) {
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
#else
	char *p = get_buffer;
#endif

	ulong duration = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("repl"), true)) {
		duration = strtoul(tmp_buffer, NULL, 0);
		pd.resume_stations();
		os.status.pause_state = 0;
		if(duration != 0){
			os.pause_timer = duration;
			pd.set_pause();
			os.status.pause_state = 1;
		}
		
		handle_return(HTML_SUCCESS);
	}

	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("dur"), true)) {
		duration = strtoul(tmp_buffer, NULL, 0);
	}

	pd.toggle_pause(duration);

	handle_return(HTML_SUCCESS);
}

/** Output all JSON data, including jc, jp, jo, js, jn */
void server_json_all(OTF_PARAMS_DEF) {
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS,true)) return;
	rewind_ether_buffer();
	print_header(OTF_PARAMS);
#else
	print_header();
#endif
	bfill.emit_p(PSTR("{\"settings\":{"));
	server_json_controller_main(OTF_PARAMS);
	send_packet(OTF_PARAMS);
	bfill.emit_p(PSTR(",\"programs\":{"));
	server_json_programs_main(OTF_PARAMS);
	send_packet(OTF_PARAMS);
	bfill.emit_p(PSTR(",\"options\":{"));
	server_json_options_main();
	send_packet(OTF_PARAMS);
	bfill.emit_p(PSTR(",\"status\":{"));
	server_json_status_main();
	send_packet(OTF_PARAMS);
	bfill.emit_p(PSTR(",\"stations\":{"));
	server_json_stations_main(OTF_PARAMS);
	bfill.emit_p(PSTR("}"));
	handle_return(HTML_OK);
}

#if defined(ARDUINO)

#if defined(OS_AVR)
static int freeHeap () {
	extern int __heap_start, *__brkval;
	int v;
	return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}
#endif
#else
#include <sys/sysinfo.h>
static unsigned long freeHeap() {
	//return sysconf(_SC_AVPHYS_PAGES) * sysconf(_SC_PAGESIZE);
	struct sysinfo info;
	if (sysinfo(&info) == 0) {
		return info.freeram;
	} else {
		return 0;
	}
}
#endif

void server_json_debug(OTF_PARAMS_DEF) {
#if defined(USE_OTF)
	rewind_ether_buffer();
	print_header(OTF_PARAMS);
#else
	print_header();
#endif
	bfill.emit_p(PSTR("{\"date\":\"$S\",\"time\":\"$S\",\"heap\":$L"), __DATE__, __TIME__,
#if defined(ESP8266)
	(unsigned long)ESP.getFreeHeap());
	FSInfo fs_info;
	LittleFS.info(fs_info);
	bfill.emit_p(PSTR(",\"flash\":$D,\"used\":$D,"), fs_info.totalBytes, fs_info.usedBytes);
	if(useEth) {
		bfill.emit_p(PSTR("\"isW5500\":$D}"), eth.isW5500);
	} else {
		bfill.emit_p(PSTR("\"rssi\":$D,\"bssid\":\"$S\",\"bssidchl\":\"$O\"}"),
		WiFi.RSSI(), WiFi.BSSIDstr().c_str(), SOPT_STA_BSSID_CHL);
	}

/*
// print out all log files and all files in the main folder with file sizes
	DEBUG_PRINTLN(F("List Files:"));
	Dir dir = LittleFS.openDir("/logs/");
	while (dir.next()) {
		DEBUG_PRINT(dir.fileName());
		DEBUG_PRINT("/");
		DEBUG_PRINTLN(dir.fileSize());
	}
	dir = LittleFS.openDir("/");
	while (dir.next()) {
		DEBUG_PRINT(dir.fileName());
		DEBUG_PRINT("/");
		DEBUG_PRINTLN(dir.fileSize());
	}
*/
#else
	(unsigned long)freeHeap());
	bfill.emit_p(PSTR("}"));
#endif
	handle_return(HTML_OK);
}

/*
// fill ESP8266 flash with some dummy files
void server_fill_files(OTF_PARAMS_DEF) {
	memset(ether_buffer, 65, 75);
	ether_buffer[75] = 0;
	FSInfo fs_info;
	for(int index=1;index<64;index++) {
		snprintf(tmp_buffer, TMP_BUFFER_SIZE*2 , "%d", index);
		make_logfile_name(tmp_buffer);
		DEBUG_PRINT(F("creating "));
		DEBUG_PRINT(tmp_buffer);
		File file = LittleFS.open(tmp_buffer, "w");
		file.write(ether_buffer, strlen(ether_buffer));
		file.close();
		DEBUG_PRINTLN(F(" done. "));
		LittleFS.info(fs_info);
		DEBUG_PRINTLN(fs_info.usedbytes);
	}
	handle_return(HTML_SUCCESS);
}
*/

char* urlDecodeAndUnescape(char *buf) {
	#if !defined(USE_OTF)
	urlDecode(buf);
	#endif
	strReplace(buf, '\"', '\'');
	strReplace(buf, '\\', '/');
	return buf;
}

/**
 * si
 * Sensor config User Defined
 * {"nr":1,"fac":100,"div":1,"unit":"bar"}
 */
void server_sensor_config_userdef(OTF_PARAMS_DEF)
{
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
#else
	char *p = get_buffer;
#endif

	DEBUG_PRINTLN(F("server_sensor_config userdef"));

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("nr"), true))
		handle_return(HTML_DATA_MISSING);
	uint nr = strtoul(tmp_buffer, NULL, 0); // Sensor nr
	if (nr == 0) handle_return(HTML_DATA_MISSING);

	int16_t factor = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("fac"), true))
		factor = strtol(tmp_buffer, NULL, 0); // factor

	int16_t divider = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("div"), true))
		divider = strtol(tmp_buffer, NULL, 0); // divider

	char userdef_unit[8];
	memset(userdef_unit, 0, sizeof(userdef_unit));
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("unit"), true)) {
		strncpy(userdef_unit, urlDecodeAndUnescape(tmp_buffer), sizeof(userdef_unit)-1); // unit
	}
	int16_t assigned_unitid = -1;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("unitid"), true))
		assigned_unitid = strtol(tmp_buffer, NULL, 0); // divider

	int16_t offset_mv = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("offset"), true))
		offset_mv = strtol(tmp_buffer, NULL, 0); // offset in millivolt
	int16_t offset2 = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("offset2"), true))
		offset2 = strtol(tmp_buffer, NULL, 0); // offset unit value

	int ret = sensor_define_userdef(nr, factor, divider, userdef_unit, offset_mv, offset2, assigned_unitid);
	ret = ret == HTTP_RQT_SUCCESS?HTML_SUCCESS:HTML_DATA_MISSING;
	handle_return(ret);
}

/**
 * sj
 * get sensorurl
 * {"nr":1,"type":1}
 * return { "value": "text"}
 */
void server_sensorurl_get(OTF_PARAMS_DEF)
{
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
#else
	char *p = get_buffer;
#endif
	DEBUG_PRINTLN(F("sensorUrl_get"));

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("nr"), true))
		handle_return(HTML_DATA_MISSING);
	uint nr = strtoul(tmp_buffer, NULL, 0); // Sensor nr
	if (nr == 0) handle_return(HTML_DATA_MISSING);

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("type"), true))
		handle_return(HTML_DATA_MISSING);
	uint type = strtoul(tmp_buffer, NULL, 0); // Sensor type

	char *value = SensorUrl_get(nr, type);
	bfill.emit_p(PSTR("{\"value\":\"$S\"}"), value?value:"");
	handle_return(HTML_OK);
}

/**
 * sk
 * MQTT and other URL configuration
 * type = 0 URL, 1=MQTT Subscription, 2=JSON Filter
 * {"nr":1,"type":1,"value":abc}
 */
void server_sensorurl_config(OTF_PARAMS_DEF)
{
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
#else
	char *p = get_buffer;
#endif
	DEBUG_PRINTLN(F("serverUrl_config"));

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("nr"), true))
		handle_return(HTML_DATA_MISSING);
	uint nr = strtoul(tmp_buffer, NULL, 0); // Sensor nr
	if (nr == 0) handle_return(HTML_DATA_MISSING);

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("type"), true))
		handle_return(HTML_DATA_MISSING);
	uint type = strtoul(tmp_buffer, NULL, 0); // Sensor type

	char *value = NULL;
	uint8_t value_found = 0;
	findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("value"), true, &value_found);
	if (!value_found)
		handle_return(HTML_DATA_MISSING);
	DEBUG_PRINTLN(tmp_buffer);
	urlDecodeAndUnescape(tmp_buffer);
	DEBUG_PRINTLN(tmp_buffer);
	value = strdup(tmp_buffer);

	bool ok = SensorUrl_add(nr, type, value);
	free(value);
	handle_return(ok?HTML_SUCCESS:HTML_DATA_MISSING);
}

/**
 * sc
 * Sensor config
 * {"nr":1,"type":1,"group":0,"name":"myname","ip":123456789,"port":3000,"id":1,"ri":1000,"enable":1,"log":1}
 */
void server_sensor_config(OTF_PARAMS_DEF)
{
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
#else
	char *p = get_buffer;
#endif

	DEBUG_PRINTLN(F("server_sensor_config"));

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("nr"), true))
		handle_return(HTML_DATA_MISSING);
	uint nr = strtoul(tmp_buffer, NULL, 0); // Sensor nr
	if (nr == 0) handle_return(HTML_DATA_MISSING);

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("type"), true))
		handle_return(HTML_DATA_MISSING);
	uint type = strtoul(tmp_buffer, NULL, 0); // Sensor type

	if (type == 0) {
		sensor_delete(nr);
		handle_return(HTML_SUCCESS);
	}

	DEBUG_PRINTLN(F("server_sensor_config2"));

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("group"), true))
		handle_return(HTML_DATA_MISSING);
	uint group = strtoul(tmp_buffer, NULL, 0); // Sensor group

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("name"), true))
		handle_return(HTML_DATA_MISSING);
	char name[30];
	strncpy(name, urlDecodeAndUnescape(tmp_buffer), sizeof(name)-1); // Sensor name

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("ip"), true))
		handle_return(HTML_DATA_MISSING);
	uint32_t ip = strtoul(tmp_buffer, NULL, 0); // Sensor ip

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("port"), true))
		handle_return(HTML_DATA_MISSING);
	uint port = strtoul(tmp_buffer, NULL, 0); // Sensor port

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("id"), true))
		handle_return(HTML_DATA_MISSING);
	uint id = strtoul(tmp_buffer, NULL, 0); // Sensor modbus id

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("ri"), true))
		handle_return(HTML_DATA_MISSING);
	uint ri = strtoul(tmp_buffer, NULL, 0); // Read Interval (s)

	int16_t factor = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("fac"), true))
		factor = strtol(tmp_buffer, NULL, 0); // factor

	int16_t divider = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("div"), true))
		divider = strtol(tmp_buffer, NULL, 0); // divider

	DEBUG_PRINTLN(F("server_sensor_config3"));

	char userdef_unit[8];
	memset(userdef_unit, 0, sizeof(userdef_unit));
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("unit"), true)) {
		DEBUG_PRINTLN(tmp_buffer)
		urlDecodeAndUnescape(tmp_buffer);
		DEBUG_PRINTLN(tmp_buffer)
		strncpy(userdef_unit, tmp_buffer, sizeof(userdef_unit)-1); // unit
	}
	int16_t assigned_unitid = -1;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("unitid"), true))
		assigned_unitid = strtol(tmp_buffer, NULL, 0); // divider

	int16_t offset_mv = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("offset"), true))
		offset_mv = strtol(tmp_buffer, NULL, 0); // offset in millivolt

	int16_t offset2 = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("offset2"), true))
		offset2 = strtol(tmp_buffer, NULL, 0); // offset2

	uint enable = 1;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("enable"), true))
		enable = strtoul(tmp_buffer, NULL, 0); // 1=enable/0=disable

	uint log = 1;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("log"), true))
		log = strtoul(tmp_buffer, NULL, 0); // 1=logging enabled/0=logging disabled

	uint show = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("show"), true))
		show = strtoul(tmp_buffer, NULL, 0); // 1=show enabled/0=show disabled
	
	//mqtt and other:
	char* url = NULL;
	uint8_t url_found = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("url"), true, &url_found))
		url = strdup(urlDecodeAndUnescape(tmp_buffer));
	char* topic = NULL;
	uint8_t topic_found = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("topic"), true, &topic_found)) {
		DEBUG_PRINTLN(tmp_buffer)
		urlDecodeAndUnescape(tmp_buffer);
		DEBUG_PRINTLN(tmp_buffer)
		topic = strdup(tmp_buffer);
	}
	char* filter = NULL;
	uint8_t filter_found = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("filter"), true, &filter_found))
		filter = strdup(urlDecodeAndUnescape(tmp_buffer));

	DEBUG_PRINTLN(F("server_sensor_config4"));

	SensorFlags_t flags = {.enable=enable, .log=log, .show=show};
	int ret = sensor_define(nr, name, type, group, ip, port, id, ri, factor, divider, userdef_unit, offset_mv, offset2, flags, assigned_unitid);
	if (url_found) {
		SensorUrl_add(nr, SENSORURL_TYPE_URL, url);
		free(url);
	}
	if (topic_found) {
		SensorUrl_add(nr, SENSORURL_TYPE_TOPIC, topic);
		free(topic);
	}
	if (filter_found) {
		SensorUrl_add(nr, SENSORURL_TYPE_FILTER, filter);
		free(filter);
	}
	
	ret = ret == HTTP_RQT_SUCCESS?HTML_SUCCESS:HTML_DATA_MISSING;
	handle_return(ret);

	DEBUG_PRINTLN(F("server_sensor_config5"));

}

/**
 * sa
 * Modus RS485 Sensor set address help function
 * {"nr":1,"id":1}
 */
void server_set_sensor_address(OTF_PARAMS_DEF) {
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
#else
	char *p = get_buffer;
#endif

	DEBUG_PRINTLN(F("server_set_sensor_address"));

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("nr"), true))
		handle_return(HTML_DATA_MISSING);
	uint nr = strtoul(tmp_buffer, NULL, 0); // Sensor nr

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("id"), true))
		handle_return(HTML_DATA_MISSING);
	uint id = strtoul(tmp_buffer, NULL, 0); // Sensor modbus id

	int ret = set_sensor_address(sensor_by_nr(nr), id);
	ret = ret == HTTP_RQT_SUCCESS?HTML_SUCCESS:HTML_DATA_MISSING;
	handle_return(ret);
}

/**
 * sg
 * @brief return one or all last sensor values
 *
 */
void server_sensor_get(OTF_PARAMS_DEF) {
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
#else
	char *p = get_buffer;
#endif

	DEBUG_PRINTLN(F("server_sensor_get"));

	uint nr = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("nr"), true))
		nr = strtoul(tmp_buffer, NULL, 0); // Sensor nr

#if defined(USE_OTF)
	// as the log data can be large, we will use ESP8266's sendContent function to
	// send multiple packets of data, instead of the standard way of using send().
	rewind_ether_buffer();
	print_header(OTF_PARAMS);
#else
	print_header();
#endif

	bfill.emit_p(PSTR("{\"datas\":["));
	uint count = sensor_count();
	bool first = true;
	for (uint i = 0; i < count; i++) {

		Sensor_t *sensor = sensor_by_idx(i);
		if (!sensor || (nr != 0 && nr != sensor->nr))
			continue;

		if (first) first = false; else bfill.emit_p(PSTR(","));

		bfill.emit_p(PSTR("{\"nr\":$D,\"nativedata\":$L,\"data\":$E,\"unit\":\"$S\",\"unitid\":$D,\"last\":$L}"),
			sensor->nr,
			sensor->last_native_data,
			sensor->last_data,
			getSensorUnit(sensor),
			getSensorUnitId(sensor),
			sensor->last_read);
		send_packet(OTF_PARAMS);
	}
	bfill.emit_p(PSTR("]}"));
	handle_return(HTML_OK);
}

/**
 * sr
 * @brief read now and return status and last data
 *
 */
void server_sensor_readnow(OTF_PARAMS_DEF) {
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
#else
	char *p = get_buffer;
#endif

	DEBUG_PRINTLN(F("server_sensor_readnow"));

	uint nr = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("nr"), true))
		nr = strtoul(tmp_buffer, NULL, 0); // Sensor nr

#if defined(USE_OTF)
	// as the log data can be large, we will use ESP8266's sendContent function to
	// send multiple packets of data, instead of the standard way of using send().
	rewind_ether_buffer();
	print_header(OTF_PARAMS);
#else
	print_header();
#endif

	bfill.emit_p(PSTR("{\"datas\":["));
	uint count = sensor_count();
	bool first = true;
	ulong time = os.now_tz();

	for (uint i = 0; i < count; i++) {

		Sensor_t *sensor = sensor_by_idx(i);
		if (!sensor || (nr != 0 && nr != sensor->nr))
			continue;

		sensor->last_read = time - sensor->read_interval - 1;
		int status = read_sensor(sensor, time);

		if (first) first = false; else bfill.emit_p(PSTR(","));

		bfill.emit_p(PSTR("{\"nr\":$D,\"status\":$D,\"nativedata\":$L,\"data\":$E,\"unit\":\"$S\",\"unitid\":$D}"),
			sensor->nr,
			status,
			sensor->last_native_data,
			sensor->last_data,
			getSensorUnit(sensor),
			getSensorUnitId(sensor));

		send_packet(OTF_PARAMS);
	}
	bfill.emit_p(PSTR("]}"));
	handle_return(HTML_OK);
}

void sensorconfig_json(OTF_PARAMS_DEF) {
	int count = sensor_count();
	bool first = true;
	for (int i = 0; i < count; i++) {
		Sensor_t *sensor = sensor_by_idx(i);

		if (first) first = false; else bfill.emit_p(PSTR(","));

		char* url = SensorUrl_get(sensor->nr, SENSORURL_TYPE_URL);
		char* topic = SensorUrl_get(sensor->nr, SENSORURL_TYPE_TOPIC);
		char* filter = SensorUrl_get(sensor->nr, SENSORURL_TYPE_FILTER);

		bfill.emit_p(PSTR("{\"nr\":$D,\"type\":$D,\"group\":$D,\"name\":\"$S\",\"ip\":$L,\"port\":$D,\"id\":$D,\"ri\":$D,\"fac\":$D,\"div\":$D,\"offset\":$D,\"offset2\":$D,\"nativedata\":$L,\"data\":$E,\"unit\":\"$S\",\"unitid\":$D,\"enable\":$D,\"log\":$D,\"show\":$D,\"data_ok\":$D,\"last\":$L,\"url\":\"$S\",\"topic\":\"$S\",\"filter\":\"$S\"}"),
			sensor->nr,
			sensor->type,
			sensor->group,
			sensor->name,
			sensor->ip,
			sensor->port,
			sensor->id,
			sensor->read_interval,
			sensor->factor,
			sensor->divider,
			sensor->offset_mv,
			sensor->offset2,
			sensor->last_native_data,
			sensor->last_data,
			getSensorUnit(sensor),
			getSensorUnitId(sensor),
			sensor->flags.enable,
			sensor->flags.log,
			sensor->flags.show,
			sensor->flags.data_ok,
			sensor->last_read,
			url?url:"", topic?topic:"", filter?filter:"");
		send_packet(OTF_PARAMS);
	}
}

/**
 * sl
 * @brief Lists all sensors
 *
 */
void server_sensor_list(OTF_PARAMS_DEF) {
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
#else
	char *p = get_buffer;
#endif

	//DEBUG_PRINTLN(F("server_sensor_list"));
	//DEBUG_PRINT(F("server_count: "));
	//DEBUG_PRINTLN(sensor_count());

	uint test = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("test"), true))
		test = strtoul(tmp_buffer, NULL, 0); // Sensor nr

#if defined(USE_OTF)
	// as the log data can be large, we will use ESP8266's sendContent function to
	// send multiple packets of data, instead of the standard way of using send().
	rewind_ether_buffer();
	print_header(OTF_PARAMS);
#else
	print_header();
#endif

	if (test) {
		bfill.emit_p(PSTR("{\"test\":$D,"), test);
		bfill.emit_p(PSTR("\"detected\":$D}"), get_asb_detected_boards());
	} else {
		int count = sensor_count();
		bfill.emit_p(PSTR("{\"count\":$D,"), count);
		bfill.emit_p(PSTR("\"detected\":$D,"), get_asb_detected_boards());
		bfill.emit_p(PSTR("\"sensors\":["));
		sensorconfig_json(OTF_PARAMS);
		bfill.emit_p(PSTR("]"));
		bfill.emit_p(PSTR("}"));
	}
	handle_return(HTML_OK);
}

/**
 * so
 * @brief output sensorlog
 *
 */
void server_sensorlog_list(OTF_PARAMS_DEF) {
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
#else
	char *p = get_buffer;
#endif

	DEBUG_PRINTLN(F("server_sensorlog_list"));

	uint8_t log = LOG_STD;

	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("log"), true)) // Log type 0=DAY 1=WEEK 2=MONTH
		log = strtoul(tmp_buffer, NULL, 0);
	if (log > LOG_MONTH)
		log = LOG_STD;
	ulong log_size = sensorlog_size(log);

	//start / max:
	ulong startAt = 0;
	ulong maxResults = log_size;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("start"), true)) // Log start
		startAt = strtoul(tmp_buffer, NULL, 0);

	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("max"), true)) // Log Lines count
		maxResults = strtoul(tmp_buffer, NULL, 0);

	//Filters:
	uint nr = 0;
	uint type = 0;
	ulong after = 0;
	ulong before = 0;
	ulong lastHours = 0;
	bool isjson = true;
	bool shortcsv = false;

	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("nr"), true)) // Filter log for sensor-nr
		nr = strtoul(tmp_buffer, NULL, 0);

	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("type"), true)) // Filter log for sensor-type
		type = strtoul(tmp_buffer, NULL, 0);

	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("after"), true)) // Filter time after
		after = strtoul(tmp_buffer, NULL, 0);

	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("before"), true)) // Filter time before
		before = strtoul(tmp_buffer, NULL, 0);

	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("lasthours"), true)) // Filter last hours
		lastHours = strtoul(tmp_buffer, NULL, 0);

	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("lastdays"), true)) // Filter last days
		lastHours = strtoul(tmp_buffer, NULL, 0) * 24 + lastHours;

	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("csv"), true)) { // Filter last days
		int csv = atoi(tmp_buffer);
		isjson = csv == 0;
		shortcsv = csv == 2;
	}

#if defined(USE_OTF)
	// as the log data can be large, we will use ESP8266's sendContent function to
	// send multiple packets of data, instead of the standard way of using send().
	rewind_ether_buffer();
	if (isjson)	print_header(OTF_PARAMS); else print_header_download(OTF_PARAMS);
#else
	if (isjson)	print_header(); else print_header_download();
#endif

	if (isjson) {
		bfill.emit_p(PSTR("{\"logtype\":$D,\"logsize\":$D,\"filesize\":$D,\"log\":["),
			log, log_size, sensorlog_filesize(log));
	} else {
		if (shortcsv)
			bfill.emit_p(PSTR("nr;time;data\r\n"));
		else
			bfill.emit_p(PSTR("nr;type;time;nativedata;data;unit;unitid\r\n"));
	}

	#define BLOCKSIZE 64
	ulong count = 0;
 	SensorLog_t *sensorlog = (SensorLog_t*)malloc(sizeof(SensorLog_t)*BLOCKSIZE);
	Sensor_t *sensor = NULL;

	//lastHours: find limit for this
	if (lastHours > 0 && log_size > 0) {
		after = os.now_tz() - lastHours * 60 * 60; //seconds
		DEBUG_PRINT(F("lastHours="));
		DEBUG_PRINTLN(lastHours);

		ulong a = 0;
		ulong b = log_size-1;
		ulong lastIdx = 0;
		while (true) {
			ulong idx = (b-a)/2+a;
			sensorlog_load(log, idx, sensorlog);
			if (sensorlog->time < after) {
				a = idx;
			} else if (sensorlog->time > after) {
				b = idx;
			}
			if (a >= b || idx == lastIdx) break;
			lastIdx = idx;
		}
		startAt = lastIdx;
	}
	if (maxResults > 0 && maxResults < log_size)
	{
		DEBUG_PRINT(F("max="));
		DEBUG_PRINTLN(maxResults);
		ulong startAt2 = log_size-maxResults;
		if (startAt2 > startAt)
			startAt = startAt2;
	}

	uint sensor_type = 0;

	DEBUG_PRINTLN(F("start so"));
	ulong idx = startAt;
	while (idx < log_size) {
		int n = sensorlog_load2(log, idx, BLOCKSIZE, sensorlog);
		if (n <= 0) break;

		for (int i = 0; i < n; i++) {
			idx++;
			if (nr && sensorlog[i].nr != nr)
				continue;

			if (after && sensorlog[i].time <= after)
				continue;

			if (before && sensorlog[i].time >= before)
				continue;

			if (!shortcsv || type) {
				if (!sensor || sensor->nr != sensorlog[i].nr)
					sensor = sensor_by_nr(sensorlog[i].nr);
				sensor_type = sensor?sensor->type:0;
				if (type && sensor_type != type)
					continue;
			}

			if (count > 0 && isjson) {
				bfill.emit_p(PSTR(","));
			}

			if (isjson) {
				bfill.emit_p(PSTR("{\"nr\":$D,\"type\":$D,\"time\":$L,\"nativedata\":$L,\"data\":$E,\"unit\":\"$S\",\"unitid\":$D}"),
				sensorlog[i].nr,          //sensor-nr
				sensor_type,           //sensor-type
				sensorlog[i].time,        //timestamp
				sensorlog[i].native_data, //native data
				sensorlog[i].data,
				getSensorUnit(sensor),
				getSensorUnitId(sensor));
			} else {
				if (shortcsv)
					bfill.emit_p(PSTR("$D;$L;$E\r\n"),
						sensorlog[i].nr,          //sensor-nr
						sensorlog[i].time,        //timestamp
						sensorlog[i].data);
				else
					bfill.emit_p(PSTR("$D;$D;$L;$L;$E;$S;$D\r\n"),
						sensorlog[i].nr,          //sensor-nr
						sensor_type,           //sensor-type
						sensorlog[i].time,        //timestamp
						sensorlog[i].native_data, //native data
						sensorlog[i].data,
						getSensorUnit(sensor),
						getSensorUnitId(sensor));
			}
			// if available ether buffer is getting small
			// send out a packet
			if (!buffer_available() ) {
				send_packet(OTF_PARAMS);
			}
			if (++count >= maxResults) {
				break;
			}
		}
		if (count >= maxResults)
			break;
	}
	DEBUG_PRINTLN(F("end so"));

	if (isjson)
		bfill.emit_p(PSTR("]}"));
	else
		bfill.emit_p(PSTR("\r\n"));
	free(sensorlog);

	DEBUG_PRINTLN(F("finish so"));

	handle_return(HTML_OK);
}

/**
 * sn
 * @brief Delete/Clear Sensor log
 *
 */
void server_sensorlog_clear(OTF_PARAMS_DEF) {
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
#else
	char *p = get_buffer;
#endif
	int log = -1;
	uint nr = 0;
	double under = 0;
	bool use_under = false;
	double over = 0;
	bool use_over = false;
	ulong before = 0;
	ulong after = 0;

	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("log"), true)) // Filter log for sensor-nr
		log = atoi(tmp_buffer);
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("nr"), true)) // Filter log for sensor-nr
		nr = strtoul(tmp_buffer, NULL, 0);
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("under"), true)) // values lower than 
		use_under = sscanf(tmp_buffer, "%lf", &under) == 1;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("over"), true)) // values higher than 
		use_over = sscanf(tmp_buffer, "%lf", &over) == 1;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("before"), true)) // values higher than 
		sscanf(tmp_buffer, "%lu", &before);
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("after"), true)) // values higher than 
		sscanf(tmp_buffer, "%lu", &after);

	DEBUG_PRINTLN(F("server_sensorlog_clear"));

#if defined(USE_OTF)
	// as the log data can be large, we will use ESP8266's sendContent function to
	// send multiple packets of data, instead of the standard way of using send().
	rewind_ether_buffer();
	print_header(OTF_PARAMS);
#else
	print_header();
#endif

	DEBUG_PRINTLN(F("start log cleaning"));
	if (nr > 0 || use_under || use_over || before || after) {
		ulong n = 0;
		if (log == -1) {
			n += sensorlog_clear_sensor(nr, LOG_STD, use_under, under, use_over, over, before, after);
			n += sensorlog_clear_sensor(nr, LOG_WEEK, use_under, under, use_over, over, before, after);
			n += sensorlog_clear_sensor(nr, LOG_MONTH, use_under, under, use_over, over, before, after);
		} else {
			n += sensorlog_clear_sensor(nr, log, use_under, under, use_over, over, before, after);
		}
		bfill.emit_p(PSTR("{\"deleted\":$L}"), n);
	} else {
		ulong log_size = sensorlog_size(LOG_STD);
		ulong log_sizeW = sensorlog_size(LOG_WEEK);
		ulong log_sizeM = sensorlog_size(LOG_MONTH);

		if (log == -1) {
			sensorlog_clear_all();
			bfill.emit_p(PSTR("{\"deleted\":$L,\"deleted_week\":$L,\"deleted_month\":$L}"), log_size, log_sizeW, log_sizeM);
		}
		else {
			sensorlog_clear(log==LOG_STD, log==LOG_WEEK, log==LOG_MONTH);
			bfill.emit_p(PSTR("{\"deleted\":$L}"), log==LOG_STD?log_size:log=LOG_WEEK?log_sizeW:log_sizeM);
		}
	}
	DEBUG_PRINTLN(F("end log cleaning"));
	handle_return(HTML_OK);
}

/**
 * mt
 * supported monitor types
 */
void server_monitor_types(OTF_PARAMS_DEF) {
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
#else
	char *p = get_buffer;
#endif

	DEBUG_PRINTLN(F("server_monitor_types"));

#if defined(USE_OTF)
	// as the log data can be large, we will use ESP8266's sendContent function to
	// send multiple packets of data, instead of the standard way of using send().
	rewind_ether_buffer();
	print_header(OTF_PARAMS);
#else
	print_header();
#endif

	bfill.emit_p(PSTR("{\"monitortypes\": ["));
	bfill.emit_p(PSTR("{\"name\":\"Min\",\"type\":$D},"), MONITOR_MIN);
	bfill.emit_p(PSTR("{\"name\":\"Max\",\"type\":$D},"), MONITOR_MAX);
	bfill.emit_p(PSTR("{\"name\":\"SN 1/2\",\"type\":$D},"), MONITOR_SENSOR12);
	bfill.emit_p(PSTR("{\"name\":\"SET SN 1/2\",\"type\":$D},"), MONITOR_SET_SENSOR12);
	bfill.emit_p(PSTR("{\"name\":\"AND\",\"type\":$D},"), MONITOR_AND);
	bfill.emit_p(PSTR("{\"name\":\"OR\",\"type\":$D},"), MONITOR_OR);
	bfill.emit_p(PSTR("{\"name\":\"XOR\",\"type\":$D},"), MONITOR_XOR);
	bfill.emit_p(PSTR("{\"name\":\"NOT\",\"type\":$D}," ), MONITOR_NOT);
	bfill.emit_p(PSTR("{\"name\":\"REMOTE\",\"type\":$D}" ), MONITOR_REMOTE);
	bfill.emit_p(PSTR("]}"));
	handle_return(HTML_OK);
}

/**
 * mc
 * define a monitor
 */
void server_monitor_config(OTF_PARAMS_DEF) {
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
#else
	char *p = get_buffer;
#endif

	DEBUG_PRINTLN(F("server_monitor_config"));

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("nr"), true))
		handle_return(HTML_DATA_MISSING);
	uint16_t nr = strtoul(tmp_buffer, NULL, 0); // Adjustment nr
	if (nr == 0)
		handle_return(HTML_DATA_MISSING);

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("type"), true))
		handle_return(HTML_DATA_MISSING);
	uint16_t type = strtoul(tmp_buffer, NULL, 0); // Adjustment type

	if (type == 0) {
		monitor_delete(nr);
		handle_return(HTML_SUCCESS);
	}

	uint16_t sensor = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("sensor"), true))
		sensor = strtoul(tmp_buffer, NULL, 0); // Sensor nr

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("prog"), true))
		handle_return(HTML_DATA_MISSING);
	uint16_t prog = strtoul(tmp_buffer, NULL, 0); // Program nr

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("zone"), true))
		handle_return(HTML_DATA_MISSING);
	uint16_t zone = strtoul(tmp_buffer, NULL, 0); // Zone

	char name[30] = {0};
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("name"), true))
		strncpy(name, tmp_buffer, sizeof(name)-1);

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("maxrun"), true))
		handle_return(HTML_DATA_MISSING);
	ulong maxRuntime = strtoul(tmp_buffer, NULL, 0); // Zone

	uint8_t prio = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("prio"), true))
		prio = strtoul(tmp_buffer, NULL, 0); // prio

	//type-dependend parameters:
	//type = MIN/MAX of sensor value:
	double value1 = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("value1"), true))
		value1 = atof(tmp_buffer); // Value 1

	double value2 = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("value2"), true))
		value2 = atof(tmp_buffer); // Value 2

	//type = SENSOR12
	uint16_t sensor12 = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("sensor12"), true))
		sensor12 = strtoul(tmp_buffer, NULL, 0); 
	bool invers = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("invers"), true))
		invers = strtoul(tmp_buffer, NULL, 0) > 0; 

	//type = AND/OR/XOR:
	uint16_t monitor1 = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("monitor1"), true))
		monitor1 = strtoul(tmp_buffer, NULL, 0); 
	uint16_t monitor2 = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("monitor2"), true))
		monitor2 = strtoul(tmp_buffer, NULL, 0); 
	uint16_t monitor3 = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("monitor3"), true))
		monitor3 = strtoul(tmp_buffer, NULL, 0); 
	uint16_t monitor4 = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("monitor4"), true))
		monitor4 = strtoul(tmp_buffer, NULL, 0); 

	bool invers1 = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("invers1"), true))
		invers1 = strtoul(tmp_buffer, NULL, 0) > 0; 
	bool invers2 = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("invers2"), true))
		invers2 = strtoul(tmp_buffer, NULL, 0) > 0; 
	bool invers3 = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("invers3"), true))
		invers3 = strtoul(tmp_buffer, NULL, 0) > 0; 
	bool invers4 = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("invers4"), true))
		invers4 = strtoul(tmp_buffer, NULL, 0) > 0; 

	//type = NOT 
	uint16_t monitor = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("monitor"), true))
		monitor = strtoul(tmp_buffer, NULL, 0); 

	//type = REMOTE ip
	uint16_t rmonitor = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("rmonitor"), true))
		rmonitor = strtoul(tmp_buffer, NULL, 0); 
	uint32_t ip = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("ip"), true))
		ip = strtoul(tmp_buffer, NULL, 0); 
	uint16_t port = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("port"), true))
		port = strtoul(tmp_buffer, NULL, 0); 

	Monitor_Union_t m; 
	switch (type) {
		case MONITOR_MIN:
		case MONITOR_MAX:
			m = (Monitor_Union_t){.minmax = {.value1 = value1, .value2 = value2}};
			break;
		case MONITOR_SENSOR12:
			m = (Monitor_Union_t){.sensor12 = {.sensor12 = sensor12, .invers = invers}};
			break;
		case MONITOR_SET_SENSOR12:
			m = (Monitor_Union_t){.set_sensor12 = {.monitor = monitor, .sensor12 = sensor12}};
			break;
		case MONITOR_AND:
		case MONITOR_OR:
		case MONITOR_XOR:
			m = (Monitor_Union_t){.andorxor = {.monitor1 = monitor1, .monitor2 = monitor2, .monitor3 = monitor3, .monitor4 = monitor4,
				.invers1 = invers1, .invers2 = invers2, .invers3 = invers3, .invers4 = invers4}};
			break;
		case MONITOR_NOT:
			m = (Monitor_Union_t){.mnot = {.monitor = monitor}};
			break;
		case MONITOR_REMOTE:
			m = (Monitor_Union_t){.remote = {.rmonitor = rmonitor, .ip = ip, .port = port}};
			break;
		default: handle_return(HTML_DATA_FORMATERROR);
	}
	int ret = monitor_define(nr, type, sensor, prog, zone, m, name, maxRuntime, prio);
	ret = ret >= HTTP_RQT_SUCCESS?HTML_SUCCESS:HTML_DATA_MISSING;
	handle_return(ret);
}

void monitorconfig_json(Monitor_t *mon) {
	bfill.emit_p(PSTR("{\"nr\":$D,\"type\":$D,\"sensor\":$D,\"prog\":$D,\"zone\":$D,\"name\":\"$S\",\"maxrun\":$L,\"prio\":$D,\"active\":$D,\"time\":$L,"),
				mon->nr,
				mon->type,
				mon->sensor,
				mon->prog,
				mon->zone,
				mon->name,
				mon->maxRuntime,
				mon->prio,
				mon->active,
				mon->time);

	switch(mon->type) {
		case MONITOR_MIN:
		case MONITOR_MAX:
			bfill.emit_p(PSTR("\"value1\":$E,\"value2\":$E}"),
				isnan(mon->m.minmax.value1)?0:mon->m.minmax.value1,
				isnan(mon->m.minmax.value2)?0:mon->m.minmax.value2);
			break;
		case MONITOR_SENSOR12:
			bfill.emit_p(PSTR("\"sensor12\":$D,\"invers\":$D}"),
				mon->m.sensor12.sensor12,
				mon->m.sensor12.invers);
			break;
		case MONITOR_SET_SENSOR12:
			bfill.emit_p(PSTR("\"monitor\":$D,\"sensor12\":$D}"),
				mon->m.set_sensor12.monitor,
				mon->m.set_sensor12.sensor12);
			break;
		case MONITOR_AND:
		case MONITOR_OR:
		case MONITOR_XOR:
			bfill.emit_p(PSTR("\"monitor1\":$D,\"monitor2\":$D,\"monitor3\":$D,\"monitor4\":$D,\"invers1\":$D,\"invers2\":$D,\"invers3\":$D,\"invers4\":$D}"),
				mon->m.andorxor.monitor1, mon->m.andorxor.monitor2, mon->m.andorxor.monitor3, mon->m.andorxor.monitor4,
				mon->m.andorxor.invers1, mon->m.andorxor.invers2,  mon->m.andorxor.invers3,  mon->m.andorxor.invers4); 
			break;
		case MONITOR_NOT:
			bfill.emit_p(PSTR("\"monitor\":$D}"),
				mon->m.mnot.monitor);
			break;
		case MONITOR_REMOTE:
			bfill.emit_p(PSTR("\"rmonitor\":$D,\"ip\":$L,\"port\":$D}"),
				mon->m.remote.rmonitor,
				mon->m.remote.ip,
				mon->m.remote.port);
			break;

	}
}

void monitorconfig_json() {
	uint count = monitor_count();
	for (uint i = 0; i < count; i++) {
		Monitor_t *mon = monitor_by_idx(i);
		if (i > 0) bfill.emit_p(PSTR(","));
		monitorconfig_json(mon);
	}
}

/**
 * ml
 * list monitors
 */
void server_monitor_list(OTF_PARAMS_DEF) {
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
#else
	char *p = get_buffer;
#endif

	DEBUG_PRINTLN(F("server_monitor_list"));

	uint nr = 0;
	int prog = -1;
	uint sensor_nr = 0;

	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("nr"), true))
		nr = strtoul(tmp_buffer, NULL, 0);

	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("prog"), true))
		prog = strtoul(tmp_buffer, NULL, 0);

	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("sensor"), true))
		 sensor_nr = strtoul(tmp_buffer, NULL, 0);

#if defined(USE_OTF)
	// as the log data can be large, we will use ESP8266's sendContent function to
	// send multiple packets of data, instead of the standard way of using send().
	rewind_ether_buffer();
	print_header(OTF_PARAMS);
#else
	print_header();
#endif

	bfill.emit_p(PSTR("{\"monitors\": ["));
	uint8_t idx = 0;
	uint8_t count = monitor_count();
	bool first = true;

	while (idx < count) {
		Monitor_t *mon = monitor_by_idx(idx++);
		if (nr > 0 && mon->nr != nr)
			continue;
		if (prog >= 0 && mon->prog != (uint)prog)
			continue;
		if (sensor_nr > 0 && mon->sensor != sensor_nr)
			continue;

		if (!first)
			bfill.emit_p(PSTR(","));
		first = false;
		monitorconfig_json(mon);
		send_packet(OTF_PARAMS);
	}
	bfill.emit_p(PSTR("]}"));
	handle_return(HTML_OK);
}



/**
 * sb
 * define a program adjustment
*/
void server_sensorprog_config(OTF_PARAMS_DEF) {
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
#else
	char *p = get_buffer;
#endif

	DEBUG_PRINTLN(F("server_sensorprog_config"));
	//uint nr, uint type, uint sensor, uint prog, double factor1, double factor2, double min, double max

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("nr"), true))
		handle_return(HTML_DATA_MISSING);
	uint nr = strtoul(tmp_buffer, NULL, 0); // Adjustment nr
	if (nr == 0)
		handle_return(HTML_DATA_MISSING);

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("type"), true))
		handle_return(HTML_DATA_MISSING);
	uint type = strtoul(tmp_buffer, NULL, 0); // Adjustment type

	if (type == 0) {
		prog_adjust_delete(nr);
		handle_return(HTML_SUCCESS);
	}

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("sensor"), true))
		handle_return(HTML_DATA_MISSING);
	uint sensor = strtoul(tmp_buffer, NULL, 0); // Sensor nr

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("prog"), true))
		handle_return(HTML_DATA_MISSING);
	uint prog = strtoul(tmp_buffer, NULL, 0); // Program nr

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("factor1"), true))
		handle_return(HTML_DATA_MISSING);
	double factor1 = atof(tmp_buffer); // Factor 1

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("factor2"), true))
		handle_return(HTML_DATA_MISSING);
	double factor2 = atof(tmp_buffer); // Factor 2

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("min"), true))
		handle_return(HTML_DATA_MISSING);
	double min = atof(tmp_buffer); // Min value

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("max"), true))
		handle_return(HTML_DATA_MISSING);
	double max = atof(tmp_buffer); // Max value

	char name[30] = {0};
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("name"), true))
		strncpy(name, tmp_buffer, sizeof(name)-1);

	int ret = prog_adjust_define(nr, type, sensor, prog, factor1, factor2, min, max, name);
	ret = ret >= HTTP_RQT_SUCCESS?HTML_SUCCESS:HTML_DATA_MISSING;
	handle_return(ret);
}

void progconfig_json(ProgSensorAdjust_t *p, double current) {
	bfill.emit_p(PSTR("{\"nr\":$D,\"type\":$D,\"sensor\":$D,\"prog\":$D,\"factor1\":$E,\"factor2\":$E,\"min\":$E,\"max\":$E,\"name\":\"$S\",\"current\":$E}"),
		p->nr,
		p->type,
		p->sensor,
		p->prog,
		p->factor1,
		p->factor2,
		p->min,
		p->max,
		(p->name[0] < 0x20 || p->name[0] > 0xEE || p->name[0] == 0x90) ? "" : p->name,
		current);
}

void progconfig_json() {
	uint count = prog_adjust_count();
	bool first = true;
	for (uint i = 0; i < count; i++) {
		ProgSensorAdjust_t *p = prog_adjust_by_idx(i);
		double current = calc_sensor_watering_by_nr(p->nr);

		if (first) first = false; else bfill.emit_p(PSTR(","));

		progconfig_json(p, current);
	}
}

/**
 * se
 * define a program adjustment
*/
void server_sensorprog_list(OTF_PARAMS_DEF) {
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
#else
	char *p = get_buffer;
#endif

	DEBUG_PRINTLN(F("server_sensorprog_list"));

	uint nr = 0;
	int prog = -1;
	uint sensor_nr = 0;
	
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("nr"), true))
		nr = strtoul(tmp_buffer, NULL, 0);

	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("prog"), true))
		prog = strtoul(tmp_buffer, NULL, 0);

	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("sensor"), true))
		 sensor_nr = strtoul(tmp_buffer, NULL, 0);

#if defined(USE_OTF)
	// as the log data can be large, we will use ESP8266's sendContent function to
	// send multiple packets of data, instead of the standard way of using send().
	rewind_ether_buffer();
	print_header(OTF_PARAMS);
#else
	print_header();
#endif

	uint n = prog_adjust_count();
	uint idx = 0;
	uint count = 0;
	while (idx < n) {
		ProgSensorAdjust_t *p = prog_adjust_by_idx(idx++);
		if (nr > 0 && p->nr != nr)
			continue;
		if (prog >= 0 && p->prog != (uint)prog)
			continue;
		if (sensor_nr > 0 && p->sensor != sensor_nr)
			continue;
		count++;
	}

	bfill.emit_p(PSTR("{\"count\": $D,"), count);

	bfill.emit_p(PSTR("\"progAdjust\": ["));
	idx = 0;
	count = 0;

	while (idx < n) {
		ProgSensorAdjust_t *p = prog_adjust_by_idx(idx++);
		if (nr > 0 && p->nr != nr)
			continue;
		if (prog >= 0 && p->prog != (uint)prog)
			continue;
		if (sensor_nr > 0 && p->sensor != sensor_nr)
			continue;

		double current = calc_sensor_watering_by_nr(p->nr);

		if (count++ > 0)
			bfill.emit_p(PSTR(","));
		progconfig_json(p, current);
		send_packet(OTF_PARAMS);
	}
	bfill.emit_p(PSTR("]}"));
	handle_return(HTML_OK);
}

static const int sensor_types[] = {
	SENSOR_SMT100_MOIS,
	SENSOR_SMT100_TEMP,
	SENSOR_SMT100_PMTY,
	SENSOR_TH100_MOIS,
	SENSOR_TH100_TEMP,

#if defined(ESP8266)
	SENSOR_ANALOG_EXTENSION_BOARD,
	SENSOR_ANALOG_EXTENSION_BOARD_P,
	SENSOR_SMT50_MOIS,
	SENSOR_SMT50_TEMP,
	SENSOR_SMT100_ANALOG_MOIS,
	SENSOR_SMT100_ANALOG_TEMP,
	SENSOR_VH400,
	SENSOR_THERM200,
	SENSOR_AQUAPLUMB,
	SENSOR_USERDEF,
#endif
#if defined ADS1115||PCF8591
	SENSOR_OSPI_ANALOG,
	SENSOR_OSPI_ANALOG_P,
	SENSOR_OSPI_ANALOG_SMT50_MOIS,
	SENSOR_OSPI_ANALOG_SMT50_TEMP,
#endif
#if defined(OSPI)
    SENSOR_OSPI_INTERNAL_TEMP,
#endif
	SENSOR_MQTT,
	SENSOR_REMOTE,
	SENSOR_WEATHER_TEMP_F,
	SENSOR_WEATHER_TEMP_C,
	SENSOR_WEATHER_HUM,
	SENSOR_WEATHER_PRECIP_IN,
	SENSOR_WEATHER_PRECIP_MM,
	SENSOR_WEATHER_WIND_MPH,
	SENSOR_WEATHER_WIND_KMH,
	SENSOR_GROUP_MIN,
	SENSOR_GROUP_MAX,
	SENSOR_GROUP_AVG,
	SENSOR_GROUP_SUM,
#if defined(ESP8266)	
	SENSOR_FREE_MEMORY,
	SENSOR_FREE_STORE,
#endif
};

static const char* sensor_names[] = {
	"Truebner SMT100 RS485 Modbus, moisture mode",
	"Truebner SMT100 RS485 Modbus, temperature mode",
	"Truebner SMT100 RS485 Modbus, permittivity mode",
	"Truebner TH100 RS485 Modbus, humidity mode",
	"Truebner TH100 RS485 Modbus, temperature mode",
 #if defined(ESP8266)
	"ASB - voltage mode 0..5V",
	"ASB - 0..3.3V to 0..100%",
	"ASB - SMT50 moisture mode",
	"ASB - SMT50 temperature mode",
	"ASB - SMT100-analog moisture mode",
	"ASB - SMT100-analog temperature mode",

	"ASB - Vegetronix VH400",
	"ASB - Vegetronix THERM200",
	"ASB - Vegetronix AquaPlumb",

	"ASB - user defined sensor",
#endif
#if defined ADS1115||PCF8591
	"OSPi analog input - voltage mode 0..3.3V",
	"OSPi analog input - 0.3.3V to 0..100%",
	"OSPi analog input - SMT50 moisture mode",
	"OSPi analog input - SMT50 temperature mode",
#endif
#if defined(OSPI)
    "Internal Raspbery Pi temperature",
#endif
	"MQTT subscription",
	"Remote opensprinkler sensor",
	"Weather data - temperature (°F)",
	"Weather data - temperature (°C)",
	"Weather data - humidity (%)",
	"Weather data - precip (inch)",
	"Weather data - precip (mm)",
	"Weather data - wind (mph)",
	"Weather data - wind (kmh)",
	"Sensor group with min value",
	"Sensor group with max value",
	"Sensor group with avg value",
	"Sensor group with sum value",
#if defined(ESP8266)
	"Free Memory",
	"Free Storage",
#endif
};

void free_tmp_memory() {
#if defined(ESP8266)
	DEBUG_PRINT(F("freememory start: "));
	DEBUG_PRINTLN(freeMemory());

	sensor_save_all();
	sensor_api_free();

	DEBUG_PRINT(F("freememory now: "));
	DEBUG_PRINTLN(freeMemory());
#endif
}

void restore_tmp_memory() {
#if defined(ESP8266)
	sensor_api_init(false);

	DEBUG_PRINT(F("freememory restore: "));
	DEBUG_PRINTLN(freeMemory());
#endif
}

/**
 * sf
 * List supported sensor types
 **/
void server_sensor_types(OTF_PARAMS_DEF) {
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
#else
	char *p = get_buffer;
#endif

	DEBUG_PRINTLN(F("server_sensor_types"));

#if defined(USE_OTF)
	// as the log data can be large, we will use ESP8266's sendContent function to
	// send multiple packets of data, instead of the standard way of using send().
	rewind_ether_buffer();
	print_header(OTF_PARAMS);
#else
	print_header();
#endif

	int count = sizeof(sensor_types)/sizeof(int);
	boolean use_asb = get_asb_detected_boards() > 0;
	if (!use_asb) count -= 10;

	bfill.emit_p(PSTR("{\"count\":$D,\"detected\":$D,\"sensorTypes\":["), count, get_asb_detected_boards());

	for (uint i = 0; i < sizeof(sensor_types)/sizeof(int); i++)
	{
		int type = sensor_types[i];
		if (!use_asb && type >= SENSOR_ANALOG_EXTENSION_BOARD && type <= SENSOR_USERDEF)
			continue;
		if (i > 0)
			bfill.emit_p(PSTR(","));
		unsigned char unitid = getSensorUnitId(type);
		bfill.emit_p(PSTR("{\"type\":$D,\"name\":\"$S\",\"unit\":\"$S\",\"unitid\":$D}"),
			type, sensor_names[i], getSensorUnit(unitid), unitid);
		send_packet(OTF_PARAMS);
	}
	bfill.emit_p(PSTR("]}"));

	handle_return(HTML_OK);
}

/**
 * du
 * system resources status
 **/
void server_usage(OTF_PARAMS_DEF) {
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
#else
	char *p = get_buffer;
#endif

	DEBUG_PRINTLN(F("server_usage"));

#if defined(USE_OTF)
	// as the log data can be large, we will use ESP8266's sendContent function to
	// send multiple packets of data, instead of the standard way of using send().
	rewind_ether_buffer();
	print_header(OTF_PARAMS);
#else
	print_header();
#endif

extern uint32_t ping_ok;

#if defined(ESP8266)

	struct FSInfo fsinfo;

	boolean ok = LittleFS.info(fsinfo);

	bfill.emit_p(PSTR("{\"status\":$D,\"freeMemory\":$D,\"totalBytes\":$D,\"usedBytes\":$D,\"freeBytes\":$D,\"blockSize\":$D,\"pageSize\":$D,\"maxOpenFiles\":$D,\"maxPathLength\":$D,\"pingok\":$D,\"mqtt\":$D,\"ifttt\":$D"),
		ok,
		freeMemory(),
		fsinfo.totalBytes,
		fsinfo.usedBytes,
		fsinfo.totalBytes-fsinfo.usedBytes,
		fsinfo.blockSize,
		fsinfo.pageSize,
		fsinfo.maxOpenFiles,
		fsinfo.maxPathLength,
		ping_ok,
		os.mqtt.connected(),
		get_notif_enabled());

#else
	bfill.emit_p(PSTR("{\"status\":$D,\"mqtt\":$D,\"ifttt\":$D"), 1, os.mqtt.connected(), get_notif_enabled());

#endif

	bfill.emit_p(PSTR(",\"logfiles\":{\"l01\":$D,\"l02\":$D,\"l11\":$D,\"l12\":$D,\"l21\":$D,\"l22\":$D}"), 
		file_size(SENSORLOG_FILENAME1) / sizeof(SensorLog_t),
		file_size(SENSORLOG_FILENAME2) / sizeof(SensorLog_t),
		file_size(SENSORLOG_FILENAME_WEEK1) / sizeof(SensorLog_t),
		file_size(SENSORLOG_FILENAME_WEEK2) / sizeof(SensorLog_t),
		file_size(SENSORLOG_FILENAME_MONTH1) / sizeof(SensorLog_t),
		file_size(SENSORLOG_FILENAME_MONTH2) / sizeof(SensorLog_t));

	bfill.emit_p(PSTR("}"));


	handle_return(HTML_OK);
}

/**
 * sd
 * Program calc
 **/
void server_sensorprog_calc(OTF_PARAMS_DEF) {
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
#else
	char *p = get_buffer;
#endif

	DEBUG_PRINTLN(F("server_sensorprog_calc"));
	//uint nr or uint prog

	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("nr"), true)) {
		uint nr = strtoul(tmp_buffer, NULL, 0); // Adjustment nr
		double adj = calc_sensor_watering_by_nr(nr);
		bfill.emit_p(PSTR("{\"adjustment\":$E}"), adj);
		handle_return(HTML_OK);
	}

	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("prog"), true)) {
		uint prog = strtoul(tmp_buffer, NULL, 0); // Adjustment nr
    	double adj = calc_sensor_watering(prog);
		bfill.emit_p(PSTR("{\"adjustment\":$E}"), adj);
		handle_return(HTML_OK);
	}

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("type"), true))
		handle_return(HTML_DATA_MISSING);
	uint type = strtoul(tmp_buffer, NULL, 0); // Adjustment type
	if (type == 0) {
		handle_return(HTML_DATA_MISSING);
	}

	//methods for visual calculation:
	ProgSensorAdjust_t progAdj;
	memset(&progAdj, 0, sizeof(progAdj));
	progAdj.type = type;	
	
	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("sensor"), true))
		handle_return(HTML_DATA_MISSING);
	progAdj.sensor = strtoul(tmp_buffer, NULL, 0);
	Sensor_t *sensor = sensor_by_nr(progAdj.sensor); // Sensor nr
	if (!sensor)
		handle_return(HTML_DATA_MISSING);
	
	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("factor1"), true))
		handle_return(HTML_DATA_MISSING);
	progAdj.factor1 = atof(tmp_buffer); // Factor 1

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("factor2"), true))
		handle_return(HTML_DATA_MISSING);
	progAdj.factor2 = atof(tmp_buffer); // Factor 2

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("min"), true))
		handle_return(HTML_DATA_MISSING);
	progAdj.min = atof(tmp_buffer); // Min value

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("max"), true))
		handle_return(HTML_DATA_MISSING);
	progAdj.max = atof(tmp_buffer); // Max value

	unsigned char unitId = getSensorUnitId(sensor);

	int diff = progAdj.max-progAdj.min;
	int minEx = progAdj.min - diff/2;
	if (minEx < 0 && (unitId == UNIT_PERCENT || (unitId >= UNIT_VOLT && unitId < UNIT_USERDEF)))
		minEx = 0;
	int maxEx = progAdj.max + diff/2;

#if defined(USE_OTF)
	// as the log data can be large, we will use ESP8266's sendContent function to
	// send multiple packets of data, instead of the standard way of using send().
	rewind_ether_buffer();
	print_header(OTF_PARAMS);
#else
	print_header();
#endif

	bfill.emit_p(PSTR("{\"adjustment\":{\"min\":$D,\"max\":$D,\"current\":$E,\"adjust\":$E,\"unit\":\"$S\","), minEx, maxEx, 
		sensor->last_data, calc_sensor_watering_int(&progAdj, sensor->last_data), getSensorUnit(sensor));

	int nvalues = max(11, maxEx-minEx+1);
	double inVal[nvalues];
	double outVal[nvalues];
	for (int i = 0; i < nvalues; i++) {
		inVal[i] = (double)(maxEx - minEx) * (double)i / (nvalues-1) + minEx;
		outVal[i] = calc_sensor_watering_int(&progAdj, inVal[i]);
	}

	bfill.emit_p(PSTR("\"inval\":["));
	for (int i = 0; i < nvalues; i++) {
		if(i > 0) bfill.emit_p(PSTR(","));
		bfill.emit_p(PSTR("$E"), inVal[i]);
	}
	bfill.emit_p(PSTR("],\"outval\":["));
	for (int i = 0; i < nvalues; i++) {
		if(i > 0) bfill.emit_p(PSTR(","));
		bfill.emit_p(PSTR("$E"), outVal[i]);
	}
	bfill.emit_p(PSTR("]}}"));
	
	handle_return(HTML_OK);
}

const int prog_types[] = {
	PROG_NONE,
	PROG_LINEAR,
	PROG_DIGITAL_MIN,
	PROG_DIGITAL_MAX,
	PROG_DIGITAL_MINMAX,
};

const char* prog_names[] = {
	"No Adjustment",
	"Linear scaling",
	"Digital under min",
	"Digital over max",
	"Digital under min or over max",
};

/**
 * sh
 * List supported adjustment types
 */
void server_sensorprog_types(OTF_PARAMS_DEF) {
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
#else
	char *p = get_buffer;
#endif

	DEBUG_PRINTLN(F("server_sensorprog_types"));

#if defined(USE_OTF)
	// as the log data can be large, we will use ESP8266's sendContent function to
	// send multiple packets of data, instead of the standard way of using send().
	rewind_ether_buffer();
	print_header(OTF_PARAMS);
#else
	print_header();
#endif

	bfill.emit_p(PSTR("{\"count\":$D,\"progTypes\":["), sizeof(prog_types)/sizeof(int));

	for (uint i = 0; i < sizeof(prog_types)/sizeof(int); i++)
	{

		if (i > 0)
			bfill.emit_p(PSTR(","));
		bfill.emit_p(PSTR("{\"type\":$D,\"name\":\"$S\"}"), prog_types[i], prog_names[i]);

		send_packet(OTF_PARAMS);
	}
	bfill.emit_p(PSTR("]}"));

	handle_return(HTML_OK);
}

/**
 * sx
 * @brief backup sensor configuration
 *
 */
void server_sensorconfig_backup(OTF_PARAMS_DEF) {
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
#else
	char *p = get_buffer;
#endif

#define BACKUP_SENSORS 1
#define BACKUP_ADJUSTMENTS 2
#define BACKUP_MONITORS 4

	//Backup type: 0=no backup 1=Sensors 2=Adjustments 3=Sensors+Adjustments
	int backup = BACKUP_SENSORS|BACKUP_ADJUSTMENTS|BACKUP_MONITORS;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("backup"), true)) {
		backup = strtol(tmp_buffer, NULL, 0); 
	}


#if defined(USE_OTF)
	// as the log data can be large, we will use ESP8266's sendContent function to
	// send multiple packets of data, instead of the standard way of using send().
	rewind_ether_buffer();
	print_header(OTF_PARAMS);
#else
	print_header();
#endif

	ulong time = os.now_tz();
	bfill.emit_p(PSTR("{\"backup\":$D,\"time\":$L,\"os-version\":$D,\"minor\":$D"), backup, time, OS_FW_VERSION, OS_FW_MINOR);
	if (backup & BACKUP_SENSORS) {
		bfill.emit_p(PSTR(",\"sensors\":["));
		sensorconfig_json(OTF_PARAMS);
		bfill.emit_p(PSTR("]"));
		send_packet(OTF_PARAMS);
	}
	if (backup & BACKUP_ADJUSTMENTS)  {
		bfill.emit_p(PSTR(",\"progadjust\":["));
		progconfig_json();
		bfill.emit_p(PSTR("]"));
		send_packet(OTF_PARAMS);
	}
	send_packet(OTF_PARAMS);
	if (backup & BACKUP_MONITORS)  {
		bfill.emit_p(PSTR(",\"monitors\":["));
		monitorconfig_json();
		bfill.emit_p(PSTR("]"));
	}
	bfill.emit_p(PSTR("}"));
	send_packet(OTF_PARAMS);

	handle_return(HTML_OK);
}

/**
 * is
 * @brief influx set config
 *
 */
void server_influx_set(OTF_PARAMS_DEF) {
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
#else
	char *p = get_buffer;
#endif

	int enabled = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("en"), true)) {
		enabled = strtol(tmp_buffer, NULL, 0); 
	}

	char *url = NULL;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("url"), true)) {
		urlDecodeAndUnescape(tmp_buffer);
		DEBUG_PRINTLN(tmp_buffer);
		url = strdup(tmp_buffer);
	}

	int port = 8086;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("port"), true)) {
		DEBUG_PRINTLN(tmp_buffer);
		port = strtol(tmp_buffer, NULL, 0); 
	}

	char *org = NULL;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("org"), true)) {
		urlDecodeAndUnescape(tmp_buffer);
		DEBUG_PRINTLN(tmp_buffer);
		org = strdup(tmp_buffer);
	}

	char *bucket = NULL;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("bucket"), true)) {
		urlDecodeAndUnescape(tmp_buffer);
		DEBUG_PRINTLN(tmp_buffer);
		bucket = strdup(tmp_buffer);
	}

	char *token = NULL;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("token"), true)) {
		urlDecodeAndUnescape(tmp_buffer);
		DEBUG_PRINTLN(tmp_buffer);
		token = strdup(tmp_buffer);
	}

#if defined(USE_OTF)
	// as the log data can be large, we will use ESP8266's sendContent function to
	// send multiple packets of data, instead of the standard way of using send().
	rewind_ether_buffer();
	print_header(OTF_PARAMS);
#else
	print_header();
#endif

	os.influxdb.set_influx_config(enabled, url, port, org, bucket, token);

	handle_return(HTML_OK);
}


/**
 * ig
 * @brief influx get config
 *
 */
void server_influx_get(OTF_PARAMS_DEF) {
#if defined(USE_OTF)
	if(!process_password(OTF_PARAMS)) return;
#else
	char *p = get_buffer;
#endif

#if defined(USE_OTF)
	// as the log data can be large, we will use ESP8266's sendContent function to
	// send multiple packets of data, instead of the standard way of using send().
	rewind_ether_buffer();
	print_header(OTF_PARAMS);
#else
	print_header();
#endif
	server_influx_get_main();

	send_packet(OTF_PARAMS);
	handle_return(HTML_OK);
}

void server_influx_get_main() {
	os.influxdb.get_influx_config(tmp_buffer);
	bfill.emit_p(tmp_buffer);
}

typedef void (*URLHandler)(OTF_PARAMS_DEF);

/* Server function urls
 * To save RAM space, each GET command keyword is exactly
 * 2 characters long, with no ending 0
 * The order must exactly match the order of the
 * handler functions below
 */
const char _url_keys[] PROGMEM =
	"cv"
	"jc"
	"dp"
	"cp"
	"cr"
	"mp"
	"up"
	"jp"
	"co"
	"jo"
	"sp"
	"js"
	"cm"
	"cs"
	"jn"
	"je"
	"jl"
	"dl"
	"su"
	"cu"
	"ja"
	"pq"
	"si"
	"sj"
	"sk"
	"sc"
	"sl"
	"sg"
	"sr"
	"sa"
	"so"
	"sn"
	"sb"
	"sd"
	"se"
	"sf"
	"du"
	"sh"
	"sx"
    "db"
	"is"
	"ig"
	"mc"
	"ml"
	"mt"
#if defined(ARDUINO)
	//"ff"
#endif
	;

// Server function handlers
URLHandler urls[] = {
	server_change_values,   // cv
	server_json_controller, // jc
	server_delete_program,  // dp
	server_change_program,  // cp
	server_change_runonce,  // cr
	server_manual_program,  // mp
	server_moveup_program,  // up
	server_json_programs,   // jp
	server_change_options,  // co
	server_json_options,    // jo
	server_change_password, // sp
	server_json_status,     // js
	server_change_manual,   // cm
	server_change_stations, // cs
	server_json_stations,   // jn
	server_json_station_special,// je
	server_json_log,        // jl
	server_delete_log,      // dl
	server_view_scripturl,  // su
	server_change_scripturl,// cu
	server_json_all,        // ja
	server_pause_queue,     // pq
	server_sensor_config_userdef,//si
	server_sensorurl_get,//sj
	server_sensorurl_config,//sk
	server_sensor_config,//sc
	server_sensor_list,//sl
	server_sensor_get,//sg
	server_sensor_readnow,//sr
	server_set_sensor_address,//sa
	server_sensorlog_list,//so
	server_sensorlog_clear,//sn
	server_sensorprog_config,//sb
	server_sensorprog_calc,//sd
	server_sensorprog_list,//se
	server_sensor_types,//sf
	server_usage,//du
	server_sensorprog_types,//sh
	server_sensorconfig_backup,//sx
	server_json_debug,      // db
	server_influx_set,// is
	server_influx_get,// ig
	server_monitor_config, // mc
	server_monitor_list, // ml
	server_monitor_types, // mt
	//server_fill_files,
};

// handle Ethernet request
#if defined(ESP8266)
void on_ap_update(OTF_PARAMS_DEF) {
	print_header(OTF_PARAMS, false, strlen_P((char*)ap_update_html));
	res.writeBodyChunk((char *) "%s", ap_update_html);
}

void on_sta_update(OTF_PARAMS_DEF) {
	if(req.isCloudRequest()) otf_send_result(OTF_PARAMS, HTML_NOT_PERMITTED, "fw update");
	else {
		print_header(OTF_PARAMS, false, strlen_P((char*)sta_update_html));
		res.writeBodyChunk((char *) "%s", sta_update_html);
	}
}

void on_sta_upload_fin() {
	if(!(update_server->hasArg("pw") && os.password_verify(update_server->arg("pw").c_str()))) {
		update_server_send_result(HTML_UNAUTHORIZED);
		Update.end(false);
		return;
	}
	// finish update and check error
	if(!Update.end(true) || Update.hasError()) {
		update_server_send_result(HTML_UPLOAD_FAILED);
		//handle_return(HTML_UPLOAD_FAILED);
	}

	update_server_send_result(HTML_SUCCESS);
	os.reboot_dev(REBOOT_CAUSE_FWUPDATE);
}

void on_ap_upload_fin() { on_sta_upload_fin(); }

void on_sta_upload() {
	HTTPUpload& upload = update_server->upload();
	if(upload.status == UPLOAD_FILE_START){
		// todo:
		// WiFiUDP::stopAll();
		//mqtt_client->disconnect();
		DEBUG_PRINT(F("upload: "));
		DEBUG_PRINTLN(upload.filename);
		uint32_t maxSketchSpace = (ESP.getFreeSketchSpace()-0x1000)&0xFFFFF000;
		if(!Update.begin(maxSketchSpace)) {
			DEBUG_PRINT(F("begin failed "));
			DEBUG_PRINTLN(maxSketchSpace);
		}

	} else if(upload.status == UPLOAD_FILE_WRITE) {
		DEBUG_PRINT(".");
		if(Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
			DEBUG_PRINTLN(F("size mismatch"));
		}

	} else if(upload.status == UPLOAD_FILE_END) {

		DEBUG_PRINTLN(F("completed"));

	} else if(upload.status == UPLOAD_FILE_ABORTED){
		Update.end();
		DEBUG_PRINTLN(F("aborted"));
	}
	delay(0);
}

void on_ap_upload() { on_sta_upload(); }

void start_server_client() {
	if(!otf) return;
	static bool callback_initialized = false;

	if(!callback_initialized) {
		otf->on("/", server_home);  // handle home page
		otf->on("/index.html", server_home);
		otf->on("/update", on_sta_update, OTF::HTTP_GET); // handle firmware update
		update_server->on("/update", HTTP_POST, on_sta_upload_fin, on_sta_upload);

		// set up all other handlers
		char uri[4];
		uri[0]='/';
		uri[3]=0;
		for(unsigned char i=0;i<sizeof(urls)/sizeof(URLHandler);i++) {
			uri[1]=pgm_read_byte(_url_keys+2*i);
			uri[2]=pgm_read_byte(_url_keys+2*i+1);
			otf->on(uri, urls[i]);
		}
		callback_initialized = true;
	}
	update_server->begin();
}

void start_server_ap() {
	if(!otf) return;

	scanned_ssids = scan_network();
	String ap_ssid = get_ap_ssid();
	start_network_ap(ap_ssid.c_str(), NULL);
	delay(500);
	otf->on("/", on_ap_home);
	otf->on("/jsap", on_ap_scan);
	otf->on("/ccap", on_ap_change_config);
	otf->on("/jtap", on_ap_try_connect);
	otf->on("/update", on_ap_update, OTF::HTTP_GET);
	update_server->on("/update", HTTP_POST, on_ap_upload_fin, on_ap_upload);
	otf->onMissingPage(on_ap_home);
	update_server->begin();

	// set up all other handlers
	char uri[4];
	uri[0]='/';
	uri[3]=0;
	for(unsigned char i=0;i<sizeof(urls)/sizeof(URLHandler);i++) {
		uri[1]=pgm_read_byte(_url_keys+2*i);
		uri[2]=pgm_read_byte(_url_keys+2*i+1);
		otf->on(uri, urls[i]);
	}

	os.lcd.setCursor(0, -1);
	os.lcd.print(F("OSAP:"));
	os.lcd.print(ap_ssid);
	os.lcd.setCursor(0, 2);
	os.lcd.print(WiFi.softAPIP());
}

#endif

#if defined(USE_OTF) && !defined(ARDUINO)
void initialize_otf() {
	if(!otf) return;
	static bool callback_initialized = false;

	if(!callback_initialized) {
		otf->on("/", server_home);  // handle home page
		otf->on("/index.html", server_home);

		// set up all other handlers
		char uri[4];
		uri[0]='/';
		uri[3]=0;
		for(unsigned char i=0;i<sizeof(urls)/sizeof(URLHandler);i++) {
			uri[1]=pgm_read_byte(_url_keys+2*i);
			uri[2]=pgm_read_byte(_url_keys+2*i+1);
			otf->on(uri, urls[i]);
		}
		callback_initialized = true;
	}
}
#endif

#if !defined(USE_OTF)
// This funtion is only used for non-OTF platforms
void handle_web_request(char *p) {
	rewind_ether_buffer();

	// assume this is a GET request
	// GET /xx?xxxx
	char *com = p+5;
	char *dat = com+3;

	if(com[0]==' ') {
		server_home();  // home page handler
		send_packet();
		m_client->stop();
	} else {
		// server funtion handlers
		unsigned char i;
		for(i=0;i<sizeof(urls)/sizeof(URLHandler);i++) {
			if(pgm_read_byte(_url_keys+2*i)==com[0]
			 &&pgm_read_byte(_url_keys+2*i+1)==com[1]) {

				// check password
				int ret = HTML_UNAUTHORIZED;

				if (com[0]=='s' && com[1]=='u') { // for /su do not require password
					get_buffer = dat;
					(urls[i])();
					ret = return_code;
				} else if ((com[0]=='j' && com[1]=='o') ||
									 (com[0]=='j' && com[1]=='a'))  { // for /jo and /ja we output fwv if password fails
					if(check_password(dat)==false) {
						print_header();
						bfill.emit_p(PSTR("{\"$F\":$D}"),
									 iopt_json_names+0, os.iopts[0]);
						ret = HTML_OK;
					} else {
						get_buffer = dat;
						(urls[i])();
						ret = return_code;
					}
				} else if (com[0]=='d' && com[1]=='b') {
					get_buffer = dat;
					(urls[i])();
					ret = return_code;
				} else {
					// first check password
					if(check_password(dat)==false) {
						ret = HTML_UNAUTHORIZED;
					} else {
						get_buffer = dat;
						(urls[i])();
						ret = return_code;
					}
				}
				if (ret == -1) {
					if (m_client)
						m_client->stop();
					return;
				}
				switch(ret) {
				case HTML_OK:
					break;
				case HTML_REDIRECT_HOME:
					print_header(false);
					bfill.emit_p(PSTR("$F"), htmlReturnHome);
					break;
				default:
					print_header();
					bfill.emit_p(PSTR("{\"result\":$D}"), ret);
				}
				break;
			}
		}

		if(i==sizeof(urls)/sizeof(URLHandler)) {
			// no server funtion found
			print_header();
			bfill.emit_p(PSTR("{\"result\":$D}"), HTML_PAGE_NOT_FOUND);
		}
		send_packet();
		m_client->stop();
	}
}
#endif

#if defined(ARDUINO)
#define NTP_NTRIES 10
/** NTP sync request */
#if defined(ESP8266)
// due to lwip not supporting UDP, we have to use configTime and time() functions
// othewise, using UDP is much faster for NTP sync
ulong getNtpTime() {
	static bool configured = false;
	static char customAddress[16];
	if(!configured) {
		unsigned char ntpip[4] = {
		os.iopts[IOPT_NTP_IP1],
		os.iopts[IOPT_NTP_IP2],
		os.iopts[IOPT_NTP_IP3],
		os.iopts[IOPT_NTP_IP4]};	// todo: handle changes to ntpip dynamically
		if (!os.iopts[IOPT_NTP_IP1] || os.iopts[IOPT_NTP_IP1] == '0') {
			DEBUG_PRINTLN(F("using default time servers"));
			configTime(0, 0, "time.google.com", "time.nist.gov", "time.windows.com");
		} else {
			DEBUG_PRINTLN(F("using custom time server"));
			String ntp = IPAddress(ntpip[0],ntpip[1],ntpip[2],ntpip[3]).toString();
			strncpy(customAddress, ntp.c_str(), sizeof customAddress);
			customAddress[sizeof customAddress - 1] = 0;
			configTime(0, 0, customAddress, "time.google.com", "time.nist.gov");
		}
		configured = true;
	}
	unsigned char tries = 0;
	ulong gt = 0;
	while(tries<NTP_NTRIES) {
		gt = time(NULL);
		if(gt>1577836800UL)	break;
		else gt = 0;
		delay(1000);
		tries++;
	}
	return gt;
}
#else	// AVR
ulong getNtpTime() {

	// only proceed if we are connected
	if(!os.network_connected()) return 0;

	uint16_t port = (uint16_t)(os.iopts[IOPT_HTTPPORT_1]<<8) + (uint16_t)os.iopts[IOPT_HTTPPORT_0];
	port = (port==8000) ? 8888:8000; // use a different port than http port
	UDP *udp = new EthernetUDP();

	#define NTP_PACKET_SIZE 48
	#define NTP_PORT 123
	#define N_PUBLIC_SERVERS 5

	static const char* public_ntp_servers[] = {
		"time.google.com",
		"time.nist.gov",
		"time.windows.com",
		"time.cloudflare.com",
		"pool.ntp.org" };
	static uint8_t sidx = 0;

	static unsigned char packetBuffer[NTP_PACKET_SIZE];
	unsigned char ntpip[4] = {
		os.iopts[IOPT_NTP_IP1],
		os.iopts[IOPT_NTP_IP2],
		os.iopts[IOPT_NTP_IP3],
		os.iopts[IOPT_NTP_IP4]};
	unsigned char tries=0;
	ulong gt = 0;
	ulong startt = millis();
	while(tries<NTP_NTRIES) {
		// sendNtpPacket
		udp->begin(port);

		memset(packetBuffer, 0, NTP_PACKET_SIZE);
		packetBuffer[0] = 0b11100011;  // LI, Version, Mode
		packetBuffer[1] = 0;  // Stratum, or type of clock
		packetBuffer[2] = 6;  // Polling Interval
		packetBuffer[3] = 0xEC;  // Peer Clock Precision
		// 8 bytes of zero for Root Delay & Root Dispersion
		packetBuffer[12] = 49;
		packetBuffer[13] = 0x4E;
		packetBuffer[14] = 49;
		packetBuffer[15] = 52;

		// use one of the public NTP servers if ntp ip is unset

		DEBUG_PRINT(F("ntp: "));
		int ret;
		if (!os.iopts[IOPT_NTP_IP1] || os.iopts[IOPT_NTP_IP1] == '0') {
			DEBUG_PRINT(public_ntp_servers[sidx]);
			ret = udp->beginPacket(public_ntp_servers[sidx], NTP_PORT);
		} else {
			DEBUG_PRINTLN(IPAddress(ntpip[0],ntpip[1],ntpip[2],ntpip[3]));
			ret = udp->beginPacket(ntpip, NTP_PORT);
		}
		if(ret!=1) {
			DEBUG_PRINT(F(" not available (ret: "));
			DEBUG_PRINT(ret);
			DEBUG_PRINTLN(")");
			udp->stop();
			tries++;
			sidx=(sidx+1)%N_PUBLIC_SERVERS;
			continue;
		} else {
			DEBUG_PRINTLN(F(" connected"));
		}
		udp->write(packetBuffer, NTP_PACKET_SIZE);
		udp->endPacket();
		// end of sendNtpPacket

		// process response
		ulong timeout = millis()+2000;
		while(millis() < timeout) {
			if(udp->parsePacket()) {
				udp->read(packetBuffer, NTP_PACKET_SIZE);
				ulong highWord = word(packetBuffer[40], packetBuffer[41]);
				ulong lowWord = word(packetBuffer[42], packetBuffer[43]);
				ulong secsSince1900 = highWord << 16 | lowWord;
				ulong seventyYears = 2208988800UL;
				ulong gt = secsSince1900 - seventyYears;
				// check validity: has to be larger than 1/1/2020 12:00:00
				if(gt>1577836800UL) {
					udp->stop();
					delete udp;
					DEBUG_PRINT(F("took "));
					DEBUG_PRINT(millis()-startt);
					DEBUG_PRINTLN(F("ms"));
					return gt;
				}
			}
		}
		tries++;
		udp->stop();
		sidx=(sidx+1)%N_PUBLIC_SERVERS;
	}
	if(tries==NTP_NTRIES) {DEBUG_PRINTLN(F("NTP failed!!"));}
	udp->stop();
	delete udp;
	return 0;
}
#endif
#endif
