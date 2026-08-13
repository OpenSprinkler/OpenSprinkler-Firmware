/* OpenSprinkler Unified Firmware
 * Copyright (C) 2015 by Ray Wang (ray@opensprinkler.com)
 *
 * Notifier data structures and functions
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

#include "notifier.h"
#include "../core/program.h"
#include "external/ArduinoJson.hpp"
#include "../api/server.h"
#if defined(ESP32)
#include "EMailSender.h"
#endif

uint8_t NotifQueue::nqueue = 0;
uint8_t NotifQueue::head = 0;
uint8_t NotifQueue::tail = 0;
NotifNodeStruct NotifQueue::queue[NOTIF_QUEUE_MAXSIZE];

extern OpenSprinkler os;
extern ProgramData pd;
extern char tmp_buffer[];
extern char ether_buffer[];
extern float flow_last_gpm;

extern const char *user_agent_string;

void default_http_callback(char*);
void ip2string(char* str, size_t str_len, unsigned char ip[4]);

#define DEFAULT_EMAIL_PORT 465

// =====================================================================
// Email config: pointer-based to avoid copying credentials onto the stack.
//
// EmailConfig holds raw pointers into a JsonDocument's internal storage.
// EmailConfigContext bundles cfg + doc; the cfg pointers are valid only
// while the owning context is alive. We hold one as file-static (BSS) and
// reuse it across notifications, so push_message pays no per-call stack
// cost for credentials.
// =====================================================================
struct EmailConfig {
	const char *host;
	int         port;
	const char *user;
	const char *pass;
	const char *recipient;
	bool        enabled;
};

struct EmailConfigContext {
	EmailConfig cfg;
	ArduinoJson::JsonDocument doc;

	EmailConfigContext() = default;
	// Non-copyable: cfg's pointers reference doc; aliasing them would invite UAF.
	EmailConfigContext(const EmailConfigContext&) = delete;
	EmailConfigContext& operator=(const EmailConfigContext&) = delete;
};

// File-static instance; load_email_config() refreshes it on each call.
// Lives in BSS, not on the stack.
static EmailConfigContext s_email_ctx;

// Loads SOPT_EMAIL_OPTS into ctx.doc and points ctx.cfg at the parsed strings.
// On parse failure or absent config, ctx.cfg.enabled is set false; callers should
// check cfg.enabled before using the other fields.
static void load_email_config(EmailConfigContext &ctx) {
	// Reset to a known-empty state so partial parses don't carry stale pointers.
	ctx.doc.clear();
	ctx.cfg.host      = nullptr;
	ctx.cfg.user      = nullptr;
	ctx.cfg.pass      = nullptr;
	ctx.cfg.recipient = nullptr;
	ctx.cfg.port      = DEFAULT_EMAIL_PORT;
	ctx.cfg.enabled   = false;

	// Load + wrap the stored JSON fragment with braces. tmp_buffer is reused
	// for parsing only; once deserializeJson returns, doc owns its own copy
	// of the strings (ArduinoJson 7 copies when source is non-const char*).
	char *postval = tmp_buffer + 1;   // +1 to leave room for the opening '{'
	os.sopt_load(SOPT_EMAIL_OPTS, postval);
	if (*postval == 0) return;        // no config saved → disabled

	postval = tmp_buffer;
	postval[0] = '{';
	int len = strlen(postval);
	postval[len] = '}';
	postval[len + 1] = 0;

	ArduinoJson::DeserializationError error = ArduinoJson::deserializeJson(ctx.doc, postval);
	if (error) {
		DEBUG_PRINT(F("email: deserializeJson() failed: "));
		DEBUG_PRINTLN(error.c_str());
		return;
	}

	int en = ctx.doc["en"];
	ctx.cfg.host      = ctx.doc["host"];
	ctx.cfg.port      = ctx.doc["port"] | DEFAULT_EMAIL_PORT;
	ctx.cfg.user      = ctx.doc["user"];
	ctx.cfg.pass      = ctx.doc["pass"];
	ctx.cfg.recipient = ctx.doc["recipient"];
	ctx.cfg.enabled   = (en != 0);
}

// =====================================================================
// Per-integration senders. Each takes already-formatted text; no parsing,
// no allocation beyond what the underlying network/email stack does.
// =====================================================================

// Sends an IFTTT POST whose JSON body wraps `body` as the "value1" field.
// `body` is plain text (e.g., "On site [Garden], Station [Front] just turned on.")
// — we add the {"value1":"..."} wrapper here. The IFTTT key is loaded on demand
// via the BufferFiller $O directive; never copied into a local buffer.
static void send_ifttt(const char *body) {
	BufferFiller bf = BufferFiller(ether_buffer, TMP_BUFFER_SIZE);
	bf.emit_p(PSTR("POST /trigger/sprinkler/with/key/$O HTTP/1.0\r\n"
	                "Host: $S\r\n"
	                "User-Agent: $S\r\n"
	                "Accept: */*\r\n"
	                "Content-Length: $D\r\n"
	                "Content-Type: application/json\r\n\r\n"
	                "{\"value1\":\"$S\"}"),
	          SOPT_IFTTT_KEY, DEFAULT_IFTTT_URL, user_agent_string,
	          (int)(strlen(body) + 13),         // +13 for {"value1":""}
	          body);
	if (bf.overflowed()) {
		DEBUG_PRINTLN(F("IFTTT request too large"));
		return;
	}

	os.send_http_request(DEFAULT_IFTTT_URL, 80, ether_buffer, default_http_callback);
}

