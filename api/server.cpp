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

#include "server.h"
#include "../types.h"
#include "../OpenSprinkler.h"
#include "../core/program.h"
#include "../bfiller.h"
#include "../services/weather.h"
#include "../services/mqtt.h"
#include "../services/firmware_update.h"
#include "../core/scheduler.h"
#include "../storage/logging.h"
#include "storage/maintenance.h"
#include "handlers.h"
#include "routes.h"
#include "api/commands.h"

// External variables defined in main ion file
extern OTF::OpenThingsFramework *otf;
#define OTF_PARAMS_DEF const OTF::Request &req,OTF::Response &res
#define OTF_PARAMS req,res
#define FKV_SOURCE req
#define handle_return(x) {if(x!=HTML_OK) otf_send_result(req,res,x); return;}

#if defined(ARDUINO)
	#include <FS.h>
	#include <LittleFS.h>
	#include "../services/espconnect.h"
	#if defined(ESP8266)
	extern ESP8266WebServer *update_server;
	extern ENC28J60lwIP enc28j60;
	extern Wiznet5500lwIP w5500;
	extern lwipEth eth;
	#elif defined(ESP32)
	#include <WebServer.h>
	#include <Update.h>
	extern WebServer *update_server;
	#endif
#else
	#include <stdarg.h>
	#include <stdlib.h>
	#include "etherport.h"
#endif

extern char tmp_buffer[];
extern char ether_buffer[];
extern OpenSprinkler os;
extern ProgramData pd;
extern uint32_t flow_count;

static const char htmlMobileHeader[] PROGMEM =
	"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1.0,minimum-scale=1.0,user-scalable=no\">"
;

static const char htmlReturnHome[] PROGMEM =
	"<script>window.location=\"/\";</script>\n"
;

#if !defined(ARDUINO)
string two_digits(uint8_t x) {
	return std::to_string(x);
}
#else
String two_digits(uint8_t x) {
	return String(x/10) + (x%10);
}
#endif

String toHMS(uint32_t t) {
	return two_digits(t/3600)+":"+two_digits((t/60)%60)+":"+two_digits(t%60);
}

#if defined(ARDUINO)
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
	if(os.get_wifi_mode()!=OS_WIFI_MODE_AP) return;
	print_header_compressed_html(OTF_PARAMS, ap_home_html_gz_len);
	//res.writeBodyChunk((char *) "%s", ap_home_html_gz);
	res.writeBodyData((const __FlashStringHelper*)ap_home_html_gz, ap_home_html_gz_len);
}

void on_ap_scan(OTF_PARAMS_DEF) {
	if(os.get_wifi_mode()!=OS_WIFI_MODE_AP) return;
	print_header(OTF_PARAMS, CT_JSON, scanned_ssids.length());
	res.writeBodyChunk((char *)"%s",scanned_ssids.c_str());
}

void on_ap_change_config(OTF_PARAMS_DEF) {
	if(os.get_wifi_mode()!=OS_WIFI_MODE_AP) return;
	char *ssid = req.getQueryParameter("ssid");
	if(ssid!=NULL&&strlen(ssid)!=0) {
		bool storage_ok = true;
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
			if (!os.sopt_save(SOPT_STA_BSSID_CHL, extra)) storage_ok = false;
			*mac=0; // terminate bssid string
			str2mac(extra, os.wifi_bssid); // update controller variables
			os.wifi_channel = chl;
		} else {
			if (!os.sopt_save(SOPT_STA_BSSID_CHL, DEFAULT_EMPTY_STRING)) storage_ok = false;
		}
		if (!os.sopt_save(SOPT_STA_SSID, os.wifi_ssid.c_str())) storage_ok = false;
		if (!os.sopt_save(SOPT_STA_PASS, os.wifi_pass.c_str())) storage_ok = false;
		if (!storage_ok) {
			otf_send_result(OTF_PARAMS, HTML_INTERNAL_ERROR, "storage");
			return;
		}
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
	if(os.get_wifi_mode()!=OS_WIFI_MODE_AP) return;
	String json = "{";
	json += F("\"ip\":");
	json += (WiFi.status() == WL_CONNECTED) ? (uint32_t)WiFi.localIP() : 0;
	json += F("}");
	print_header(OTF_PARAMS, CT_JSON, json.length());
	res.writeBodyChunk((char *)"%s",json.c_str());
	if(WiFi.status() == WL_CONNECTED && WiFi.localIP()) {
		os.iopts[IOPT_WIFI_MODE] = OS_WIFI_MODE_STA;
		os.iopts_save();
		DEBUG_PRINTLN(F("IP received by client, restart."));
		reboot_in(1000);
	}
}
#endif


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

/** Manual start program
 * Command: /mp?pw=xxx&pid=xx&uwt=x&qo=x
 *
 * pw:	password
 * pid: program index (0 refers to the first program)
 * uwt: use weather (i.e. watering percentage)
 * qo: queue option (0: append; 1: insert at front; 2: replace (default) )
 */
void server_manual_program(OTF_PARAMS_DEF) {
	if(!process_password(OTF_PARAMS)) return;
	handle_return(execute_manual_program(ParamSource(req)));
}

/**
 * Change run-once program
 * Command: /cr?pw=xxx&t=[x,x,x...]&cnt?=xxx&int?=xxx&uwt?=xxx&&anno?=xxx
 *
 * pw: password
 * t:  station water time
 * cnt?: repeat count
 * int?: repeat interval
 * uwt?: use weather adjustment
 * anno?: annotation for station ordering (refer to program name annotation)
 */
void server_change_runonce(OTF_PARAMS_DEF) {
	if(!process_password(OTF_PARAMS)) return;
	handle_return(execute_runonce(ParamSource(req)));
}


/**
 * Delete a program
 * Command: /dp?pw=xxx&pid=xxx
 *
 * pw: password
 * pid:program index (-1 will delete all programs)
 */
void server_delete_program(OTF_PARAMS_DEF) {
	if(!process_password(OTF_PARAMS)) return;
	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("pid"), true))
		handle_return(HTML_DATA_MISSING);

	int pid=atoi(tmp_buffer);
	if (pid == -1) {
		if (!pd.eraseall()) handle_return(HTML_INTERNAL_ERROR);
	} else if (pid >= 0 && pid < pd.nprograms) {
		if (!pd.del(pid)) handle_return(HTML_INTERNAL_ERROR);
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
	if(!process_password(OTF_PARAMS)) return;
	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("pid"), true))
		handle_return(HTML_DATA_MISSING);

	int pid=atoi(tmp_buffer);
	if (!(pid>=1 && pid< pd.nprograms))
		handle_return(HTML_DATA_OUTOFBOUND);

	if (!pd.moveup(pid)) handle_return(HTML_INTERNAL_ERROR);

	handle_return(HTML_SUCCESS);
}

/**
 * Change a program
 * Command: /cp?pw=xxx&pid=x&v=[flag,days0,days1,[start0,start1,start2,start3],[dur0,dur1,dur2..]]
 *              &name=x&from=x&to=x&snadj=flag,uuid,x0,y0,x1,y1,...
 *
 * pw:    password
 * pid:   program index (-1 to add new)
 * v:     packed program data: flag, days0, days1, start times, durations
 * name:  program name
 * from:  start date (month*32+day); 0 = no restriction
 * to:    end date, same format as from; 0 = no restriction
 * snadj: (optional) sensor adjustment — comma-separated: enable flag (uint8), sensor UUID (uint16),
 *        then x,y point pairs (floats). Omit to preserve existing adjustment.
 *        e.g. snadj=1,5,0.0,1.0,500.0,0.5 — enabled, sensor uuid=5, two interpolation points.
 *        snadj=0,0 — disabled with no sensor assigned.
*/
const char _str_program[] PROGMEM = "Program ";
void server_change_program(OTF_PARAMS_DEF) {
	if(!process_password(OTF_PARAMS)) return;

	unsigned char i;

	ProgramStruct prog;

	// parse program index
	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("pid"), true)) handle_return(HTML_DATA_MISSING);

	int pid=atoi(tmp_buffer);
	if (!(pid>=-1 && pid< pd.nprograms)) handle_return(HTML_DATA_OUTOFBOUND);

	// check if "en" parameter is present
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("en"), true)) {
		if(pid<0) handle_return(HTML_DATA_OUTOFBOUND);
		handle_return(pd.set_flagbit(pid, PROGRAMSTRUCT_EN_BIT, (tmp_buffer[0]=='0')?0:1) ?
			HTML_SUCCESS : HTML_INTERNAL_ERROR);
	}

	// check if "uwt" parameter is present
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("uwt"), true)) {
		if(pid<0) handle_return(HTML_DATA_OUTOFBOUND);
		handle_return(pd.set_flagbit(pid, PROGRAMSTRUCT_UWT_BIT, (tmp_buffer[0]=='0')?0:1) ?
			HTML_SUCCESS : HTML_INTERNAL_ERROR);
	}

	// parse program name
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("name"), true)) {
		strReplaceQuoteBackslash(tmp_buffer);
		if (strncmp_P(tmp_buffer, PSTR(RUNONCE_REPEAT_PREFIX), sizeof(RUNONCE_REPEAT_PREFIX) - 1) == 0) {
			handle_return(HTML_NOT_PERMITTED);
		}
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

	SensorAdjustment *snadj_ptr = nullptr;
	SensorAdjustment snadj(SENSOR_UUID_NONE, 0, 0, nullptr);
	// snadj=flag,uuid,x0,y0,x1,y1,... — if absent, existing adjustment is left untouched
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("snadj"), true)) {
		char *ptr = tmp_buffer;
		char *end;
		uint8_t  adj_flag  = 0;
		uint16_t adj_uuid  = SENSOR_UUID_NONE;
		uint32_t point_count = 0;
		sensor_adjustment_point_t points[SENSOR_ADJUSTMENT_POINTS] = {0.0, 0.0};
		uint32_t v;

		v = strtoul(ptr, &end, 10);
		if (end == ptr || (*end != ',' && *end != '\0') || v > 0xFF) handle_return(HTML_DATA_FORMATERROR);
		adj_flag = (uint8_t)v;

		if (*end == ',') {
			ptr = end + 1;
			v = strtoul(ptr, &end, 10);
			if (end == ptr || (*end != ',' && *end != '\0') || v > 0xFFFF) handle_return(HTML_DATA_FORMATERROR);
			adj_uuid = (uint16_t)v;

			if (*end == ',') {
				ptr = end + 1;
				float last_x = -std::numeric_limits<float>::infinity();
				while (*ptr != '\0') {
					if (point_count >= SENSOR_ADJUSTMENT_POINTS) handle_return(HTML_DATA_FORMATERROR);
					float x = strtof(ptr, &end);
					if (end == ptr || *end != ',') handle_return(HTML_DATA_FORMATERROR);
					ptr = end + 1;
					float y = strtof(ptr, &end);
					if (end == ptr || (*end != ',' && *end != '\0')) handle_return(HTML_DATA_FORMATERROR);
					if (!isfinite(x) || !isfinite(y) || y < 0 || x < last_x) handle_return(HTML_DATA_FORMATERROR);
					points[point_count++] = {x, y};
					last_x = x;
					ptr = (*end == ',') ? end + 1 : end;
				}
			}
		}

		snadj = SensorAdjustment(adj_uuid, point_count, adj_flag, points);
		snadj_ptr = &snadj;
	}

	if(!findKeyVal(FKV_SOURCE,tmp_buffer,TMP_BUFFER_SIZE, "v",false)) handle_return(HTML_DATA_MISSING);
	char *pv = tmp_buffer+1;

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
		if (pd.nprograms >= MAX_NUM_PROGRAMS) handle_return(HTML_DATA_OUTOFBOUND);
		if(!pd.add(&prog, snadj_ptr)) handle_return(HTML_INTERNAL_ERROR);
	} else {
		if(!pd.modify(pid, &prog, snadj_ptr)) handle_return(HTML_INTERNAL_ERROR);
	}
	handle_return(HTML_SUCCESS);
}

