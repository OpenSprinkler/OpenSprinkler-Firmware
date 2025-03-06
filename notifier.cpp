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
#include "program.h"
#include "ArduinoJson.hpp"
#include "opensprinkler_server.h"

NotifNodeStruct* NotifQueue::head = NULL;
NotifNodeStruct* NotifQueue::tail = NULL;
unsigned char NotifQueue::nqueue = 0;

extern OpenSprinkler os;
extern ProgramData pd;
extern char tmp_buffer[];
extern char ether_buffer[];
extern float flow_last_gpm;
void remote_http_callback(char*);

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

bool NotifQueue::add(uint16_t t, uint32_t l, float f, uint8_t b) {
		if (!is_notif_enabled(t)) { // if not subscribed to this type, return
		return false;
	}
	if(nqueue<NOTIF_QUEUE_MAXSIZE) {
		NotifNodeStruct* node = new NotifNodeStruct(t, l, f, b);
		if(tail==NULL) {
			head = node;
		} else {
			tail->next = node;
		}
		tail = node;
		nqueue++;
		DEBUG_PRINTF("NotifQueue::add (type %d) [%d]\n", t, nqueue);
		return true;
	}
	DEBUG_PRINTLN(F("NotifQueue::add queue is full!"));
	return false;
}

void NotifQueue::clear() {
	while(nqueue!=0) {
		NotifNodeStruct* node = head;
		head = head->next;
		if(head==NULL) {
			tail = NULL;
		}
		delete node;
		nqueue--;
	}
}

void push_message(uint16_t type, uint32_t lval, float fval, uint8_t bval);

bool NotifQueue::run(int n) {
	if(nqueue == 0) return false; // queue is empty
	if(n<=0 || n>nqueue) n=nqueue;
	while(nqueue!=0 && n!=0) {
		NotifNodeStruct* node = head;
		head = head->next;
		if(head==NULL) {
			tail = NULL;
		}
		push_message(node->type, node->lval, node->fval, node->bval);
		DEBUG_PRINTF("NotifQueue::run (type %d) [%d]\n", node->type, nqueue);
		delete node;
		nqueue--;
		n--;
	}
	return true;
}

#define PUSH_TOPIC_LEN	120
#define PUSH_PAYLOAD_LEN TMP_BUFFER_SIZE