// Convenience: copy a PROGMEM string into a fixed-size dst, NUL-terminating.
static inline void set_pstr(char *dst, size_t cap, PGM_P src) {
	strncpy_P(dst, src, cap - 1);
	dst[cap - 1] = '\0';
}

// =====================================================================
// NotifBuffers + format_notification: the event-to-message switch.
//
// format_notification is a pure formatter — given an event type and its
// associated values, it writes per-integration text into the caller-owned
// buffers. No I/O, no allocation, no implicit state. The orchestrator
// (push_message) decides which integrations to actually dispatch based on
// configuration; format_notification always populates whatever it has
// content for, leaving the rest empty.
//
// Buffer semantics:
//   topic / payload  — MQTT topic and payload (empty if event has no MQTT msg)
//   body             — event-specific text only (NO "On site [..], " prefix —
//                      that's prepended by the caller in tmp_buffer)
//   subject_suffix   — event-specific suffix only (NO device-name prefix —
//                      that's prepended by the caller in the subject buffer)
// =====================================================================
struct NotifBuffers {
	char  *topic;             size_t topic_cap;
	char  *payload;           size_t payload_cap;
	char  *body;              size_t body_cap;
	char  *subject_suffix;    size_t subject_suffix_cap;
};

static void format_notification(uint16_t type, uint32_t lval, float fval,
                                 uint8_t bval, float fval2,
                                 NotifBuffers &bufs) {
	uint32_t flowrate100 = (((uint32_t)os.iopts[IOPT_PULSE_RATE_1])<<8) + os.iopts[IOPT_PULSE_RATE_0];

	switch (type) {
		case NOTIFY_STATION_ON: {
			snprintf_P(bufs.topic, bufs.topic_cap, PSTR("station/%d"), lval);
			strncpy_P(bufs.payload, PSTR("{\"state\":1"), bufs.payload_cap - 1);
			if ((int)fval > 0) {
				snprintf_P(bufs.payload + strlen(bufs.payload), bufs.payload_cap - strlen(bufs.payload),
				           PSTR(",\"duration\":%d"), (int)fval);
			}
			strncat_P(bufs.payload, PSTR("}"), bufs.payload_cap - strlen(bufs.payload) - 1);

			strcat_P(bufs.body, PSTR("Station ["));
			os.get_station_name(lval, bufs.body + strlen(bufs.body));
			strcat_P(bufs.body, PSTR("] just turned on."));
			if ((int)fval > 0) {
				strcat_P(bufs.body, PSTR(" It's scheduled to run for "));
				snprintf_P(bufs.body + strlen(bufs.body), bufs.body_cap - strlen(bufs.body),
				           PSTR(" %d minutes %d seconds."), (int)fval/60, (int)fval%60);
			}
			set_pstr(bufs.subject_suffix, bufs.subject_suffix_cap, PSTR("station event"));
			break;
		}

		case NOTIFY_STATION_OFF: {
			snprintf_P(bufs.topic, bufs.topic_cap, PSTR("station/%d"), lval);
			strncpy_P(bufs.payload, PSTR("{\"state\":0"), bufs.payload_cap - 1);
			if ((int)fval > 0) {
				snprintf_P(bufs.payload + strlen(bufs.payload), bufs.payload_cap - strlen(bufs.payload),
				           PSTR(",\"duration\":%d"), (int)fval);
				if (os.iopts[IOPT_SENSOR1_TYPE] == SENSOR_TYPE_FLOW) {
					float gpm = flow_last_gpm * flowrate100 / 100.f;
					snprintf_P(bufs.payload + strlen(bufs.payload), bufs.payload_cap - strlen(bufs.payload),
					           PSTR(",\"flow\":%.2f"), gpm);
				}
			}
			strncat_P(bufs.payload, PSTR("}"), bufs.payload_cap - strlen(bufs.payload) - 1);

			strcat_P(bufs.body, PSTR("Station ["));
			os.get_station_name(lval, bufs.body + strlen(bufs.body));
			strcat_P(bufs.body, PSTR("] closed."));
			if ((int)fval > 0) {
				strcat_P(bufs.body, PSTR(" It ran for "));
				snprintf_P(bufs.body + strlen(bufs.body), bufs.body_cap - strlen(bufs.body),
				           PSTR(" %d minutes %d seconds."), (int)fval/60, (int)fval%60);
			}
			if (os.iopts[IOPT_SENSOR1_TYPE] == SENSOR_TYPE_FLOW) {
				float gpm = flow_last_gpm * flowrate100 / 100.f;
				snprintf_P(bufs.body + strlen(bufs.body), bufs.body_cap - strlen(bufs.body),
				           PSTR(" Flow rate: %.2f"), gpm);
			}
			set_pstr(bufs.subject_suffix, bufs.subject_suffix_cap, PSTR("station event"));
			break;
		}

		case NOTIFY_FLOW_ALERT: {
			// Determine if a flow alert should be sent.
			float flow_gpm_alert_setpoint = 999.9f;
			char tmp_station_name[STATION_NAME_SIZE];
			os.get_station_name(lval, tmp_station_name);

			bool flow_alert_flag = false;
			if (flow_last_gpm > 0 && strlen(tmp_station_name) > 5) {
				const char *station_name_last_five_chars = tmp_station_name + strlen(tmp_station_name) - 5;
				char *endptr;
				flow_gpm_alert_setpoint = strtod(station_name_last_five_chars, &endptr);
				if (endptr != station_name_last_five_chars &&
				    (flow_last_gpm * flowrate100 / 100.f) > flow_gpm_alert_setpoint) {
					flow_alert_flag = true;
				}
			}
			if (!flow_alert_flag) break;   // leaves all bufs empty → no dispatch

			snprintf_P(bufs.topic, bufs.topic_cap, PSTR("station/%d/alert/flow"), lval);
			float gpm = flow_last_gpm * flowrate100 / 100.f;
			snprintf_P(bufs.payload, bufs.payload_cap,
			           PSTR("{\"flow_rate\":%.2f,\"duration\":%d,\"alert_setpoint\":%.4f}"),
			           gpm, (int)fval, flow_gpm_alert_setpoint);

			// Body: "at YYYY-MM-DD hh:mm:ss, Station [...] ran for X. FLOW ALERT! ..."
			strcat_P(bufs.body, PSTR("at "));
			time_os_t curr_time = os.now_tz();
				#if defined(ARDUINO)
				tmElements_t tm;
				breakTime(curr_time, tm);
				snprintf_P(bufs.body + strlen(bufs.body), bufs.body_cap - strlen(bufs.body),
				           PSTR("%04d-%02d-%02d %02d:%02d:%02d"),
				           1970+tm.Year, tm.Month, tm.Day, tm.Hour, tm.Minute, tm.Second);
			#else
				time_t _ct = curr_time;
				struct tm *ti = gmtime(&_ct);
				snprintf_P(bufs.body + strlen(bufs.body), bufs.body_cap - strlen(bufs.body),
				           PSTR("%04d-%02d-%02d %02d:%02d:%02d"),
				           ti->tm_year+1900, ti->tm_mon+1, ti->tm_mday, ti->tm_hour, ti->tm_min, ti->tm_sec);
			#endif
			strcat_P(bufs.body, PSTR(", Station ["));
			tmp_station_name[strlen(tmp_station_name) - 5] = '\0';   // strip the 5-char setpoint suffix
			strcat(bufs.body, tmp_station_name);
			strcat_P(bufs.body, PSTR("]"));
			if (fval > 0) {
				strcat_P(bufs.body, PSTR(" ran for "));
				snprintf_P(bufs.body + strlen(bufs.body), bufs.body_cap - strlen(bufs.body),
				           PSTR("%d minutes %d seconds."), (int)fval/60, (int)fval%60);
			}
			strcat_P(bufs.body, PSTR(" FLOW ALERT!"));
			snprintf_P(bufs.body + strlen(bufs.body), bufs.body_cap - strlen(bufs.body),
			           PSTR(" | Flow rate: %.2f > Flow alert setpoint: %.4f"), gpm, flow_gpm_alert_setpoint);
			set_pstr(bufs.subject_suffix, bufs.subject_suffix_cap, PSTR("- FLOW ALERT"));
			break;
		}

		case NOTIFY_PROGRAM_SCHED: {
			snprintf_P(bufs.topic, bufs.topic_cap, PSTR("program/%d"), lval);
			if (fval < 0) {
				strcat_P(bufs.payload, PSTR("{\"state\":\"skipped\",\"wtrestr\":"));
				snprintf_P(bufs.payload + strlen(bufs.payload), bufs.payload_cap - strlen(bufs.payload),
				           PSTR("%d"), (int)bval);
			} else {
				snprintf_P(bufs.payload + strlen(bufs.payload), bufs.payload_cap - strlen(bufs.payload),
				           PSTR("{\"state\":1,\"wl\":%d,\"wa\":%.4g,\"sa\":%.4g,\"ta\":%.4g"),
				           (int)fval, fval/100.f, fval2, fval/100.f*fval2);
			}
			strncat_P(bufs.payload, PSTR("}"), bufs.payload_cap - strlen(bufs.payload) - 1);

			if (fval < 0) {
				strcat_P(bufs.body, PSTR("skipped"));
				if (bval > 0) strcat_P(bufs.body, PSTR(" due to weather restriction."));
			} else {
				if (bval) strcat_P(bufs.body, PSTR("manually scheduled "));
				else      strcat_P(bufs.body, PSTR("automatically scheduled "));
			}
			if (lval == RUNONCE_PID) {
				strcat_P(bufs.body, PSTR("Run-once program"));
			} else if (lval < pd.nprograms) {
				ProgramStruct prog;
				pd.read(lval, &prog);
				strcat(bufs.body, prog.name);
			}
			if (fval > 0) {
				snprintf_P(bufs.body + strlen(bufs.body), bufs.body_cap - strlen(bufs.body),
				           PSTR(". Adjustments: Weather->%d%%, Sensor->%.2f%%, Total->%.2f%%."),
				           (int)fval, fval2*100.f, fval*fval2);
			}
			set_pstr(bufs.subject_suffix, bufs.subject_suffix_cap, PSTR("program event"));
			break;
		}

		case NOTIFY_SENSOR1: {
			strncpy_P(bufs.topic, PSTR("sensor1"), bufs.topic_cap - 1);
			snprintf_P(bufs.payload, bufs.payload_cap, PSTR("{\"state\":%d}"), (int)fval);
			strcat_P(bufs.body, PSTR("sensor 1 "));
			strcat_P(bufs.body, ((int)fval) ? PSTR("activated.") : PSTR("de-activated."));
			set_pstr(bufs.subject_suffix, bufs.subject_suffix_cap, PSTR("sensor 1 event"));
			break;
		}

		case NOTIFY_SENSOR2: {
			strncpy_P(bufs.topic, PSTR("sensor2"), bufs.topic_cap - 1);
			snprintf_P(bufs.payload, bufs.payload_cap, PSTR("{\"state\":%d}"), (int)fval);
			strcat_P(bufs.body, PSTR("sensor 2 "));
			strcat_P(bufs.body, ((int)fval) ? PSTR("activated.") : PSTR("de-activated."));
			set_pstr(bufs.subject_suffix, bufs.subject_suffix_cap, PSTR("sensor 2 event"));
			break;
		}

		case NOTIFY_SENSOR3: {
			strncpy_P(bufs.topic, PSTR("sensor3"), bufs.topic_cap - 1);
			snprintf_P(bufs.payload, bufs.payload_cap, PSTR("{\"state\":%d}"), (int)fval);
			strcat_P(bufs.body, PSTR("sensor 3 "));
			strcat_P(bufs.body, ((int)fval) ? PSTR("activated.") : PSTR("de-activated."));
			set_pstr(bufs.subject_suffix, bufs.subject_suffix_cap, PSTR("sensor 3 event"));
			break;
		}

		case NOTIFY_SENSOR4: {
			strncpy_P(bufs.topic, PSTR("sensor4"), bufs.topic_cap - 1);
			snprintf_P(bufs.payload, bufs.payload_cap, PSTR("{\"state\":%d}"), (int)fval);
			strcat_P(bufs.body, PSTR("sensor 4 "));
			strcat_P(bufs.body, ((int)fval) ? PSTR("activated.") : PSTR("de-activated."));
			set_pstr(bufs.subject_suffix, bufs.subject_suffix_cap, PSTR("sensor 4 event"));
			break;
		}

		case NOTIFY_RAINDELAY: {
			strncpy_P(bufs.topic, PSTR("raindelay"), bufs.topic_cap - 1);
			snprintf_P(bufs.payload, bufs.payload_cap, PSTR("{\"state\":%d}"), (int)fval);
			strcat_P(bufs.body, PSTR("rain delay "));
			strcat_P(bufs.body, ((int)fval) ? PSTR("activated.") : PSTR("de-activated."));
			set_pstr(bufs.subject_suffix, bufs.subject_suffix_cap, PSTR("rain delay event"));
			break;
		}

		case NOTIFY_FLOWSENSOR: {
			float vol = lval * flowrate100 / 100.f;
			strncpy_P(bufs.topic, PSTR("sensor/flow"), bufs.topic_cap - 1);
			snprintf_P(bufs.payload, bufs.payload_cap, PSTR("{\"count\":%d,\"volume\":%.2f}"), (int)lval, vol);
			snprintf_P(bufs.body + strlen(bufs.body), bufs.body_cap - strlen(bufs.body),
			           PSTR("Flow count: %d, volume: %.2f"), (int)lval, vol);
			set_pstr(bufs.subject_suffix, bufs.subject_suffix_cap, PSTR("flow sensor event"));
			break;
		}

		case NOTIFY_CURR_ALERT: {
			int16_t curr = (int16_t)fval;
			int16_t imin = os.get_imin();
			int16_t imax = os.get_imax();
			switch (bval) {
				case CURR_ALERT_TYPE_UNDER:
				case CURR_ALERT_TYPE_OVER_STATION:
					snprintf_P(bufs.topic, bufs.topic_cap, PSTR("station/%d/alert/curr"), lval);
					if (bval == CURR_ALERT_TYPE_UNDER)
						snprintf_P(bufs.payload, bufs.payload_cap,
						           PSTR("{\"curr_value\":%d,\"imin_threshold\":%d}"), curr, imin);
					else
						snprintf_P(bufs.payload, bufs.payload_cap,
						           PSTR("{\"curr_value\":%d,\"imax_limit\":%d}"),
						           curr, (imax + OVERCURRENT_INRUSH_EXTRA));
					break;
				case CURR_ALERT_TYPE_OVER_SYSTEM:
					strncpy_P(bufs.topic, PSTR("overcurrent"), bufs.topic_cap - 1);
					snprintf_P(bufs.payload, bufs.payload_cap,
					           PSTR("{\"curr_value\":%d,\"imax_limit\":%d}"), curr, imax);
					break;
			}

			strcat_P(bufs.body, PSTR("at "));
			time_os_t curr_time = os.now_tz();
				#if defined(ARDUINO)
				tmElements_t tm;
				breakTime(curr_time, tm);
				snprintf_P(bufs.body + strlen(bufs.body), bufs.body_cap - strlen(bufs.body),
				           PSTR("%04d-%02d-%02d %02d:%02d:%02d"),
				           1970+tm.Year, tm.Month, tm.Day, tm.Hour, tm.Minute, tm.Second);
			#else
				time_t _ct = curr_time;
				struct tm *ti = gmtime(&_ct);
				snprintf_P(bufs.body + strlen(bufs.body), bufs.body_cap - strlen(bufs.body),
				           PSTR("%04d-%02d-%02d %02d:%02d:%02d"),
				           ti->tm_year+1900, ti->tm_mon+1, ti->tm_mday, ti->tm_hour, ti->tm_min, ti->tm_sec);
			#endif

			if (bval == CURR_ALERT_TYPE_UNDER || bval == CURR_ALERT_TYPE_OVER_STATION) {
				char tmp_station_name[STATION_NAME_SIZE];
				os.get_station_name(lval, tmp_station_name);
				strcat_P(bufs.body, PSTR(", Station ["));
				strcat(bufs.body, tmp_station_name);
				strcat_P(bufs.body, PSTR("]"));
			} else {
				strcat_P(bufs.body, PSTR(", System"));
			}

			switch (bval) {
				case CURR_ALERT_TYPE_UNDER:
					strcat_P(bufs.body, PSTR(" UNDERCURRENT detected!"));
					snprintf_P(bufs.body + strlen(bufs.body), bufs.body_cap - strlen(bufs.body),
					           PSTR(" | %dmA < imin threshold: %dmA"), curr, imin);
					break;
				case CURR_ALERT_TYPE_OVER_STATION:
				case CURR_ALERT_TYPE_OVER_SYSTEM:
					strcat_P(bufs.body, PSTR(" OVERCURRENT detected!"));
					snprintf_P(bufs.body + strlen(bufs.body), bufs.body_cap - strlen(bufs.body),
					           PSTR(" | %dmA > imax limit: %dmA. The affected station(s) have been closed."),
					           curr, imax + ((bval == CURR_ALERT_TYPE_OVER_STATION) ? OVERCURRENT_INRUSH_EXTRA : 0));
					break;
			}
			set_pstr(bufs.subject_suffix, bufs.subject_suffix_cap, PSTR("- CURRENT ALERT"));
			break;
		}

		case NOTIFY_WEATHER_UPDATE: {
			strncpy_P(bufs.topic, PSTR("weather"), bufs.topic_cap - 1);
			snprintf_P(bufs.payload, bufs.payload_cap, PSTR("{\"water level\":%d}"), (int)fval);
			if (lval > 0) {
				strcat_P(bufs.body, PSTR("external IP updated: "));
				unsigned char ip[4] = {(unsigned char)((lval>>24)&0xFF),
				                       (unsigned char)((lval>>16)&0xFF),
				                       (unsigned char)((lval>>8)&0xFF),
				                       (unsigned char)(lval&0xFF)};
				ip2string(bufs.body, bufs.body_cap, ip);
			}
			if (fval >= 0) {
				snprintf_P(bufs.body + strlen(bufs.body), bufs.body_cap - strlen(bufs.body),
				           PSTR("water level updated: %d%%."), (int)fval);
			}
			set_pstr(bufs.subject_suffix, bufs.subject_suffix_cap, PSTR("weather update event"));
			break;
		}

		case NOTIFY_REBOOT: {
			strncpy_P(bufs.topic, PSTR("system"), bufs.topic_cap - 1);
			snprintf_P(bufs.payload, bufs.payload_cap,
			           PSTR("{\"state\":\"started\",\"cause\":%d}"), (int)os.last_reboot_cause);
				#if defined(ESP8266)
				snprintf_P(bufs.body + strlen(bufs.body), bufs.body_cap - strlen(bufs.body),
				           PSTR("rebooted. Cause: %d. Device IP: "), os.last_reboot_cause);
				IPAddress _ip = useEth ? eth.localIP() : WiFi.localIP();
				unsigned char ip[4] = {_ip[0], _ip[1], _ip[2], _ip[3]};
					ip2string(bufs.body, bufs.body_cap, ip);
				#elif defined(ESP32)
					snprintf_P(bufs.body + strlen(bufs.body), bufs.body_cap - strlen(bufs.body),
					           PSTR("rebooted. Cause: %d. Device IP: "), os.last_reboot_cause);
					IPAddress _ip = useEth ? ETH.localIP() : WiFi.localIP();
					unsigned char ip[4] = {_ip[0], _ip[1], _ip[2], _ip[3]};
					ip2string(bufs.body, bufs.body_cap, ip);
			#else
				strcat_P(bufs.body, PSTR("controller process restarted."));
			#endif
			set_pstr(bufs.subject_suffix, bufs.subject_suffix_cap, PSTR("reboot event"));
			break;
		}
	}
}