void server_json_options_main() {
	unsigned char oid;
	bool emitted = false;
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

		uint8_t flags = iopt_get_flags(oid);
		// Skip retired and API-hidden options.
		if (flags & (IOPT_FLAG_RETIRED | IOPT_FLAG_HIDDEN_API)) continue;

		int32_t v=os.iopts[oid];
		if (flags & IOPT_FLAG_SIGNED_TIME) {
			v=water_time_decode_signed(v);
		}

		#if defined(ARDUINO)
		if (oid==IOPT_BOOST_TIME) {
			if (os.hw_type==HW_TYPE_AC || os.hw_type==HW_TYPE_UNKNOWN) continue;
			else v<<=2;
		}

		if (oid==IOPT_I_MIN_THRESHOLD || oid==IOPT_I_MAX_LIMIT) {
			if (os.hw_type==HW_TYPE_AC || os.hw_type==HW_TYPE_DC ) v*=10;
			else continue;
		}

		if (oid==IOPT_LATCH_ON_VOLTAGE || oid==IOPT_LATCH_OFF_VOLTAGE) {
			if (os.hw_type!=HW_TYPE_LATCH) continue;
		}

		if (oid==IOPT_TARGET_PD_VOLTAGE) {
			if (!HAS_TARGET_PD_VOLTAGE(os.hw_rev, os.hw_type)) continue;
		}

		if ((oid>=IOPT_SENSOR3_TYPE && oid<=IOPT_SENSOR4_OFF_DELAY)) {
			if (!sensor_available(2)) continue;
		}
		#else
		if (oid==IOPT_BOOST_TIME || oid==IOPT_I_MIN_THRESHOLD || oid==IOPT_I_MAX_LIMIT || oid==IOPT_LATCH_ON_VOLTAGE || oid==IOPT_LATCH_OFF_VOLTAGE || oid==IOPT_TARGET_PD_VOLTAGE) continue;
		// SN3/SN4 are OS 3.4-only hardware; never expose on Linux/Pi/DEMO targets
		if (oid>=IOPT_SENSOR3_TYPE && oid<=IOPT_SENSOR4_OFF_DELAY) continue;
		#endif

		#if defined(ARDUINO)
		if (oid==IOPT_HW_VERSION) {
			v+=os.hw_rev;	// for OS3.x, add hardware revision number
		}
		#endif

		// each json name takes up to 5 characters; iopt_get_json_name copies + NUL-terminates
		iopt_get_json_name(oid, tmp_buffer);
		if (emitted) bfill.emit_p(PSTR(","));
		bfill.emit_p(PSTR("\"$S\":$D"), tmp_buffer, v);
		emitted = true;
	}

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
	if(!process_password(OTF_PARAMS,true)) return;
	begin_response(res);
	print_header(OTF_PARAMS);
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
			bfill.emit_p(PSTR("$L,"),(uint32_t)prog.durations[i]);
		}
		bfill.emit_p(PSTR("$L],\""),(uint32_t)prog.durations[i]); // this is the last element
		// program name
		strncpy(tmp_buffer, prog.name, PROGRAM_NAME_SIZE);
		tmp_buffer[PROGRAM_NAME_SIZE] = 0;	// make sure the string ends
		bfill.emit_p(PSTR("$S\",[$D,$D,$D],"), tmp_buffer,prog.en_daterange,prog.daterange[0],prog.daterange[1]);
		// sensor adjustment embedded in each program entry
		{
			SensorAdjustment *adj = SensorAdjustment::read(pid, pd.nprograms);
			if (adj) {
				bfill.emit_p(PSTR("{\"flag\":$D,\"uuid\":$D,\"splits\":["), adj->flag, adj->uuid);
				for (int j = 0; j < adj->point_count; j++) {
					if (j) bfill.emit_p(PSTR(","));
					bfill.emit_p(PSTR("{\"x\":$E,\"y\":$E}"), adj->points[j].x, adj->points[j].y);
				}
				bfill.emit_p(PSTR("]}"));
			} else {
				bfill.emit_p(PSTR("{}"));
			}
		}
		bfill.emit_p(PSTR("]"));
		if(pid!=pd.nprograms-1) {
			bfill.emit_p(PSTR(","));
		}
	}
	bfill.emit_p(PSTR("]}"));
}

/** Output program data */
void server_json_programs(OTF_PARAMS_DEF) {
	if(!process_password(OTF_PARAMS)) return;
	begin_response(res);
	print_header(OTF_PARAMS);
	bfill.emit_p(PSTR("{"));
	server_json_programs_main(OTF_PARAMS);
	handle_return(HTML_OK);
}

/** Output per-program adjustment factors and the maximum effective station runtime. */
void server_json_program_adj(OTF_PARAMS_DEF) {
	if(!process_password(OTF_PARAMS)) return;
	begin_response(res);
	print_header(OTF_PARAMS);
	bfill.emit_p(PSTR("{\"jpa\":["));
	ProgramStruct prog;
	for(uint8_t pid=0; pid<pd.nprograms; pid++) {
		pd.read(pid, &prog);
		float wa = get_program_water_percent(prog) / 100.f;
		float sa = get_program_sensor_adj(pid);
		if(pid) bfill.emit_p(PSTR(","));
		bfill.emit_p(PSTR("{\"wa\":$E,\"sa\":$E,\"ta\":$E}"), wa, sa, wa * sa);
	}
	bfill.emit_p(PSTR("],\"maxrt\":$L}"), (uint32_t)MAX_RUNTIME_DURATION);
	handle_return(HTML_OK);
}

/** Output script url form */
void server_view_scripturl(OTF_PARAMS_DEF) {
	begin_response(res);
	print_header(OTF_PARAMS, CT_HTML);
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
										"\"lupt\":$L,\"lrbtc\":$D,\"lrun\":[$D,$D,$L,$L],\"pq\":$D,\"pt\":$L,\"nq\":$D,\"ocs\":$D,"),
							(uint32_t)curr_time,
							os.nboards,
							os.status.enabled,
							os.sn_sensors[0].active,
							os.sn_sensors[1].active,
							os.status.rain_delayed,
							(uint32_t)os.nvdata.rd_stop_time,
							os.nvdata.sunrise_time,
							os.nvdata.sunset_time,
							os.nvdata.external_ip,
							(uint32_t)os.checkwt_lasttime,
							(uint32_t)os.checkwt_success_lasttime,
							(uint32_t)os.powerup_lasttime,
							os.last_reboot_cause,
							pd.lastrun.station,
							pd.lastrun.program,
							(uint32_t)pd.lastrun.duration,
							pd.lastrun.endtime,
							os.status.pause_state,
							os.pause_timer,
							pd.nqueue,
							os.status.overcurrent_sid);

	// SN3/SN4 are only emitted when present. UI uses key-presence to
	// decide whether to render the corresponding controls.
	if (sensor_available(2)) {
		bfill.emit_p(PSTR("\"sn3\":$D,\"sn4\":$D,"),
			os.sn_sensors[2].active,
			os.sn_sensors[3].active);
	}

#if defined(ARDUINO)
	bfill.emit_p(PSTR("\"RSSI\":$D,"), (int16_t)WiFi.RSSI());
	bfill.emit_p(PSTR("\"apdv\":$D,"), os.actual_pd_voltage);
#endif

	bfill.emit_p(PSTR("\"otc\":{$O},\"otcs\":$D,"), SOPT_OTC_OPTS, otf->getCloudStatus());

	unsigned char mac[6] = {0};
#if defined(ARDUINO)
	os.load_hardware_mac(mac, useEth);
#else
	os.load_hardware_mac(mac, true);