void push_message(uint16_t type, uint32_t lval, float fval, uint8_t bval) {
	if (!is_notif_enabled(type)) {
		return;
	}
	static char topic[PUSH_TOPIC_LEN+1];
	static char payload[PUSH_PAYLOAD_LEN+1];
	char* postval = tmp_buffer+1; // +1 so we can fit a opening { before the loaded config

	// check if ifttt key exists and also if the enable bit is set
	os.sopt_load(SOPT_IFTTT_KEY, tmp_buffer);
	bool ifttt_enabled = (strlen(tmp_buffer)!=0);
	// flow rate
	uint32_t flowrate100 = (((uint32_t)os.iopts[IOPT_PULSE_RATE_1])<<8) + os.iopts[IOPT_PULSE_RATE_0];

#define DEFAULT_EMAIL_PORT	465

	// parse email variables
	#if defined(SUPPORT_EMAIL)
	// define email variables
	ArduinoJson::JsonDocument doc; // make sure this has the same scope of email_x variables to prevent use after free
	const char *email_host = NULL;
	const char *email_username = NULL;
	const char *email_password = NULL;
	const char *email_recipient = NULL;
	int  email_port = DEFAULT_EMAIL_PORT;
	int  email_en = 0;

	os.sopt_load(SOPT_EMAIL_OPTS, postval);
	if (*postval != 0) {
		// Add the wrapping curly braces to the string
		postval = tmp_buffer;
		postval[0] = '{';
		int len = strlen(postval);
		postval[len] = '}';
		postval[len+1] = 0;

		ArduinoJson::DeserializationError error = ArduinoJson::deserializeJson(doc, postval);
		// Test the parsing otherwise parse
		if (error) {
			DEBUG_PRINT(F("mqtt: deserializeJson() failed: "));
			DEBUG_PRINTLN(error.c_str());
		} else {
			email_en = doc["en"];
			email_host = doc["host"];
			email_port = doc["port"];
			email_username = doc["user"];
			email_password = doc["pass"];
			email_recipient= doc["recipient"];
		}
	}
	#endif

	#if defined(ESP8266)
		EMailSender::EMailMessage email_message;
	#else
		struct {
			String subject;
			String message;
		} email_message;
	#endif

	bool email_enabled = false;
#if defined(SUPPORT_EMAIL)
	if(!email_en){  // todo: this should be simplified
		email_enabled = false;
	}else{
		email_enabled = true;
	}
#endif

	// if none if enabled, return here
	if ((!ifttt_enabled) && (!email_enabled) && (!os.mqtt.enabled()))
		return;

	if (ifttt_enabled || email_enabled) {
		strcpy_P(postval, PSTR("{\"value1\":\"On site ["));
		os.sopt_load(SOPT_DEVICE_NAME, topic, PUSH_TOPIC_LEN);
		topic[PUSH_TOPIC_LEN]=0;
		strcat(postval+strlen(postval), topic);
		strcat_P(postval, PSTR("], "));
		if(email_enabled) {		
			strcat(topic, " ");
			email_message.subject = topic; // prefix the email subject with device name
		}
	}

	if (os.mqtt.enabled()) {
		topic[0] = 0;
		payload[0] = 0;
	}

	switch(type) {
		case  NOTIFY_STATION_ON:

			if (os.mqtt.enabled()) {
				snprintf_P(topic, PUSH_TOPIC_LEN, PSTR("station/%d"), lval);
				strcat_P(payload, PSTR("{\"state\":1"));
				if((int)fval > 0){
					snprintf_P(payload+strlen(payload), PUSH_PAYLOAD_LEN, PSTR(",\"duration\":%d"), (int)fval);
				}
				strcat_P(payload, PSTR("}"));
			}
			if (ifttt_enabled || email_enabled) {
				strcat_P(postval, PSTR("Station ["));
				os.get_station_name(lval, postval+strlen(postval));
				strcat_P(postval, PSTR("] just turned on."));
				if((int)fval > 0){
					strcat_P(postval, PSTR(" It's scheduled to run for "));
					snprintf_P(postval+strlen(postval), TMP_BUFFER_SIZE, PSTR(" %d minutes %d seconds."), (int)fval/60, (int)fval%60);
				}
				if(email_enabled) { email_message.subject += PSTR("station event"); }
			}			break;

		case NOTIFY_STATION_OFF:

			if (os.mqtt.enabled()) {
				snprintf_P(topic, PUSH_TOPIC_LEN, PSTR("station/%d"), lval);
				strcat_P(payload, PSTR("{\"state\":0"));
				if((int)fval > 0) {
					snprintf_P(payload+strlen(payload), PUSH_PAYLOAD_LEN, PSTR(",\"duration\":%d"), (int)fval);
					if (os.iopts[IOPT_SENSOR1_TYPE]==SENSOR_TYPE_FLOW) {
						float gpm = flow_last_gpm * flowrate100 / 100.f;
						#if defined(OS_AVR)
						snprintf_P(payload+strlen(payload), PUSH_PAYLOAD_LEN, PSTR(",\"flow\":%d.%02d"), (int)gpm, (int)(gpm*100)%100);
						#else
						snprintf_P(payload+strlen(payload), PUSH_PAYLOAD_LEN, PSTR(",\"flow\":%.2f"), gpm);
						#endif
					}
				}
				strcat_P(payload, PSTR("}"));
			}
			if (ifttt_enabled || email_enabled) {
				strcat_P(postval, PSTR("Station ["));
				os.get_station_name(lval, postval+strlen(postval));
				strcat_P(postval, PSTR("] closed."));
				if((int)fval > 0) {
					strcat_P(postval, PSTR(" It ran for "));
					snprintf_P(postval+strlen(postval), TMP_BUFFER_SIZE, PSTR(" %d minutes %d seconds."), (int)fval/60, (int)fval%60);
				}

				if(os.iopts[IOPT_SENSOR1_TYPE]==SENSOR_TYPE_FLOW) {
					float gpm = flow_last_gpm * flowrate100 / 100.f;
					#if defined(OS_AVR)
					snprintf_P(postval+strlen(postval), TMP_BUFFER_SIZE, PSTR(" Flow rate: %d.%02d"), (int)gpm, (int)(gpm*100)%100);
					#else
					snprintf_P(postval+strlen(postval), TMP_BUFFER_SIZE, PSTR(" Flow rate: %.2f"), gpm);
					#endif
				}
				if(email_enabled) { email_message.subject += PSTR("station event"); }
			}
			break;

		case NOTIFY_FLOW_ALERT:{
			//First determine if a Flow Alert should be sent based on flow amount and setpoint

			//Added variable to track flow alert status
			bool flow_alert_flag = false;

			//Added variable for flow_gpm_alert_setpoint and set default value to max
			float flow_gpm_alert_setpoint = 999.9f;

			//Added variable for tmp station name
			char tmp_station_name[STATION_NAME_SIZE];

			//Get satation name
			os.get_station_name(lval, tmp_station_name);

			// only proceed if flow rate is positive, and the station name has at least 5 characters
			if (flow_last_gpm > 0 && strlen(tmp_station_name) > 5) {
				const char *station_name_last_five_chars = tmp_station_name;
				// extract the last 5 characters
				station_name_last_five_chars = tmp_station_name + strlen(tmp_station_name) - 5;
				// Convert last five characters to number and check if valid
				// Had to switch to use strtod because sscanf in AVR doesn't work with float :(
				char *endptr;
				flow_gpm_alert_setpoint = strtod(station_name_last_five_chars, &endptr);
				if (endptr != station_name_last_five_chars) {
					//station_name_last_five_chars was successfully converted to a number 
					//flow_last_gpm is actually collected and stored as pulses per minute, not gallons per minute
					// Alert Check - Compare flow_gpm_alert_setpoint with flow_last_gpm and enable flow_alert_flag if flow is above setpoint
					if ((flow_last_gpm*flowrate100/100.f) > flow_gpm_alert_setpoint) {
						flow_alert_flag = true;
					}
				} else {
					//Could not convert to a valid number. If a number is not detected as a station name suffix, never send an alert
					flow_alert_flag = false;
				}
			} else {
 				//Station name was not long enough to include 5 character flow setpoint.
				flow_alert_flag = false;
			}

			// If flow_alert_flag is true, format the appropriate messages, else don't send alert
			if (flow_alert_flag == true) {

				if (os.mqtt.enabled()) {
					//Format mqtt message
					snprintf_P(topic, PUSH_TOPIC_LEN, PSTR("station/%d/alert/flow"), lval);
					float gpm = flow_last_gpm * flowrate100 / 100.f;
					#if defined(OS_AVR)
					snprintf_P(payload, PUSH_PAYLOAD_LEN, PSTR("{\"flow_rate\":%d.%02d,\"duration\":%d,\"alert_setpoint\":%d.%02d}"), (int)gpm, (int)(gpm*100)%100,
					(int)fval, (int)flow_gpm_alert_setpoint, (int)(flow_gpm_alert_setpoint*100)%100);
					#else
					snprintf_P(payload, PUSH_PAYLOAD_LEN, PSTR("{\"flow_rate\":%.2f,\"duration\":%d,\"alert_setpoint\":%.4f}"), gpm, (int)fval, flow_gpm_alert_setpoint);
					#endif
				}


				if (ifttt_enabled || email_enabled) {
					//Format ifttt\email message

					// Get and format current local time as "YYYY-MM-DD hh:mm:ss AM/PM"
					strcat_P(postval, PSTR("at "));
					time_os_t curr_time = os.now_tz();
					#if defined(ARDUINO)
					tmElements_t tm;
					breakTime(curr_time, tm);
					snprintf_P(postval+strlen(postval), TMP_BUFFER_SIZE, PSTR("%04d-%02d-%02d %02d:%02d:%02d"),
						1970+tm.Year, tm.Month, tm.Day, tm.Hour, tm.Minute, tm.Second);
					#else
					struct tm *ti = gmtime(&curr_time);
					snprintf_P(postval+strlen(postval), TMP_BUFFER_SIZE, PSTR("%04d-%02d-%02d %02d:%02d:%02d"),
						ti->tm_year+1900, ti->tm_mon+1, ti->tm_mday, ti->tm_hour, ti->tm_min, ti->tm_sec);
					#endif

					strcat_P(postval, PSTR(", Station ["));
					//Truncate flow setpoint value off station name to shorten ifttt\email message
					tmp_station_name[(strlen(tmp_station_name) - 5)] = '\0';
					strcat_P(postval, tmp_station_name);
					strcat_P(postval, PSTR("]"));
					if(fval > 0){ // if there is a valid duration
						strcat_P(postval, PSTR(" ran for "));
						snprintf_P(postval+strlen(postval), TMP_BUFFER_SIZE, PSTR("%d minutes %d seconds."), (int)fval/60, ((int)fval%60));
					}

					strcat_P(postval, PSTR(" FLOW ALERT!"));
					float gpm = flow_last_gpm * flowrate100 / 100.f;
					#if defined(OS_AVR)
					snprintf_P(postval+strlen(postval), TMP_BUFFER_SIZE, PSTR(" | Flow rate: %d.%02d > Flow alert setpoint: %d.%02d"),
						(int)gpm, (int)(gpm*100)%100, (int)flow_gpm_alert_setpoint, (int)(flow_gpm_alert_setpoint*100)%100);
					#else
					snprintf_P(postval+strlen(postval), TMP_BUFFER_SIZE, PSTR(" | Flow rate: %.2f > Flow alert setpoint: %.4f"),
						gpm, flow_gpm_alert_setpoint);
					#endif

					if(email_enabled) { email_message.subject += PSTR("- FLOW ALERT"); }

				}
			} else {
				//Do not send an alert.  Flow was not above setpoint or setpoint not valid. 
				//Must force ifftt_enabled and email_enabled to false to prevent sending
				//Can not force os.mqtt.enabled() off, but it will not publish an mqtt message as topic\payload will be empty.
				ifttt_enabled=false;
				email_enabled=false;
			}
		break;
		}
 
		case NOTIFY_PROGRAM_SCHED:

			if (ifttt_enabled || email_enabled) {
				if (bval) strcat_P(postval, PSTR("manually"));
				else strcat_P(postval, PSTR("automatically"));
				strcat_P(postval, PSTR(" scheduled Program "));
				{
					ProgramStruct prog;
					pd.read(lval, &prog);
					if(lval<pd.nprograms) strcat(postval, prog.name);
				}
				snprintf_P(postval+strlen(postval), TMP_BUFFER_SIZE, PSTR(" with %d%% water level."), (int)fval);
				if(email_enabled) { email_message.subject += PSTR("program event"); }
			}
			break;

		case NOTIFY_SENSOR1:

			if (os.mqtt.enabled()) {
				strcpy_P(topic, PSTR("sensor1"));
				snprintf_P(payload, PUSH_PAYLOAD_LEN, PSTR("{\"state\":%d}"), (int)fval);
			}
			if (ifttt_enabled || email_enabled) {
				strcat_P(postval, PSTR("sensor 1 "));
				strcat_P(postval, ((int)fval)?PSTR("activated."):PSTR("de-activated."));
				if(email_enabled) { email_message.subject += PSTR("sensor 1 event"); }
			}
			break;

		case NOTIFY_SENSOR2:

			if (os.mqtt.enabled()) {
				strcpy_P(topic, PSTR("sensor2"));
				snprintf_P(payload, PUSH_PAYLOAD_LEN, PSTR("{\"state\":%d}"), (int)fval);
			}
			if (ifttt_enabled || email_enabled) {
				strcat_P(postval, PSTR("sensor 2 "));
				strcat_P(postval, ((int)fval)?PSTR("activated."):PSTR("de-activated."));
				if(email_enabled) { email_message.subject += PSTR("sensor 2 event"); }
			}
			break;

		case NOTIFY_RAINDELAY:

			if (os.mqtt.enabled()) {
				strcpy_P(topic, PSTR("raindelay"));
				snprintf_P(payload, PUSH_PAYLOAD_LEN, PSTR("{\"state\":%d}"), (int)fval);
			}
			if (ifttt_enabled || email_enabled) {
				strcat_P(postval, PSTR("rain delay "));
				strcat_P(postval, ((int)fval)?PSTR("activated."):PSTR("de-activated."));
				if(email_enabled) { email_message.subject += PSTR("rain delay event"); }
			}
			break;

		case NOTIFY_FLOWSENSOR:
			{
				float vol = lval*flowrate100/100.f;
				if (os.mqtt.enabled()) {
					strcpy_P(topic, PSTR("sensor/flow"));
					#if defined(OS_AVR)
					snprintf_P(payload, PUSH_PAYLOAD_LEN, PSTR("{\"count\":%d,\"volume\":%d.%02d}"), (int)lval, (int)vol, (int)(vol*100)%100);
					#else
					snprintf_P(payload, PUSH_PAYLOAD_LEN, PSTR("{\"count\":%d,\"volume\":%.2f}"), (int)lval, vol);
					#endif
				}
				if (ifttt_enabled || email_enabled) {
					#if defined(OS_VAR)
					snprintf_P(postval+strlen(postval), TMP_BUFFER_SIZE, PSTR("Flow count: %d, volume: %d.%02d"), (int)lval, (int)vol, (int)(vol*100)%100);
					#else
					snprintf_P(postval+strlen(postval), TMP_BUFFER_SIZE, PSTR("Flow count: %d, volume: %.2f"), (int)lval, vol);
					#endif
					if(email_enabled) { email_message.subject += PSTR("flow sensor event"); }
				}
			}
			break;

		case NOTIFY_WEATHER_UPDATE:

			if (os.mqtt.enabled()) {
				strcpy_P(topic, PSTR("weather"));
				snprintf_P(payload, PUSH_PAYLOAD_LEN, PSTR("{\"water level\":%d}"), (int)fval);
			}
			if (ifttt_enabled || email_enabled) {
				if(lval>0) {
					strcat_P(postval, PSTR("external IP updated: "));
					unsigned char ip[4] = {(unsigned char)((lval>>24)&0xFF),
									(unsigned char)((lval>>16)&0xFF),
									(unsigned char)((lval>>8)&0xFF),
									(unsigned char)(lval&0xFF)};
					ip2string(postval, TMP_BUFFER_SIZE, ip);
				}
				if(fval>=0) {
					snprintf_P(postval+strlen(postval), TMP_BUFFER_SIZE, PSTR("water level updated: %d%%."), (int)fval);
				}
				if(email_enabled) { email_message.subject += PSTR("weather update event"); }
			}
			break;

		case NOTIFY_REBOOT:
			if (os.mqtt.enabled()) {
				strcpy_P(topic, PSTR("system"));
				snprintf_P(payload, PUSH_PAYLOAD_LEN, PSTR("{\"state\":\"started\",\"cause\":%d}"), (int)os.last_reboot_cause);
			}
			if (ifttt_enabled || email_enabled) {
				#if defined(ARDUINO)
					snprintf_P(postval+strlen(postval), TMP_BUFFER_SIZE, PSTR("rebooted. Cause: %d. Device IP: "), os.last_reboot_cause);
					#if defined(ESP8266)
					{
						IPAddress _ip;
						if (useEth) {
							//_ip = Ethernet.localIP();
							_ip = eth.localIP();
						} else {
							_ip = WiFi.localIP();
						}
						unsigned char ip[4] = {_ip[0], _ip[1], _ip[2], _ip[3]};
						ip2string(postval, TMP_BUFFER_SIZE, ip);
					}
					#else
						ip2string(postval, TMP_BUFFER_SIZE, &(Ethernet.localIP()[0]));
					#endif
				#else
					strcat_P(postval, PSTR("controller process restarted."));
				#endif
				if(email_enabled) { email_message.subject += PSTR("reboot event"); }
			}
			break;
	}

	if (os.mqtt.enabled() && strlen(topic) && strlen(payload))
		os.mqtt.publish(topic, payload);

	if (ifttt_enabled) {
		strcat_P(postval, PSTR("\"}"));

		BufferFiller bf = BufferFiller(ether_buffer, TMP_BUFFER_SIZE);
		bf.emit_p(PSTR("POST /trigger/sprinkler/with/key/$O HTTP/1.0\r\n"
						"Host: $S\r\n"
						"Accept: */*\r\n"
						"Content-Length: $D\r\n"
						"Content-Type: application/json\r\n\r\n$S"),
						SOPT_IFTTT_KEY, DEFAULT_IFTTT_URL, strlen(postval), postval);

		os.send_http_request(DEFAULT_IFTTT_URL, 80, ether_buffer, remote_http_callback);
	}

	if(email_enabled){
		email_message.message = strchr(postval, 'O'); // ad-hoc: remove the value1 part from the ifttt message
		#if defined(ARDUINO)
			#if defined(ESP8266)
				if(email_host && email_username && email_password && email_recipient) { // make sure all are valid
					EMailSender emailSend(email_username, email_password);
					emailSend.setSMTPServer(email_host);
					emailSend.setSMTPPort(email_port);
					EMailSender::Response resp = emailSend.send(email_recipient, email_message);
				}
			#endif
		#else
			struct smtp *smtp = NULL;
			String email_port_str = to_string(email_port);
			smtp_status_code rc;
			if(email_host && email_username && email_password && email_recipient) { // make sure all are valid
				rc = smtp_open(email_host, email_port_str.c_str(), SMTP_SECURITY_TLS, SMTP_NO_CERT_VERIFY, NULL, &smtp);
				rc = smtp_auth(smtp, SMTP_AUTH_PLAIN, email_username, email_password);
				rc = smtp_address_add(smtp, SMTP_ADDRESS_FROM, email_username, "OpenSprinkler");
				rc = smtp_address_add(smtp, SMTP_ADDRESS_TO, email_recipient, "User");
				rc = smtp_header_add(smtp, "Subject", email_message.subject.c_str());
				rc = smtp_mail(smtp, email_message.message.c_str());
				rc = smtp_close(smtp);
				if (rc!=SMTP_STATUS_OK) {
					DEBUG_PRINTF("SMTP: Error %s\n", smtp_status_code_errstr(rc));
				}
			}
		#endif
	}
}