// Sends an email using `cfg`'s credentials, with the given subject and body.
// Platform-specific: ESP8266 uses EMailSender, others use libsmtp.
static void send_email(const EmailConfig &cfg, const char *subject, const char *body) {
	if (!cfg.host || !cfg.user || !cfg.pass || !cfg.recipient) return;

	#if defined(ARDUINO)
	EMailSender::EMailMessage msg;
	msg.subject = subject;
	msg.message = body;

	EMailSender emailSend(cfg.user, cfg.pass);
	emailSend.setSMTPServer(cfg.host);
	emailSend.setSMTPPort(cfg.port);
	if (strlen(cfg.user) == 0) {
		emailSend.setUseAuth(false);
	}
	(void)emailSend.send(cfg.recipient, msg);
	#else
	enum smtp_connection_security security_flag;
	if (cfg.port == 25) {
		security_flag = SMTP_SECURITY_NONE;
	} else if (cfg.port == 587) {
		security_flag = SMTP_SECURITY_STARTTLS;
	} else {
		// Default to Implicit SSL for 465 (or legacy configurations)
		security_flag = SMTP_SECURITY_TLS;
	}
	enum smtp_authentication_method auth_flag =
		(strlen(cfg.user) == 0) ? SMTP_AUTH_NONE : SMTP_AUTH_PLAIN;

	String port_str = to_string(cfg.port);
	struct smtp *smtp = NULL;
	smtp_status_code rc;
	rc = smtp_open(cfg.host, port_str.c_str(), security_flag, SMTP_NO_CERT_VERIFY, NULL, &smtp);
	rc = smtp_auth(smtp, auth_flag, cfg.user, cfg.pass);
	rc = smtp_address_add(smtp, SMTP_ADDRESS_FROM, cfg.user, "OpenSprinkler");
	rc = smtp_address_add(smtp, SMTP_ADDRESS_TO,   cfg.recipient, "User");
	rc = smtp_header_add(smtp, "Subject", subject);
	rc = smtp_mail(smtp, body);
	rc = smtp_close(smtp);
	if (rc != SMTP_STATUS_OK) {
		DEBUG_PRINTF("SMTP: Error %s\n", smtp_status_code_errstr(rc));
	}
#endif
}