#endif

	bfill.emit_p(PSTR("\"mac\":\"$X:$X:$X:$X:$X:$X\","), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	bfill.emit_p(PSTR("\"loc\":\"$O\",\"jsp\":\"$O\",\"wsp\":\"$O\",\"wto\":{$O},\"ifkey\":\"$O\",\"mqtt\":{$O},\"wtdata\":$S,\"wterr\":$D,\"wtrestr\":$D,\"dname\":\"$O\","),
							 SOPT_LOCATION,
							 SOPT_JAVASCRIPTURL,
							 SOPT_WEATHERURL,
							 SOPT_WEATHER_OPTS,
							 SOPT_IFTTT_KEY,
							 SOPT_MQTT_OPTS,
							 strlen(wt_rawData)==0?"{}":wt_rawData,
							 wt_errCode,
							 wt_restricted,
							 SOPT_DEVICE_NAME);

	bfill.emit_p(PSTR("\"email\":{$O},"), SOPT_EMAIL_OPTS);

	bfill.emit_p(PSTR("\"wls\":["));
	if (md_N == 0) {
		bfill.emit_p(PSTR("],"));
	}
	for (unsigned char idx = 0; idx < md_N; idx++) {
		bfill.emit_p(PSTR("$D"), (int)md_scales[idx]);
		bfill.emit_p((idx == md_N-1) ? PSTR("],") : PSTR(","));
	}

#if defined(ARDUINO)
	uint16_t current = os.read_current(true);
	if((!os.status.program_busy) && (current<os.baseline_current)) current=0;
	bfill.emit_p(PSTR("\"curr\":$D,"), current);
#endif
	if(os.iopts[IOPT_SENSOR1_TYPE]==SENSOR_TYPE_FLOW) {
		bfill.emit_p(PSTR("\"flcrt\":$L,\"flwrt\":$D,\"flcto\":$L,"), os.flowcount_rt, FLOWCOUNT_RT_WINDOW, flow_count);
	}

	bfill.emit_p(PSTR("\"sbits\":["));
	// print sbits
	for(bid=0;bid<os.nboards;bid++)
		bfill.emit_p(PSTR("$D,"), os.station_bits[bid]);
	bfill.emit_p(PSTR("0],\"ps\":["));
	// print ps
	for(sid=0;sid<os.nstations;sid++) {
		uint32_t rem = 0;
		unsigned char qid = pd.station_qid[sid];
		RuntimeQueueStruct *q = qid < pd.nqueue ? pd.queue + qid : nullptr;
		if (q) {
			uint32_t end_time = q->st + q->dur;
			if (!q->st || curr_time < q->st) rem = q->dur;
			else if (curr_time < end_time) rem = end_time - curr_time;
		}
		bfill.emit_p(PSTR("[$D,$L,$L,$D]"),
		q?q->pid:0, (uint32_t)rem, (uint32_t)(q?q->st:0), os.attrib_grp[sid]);
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

	bfill.emit_p(PSTR("}"));
}

/** Output controller variables in json */
void server_json_controller(OTF_PARAMS_DEF) {
	if(!process_password(OTF_PARAMS)) return;
	begin_response(res);
	print_header(OTF_PARAMS);

	bfill.emit_p(PSTR("{"));
	server_json_controller_main(OTF_PARAMS);
	handle_return(HTML_OK);
}

/** Output homepage */
void server_home(OTF_PARAMS_DEF)
{
	begin_response(res);
	print_header(OTF_PARAMS, CT_HTML);
	bfill.emit_p(PSTR("<!DOCTYPE html><html><head>$F</head><body><script>"), htmlMobileHeader);
	// send server variables and javascript packets
	bfill.emit_p(PSTR("var ver=$D,ipas=$D;</script>"),
							 OS_FW_VERSION, os.iopts[IOPT_IGNORE_PASSWORD]);

	bfill.emit_p(PSTR("<script src=\"$O/home.js\"></script></body></html>"), SOPT_JAVASCRIPTURL);

	handle_return(HTML_OK);
}

/**
 * Change controller variables
 * Command: /cv?pw=xxx&rsn=x&rrsn=x&rbt=x&en=x&rd=x&rocs=x&re=x&ap=x
 *
 * pw:	password
 * rsn: reset all stations (0 or 1)
 * rrsn:reset all running stations (0 or 1)
 * rbt: reboot controller (0 or 1)
 * en:	enable (0 or 1)
 * rd:	rain delay hours (0 turns off rain delay)
 * re:	remote extension mode
 * rocs: reset overcurrent status (0 or 1)
 * ap:	reset to AP mode (Arduino targets only)
 * update: launch update script (for OSPi/Linux only)
 */
void server_change_values(OTF_PARAMS_DEF)
{
	if(!process_password(OTF_PARAMS)) return;
	handle_return(execute_change_values(ParamSource(req), CV_ACTIONS_HTTP));
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
	if(!process_password(OTF_PARAMS)) return;

	#if defined(DEMO)
	handle_return(HTML_REDIRECT_HOME);
	#endif
	bool storage_ok = true;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("jsp"), true)) {
		tmp_buffer[TMP_BUFFER_SIZE-1]=0;	// make sure we don't exceed the maximum size
		// trim unwanted space characters
		string_remove_space(tmp_buffer);
		if (!os.sopt_save(SOPT_JAVASCRIPTURL, tmp_buffer)) storage_ok = false;
	}
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("wsp"), true)) {
		tmp_buffer[TMP_BUFFER_SIZE-1]=0;
		string_remove_space(tmp_buffer);
		if (!os.sopt_save(SOPT_WEATHERURL, tmp_buffer)) storage_ok = false;
	}
	if (!storage_ok) handle_return(HTML_INTERNAL_ERROR);
	begin_response(res);
	print_header(OTF_PARAMS, CT_HTML);
	bfill.emit_p(PSTR("$F"), htmlReturnHome);
	handle_return(HTML_OK);
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
	if(!process_password(OTF_PARAMS)) return;
	// temporarily save some old options values
	bool time_change = false;
	bool weather_change = false;
	bool sensor_change = false;
	bool storage_ok = true;
	#if defined(ARDUINO)
	bool tpdv_change = false;
	bool httpport_requested = false;
	const uint8_t previous_httpport_0 = os.iopts[IOPT_HTTPPORT_0];
	const uint8_t previous_httpport_1 = os.iopts[IOPT_HTTPPORT_1];
	#endif

	// !!! p and bfill share the same buffer, so don't write
	// to bfill before you are done analyzing the buffer !!!
	// process option values
	unsigned char err = 0;
	unsigned char prev_value;
	unsigned char max_value;
	for (unsigned char oid=0; oid<NUM_IOPTS; oid++) {

		uint8_t flags = iopt_get_flags(oid);
		// skip options that cannot be set through /co command
		if (flags & (IOPT_FLAG_RETIRED | IOPT_FLAG_READ_ONLY)) continue;
		// IOPT_DEVICE_ENABLE and IOPT_REMOTE_EXT_MODE are intentionally excluded from /co
		// (they're toggled via /jc) and don't fit a generic flag.
		if (oid==IOPT_DEVICE_ENABLE || oid==IOPT_REMOTE_EXT_MODE) continue;
		prev_value = os.iopts[oid];
		max_value = iopt_get_max(oid);

		// json name only
		char tbuf2[6];
		iopt_get_json_name(oid, tbuf2);
		if(findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, tbuf2)) {
			#if defined(ARDUINO)
			if (oid == IOPT_HTTPPORT_0 || oid == IOPT_HTTPPORT_1) httpport_requested = true;
			#endif
			int32_t v = atol(tmp_buffer);
			if (flags & IOPT_FLAG_SIGNED_TIME) {
				v=water_time_encode_signed(v);
			}
			if(oid==IOPT_BOOST_TIME) {
				 v>>=2;
			}
			if(oid==IOPT_I_MIN_THRESHOLD || oid==IOPT_I_MAX_LIMIT) {
				v/=10;
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
			if (oid==IOPT_USE_WEATHER) {
				weather_change = true;
				// California restriction is now indicated in wto and no longer by the highest bit of uwt. So we force that bit to 0
				os.iopts[oid] &= 0x7F;
			}
			if ((oid>=IOPT_SENSOR1_TYPE && oid<=IOPT_SENSOR2_OFF_DELAY) ||
			    (oid>=IOPT_SENSOR3_TYPE && oid<=IOPT_SENSOR4_OFF_DELAY)) sensor_change = true;
			#if defined(ARDUINO)
			if (oid==IOPT_TARGET_PD_VOLTAGE) tpdv_change = true;
			#endif
		}
	}
	#if defined(ARDUINO)
	if (httpport_requested) {
		uint16_t httpport = (uint16_t)(os.iopts[IOPT_HTTPPORT_1] << 8) | os.iopts[IOPT_HTTPPORT_0];
		if (httpport == FIRMWARE_UPDATE_PORT) {
			os.iopts[IOPT_HTTPPORT_0] = previous_httpport_0;
			os.iopts[IOPT_HTTPPORT_1] = previous_httpport_1;
			err = 1;
		}
	}
	#endif

	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("loc"), true)) {
		strReplaceQuoteBackslash(tmp_buffer);
		bool changed = false;
		if (!os.sopt_save(SOPT_LOCATION, tmp_buffer, &changed)) storage_ok = false;
		if (changed) {
			weather_change = true;
		}
	}
	uint8_t keyfound = 0;
	if(findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("wto"), true)) {
		bool changed = false;
		if (!os.sopt_save(SOPT_WEATHER_OPTS, tmp_buffer, &changed)) storage_ok = false;
		if (changed) {
			os.sopt_load(SOPT_WEATHER_OPTS, tmp_buffer+1); // make room for the leading '{'
			parse_wto(tmp_buffer); // parse wto
			apply_monthly_adjustment(os.now_tz());
			weather_change = true;
		}
	}

	keyfound = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("ifkey"), true, &keyfound)) {
		if (!os.sopt_save(SOPT_IFTTT_KEY, tmp_buffer)) storage_ok = false;
	} else if (keyfound) {
		tmp_buffer[0]=0;
		if (!os.sopt_save(SOPT_IFTTT_KEY, tmp_buffer)) storage_ok = false;
	}

	keyfound = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("otc"), true, &keyfound)) {
		if (!os.sopt_save(SOPT_OTC_OPTS, tmp_buffer)) storage_ok = false;
	} else if (keyfound) {
		tmp_buffer[0]=0;
		if (!os.sopt_save(SOPT_OTC_OPTS, tmp_buffer)) storage_ok = false;
	}

	keyfound = 0;
	if(findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("mqtt"), true, &keyfound)) {
		if (!os.sopt_save(SOPT_MQTT_OPTS, tmp_buffer)) storage_ok = false;
		os.status.req_mqtt_restart = true;
	} else if (keyfound) {
		tmp_buffer[0]=0;
		if (!os.sopt_save(SOPT_MQTT_OPTS, tmp_buffer)) storage_ok = false;
		os.status.req_mqtt_restart = true;
	}

	keyfound = 0;
	if(findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("email"), true, &keyfound)) {
		if (!os.sopt_save(SOPT_EMAIL_OPTS, tmp_buffer)) storage_ok = false;
	} else if (keyfound) {
		tmp_buffer[0]=0;
		if (!os.sopt_save(SOPT_EMAIL_OPTS, tmp_buffer)) storage_ok = false;
	}

	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("dname"), true)) {
		strReplaceQuoteBackslash(tmp_buffer);
		if (!os.sopt_save(SOPT_DEVICE_NAME, tmp_buffer)) storage_ok = false;
	}

	// if not using NTP and manually setting time
	if (!os.iopts[IOPT_USE_NTP] && findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("ttt"), true)) {
#if defined(ARDUINO)
		uint32_t t;
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

	if (!os.iopts_save()) storage_ok = false;
	if (!storage_ok) handle_return(HTML_INTERNAL_ERROR);
	os.populate_master();

#if defined(ARDUINO)
	if (tpdv_change) {
		os.setup_pd_voltage();
	}
#endif

	if(time_change) {
		os.status.req_ntpsync = 1;
	}

	if(weather_change) {
		DEBUG_PRINTLN("weather change happened");
		//os.iopts[IOPT_WATER_PERCENTAGE] = 100;  // reset watering percentage to 100%
		wt_restricted = 0; // reset wt_restrcited, wt_rawData and errCode
		wt_rawData[0] = 0;
		wt_errCode = HTTP_RQT_NOT_RECEIVED;
		os.checkwt_lasttime = 0;  // force weather update
		weather_sensor_reset_cache();
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

	if(!process_password(OTF_PARAMS)) return;

	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("npw"), true)) {
		const int pwBufferSize = TMP_BUFFER_SIZE/2;
		char *tbuf2 = tmp_buffer + pwBufferSize;	// use the second half of tmp_buffer
		if (findKeyVal(FKV_SOURCE, tbuf2, pwBufferSize, PSTR("cpw"), true) && strncmp(tmp_buffer, tbuf2, pwBufferSize) == 0) {
			handle_return(os.sopt_save(SOPT_PASSWORD, tmp_buffer) ? HTML_SUCCESS : HTML_INTERNAL_ERROR);
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
	if(!process_password(OTF_PARAMS)) return;
	begin_response(res);
	print_header(OTF_PARAMS);

	bfill.emit_p(PSTR("{"));
	server_json_status_main();
	handle_return(HTML_OK);
}

/**
 * Test station (previously manual operation)
 * Command: /cm?pw=xxx&sid=x&en=x&t=x&ssta=x&qo=x
 *
 * pw: password
 * sid:station index (starting from 0)
 * en: enable (0 or 1)
 * t:  timer (required if en=1)
 * ssta: shift remaining stations
 * qo: queuing option (0: append after others; 1: run now and pause others)
 */
void server_change_manual(OTF_PARAMS_DEF) {
	if(!process_password(OTF_PARAMS)) return;
	handle_return(execute_manual_station(ParamSource(req)));
}


#if defined(ARDUINO)
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
	if(!process_password(OTF_PARAMS)) return;

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

	// as the log data can be large, we will use ESP8266's sendContent function to
	// send multiple packets of data, instead of the standard way of using send().
	begin_response(res);
	print_header(OTF_PARAMS);

	bfill.emit_p(PSTR("["));

	bool comma = 0;
	for(unsigned int i=start;i<=end;i++) {
		snprintf(tmp_buffer, TMP_BUFFER_ALLOC_SIZE , "%d", i);
		make_logfile_name(tmp_buffer);

	#if defined(ARDUINO)
		File file = LittleFS.open(tmp_buffer, "r");
		if(!file) continue;
#else // prepare to open log file for Linux
		FILE *file = fopen(get_filename_fullpath(tmp_buffer), "rb");
		if(!file) continue;
#endif // prepare to open log file
		int result;
		while(true) {
		#if defined(ARDUINO)
			// do not use file.read_byte or read_byteUntil because it's very slow
			result = file_fgets(file, tmp_buffer, TMP_BUFFER_SIZE);
			if (result <= 0) {
				file.close();
				break;
			}
			tmp_buffer[result]=0;
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
	if(!process_password(OTF_PARAMS)) return;
	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("day"), true))
		handle_return(HTML_DATA_MISSING);

	handle_return(delete_log(tmp_buffer) ? HTML_SUCCESS : HTML_INTERNAL_ERROR);
}

/**
 * Command: "/pq?pw=x&dur=x&repl=x"
 * dur: duration (in units of seconds)
 * repl: replace (in units of seconds) (New UI allows for replace, extend, and pause using this)
 */
void server_pause_queue(OTF_PARAMS_DEF) {
	if(!process_password(OTF_PARAMS)) return;

	uint32_t duration = 0;
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

void server_json_sensors_main(OTF_PARAMS_DEF) {
	bfill.emit_p(PSTR("\"sn\":["));
	uint8_t sensor_count = 0;

	Sensor *sensor;
	for (size_t i = 0; i < os.nsensors; i++) {
		if (os.sensors[i].interval && (sensor = Sensor::get(i))) {
			if (sensor_count) bfill.emit_p(PSTR(","));
			bfill.emit_p(PSTR("{\"uuid\":$D,\"name\":\"$S\",\"unit\":$D,\"flag\":$D,\"status\":$D,\"interval\":$L,\"min\":$E,\"max\":$E,\"value\":$E,\"type\":$D,\"extra\":"), sensor->uuid, sensor->name, static_cast<uint8_t>(sensor->unit), sensor->flag, os.sensors[i].status, sensor->interval, sensor->min, sensor->max, os.sensors[i].value, static_cast<uint8_t>(sensor->get_sensor_type()));
			sensor->emit_extra_json(&bfill);
			bfill.emit_p(PSTR("}"));
			sensor_count += 1;
		}
	}


	bfill.emit_p(PSTR("],\"count\":$D}"), sensor_count);
}

/** Sensor status */
void server_json_sensors(OTF_PARAMS_DEF)
{
	if(!process_password(OTF_PARAMS)) return;
	begin_response(res);
	print_header(OTF_PARAMS);

	bfill.emit_p(PSTR("{"));
	server_json_sensors_main(OTF_PARAMS);
	handle_return(HTML_OK);
}

/**
 * Add or change a sensor
 * Command: /csn?pw=xxx&[uuid=xxx|sid=xxx]&type=xxx&...
 *
 * pw:   password
 * uuid: sensor stable ID (1-65535; -1 to add new)
 * sid:  sensor positional index (0-based; -1 to add new)
 *       (uuid takes precedence if both are provided)
 * type: sensor type (0: Aggregate, 1: ADS1115, 2: Weather)
 * name: sensor name
 * min/max: output clamping range
 * interval: sampling interval in minutes
 * unit: sensor unit index
 * flag: bitmask (bit 0: enable, bit 1: log)
 * [Aggregate] children: semicolon separated list of "uuid,scale,offset;"
 * [Aggregate] action: aggregate action index (0: Min, 1: Max, 2: Average, 3: Sum, 4: Median, 5: Range)
 * [ADS1115]  pin: pin number (1-16)
 * [Weather]  action: weather information index
 */
void server_change_sensor(OTF_PARAMS_DEF) {
	if(!process_password(OTF_PARAMS)) return;

	char *end;
	int32_t sid = -1;
	bool is_new = false;

	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("uuid"), true)) {
		int32_t uuid_param = (int32_t)strtol(tmp_buffer, &end, 10);
		if (*end != '\0') handle_return(HTML_DATA_FORMATERROR);
		if (uuid_param == -1) {
			is_new = true;
			sid = os.nsensors;
		} else {
			if (uuid_param < 1 || uuid_param > 0xFFFF) handle_return(HTML_DATA_OUTOFBOUND);
			sid = Sensor::find_index((uint16_t)uuid_param);
			if (sid >= os.nsensors) handle_return(HTML_DATA_OUTOFBOUND);
		}
	} else if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("sid"), true)) {
		int32_t sid_param = (int32_t)strtol(tmp_buffer, &end, 10);
		if (*end != '\0') handle_return(HTML_DATA_FORMATERROR);
		if (sid_param == -1) {
			is_new = true;
			sid = os.nsensors;
		} else {
			if (sid_param < 0 || sid_param >= os.nsensors) handle_return(HTML_DATA_OUTOFBOUND);
			sid = sid_param;
		}
	} else {
		handle_return(HTML_DATA_MISSING);
	}

	if (is_new && sid >= MAX_SENSORS) handle_return(HTML_DATA_OUTOFBOUND);

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("type"), true)) handle_return(HTML_DATA_MISSING);

	uint32_t type_raw = strtol(tmp_buffer, &end, 10);
	if (*end != '\0') handle_return(HTML_DATA_FORMATERROR);
	if (type_raw >= (uint32_t)SensorType::MAX_VALUE) handle_return(HTML_DATA_OUTOFBOUND);

	SensorType sensor_type = static_cast<SensorType>(type_raw);

	Sensor *sensor = nullptr;
	float min = SENSOR_DEFAULT_MIN;
	float max = SENSOR_DEFAULT_MAX;
	uint32_t interval = SENSOR_DEFAULT_INTERVAL;
	SensorUnit unit = SENSOR_DEFAULT_UNIT;
	uint8_t flag = SENSOR_DEFAULT_FLAG;
	bool min_set = false;
	bool max_set = false;
	bool interval_set = false;
	bool unit_set = false;

	char name[SENSOR_NAME_LEN];
	strncpy(name, SENSOR_DEFAULT_NAME, SENSOR_NAME_LEN);

	SensorType original_sensor_type = SensorType::MAX_VALUE;
	if (os.sensors[sid].interval) {
		if ((sensor = Sensor::get(sid))) {
			original_sensor_type = sensor->get_sensor_type();
			strncpy(name, sensor->name, SENSOR_NAME_LEN);
			min = sensor->min;
			max = sensor->max;
			interval = sensor->interval;
			unit = sensor->unit;
			flag = sensor->flag;
		}
	}

	// parse sensor name
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("name"), true)) {
		strReplaceQuoteBackslash(tmp_buffer);
		strncpy(name, tmp_buffer, SENSOR_NAME_LEN);
	}

	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("min"), true)) {
		min=strtod(tmp_buffer, &end);
		if (*end != '\0') handle_return(HTML_DATA_FORMATERROR);
		min_set = true;
	}

	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("max"), true)) {
		max=strtod(tmp_buffer, &end);
		if (*end != '\0') handle_return(HTML_DATA_FORMATERROR);
		max_set = true;
	}

	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("interval"), true)) {
		interval=strtoul(tmp_buffer, &end, 10);
		if (*end != '\0') handle_return(HTML_DATA_FORMATERROR);
		interval_set = true;
	}

	if (interval < 1) handle_return(HTML_DATA_OUTOFBOUND);

	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("unit"), true)) {
		uint32_t unit_raw = strtol(tmp_buffer, &end, 10);
		if (*end != '\0') handle_return(HTML_DATA_FORMATERROR);
		if (unit_raw >= (uint32_t)SensorUnit::MAX_VALUE) handle_return(HTML_DATA_OUTOFBOUND);
		unit = static_cast<SensorUnit>(unit_raw);
		unit_set = true;
	}

	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("flag"), true)) {
		flag = (uint8_t)strtoul(tmp_buffer, &end, 10);
		if (*end != '\0') handle_return(HTML_DATA_FORMATERROR);
	}

	Sensor *result_sensor;
	switch (sensor_type) {
		case SensorType::Aggregate: {
			uint8_t children_count = 0;
			aggregate_children_t children[AGGREGATE_SENSOR_CHILDREN_COUNT];
			for (size_t i = 0; i < AGGREGATE_SENSOR_CHILDREN_COUNT; i++) {
				children[i].uuid = SENSOR_UUID_NONE;
			}

			AggregateAction action = static_cast<AggregateAction>(AGGREGATE_DEFAULT_ACTION);

			if (sensor_type == original_sensor_type) {
				if ((sensor = Sensor::get(sid))) {
					AggregateSensor* e = static_cast<AggregateSensor*>(sensor);
					for (size_t i = 0; i < AGGREGATE_SENSOR_CHILDREN_COUNT; i++) {
						children[i] = e->children[i];
					}

					action = e->action;
				}
			}

			if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("children"), true)) {
				unsigned int i = 0;
				int d;
				float d1, d2;
				const char *ptr = tmp_buffer;
				int result;

				while (*ptr != '\0') {
					if (i >= AGGREGATE_SENSOR_CHILDREN_COUNT) handle_return(HTML_DATA_FORMATERROR);

					result = sscanf(ptr, "%d,%f,%f;", &d, &d1, &d2);

					if (result != 3) {
						handle_return(HTML_DATA_FORMATERROR);
					}

					// d is the child sensor's UUID; out-of-range values map to disabled
					uint16_t child_uuid = (d >= 1 && d <= 0xFFFF) ? (uint16_t)d : SENSOR_UUID_NONE;

					children[i++] = aggregate_children_t {d1, d2, child_uuid};

					while (*ptr != '\0' && *(ptr++) != ';') {}
				}

				children_count = i;
			}

			if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("action"), true)) {
				uint32_t action_raw = strtol(tmp_buffer, &end, 10);
				if (*end != '\0') handle_return(HTML_DATA_FORMATERROR);
				if (action_raw >= (uint32_t)AggregateAction::MAX_VALUE) handle_return(HTML_DATA_OUTOFBOUND);
				action = static_cast<AggregateAction>(action_raw);
			}

			result_sensor = new AggregateSensor(interval, min, max, (const char*)&name, unit, flag, os.sensors, children, children_count, action);
			break;
		}
		case SensorType::ADS1115: {
			uint32_t sensor_index = 0;
			uint32_t sensor_pin = 0;
			float scale = ADS1115_DEFAULT_SCALE;
			float offset = ADS1115_DEFAULT_OFFSET;
			ADS1115Subtype subtype = ADS1115Subtype::LINEAR;
			uint8_t num_points = 0;
			sensor_adjustment_point_t points[ADS1115_PIECEWISE_POINTS] = {};

			if (sensor_type == original_sensor_type) {
				if ((sensor = Sensor::get(sid))) {
					ADS1115Sensor* e = static_cast<ADS1115Sensor*>(sensor);
					sensor_index = e->sensor_index;
					sensor_pin = e->pin;
					scale = e->scale;
					offset = e->offset;
					subtype = e->subtype;
					num_points = e->num_points;
					for (uint8_t p = 0; p < num_points; p++) points[p] = e->points[p];
				}
			}

			if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("pin"), true)) {
				uint32_t raw_sensor_pin = strtoul(tmp_buffer, &end, 10);
				if (*end != '\0') handle_return(HTML_DATA_FORMATERROR);
				if (raw_sensor_pin == 0 || raw_sensor_pin > 16) handle_return(HTML_DATA_OUTOFBOUND);
				raw_sensor_pin -= 1;
				sensor_index = raw_sensor_pin >> 2;
				sensor_pin = raw_sensor_pin & 0b11;
			}

			if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("scale"), true)) {
				scale = strtod(tmp_buffer, &end);
				if (*end != '\0') handle_return(HTML_DATA_FORMATERROR);
			}

			if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("offset"), true)) {
				offset = strtod(tmp_buffer, &end);
				if (*end != '\0') handle_return(HTML_DATA_FORMATERROR);
			}

			if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("subtype"), true)) {
				uint32_t st = strtoul(tmp_buffer, &end, 10);
				if (*end != '\0') handle_return(HTML_DATA_FORMATERROR);
				if (st >= static_cast<uint32_t>(ADS1115Subtype::MAX_VALUE)) handle_return(HTML_DATA_OUTOFBOUND);
				ADS1115Subtype new_subtype = static_cast<ADS1115Subtype>(st);
				if (!ads1115_subtype_is_supported(new_subtype)) handle_return(HTML_DATA_OUTOFBOUND);
				if (new_subtype != subtype) {
					subtype = new_subtype;
					// Drop carried-over points when leaving PIECEWISE_LINEAR.
					if (subtype != ADS1115Subtype::PIECEWISE_LINEAR) {
						num_points = 0;
					}
				}
			}

			// points=x0,y0,x1,y1,... — only meaningful for PIECEWISE_LINEAR.
			if (subtype == ADS1115Subtype::PIECEWISE_LINEAR &&
			    findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("points"), true)) {
				char *ptr = tmp_buffer;
				uint8_t n = 0;
				sensor_adjustment_point_t parsed[ADS1115_PIECEWISE_POINTS] = {};
				float last_x = -std::numeric_limits<float>::infinity();
				while (*ptr != '\0') {
					if (n >= ADS1115_PIECEWISE_POINTS) handle_return(HTML_DATA_FORMATERROR);
					float x = strtof(ptr, &end);
					if (end == ptr || *end != ',') handle_return(HTML_DATA_FORMATERROR);
					ptr = end + 1;
					float y = strtof(ptr, &end);
					if (end == ptr || (*end != ',' && *end != '\0')) handle_return(HTML_DATA_FORMATERROR);
					if (!isfinite(x) || !isfinite(y) || x < last_x) handle_return(HTML_DATA_FORMATERROR);
					parsed[n++] = {x, y};
					last_x = x;
					ptr = (*end == ',') ? end + 1 : end;
				}
				num_points = n;
				for (uint8_t p = 0; p < n; p++) points[p] = parsed[p];
			}

			if (subtype == ADS1115Subtype::PIECEWISE_LINEAR && num_points < 2) {
				handle_return(HTML_DATA_MISSING);
			}

			// Baked subtypes constrain the unit to the same group as the formula's native unit.
			SensorUnit native_unit = ads1115_subtype_native_unit(subtype);
			if (native_unit != SensorUnit::None &&
			    get_sensor_unit_group(unit) != get_sensor_unit_group(native_unit)) {
				handle_return(HTML_DATA_OUTOFBOUND);
			}

			result_sensor = new ADS1115Sensor(interval, min, max, (const char*)&name, unit, flag,
			                                  os.ads1115_devices, sensor_index, sensor_pin,
			                                  scale, offset, subtype, num_points, points);
			break;
		}
		case SensorType::Weather: {
			WeatherAction action = WeatherAction::CurrentTemperature;

			if (sensor_type == original_sensor_type) {
				if ((sensor = Sensor::get(sid))) {
					WeatherSensor* e = static_cast<WeatherSensor*>(sensor);
					action = e->action;
				}
			}

			if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("action"), true)) {
				uint32_t action_raw = strtol(tmp_buffer, &end, 10);
				if (*end != '\0') handle_return(HTML_DATA_FORMATERROR);
				if (action_raw >= (uint32_t)WeatherAction::MAX_VALUE) handle_return(HTML_DATA_OUTOFBOUND);
				action = static_cast<WeatherAction>(action_raw);
			}

			if (original_sensor_type != SensorType::Weather) {
				uint32_t default_interval;
				float default_min, default_max;
				SensorUnit default_unit;
				weather_action_defaults(action, &default_interval, &default_min, &default_max, &default_unit);
				if (!interval_set) interval = default_interval;
				if (!min_set) min = default_min;
				if (!max_set) max = default_max;
				if (!unit_set) unit = default_unit;
			}
			if (!weather_action_unit_is_valid(action, unit)) handle_return(HTML_DATA_OUTOFBOUND);

			result_sensor = new WeatherSensor(interval, min, max, (const char*)&name, unit, flag, os.get_sensor_weather_data, action);

			break;
		}
		case SensorType::SystemInternal: {
			SystemMetric metric = SystemMetric::MAX_VALUE;

			if (sensor_type == original_sensor_type) {
				if ((sensor = Sensor::get(sid))) {
					SystemInternalSensor* e = static_cast<SystemInternalSensor*>(sensor);
					metric = e->metric;
				}
			}

			if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("metric"), true)) {
				uint32_t m = strtoul(tmp_buffer, &end, 10);
				if (*end != '\0') handle_return(HTML_DATA_FORMATERROR);
				if (m >= (uint32_t)SystemMetric::MAX_VALUE) handle_return(HTML_DATA_OUTOFBOUND);
				SystemMetric new_metric = static_cast<SystemMetric>(m);
				if (!system_metric_is_supported(new_metric)) handle_return(HTML_DATA_OUTOFBOUND);
				metric = new_metric;
			}

			if (metric == SystemMetric::MAX_VALUE) handle_return(HTML_DATA_MISSING);

			// Unit constraint: same group as the metric's native unit (Temperature for CPU temp).
			SensorUnit native_unit = system_metric_native_unit(metric);
			if (native_unit != SensorUnit::None &&
			    get_sensor_unit_group(unit) != get_sensor_unit_group(native_unit)) {
				handle_return(HTML_DATA_OUTOFBOUND);
			}

			result_sensor = new SystemInternalSensor(interval, min, max, (const char*)&name, unit, flag, metric);
			break;
		}
		case SensorType::OnboardDigital: {
			OnboardInput input = OnboardInput::MAX_VALUE;

			if (sensor_type == original_sensor_type) {
				if ((sensor = Sensor::get(sid))) {
					OnboardDigitalSensor* e = static_cast<OnboardDigitalSensor*>(sensor);
					input = e->input;
				}
			}

			if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("input"), true)) {
				uint32_t v = strtoul(tmp_buffer, &end, 10);
				if (*end != '\0') handle_return(HTML_DATA_FORMATERROR);
				if (v >= (uint32_t)OnboardInput::MAX_VALUE) handle_return(HTML_DATA_OUTOFBOUND);
				input = static_cast<OnboardInput>(v);
			}

			if (input == OnboardInput::MAX_VALUE) handle_return(HTML_DATA_MISSING);

			result_sensor = new OnboardDigitalSensor(interval, min, max, (const char*)&name, unit, flag, input);
			break;
		}
		default: {
			handle_return(HTML_DATA_OUTOFBOUND)
			break;
		}
	}

	if (is_new) {
		// Assign a new UUID for this sensor
		uint16_t previous_uuid = os.nvdata.last_sensor_uuid;
		uint16_t new_uuid = os.nvdata.last_sensor_uuid + 1;
		if (new_uuid == SENSOR_UUID_NONE) new_uuid = 1;
		os.nvdata.last_sensor_uuid = new_uuid;
		if (!os.nvdata_save()) {
			os.nvdata.last_sensor_uuid = previous_uuid;
			delete result_sensor;
			handle_return(HTML_INTERNAL_ERROR);
		}
		result_sensor->uuid = new_uuid;

		if (!Sensor::add(result_sensor)) {
			delete result_sensor;
			handle_return(HTML_INTERNAL_ERROR);
		}
	} else {
		result_sensor->uuid = os.sensors[sid].uuid;
		if (!Sensor::modify(sid, result_sensor)) {
			delete result_sensor;
			handle_return(HTML_INTERNAL_ERROR);
		}
	}

	delete result_sensor;
	weather_sensor_schedule_refresh();

	handle_return(HTML_SUCCESS);
}