bool is_notif_enabled(uint16_t type) {
	uint16_t notif = (uint16_t)os.iopts[IOPT_NOTIF_ENABLE] | ((uint16_t)os.iopts[IOPT_NOTIF2_ENABLE] << 8);
	return  (notif&type) != 0;
}

uint16_t get_notif_enabled() {
	return (uint16_t)os.iopts[IOPT_NOTIF_ENABLE]|((uint16_t)os.iopts[IOPT_NOTIF2_ENABLE]<<8);
}

void set_notif_enabled(uint16_t notif) {
	os.iopts[IOPT_NOTIF_ENABLE] = notif&0xFF;
	os.iopts[IOPT_NOTIF2_ENABLE] = notif >> 8;
}

void ip2string(char* str, size_t str_len, unsigned char ip[4]) {
	snprintf_P(str+strlen(str), str_len, PSTR("%d.%d.%d.%d"), ip[0], ip[1], ip[2], ip[3]);
}

bool NotifQueue::add(uint16_t t, uint32_t l, float f, uint8_t b, float f2) {
		if (!is_notif_enabled(t)) { // if not subscribed to this type, return
		return false;
	}
	if(nqueue<NOTIF_QUEUE_MAXSIZE) {
		queue[tail] = NotifNodeStruct(t, l, f, b, f2);
		tail = (tail + 1 )% NOTIF_QUEUE_MAXSIZE;
		nqueue++;
		DEBUG_PRINTF("NotifQueue::add (type %d) [%d|%d|%d]\n", t, nqueue, head, tail);
		return true;
	}
	DEBUG_PRINTLN(F("NotifQueue::add queue is full!"));
	return false;
}

void NotifQueue::clear() {
	nqueue = 0;
	head = 0;
	tail = 0;
}

void push_message(uint16_t type, uint32_t lval, float fval, uint8_t bval, float fval2=0.f);

bool NotifQueue::run(int n) {
	if(nqueue == 0) return false; // queue is empty
	if(n<=0 || n>nqueue) n=nqueue;
	while(nqueue!=0 && n!=0) {
		NotifNodeStruct* node = &queue[head];
		head = (head + 1) % NOTIF_QUEUE_MAXSIZE;
		push_message(node->type, node->lval, node->fval, node->bval, node->fval2);
		nqueue--;
		n--;
		DEBUG_PRINTF("NotifQueue::run (type %d) [%d|%d|%d]\n", node->type, nqueue, head, tail);
	}
	return true;
}

#define PUSH_TOPIC_LEN	120
#define PUSH_PAYLOAD_LEN TMP_BUFFER_SIZE

void push_message(uint16_t type, uint32_t lval, float fval, uint8_t bval, float fval2) {
	if (!is_notif_enabled(type)) {
		return;
	}

	// IFTTT: probe SOPT_IFTTT_KEY into tmp_buffer just to test enabled-ness; the
	// key itself is loaded on demand by send_ifttt via the BufferFiller $O
	// directive, so we don't keep it resident.
	os.sopt_load(SOPT_IFTTT_KEY, tmp_buffer);
	bool ifttt_enabled = (tmp_buffer[0] != 0);

	// Email: parse config into the file-static EmailConfigContext (BSS-resident).
	// cfg pointers reference s_email_ctx.doc; valid for the duration of this call.
	load_email_config(s_email_ctx);
	const EmailConfig &email_cfg = s_email_ctx.cfg;
	bool email_enabled = email_cfg.enabled;

	bool mqtt_enabled = os.mqtt.enabled();
	if (!mqtt_enabled && !ifttt_enabled && !email_enabled) return;

	// Owned buffers — all in BSS, no per-call stack/heap pressure.
	static char topic[PUSH_TOPIC_LEN + 1];
	static char payload[PUSH_PAYLOAD_LEN + 1];
	static char subject[PUSH_TOPIC_LEN + 64 + 2];   // "<device> <suffix>" + NUL

	// Body lives in tmp_buffer. We pre-write the "On site [<device>], " prefix,
	// then format_notification appends event text after it. The full body
	// (prefix + event text) is what send_ifttt and send_email receive.
	// Device name is loaded into `topic` briefly, then `topic` is reset for
	// MQTT use by format_notification.
	int body_prefix_len = 0;
	int subject_prefix_len = 0;
	if (ifttt_enabled || email_enabled) {
		os.sopt_load(SOPT_DEVICE_NAME, topic, PUSH_TOPIC_LEN);
		topic[PUSH_TOPIC_LEN] = 0;
		body_prefix_len    = snprintf(tmp_buffer, TMP_BUFFER_SIZE,
		                               "On site [%s], ", topic);
		subject_prefix_len = snprintf(subject, sizeof(subject),
		                               "%s ", topic);
	} else {
		// MQTT-only path: body and subject aren't used.
		tmp_buffer[0] = '\0';
		subject[0]    = '\0';
	}

	// Reset topic/payload — format_notification will populate (or leave empty
	// for events that don't emit MQTT messages).
	topic[0]   = '\0';
	payload[0] = '\0';

	NotifBuffers bufs{
		topic,                       sizeof(topic),
		payload,                     sizeof(payload),
		tmp_buffer + body_prefix_len,
		(size_t)(TMP_BUFFER_SIZE - body_prefix_len),
		subject + subject_prefix_len,
		(size_t)(sizeof(subject) - subject_prefix_len)
	};
	format_notification(type, lval, fval, bval, fval2, bufs);

	// Dispatch. Each integration has its own gating: empty buffers → skip.
	if (mqtt_enabled && topic[0] && payload[0]) {
		os.mqtt.publish(topic, payload);
	}
	// Body's "event text" portion may be empty if format_notification declined
	// to emit (e.g., NOTIFY_FLOW_ALERT no-alert case). Suppress IFTTT/email in
	// that case — sending just "On site [Garden]," with no event is noise.
	if (tmp_buffer[body_prefix_len] != '\0') {
		if (ifttt_enabled) {
			send_ifttt(tmp_buffer);
		}
		if (email_enabled) {
			send_email(email_cfg, subject, tmp_buffer);
		}
	}
}