/**
 * Delete a sensor
 * Command: /dsn?pw=xxx&[uuid=xxx|sid=xxx]
 *
 * pw:   password
 * uuid: sensor stable ID (1-65535; -1 to delete all sensors)
 * sid:  sensor positional index (0-based; -1 to delete all sensors)
 *       (uuid takes precedence if both are provided)
 */
void server_delete_sensor(OTF_PARAMS_DEF) {
	if(!process_password(OTF_PARAMS)) return;

	int32_t idx = -1;
	bool delete_all = false;
	char *end;

	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("uuid"), true)) {
		int32_t uuid_param = (int32_t)strtol(tmp_buffer, &end, 10);
		if (*end != '\0') handle_return(HTML_DATA_FORMATERROR);
		if (uuid_param == -1) {
			delete_all = true;
		} else {
			if (uuid_param < 1 || uuid_param > 0xFFFF) handle_return(HTML_DATA_OUTOFBOUND);
			idx = Sensor::find_index((uint16_t)uuid_param);
			if (idx >= os.nsensors) handle_return(HTML_DATA_OUTOFBOUND);
		}
	} else if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("sid"), true)) {
		int32_t sid_param = (int32_t)strtol(tmp_buffer, &end, 10);
		if (*end != '\0') handle_return(HTML_DATA_FORMATERROR);
		if (sid_param == -1) {
			delete_all = true;
		} else {
			if (sid_param < 0 || sid_param >= os.nsensors) handle_return(HTML_DATA_OUTOFBOUND);
			idx = sid_param;
		}
	} else {
		handle_return(HTML_DATA_MISSING);
	}

	if (delete_all) {
		// Delete all sensors
		uint8_t previous_count = os.nsensors;
		os.nsensors = 0;
		if (!Sensor::save_count()) {
			os.nsensors = previous_count;
			handle_return(HTML_INTERNAL_ERROR);
		}
		for (uint8_t i = 0; i < MAX_SENSORS; i++) {
			os.sensors[i].interval = 0;
			os.sensors[i].uuid = 0;
		}
	} else {
		if (!Sensor::del((uint8_t)idx)) handle_return(HTML_INTERNAL_ERROR);
	}
	weather_sensor_schedule_refresh();

	handle_return(HTML_SUCCESS);
}

uint8_t write_buf_log(uint32_t num, char *buf) {
	if (num) {
		uint8_t index = 0;
		while (num > 0) {
			buf[index++] = (num%10) + '0';
			num /= 10;
		}

		return index;
	} else {
		buf[0] = '0';
		return 1;
	}
}

/**
 * Get sensor logs
 * Command: /jsl?pw=xxx&[uuid=xxx|sid=xxx]&count=xxx&before=xxx&after=xxx&cursor=xxx&fmt=xxx&page=1
 *
 * pw:     password
 * uuid:   sensor stable ID (1-65535; -1 for all)
 * sid:    sensor positional index (0-based; -1 for all)
 *         (uuid takes precedence if both are provided)
 * count:  max records to return
 * before: timestamp before which records are returned
 * after:  timestamp after which records are returned
 * cursor: number of records to skip
 * fmt:    output format: json (default), csv, binary
 *         json:   [[uuid,ts,value],...] — JSON array of arrays
 *         csv:    uuid,timestamp,value\n with header row; downloads as sensor_log.csv
 *         binary: packed SensorLogRecord structs (uint32 ts, float val, uint16 uuid)
 * page:   set to 1 to make count and cursor address physical record slots;
 *         pagination state is returned in X-OS-* response headers
 */
static bool sensor_log_record_is_live(const SensorLogRecord &rec) {
	return rec.timestamp != 0 && rec.uuid != SENSOR_UUID_NONE;
}

static void find_sensor_log_window(const SensorLogHeader &hdr, uint16_t first_file,
	uint16_t total_files, uint32_t total_slots, time_os_t after, time_os_t before,
	uint32_t &window_start, uint32_t &window_end,
	uint16_t &first_window_file, uint32_t &first_window_file_start) {
	window_start = 0;
	window_end = total_slots;
	first_window_file = 0;
	first_window_file_start = 0;
	SensorLogRecord rec;

	if (after) {
		uint16_t candidate_file = 0;
		uint32_t candidate_start = 0;

		// Skip files whose final record proves that the complete file is too old.
		for (; candidate_file < total_files; candidate_file++) {
			uint16_t file_no = (first_file + candidate_file) % hdr.max_files;
			os_file_type dfile = open_sensor_log(file_no, FileOpenMode::Read);
			if (!dfile) continue;
			uint32_t record_count = file_size(dfile) / sizeof(SensorLogRecord);
			if (!record_count) {
				file_close(dfile);
				continue;
			}
			bool read_last = file_seek(dfile, (record_count - 1) * sizeof(SensorLogRecord)) &&
				file_read(dfile, &rec, sizeof(rec)) == (int)sizeof(rec);
			file_close(dfile);
			if (!read_last || rec.timestamp == 0 || rec.timestamp >= after) break;
			candidate_start += record_count;
		}

		// Refine the lower boundary. Tombstone timestamps remain valid boundaries.
		bool found = false;
		uint32_t flat_start = candidate_start;
		for (uint16_t fi = candidate_file; fi < total_files && !found; fi++) {
			uint16_t file_no = (first_file + fi) % hdr.max_files;
			os_file_type dfile = open_sensor_log(file_no, FileOpenMode::Read);
			if (!dfile) continue;
			uint32_t record_count = file_size(dfile) / sizeof(SensorLogRecord);
			for (uint32_t ri = 0; ri < record_count; ri++) {
				if (file_read(dfile, &rec, sizeof(rec)) != (int)sizeof(rec)) break;
				if (rec.timestamp != 0 && rec.timestamp >= after) {
					window_start = flat_start + ri;
					first_window_file = fi;
					first_window_file_start = flat_start;
					found = true;
					break;
				}
			}
			file_close(dfile);
			if (!found) flat_start += record_count;
		}
		if (!found) {
			window_start = total_slots;
			first_window_file = total_files;
			first_window_file_start = total_slots;
		}
	}

	if (before != std::numeric_limits<time_os_t>::max()) {
		uint32_t candidate_end = total_slots;
		uint16_t candidate_file = total_files;
		uint32_t candidate_start = total_slots;
		uint32_t candidate_records = 0;

		// Skip files whose first record proves that the complete file is too new.
		for (uint16_t fi = total_files; fi > 0; fi--) {
			uint16_t logical_file = fi - 1;
			uint16_t file_no = (first_file + logical_file) % hdr.max_files;
			os_file_type dfile = open_sensor_log(file_no, FileOpenMode::Read);
			if (!dfile) continue;
			uint32_t record_count = file_size(dfile) / sizeof(SensorLogRecord);
			uint32_t file_start = candidate_end - record_count;
			if (!record_count) {
				file_close(dfile);
				candidate_end = file_start;
				continue;
			}
			bool read_first = file_read(dfile, &rec, sizeof(rec)) == (int)sizeof(rec);
			file_close(dfile);
			if (read_first && rec.timestamp != 0 && rec.timestamp > before) {
				candidate_end = file_start;
				continue;
			}
			candidate_file = logical_file;
			candidate_start = file_start;
			candidate_records = record_count;
			break;
		}

		if (candidate_file == total_files) {
			window_end = 0;
		} else {
			// Refine the upper boundary. Tombstone timestamps remain valid boundaries.
			window_end = candidate_start;
			uint16_t file_no = (first_file + candidate_file) % hdr.max_files;
			os_file_type dfile = open_sensor_log(file_no, FileOpenMode::Read);
			if (dfile) {
				for (uint32_t ri = 0; ri < candidate_records; ri++) {
					if (file_read(dfile, &rec, sizeof(rec)) != (int)sizeof(rec)) break;
					if (rec.timestamp == 0) continue;
					if (rec.timestamp > before) break;
					window_end = candidate_start + ri + 1;
				}
				file_close(dfile);
			}
		}
	}

	if (window_start > window_end) {
		window_start = window_end;
		first_window_file = total_files;
		first_window_file_start = window_end;
	}
}

void server_json_sensor_log(OTF_PARAMS_DEF) {
	if(!process_password(OTF_PARAMS)) return;
	begin_response(res);

	char *end;

	// Read central header
	os_file_type hfile = open_sensor_log_header(FileOpenMode::Read);
	if (!hfile) handle_return(HTML_INTERNAL_ERROR);
	SensorLogHeader hdr;
	file_read(hfile, &hdr, sizeof(hdr));
	file_close(hfile);
	if (hdr.magic != SENSOR_LOG_MAGIC || hdr.version != SENSOR_LOG_VERSION)
		handle_return(HTML_INTERNAL_ERROR);

	uint32_t total_capacity = (uint32_t)hdr.max_files * hdr.records_per_file;
	uint16_t first_file  = hdr.wrapped ? (uint16_t)((hdr.cur_file + 1) % hdr.max_files) : 0;
	uint16_t total_files = hdr.wrapped ? hdr.max_files : (uint16_t)(hdr.cur_file + 1);

	bool page_mode = false;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("page"), true)) {
		if (strcmp(tmp_buffer, "1") == 0) page_mode = true;
		else if (strcmp(tmp_buffer, "0") != 0) handle_return(HTML_DATA_FORMATERROR);
	}

	// Page-mode cursors address complete physical records, including deleted ones.
	uint32_t total_slots = 0;
	if (page_mode) {
		for (uint16_t fi = 0; fi < total_files; fi++) {
			uint16_t file_no = (first_file + fi) % hdr.max_files;
			os_file_type dfile = open_sensor_log(file_no, FileOpenMode::Read);
			if (!dfile) continue;
			total_slots += file_size(dfile) / sizeof(SensorLogRecord);
			file_close(dfile);
		}
	}
	uint32_t cursor_limit = page_mode ? total_slots : total_capacity;

	uint32_t max_count = 100;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("count"), true)) {
		if (strcmp(tmp_buffer, "max") == 0 || strcmp(tmp_buffer, "all") == 0) {
			max_count = cursor_limit;
		} else {
			max_count = strtoul(tmp_buffer, &end, 10);
			if (*end != '\0') handle_return(HTML_DATA_FORMATERROR);
			if (max_count == 0) handle_return(HTML_DATA_OUTOFBOUND);
			if (max_count > cursor_limit) max_count = cursor_limit;
		}
	}

	// cursor = flat sequential index from oldest record to skip before emitting
	uint32_t cursor = 0;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("cursor"), true)) {
		cursor = strtoul(tmp_buffer, &end, 10);
		if (*end != '\0') handle_return(HTML_DATA_FORMATERROR);
		if (cursor > cursor_limit) handle_return(HTML_DATA_OUTOFBOUND);
	}
	using std::numeric_limits;
	time_os_t before = numeric_limits<time_os_t>::max();
	bool has_before = findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("before"), true);
	if (has_before) {
		before = (time_os_t)strtoul(tmp_buffer, &end, 10);
		if (*end != '\0') handle_return(HTML_DATA_FORMATERROR);
		if (before == 0) handle_return(HTML_DATA_OUTOFBOUND);
	}

	time_os_t after = 0;
	bool has_after = findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("after"), true);
	if (has_after) {
		after = (time_os_t)strtoul(tmp_buffer, &end, 10);
		if (*end != '\0') handle_return(HTML_DATA_FORMATERROR);
		if (after >= before) handle_return(HTML_DATA_OUTOFBOUND);
	}
	int32_t target_uuid = -1;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("uuid"), true)) {
		target_uuid = (int32_t)strtol(tmp_buffer, &end, 10);
		if (*end != '\0') handle_return(HTML_DATA_FORMATERROR);
		if (target_uuid != -1 && (target_uuid < 1 || target_uuid > 0xFFFF)) handle_return(HTML_DATA_OUTOFBOUND);
	} else if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("sid"), true)) {
		int32_t sid_param = (int32_t)strtol(tmp_buffer, &end, 10);
		if (*end != '\0') handle_return(HTML_DATA_FORMATERROR);
		if (sid_param == -1) {
			target_uuid = -1;
		} else {
			if (sid_param < 0 || sid_param >= os.nsensors) handle_return(HTML_DATA_OUTOFBOUND);
			target_uuid = os.sensors[sid_param].uuid;
		}
	}

	enum LogFmt { FMT_JSON, FMT_CSV, FMT_BINARY } logfmt = FMT_JSON;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("fmt"), true)) {
		if      (strcmp(tmp_buffer, "csv")    == 0) logfmt = FMT_CSV;
		else if (strcmp(tmp_buffer, "binary") == 0) logfmt = FMT_BINARY;
		else if (strcmp(tmp_buffer, "json")   != 0) handle_return(HTML_DATA_FORMATERROR);
	}

	// Files are chronological even after rotation. Preserve absolute physical
	// cursor positions while skipping regions outside the requested time window.
	uint16_t first_matching_file = 0;
	uint32_t flat_idx = 0;
	SensorLogRecord rec;
	uint32_t window_start = 0;
	uint32_t window_end = page_mode ? total_slots : total_capacity;
	uint32_t scan_cursor = cursor;
	uint32_t page_end = cursor;

	if (page_mode) {
		find_sensor_log_window(hdr, first_file, total_files, total_slots,
			after, before, window_start, window_end,
			first_matching_file, flat_idx);
		if (scan_cursor < window_start) scan_cursor = window_start;
		if (scan_cursor < window_end) {
			uint32_t remaining = window_end - scan_cursor;
			page_end = scan_cursor + (max_count < remaining ? max_count : remaining);
		} else {
			page_end = scan_cursor;
			first_matching_file = total_files;
			flat_idx = page_end;
		}
	} else if (after) {
		for (; first_matching_file < total_files; first_matching_file++) {
			uint16_t file_no = (first_file + first_matching_file) % hdr.max_files;
			os_file_type dfile = open_sensor_log(file_no, FileOpenMode::Read);
			if (!dfile) continue;

			uint32_t record_count = file_size(dfile) / sizeof(SensorLogRecord);
			if (!record_count) {
				file_close(dfile);
				continue;
			}

			bool read_last = file_seek(dfile, (record_count - 1) * sizeof(SensorLogRecord)) &&
				file_read(dfile, &rec, sizeof(rec)) == (int)sizeof(rec);
			file_close(dfile);

			// Fall back to scanning this file if its boundary cannot be trusted.
			if (!read_last || rec.timestamp == 0 || rec.timestamp >= after) break;
			flat_idx += record_count;
		}
	}

	ContentType ct = (logfmt == FMT_BINARY) ? CT_BINARY : (logfmt == FMT_CSV) ? CT_CSV : CT_JSON;
	print_header(OTF_PARAMS, ct);
	if (page_mode) {
		res.writeHeader(F("X-OS-Next-Cursor"), (int)page_end);
		res.writeHeader(F("X-OS-Total-Slots"), (int)total_slots);
		res.writeHeader(F("X-OS-Window-Start"), (int)window_start);
		res.writeHeader(F("X-OS-Window-End"), (int)window_end);
		res.writeHeader(F("X-OS-Page-Done"), page_end >= window_end ? 1 : 0);
		res.writeHeader(F("Access-Control-Expose-Headers"),
			F("X-OS-Next-Cursor, X-OS-Total-Slots, X-OS-Window-Start, X-OS-Window-End, X-OS-Page-Done"));
	}
	if (logfmt == FMT_CSV)
		res.writeHeader(F("Content-Disposition"), F("attachment; filename=\"sensor_log.csv\""));
	res.writeBodyData("", 0);

	if (logfmt == FMT_JSON) res.write("[", 1);
	if (logfmt == FMT_CSV)  res.write("uuid,timestamp,value\n", 21);

	uint32_t count = 0;

	for (uint16_t fi = first_matching_file;
		fi < total_files && (page_mode ? flat_idx < page_end : count < max_count); fi++) {
		uint16_t file_no = (first_file + fi) % hdr.max_files;
		os_file_type dfile = open_sensor_log(file_no, FileOpenMode::Read);
		if (!dfile) continue;

		uint32_t record_count = file_size(dfile) / sizeof(SensorLogRecord);
		if (flat_idx < scan_cursor) {
			uint32_t records_to_skip = scan_cursor - flat_idx;
			if (records_to_skip >= record_count) {
				flat_idx += record_count;
				file_close(dfile);
				continue;
			}
			if (file_seek(dfile, records_to_skip * sizeof(SensorLogRecord))) {
				flat_idx += records_to_skip;
			}
		}

		while (page_mode ? flat_idx < page_end : count < max_count) {
			if (file_read(dfile, &rec, sizeof(rec)) != (int)sizeof(rec)) break;

			flat_idx++;
			if (flat_idx <= scan_cursor) continue;
			if (!sensor_log_record_is_live(rec)) continue;
			if (target_uuid > -1 && rec.uuid != (uint16_t)target_uuid) continue;
			if (rec.timestamp > before || rec.timestamp < after) continue;

			char rec_buf[40];
			int rec_len;
			switch (logfmt) {
			case FMT_JSON:
				rec_len = snprintf(rec_buf, sizeof(rec_buf), "%s[%u,%u,%g]",
					count == 0 ? "" : ",", rec.uuid, rec.timestamp, rec.value);
				res.write(rec_buf, rec_len);
				break;
			case FMT_CSV:
				rec_len = snprintf(rec_buf, sizeof(rec_buf), "%u,%u,%g\n",
					rec.uuid, rec.timestamp, rec.value);
				res.write(rec_buf, rec_len);
				break;
			case FMT_BINARY:
				res.write((const char*)&rec, sizeof(rec));
				break;
			}
			count++;
		}
		file_close(dfile);
	}

	if (logfmt == FMT_JSON) res.write("]", 1);

	handle_return(HTML_OK);
}


void server_delete_sensor_log(OTF_PARAMS_DEF) {
	if(!process_password(OTF_PARAMS)) return;

	int32_t uuid = -1;
	char *end;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("uuid"), true)) {
		uuid = (int32_t)strtol(tmp_buffer, &end, 10);
		if (*end != '\0') handle_return(HTML_DATA_FORMATERROR);
		if (uuid != -1 && (uuid < 1 || uuid > 0xFFFF)) handle_return(HTML_DATA_OUTOFBOUND);
	} else {
		handle_return(HTML_DATA_MISSING);
	}

	bool page_mode = false;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("page"), true)) {
		if (strcmp(tmp_buffer, "1") == 0) page_mode = true;
		else if (strcmp(tmp_buffer, "0") != 0) handle_return(HTML_DATA_FORMATERROR);
	}

	if (uuid == -1) {
		if (page_mode) handle_return(HTML_DATA_FORMATERROR);
		// Remove all log files — frees flash immediately; header recreated on next log_sensor call
		remove_sensor_log();
		handle_return(HTML_SUCCESS);
	}

	// Per-sensor clear: read central header to know file layout
	os_file_type hfile = open_sensor_log_header(FileOpenMode::Read);
	if (!hfile) handle_return(HTML_INTERNAL_ERROR);
	SensorLogHeader hdr;
	file_read(hfile, &hdr, sizeof(hdr));
	file_close(hfile);
	if (hdr.magic != SENSOR_LOG_MAGIC || hdr.version != SENSOR_LOG_VERSION)
		handle_return(HTML_INTERNAL_ERROR);

	uint16_t first_file  = hdr.wrapped ? (uint16_t)((hdr.cur_file + 1) % hdr.max_files) : 0;
	uint16_t total_files = hdr.wrapped ? hdr.max_files : (uint16_t)(hdr.cur_file + 1);

	uint32_t cursor = 0;
	if (page_mode && findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("cursor"), true)) {
		cursor = strtoul(tmp_buffer, &end, 10);
		if (*end != '\0') handle_return(HTML_DATA_FORMATERROR);
	}

	uint32_t total_slots = 0;
	uint16_t first_scan_file = 0;
	uint32_t first_scan_flat = 0;
	bool scan_start_found = (cursor == 0);
	for (uint16_t fi = 0; fi < total_files; fi++) {
		uint16_t file_no = (first_file + fi) % hdr.max_files;
		os_file_type dfile = open_sensor_log(file_no, FileOpenMode::Read);
		if (!dfile) continue;
		uint32_t record_count = file_size(dfile) / sizeof(SensorLogRecord);
		file_close(dfile);
		if (!scan_start_found && cursor < total_slots + record_count) {
			first_scan_file = fi;
			first_scan_flat = total_slots;
			scan_start_found = true;
		}
		total_slots += record_count;
	}

	if (cursor > total_slots) handle_return(HTML_DATA_OUTOFBOUND);
	if (!scan_start_found) {
		first_scan_file = total_files;
		first_scan_flat = total_slots;
	}

	uint32_t max_count = page_mode ? hdr.records_per_file : total_slots;
	if (page_mode && findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("count"), true)) {
		max_count = strtoul(tmp_buffer, &end, 10);
		if (*end != '\0') handle_return(HTML_DATA_FORMATERROR);
		if (max_count == 0) handle_return(HTML_DATA_OUTOFBOUND);
		// Keep each request bounded even if a client supplies an excessive count.
		if (max_count > hdr.records_per_file) max_count = hdr.records_per_file;
	}

	uint32_t scan_end = total_slots;
	if (page_mode) {
		uint32_t remaining = total_slots - cursor;
		scan_end = cursor + (max_count < remaining ? max_count : remaining);
	}

	uint32_t flat_idx = page_mode ? first_scan_flat : 0;
	uint32_t deleted = 0;
	const uint32_t batch_capacity = TMP_BUFFER_SIZE / sizeof(SensorLogRecord);
	SensorLogRecord rec;

	uint16_t scan_file = page_mode ? first_scan_file : 0;
	for (uint16_t fi = scan_file; fi < total_files && flat_idx < scan_end; fi++) {
		uint16_t file_no = (first_file + fi) % hdr.max_files;
		os_file_type dfile = open_sensor_log(file_no, FileOpenMode::ReadWrite);
		if (!dfile) continue;

		uint32_t record_count = file_size(dfile) / sizeof(SensorLogRecord);
		uint32_t file_record_idx = 0;
		bool io_error = false;

		if (flat_idx < cursor) {
			uint32_t records_to_skip = cursor - flat_idx;
			if (records_to_skip >= record_count) {
				flat_idx += record_count;
				file_close(dfile);
				continue;
			}
			if (!file_seek(dfile, records_to_skip * sizeof(SensorLogRecord))) {
				file_close(dfile);
				handle_return(HTML_INTERNAL_ERROR);
			}
			flat_idx += records_to_skip;
			file_record_idx = records_to_skip;
		}

		while (file_record_idx < record_count && flat_idx < scan_end) {
			uint32_t records_left = record_count - file_record_idx;
			uint32_t page_left = scan_end - flat_idx;
			uint32_t batch_records = records_left < page_left ? records_left : page_left;
			if (batch_records > batch_capacity) batch_records = batch_capacity;
			uint32_t batch_bytes = batch_records * sizeof(SensorLogRecord);
			uint32_t block_pos = file_record_idx * sizeof(SensorLogRecord);

			if (file_read(dfile, tmp_buffer, batch_bytes) != (int)batch_bytes) {
				io_error = true;
				break;
			}

			bool changed = false;
			for (uint32_t i = 0; i < batch_records; i++) {
				uint32_t offset = i * sizeof(SensorLogRecord);
				memcpy(&rec, tmp_buffer + offset, sizeof(rec));
				if (sensor_log_record_is_live(rec) && rec.uuid == (uint16_t)uuid) {
					rec.value = 0;
					rec.uuid = SENSOR_UUID_NONE;
					memcpy(tmp_buffer + offset, &rec, sizeof(rec));
					changed = true;
					deleted++;
				}
			}

			if (changed) {
				if (!file_seek(dfile, block_pos) ||
					file_write(dfile, tmp_buffer, batch_bytes) != (int)batch_bytes ||
					!file_seek(dfile, block_pos + batch_bytes)) {
					io_error = true;
					break;
				}
			}

			file_record_idx += batch_records;
			flat_idx += batch_records;
			#if defined(ARDUINO)
				yield();
			#endif
		}
		file_close(dfile);
		if (io_error) handle_return(HTML_INTERNAL_ERROR);
	}

	if (!page_mode) handle_return(HTML_SUCCESS);

	begin_response(res);
	print_header(OTF_PARAMS);
	bfill.emit_p(PSTR("{\"result\":1,\"next\":$L,\"total\":$L,\"deleted\":$L,\"done\":$D}"),
		scan_end, total_slots, deleted, scan_end >= total_slots ? 1 : 0);
	handle_return(HTML_OK);

}

template <typename T>
void bfill_enum_values(const char *name) {
	static_assert(std::is_enum<T>::value, "T must be an enum type");

	bool needs_comma = false;

	// $F: name and the enum strings below come from PSTR() helpers, i.e. they
	// live in flash. $S copies via memcpy(), which byte-reads the source and
	// faults (Exception 3) on ESP8266 flash addresses.
	bfill.emit_p(PSTR("\"$F\":["), name);

	for (size_t i = 0; i < static_cast<size_t>(T::MAX_VALUE); ++i) {
		if (needs_comma) {
			bfill.emit_p(PSTR(","));
			needs_comma = false;
		}

		const char* str = enum_string(static_cast<T>(i));
		if (str) {
			bfill.emit_p(PSTR("\"$F\""), str);
			needs_comma = true;
		}
	}

	bfill.emit_p(PSTR("]"));
}

void server_json_sensor_description_main(OTF_PARAMS_DEF) {
	bfill.emit_p(PSTR("\"sensors\":["));
	// IMPORTANT: the array index implies the SensorType enum value. Do NOT
	// skip entries — UI matches type by position. For sensor types that are
	// intentionally disabled this release, emit a minimal stub with a
	// "dis":1 flag so positions stay stable. UI hides disabled types from
	// the creation menu but keeps the indexing intact for when the type is
	// re-enabled in a future release.
	for (uint8_t i = 0; i < static_cast<uint8_t>(SensorType::MAX_VALUE); i++) {
		if (i) bfill.emit_p(PSTR(","));
		switch (static_cast<SensorType>(i)) {
			case SensorType::Aggregate:
				AggregateSensor::emit_description_json(&bfill);
				break;
			case SensorType::ADS1115:
				ADS1115Sensor::emit_description_json(&bfill);
				break;
			case SensorType::Weather:
				WeatherSensor::emit_description_json(&bfill);
				break;
			case SensorType::SystemInternal:
				SystemInternalSensor::emit_description_json(&bfill);
				break;
			case SensorType::OnboardDigital:
				OnboardDigitalSensor::emit_description_json(&bfill);
				break;
			case SensorType::MAX_VALUE:
				break;
		}
	}

	// units: compact array form [id, name, short, group]. Index/value were duplicates of id; dropped.
	bfill.emit_p(PSTR("],\"units\":["));
	for (uint8_t i = 0; i < static_cast<uint8_t>(SensorUnit::MAX_VALUE); i++) {
		if (i) bfill.emit_p(PSTR(","));
		SensorUnit unit = static_cast<SensorUnit>(i);
		bfill.emit_p(PSTR("[$D,\"$F\",\"$F\",$D]"),
			i, get_sensor_unit_name(unit), get_sensor_unit_short(unit),
			static_cast<uint8_t>(get_sensor_unit_group(unit)));
	}

	bfill.emit_p(PSTR("],\"enums\":{"));
	bfill_enum_values<SensorUnitGroup>(PSTR("SensorUnitGroup"));
	bfill.emit_p(PSTR(","));
	bfill_enum_values<AggregateAction>(PSTR("AggregateAction"));
	bfill.emit_p(PSTR(","));
	bfill_enum_values<WeatherAction>(PSTR("WeatherAction"));
	bfill.emit_p(PSTR("}"));

	bfill.emit_p(PSTR(
		",\"as\":["
		"{\"n\":\"Name\",\"a\":\"name\",\"t\":\"string::[1,32]\",\"d\":\"" SENSOR_DEFAULT_NAME "\"},"
		"{\"n\":\"Interval\",\"a\":\"interval\",\"t\":\"int::[1,any]\",\"d\":\"" SENSOR_DEFAULT_STR(SENSOR_DEFAULT_INTERVAL) "\",\"h\":\"Sensor's update interval (in minutes)\"},"
	));
	bfill.emit_p(PSTR("{\"n\":\"Unit\",\"a\":\"unit\",\"t\":\"unit\",\"d\":\"$D\"},"), static_cast<uint8_t>(SENSOR_DEFAULT_UNIT));
	bfill.emit_p(PSTR(
		"{\"n\":\"Min. Value\",\"a\":\"min\",\"t\":\"float\",\"d\":\"" SENSOR_DEFAULT_STR(SENSOR_DEFAULT_MIN) "\"},"
		"{\"n\":\"Max. Value\",\"a\":\"max\",\"t\":\"float\",\"d\":\"" SENSOR_DEFAULT_STR(SENSOR_DEFAULT_MAX) "\"},"
		"{\"n\":\"Type\",\"a\":\"type\",\"t\":\"type\",\"d\":\"" SENSOR_DEFAULT_STR(SENSOR_DEFAULT_TYPE) "\"}"
		"]"
	));

	static_assert(SENSOR_FLAG_COUNT == 3); // If this fails, update the flags array below
	bfill.emit_p(PSTR(",\"flags\":[{\"n\":\"Enabled\",\"d\":$D},{\"n\":\"Logging\",\"d\":$D},{\"n\":\"Show on Home\",\"d\":$D}]"),
		(SENSOR_DEFAULT_FLAG >> SENSOR_FLAG_ENABLE) & 1,
		(SENSOR_DEFAULT_FLAG >> SENSOR_FLAG_LOG) & 1,
		(SENSOR_DEFAULT_FLAG >> SENSOR_FLAG_SHOW) & 1);

	bfill.emit_p(PSTR("}"));
}

void server_json_sensor_desc(OTF_PARAMS_DEF)
{
	if(!process_password(OTF_PARAMS)) return;
	begin_response(res);
	print_header(OTF_PARAMS);

	bfill.emit_p(PSTR("{"));
	server_json_sensor_description_main(OTF_PARAMS);
	handle_return(HTML_OK);
}

/** Output all JSON data, including jc, jp, jo, js, jn */
void server_json_all(OTF_PARAMS_DEF) {
	if(!process_password(OTF_PARAMS,true)) return;
	begin_response(res);
	print_header(OTF_PARAMS);

	bfill.emit_p(PSTR("{\"settings\":{"));
	server_json_controller_main(OTF_PARAMS);
	bfill.emit_p(PSTR(",\"programs\":{"));
	server_json_programs_main(OTF_PARAMS);
	bfill.emit_p(PSTR(",\"options\":{"));
	server_json_options_main();
	bfill.emit_p(PSTR(",\"status\":{"));
	server_json_status_main();
	bfill.emit_p(PSTR(",\"stations\":{"));
	server_json_stations_main(OTF_PARAMS);
	bfill.emit_p(PSTR(",\"sensors\":{"));
	server_json_sensors_main(OTF_PARAMS);
	//bfill.emit_p(PSTR(",\"sensor_desc\":{"));
	//server_json_sensor_description_main(OTF_PARAMS);
	bfill.emit_p(PSTR("}"));
	handle_return(HTML_OK);
}

#if defined(ARDUINO)

#elif !defined(ARDUINO)
#include <sys/sysinfo.h>
static uint32_t freeHeap() {
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
	begin_response(res);
	print_header(OTF_PARAMS);
	#if defined(ARDUINO)
	EmbeddedStorageUsage storage = get_embedded_storage_usage();
	#endif

	bfill.emit_p(PSTR("{\"date\":\"$S\",\"time\":\"$S\",\"heap\":$L"), __DATE__, __TIME__,
	#if defined(ESP8266)
	ESP.getFreeHeap());
	bfill.emit_p(PSTR(",\"maxblock\":$L,\"frag\":$D"),
		(uint32_t)ESP.getMaxFreeBlockSize(),
		(uint8_t)ESP.getHeapFragmentation());
	bfill.emit_p(PSTR(",\"flash\":$L,\"used\":$L,\"free\":$L,\"pruned\":$L,\"devip\":\"$S\","),
		storage.total_bytes, storage.used_bytes, storage.free_bytes, embedded_storage_pruned_files(),
		(useEth?eth.localIP():WiFi.localIP()).toString().c_str());
	if(useEth) {
		bfill.emit_p(PSTR("\"isW5500\":$D,\"spi_clock\":$L,\"arp_size\":$D}"), eth.isW5500, ETHER_SPI_CLOCK, ARP_TABLE_SIZE);
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
	#elif defined(ESP32)
	ESP.getFreeHeap());
	bfill.emit_p(PSTR(",\"maxblock\":$L"), (uint32_t)ESP.getMaxAllocHeap());
	bfill.emit_p(PSTR(",\"flash\":$L,\"used\":$L,\"free\":$L,\"pruned\":$L,\"devip\":\"$S\","),
		storage.total_bytes, storage.used_bytes, storage.free_bytes, embedded_storage_pruned_files(),
		(useEth ? ETH.localIP() : WiFi.localIP()).toString().c_str());
	if (useEth) {
		bfill.emit_p(PSTR("\"isW5500\":1,\"spi_clock\":$L}"), (uint32_t)ETHER_SPI_CLOCK);
	} else {
		bfill.emit_p(PSTR("\"rssi\":$D,\"bssid\":\"$S\",\"bssidchl\":\"$O\"}"),
			WiFi.RSSI(), WiFi.BSSIDstr().c_str(), SOPT_STA_BSSID_CHL);
	}
	#else
	(uint32_t)freeHeap());
	bfill.emit_p(PSTR("}"));
#endif
	handle_return(HTML_OK);
}

/**
 * List all files
 * Command: /lf?pw=xxx
 *
 * pw:   password
 * Returns a JSON array of [filename, size]
 */
	#if defined(ESP8266)
void server_list_files(OTF_PARAMS_DEF) {
	if(!process_password(OTF_PARAMS)) return;
	begin_response(res);
	print_header(OTF_PARAMS);

	bfill.emit_p(PSTR("{\"files\":["));
	bool first = true;

	// root
	Dir dir = LittleFS.openDir("/");
	while (dir.next()) {
		if (dir.fileName().indexOf('.') < 0) continue;
		if (!first) bfill.emit_p(PSTR(","));
		bfill.emit_p(PSTR("[\"/$S\",$D]"), dir.fileName().c_str(), dir.fileSize());
		first = false;
	}
	// logs
	dir = LittleFS.openDir("/logs/");
	while (dir.next()) {
		if (!first) bfill.emit_p(PSTR(","));
		bfill.emit_p(PSTR("[\"/logs/$S\",$D]"), dir.fileName().c_str(), dir.fileSize());
		first = false;
	}

	bfill.emit_p(PSTR("]}"));
	handle_return(HTML_OK);
}
	#elif defined(ESP32)
static void emit_file_directory(BufferFiller& output, const char* path,
	const char* prefix, bool& first) {
	File directory = LittleFS.open(path);
	if (!directory || !directory.isDirectory()) return;
	for (File file = directory.openNextFile(); file; file = directory.openNextFile()) {
		String name = file.name();
		if (name.indexOf('.') >= 0) {
			if (!first) output.emit_p(PSTR(","));
			output.emit_p(PSTR("[\"$S$S\",$L]"), prefix, name.c_str(),
				(uint32_t)file.size());
			first = false;
		}
		file.close();
	}
}

void server_list_files(OTF_PARAMS_DEF) {
	if(!process_password(OTF_PARAMS)) return;
	begin_response(res);
	print_header(OTF_PARAMS);
	bfill.emit_p(PSTR("{\"files\":["));
	bool first = true;
	emit_file_directory(bfill, "/", "/", first);
	emit_file_directory(bfill, LOG_DIR, LOG_DIR, first);
	bfill.emit_p(PSTR("]}"));
	handle_return(HTML_OK);
}
	#endif

/**
 * Delete a file
 * Command: /df?pw=xxx&fn=filename
 *
 * pw:   password
 * fn:   filename to delete
 */
#if defined(ARDUINO) && defined(ENABLE_DEBUG)
void server_delete_file(OTF_PARAMS_DEF) {
	if(!process_password(OTF_PARAMS)) return;

	if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("fn"), true))
		handle_return(HTML_DATA_MISSING);

	remove_file(tmp_buffer);
	handle_return(HTML_SUCCESS);
}
#endif

/*
// fill ESP8266 flash with some dummy files
void server_fill_files(OTF_PARAMS_DEF) {
	memset(ether_buffer, 65, 75);
	ether_buffer[75] = 0;
	FSInfo fs_info;
	for(int index=1;index<64;index++) {
		snprintf(tmp_buffer, TMP_BUFFER_ALLOC_SIZE , "%d", index);
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

// handle Ethernet request
#if defined(ARDUINO)
void on_firmware_update(OTF_PARAMS_DEF) {
	if(req.isCloudRequest()) {
		otf_send_result(OTF_PARAMS, HTML_NOT_PERMITTED, "fw update");
		return;
	}
	if (!update_server) {
		otf_send_result(OTF_PARAMS, HTML_NOT_PERMITTED,
			"HTTP port 8080 is reserved for firmware update");
		return;
	}
	print_header_compressed_html(OTF_PARAMS, update_html_gz_len);
	//res.writeBodyChunk((char *) "%s", ap_update_html);
	res.writeBodyData((const __FlashStringHelper*)update_html_gz, update_html_gz_len);
}

static bool firmware_upload_authorized = false;
static bool firmware_upload_failed = false;
static bool firmware_upload_auth_failed = false;
static bool firmware_upload_legacy = false;

void on_update_page() {
	update_server->sendHeader("Content-Encoding", "gzip");
	update_server->sendHeader("Cache-Control", "no-store");
	update_server->send_P(200, PSTR("text/html"),
		reinterpret_cast<PGM_P>(update_html_gz), update_html_gz_len);
}

void on_update_info() {
	String json = F("{\"configured\":");
	json += firmware_update.configured() ? F("true") : F("false");
	json += F(",\"target\":\"");
	json += firmware_update.target();
	json += F("\",\"extension\":\"");
	json += firmware_update.file_extension();
	json += F("\",\"current_version\":");
	json += OS_FW_VERSION;
	json += F(",\"current_build\":");
	json += OS_FW_MINOR;
	json += F(",\"ap_mode\":");
	json += os.get_wifi_mode() == OS_WIFI_MODE_AP ? F("true") : F("false");
	json += F(",\"base_url\":\"");
	json += firmware_update.base_url();
	json += F("\"}");
	update_server->sendHeader("Access-Control-Allow-Origin", "*");
	update_server->sendHeader("Cache-Control", "no-store");
	update_server->send(200, "application/json", json);
}

void on_update_prepare() {
	if (firmware_update.busy()) {
		update_server_send_result(HTML_NOT_PERMITTED, "update busy");
		return;
	}
	if (!firmware_update.authenticate(update_server->arg("pw").c_str())) {
		update_server_send_result(HTML_UNAUTHORIZED);
		return;
	}
	String release_id = update_server->arg("id");
	String signature = update_server->arg("sig");
	String descriptor = update_server->arg("plain");
	bool allow_downgrade = update_server->arg("allow_downgrade") == "1";
	if (!release_id.length() || !signature.length() || !descriptor.length()) {
		update_server_send_result(HTML_DATA_MISSING, "signed release");
		return;
	}
	char upload_token[17];
	if (!firmware_update.prepare_verified(reinterpret_cast<const uint8_t *>(descriptor.c_str()),
		descriptor.length(), signature.c_str(), release_id.c_str(), allow_downgrade, upload_token)) {
		update_server_send_result(HTML_UPLOAD_FAILED, firmware_update.message());
		return;
	}
	String json = F("{\"result\":1,\"upload_token\":\"");
	json += upload_token;
	json += F("\",\"expires_in\":300}");
	update_server->sendHeader("Access-Control-Allow-Origin", "*");
	update_server->sendHeader("Cache-Control", "no-store");
	update_server->send(200, "application/json", json);
}

void on_update_status() {
	String json = F("{\"phase\":\"");
	json += firmware_update.phase_name();
	json += F("\",\"progress\":");
	json += firmware_update.percent();
	json += F(",\"done\":");
	json += firmware_update.bytes_done();
	json += F(",\"total\":");
	json += firmware_update.bytes_total();
	json += F(",\"message\":\"");
	json += firmware_update.message();
	json += F("\"}");
	update_server->sendHeader("Access-Control-Allow-Origin", "*");
	update_server->sendHeader("Cache-Control", "no-store");
	update_server->send(200, "application/json", json);
}

static void finish_firmware_upload(bool verified) {
	if (!verified && firmware_upload_legacy &&
		!firmware_update.authenticate(update_server->arg("pw").c_str())) {
		if (firmware_upload_authorized) firmware_update.abort_upload();
		firmware_upload_authorized = false;
		firmware_upload_failed = false;
		firmware_upload_auth_failed = false;
		firmware_upload_legacy = false;
		update_server_send_result(HTML_UNAUTHORIZED);
		return;
	}
	if (!firmware_upload_authorized) {
		bool unauthorized = firmware_upload_auth_failed;
		firmware_upload_failed = false;
		firmware_upload_auth_failed = false;
		firmware_upload_legacy = false;
		update_server_send_result(unauthorized ? HTML_UNAUTHORIZED : HTML_UPLOAD_FAILED,
			unauthorized ? nullptr : firmware_update.message());
		return;
	}
	firmware_upload_authorized = false;
	bool finished = !firmware_upload_failed &&
		(verified ? firmware_update.finish_verified() : firmware_update.finish_manual());
	if (!finished) {
		firmware_upload_failed = false;
		firmware_upload_auth_failed = false;
		firmware_upload_legacy = false;
		update_server_send_result(HTML_UPLOAD_FAILED, firmware_update.message());
		return;
	}
	firmware_upload_failed = false;
	firmware_upload_auth_failed = false;
	firmware_upload_legacy = false;
	update_server_send_result(HTML_SUCCESS);
}

void on_firmware_upload_fin() {
	finish_firmware_upload(false);
}

void on_verified_upload_fin() {
	finish_firmware_upload(true);
}

void on_update_options() {
	update_server->sendHeader("Access-Control-Allow-Origin", "*");
	update_server->sendHeader("Access-Control-Max-Age", "10000");
	update_server->sendHeader("Access-Control-Allow-Methods", "POST,GET,OPTIONS");
	update_server->sendHeader("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");
	update_server->send(200, "text/plain", "");
}

static void receive_firmware_upload(bool verified) {
	HTTPUpload& upload = update_server->upload();
	if(upload.status == UPLOAD_FILE_START){
		firmware_upload_authorized = false;
		firmware_upload_failed = false;
		firmware_upload_auth_failed = false;
		firmware_upload_legacy = false;
		if (verified) {
			firmware_upload_authorized = firmware_update.begin_verified(upload.filename.c_str(),
				update_server->arg("token").c_str());
			firmware_upload_auth_failed = !firmware_upload_authorized;
		} else {
			bool modern = update_server->hasArg("size") || update_server->hasArg("sha256");
			if (modern) {
				if (!firmware_update.authenticate(update_server->arg("pw").c_str())) {
					firmware_upload_auth_failed = true;
				} else {
					uint32_t upload_size = (uint32_t)update_server->arg("size").toInt();
					firmware_upload_authorized = firmware_update.begin_manual(
						upload.filename.c_str(), upload_size,
						update_server->arg("sha256").c_str());
				}
			} else {
				// Legacy pages send the file before the multipart password field, so
				// authentication must be deferred until the upload-finished callback.
				firmware_upload_legacy = true;
				firmware_upload_authorized =
					firmware_update.begin_legacy_manual(upload.filename.c_str());
			}
		}
		firmware_upload_failed = !firmware_upload_authorized && !firmware_upload_auth_failed;
		DEBUG_PRINT(F("upload: "));
		DEBUG_PRINTLN(upload.filename);
	} else if(upload.status == UPLOAD_FILE_WRITE) {
		if (firmware_upload_authorized && !firmware_upload_failed) {
			bool written = verified ? firmware_update.write_verified(upload.buf, upload.currentSize) :
				firmware_update.write_manual(upload.buf, upload.currentSize);
			if (!written) firmware_upload_failed = true;
		}
	} else if(upload.status == UPLOAD_FILE_END) {
		DEBUG_PRINTLN(F("completed"));
	} else if(upload.status == UPLOAD_FILE_ABORTED){
		if (firmware_upload_authorized) firmware_update.abort_upload();
		firmware_upload_failed = true;
		DEBUG_PRINTLN(F("aborted"));
	}
	delay(0);
}

void on_firmware_upload() {
	receive_firmware_upload(false);
}

void on_verified_upload() {
	receive_firmware_upload(true);
}

static void register_update_routes() {
	static decltype(update_server) registered_server = nullptr;
	if (registered_server == update_server) return;
	registered_server = update_server;
	update_server->on("/update", HTTP_GET, on_update_page);
	update_server->on("/update", HTTP_POST, on_firmware_upload_fin, on_firmware_upload);
	update_server->on("/update", HTTP_OPTIONS, on_update_options);
	update_server->on("/update/info", HTTP_GET, on_update_info);
	update_server->on("/update/info", HTTP_OPTIONS, on_update_options);
	update_server->on("/update/prepare", HTTP_POST, on_update_prepare);
	update_server->on("/update/prepare", HTTP_OPTIONS, on_update_options);
	update_server->on("/update/upload", HTTP_POST, on_verified_upload_fin, on_verified_upload);
	update_server->on("/update/upload", HTTP_OPTIONS, on_update_options);
	update_server->on("/update/status", HTTP_GET, on_update_status);
	update_server->on("/update/status", HTTP_OPTIONS, on_update_options);
}

void start_server_client() {
	if(!otf) return;
	static bool callback_initialized = false;

	if(!callback_initialized) {
		otf->on("/", server_home);  // handle home page
		otf->on("/index.html", server_home);
		otf->on("/update", on_firmware_update, OTF::HTTP_GET); // handle firmware update
		register_api_routes(*otf);
		callback_initialized = true;
	}
	if (update_server) {
		register_update_routes();
		update_server->begin();
	}
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
	otf->on("/update", on_firmware_update, OTF::HTTP_GET);
	if (update_server) register_update_routes();
	otf->onMissingPage(on_ap_home);
	if (update_server) update_server->begin();

	register_api_routes(*otf);

	os.lcd.setCursor(0, -1);
	os.lcd.print(F("OSAP:"));
	os.lcd.print(ap_ssid);
	os.lcd.setCursor(0, 2);
	os.lcd.print(WiFi.softAPIP());
}

#endif

#if !defined(ARDUINO)
void initialize_otf() {
	if(!otf) return;
	static bool callback_initialized = false;

	if(!callback_initialized) {
		otf->on("/", server_home);  // handle home page
		otf->on("/index.html", server_home);

		register_api_routes(*otf);
		callback_initialized = true;
	}
}
#endif

#if defined(ARDUINO)
#define NTP_NTRIES 10
/** NTP sync request */
// due to lwip not supporting UDP, we have to use configTime and time() functions
// othewise, using UDP is much faster for NTP sync
uint32_t getNtpTime() {
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
	uint32_t gt = 0;
	while(tries<NTP_NTRIES) {
		gt = time(NULL);
		if(gt>1577836800UL)	break;
		else gt = 0;
		delay(1000);
		tries++;
	}
	return gt;
}
#endif
