/* OpenSprinkler Unified Firmware
 * Copyright (C) 2015 by Ray Wang (ray@opensprinkler.com)
 *
 * Main loop
 * Feb 2015 @ OpenSprinkler.com
 *
 * This file is part of the OpenSprinkler Firmware
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

#include <limits.h>

#include "types.h"
#include "OpenSprinkler.h"
#include "core/program.h"
#include "services/weather.h"
#include "api/server.h"
#include "services/mqtt.h"
#include "core/scheduler.h"
#include "storage/logging.h"
#include "services/notifier.h"
#include "services/firmware_update.h"

#if defined(ESP8266)
	#include <Arduino.h>
	ESP8266WebServer *update_server = NULL;
	DNSServer *dns = NULL;
	ENC28J60lwIP enc28j60(PIN_ETHER_CS); // ENC28J60 lwip for wired Ether
	Wiznet5500lwIP w5500(PIN_ETHER_CS); // W5500 lwip for wired Ether
	lwipEth eth;
	bool useEth = false; // tracks whether we are using WiFi or wired Ether connection
	uint32_t getNtpTime();
#elif defined(ESP32)
	#include <Arduino.h>
	#include <WebServer.h>
	WebServer *update_server = NULL;
	DNSServer *dns = NULL;
	bool useEth = false;
	uint32_t getNtpTime();
#else // header and defs for RPI/Linux
	#include <dirent.h>
	#include <unistd.h>
	bool useEth = false;
#endif

OTF::OpenThingsFramework *otf = NULL;

#if defined(ARDUINO)
static uint16_t led_blink_ms = LED_FAST_BLINK;
#else
static uint16_t led_blink_ms = 0;
#endif

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

const char *user_agent_string = "OpenSprinkler/" TOSTRING(OS_FW_VERSION) "#" TOSTRING(OS_FW_MINOR);

// Small variations have been added to the timing values below
// to minimize conflicting events
#define NTP_SYNC_INTERVAL       86413L  // NTP sync interval (in seconds)
#define CHECK_NETWORK_INTERVAL  601     // Network checking timeout (in seconds)
#define CHECK_WEATHER_TIMEOUT   21613L  // Weather check interval (in seconds)
#define CHECK_WEATHER_SUCCESS_TIMEOUT 86400L // Weather check success interval (in seconds)
#define LCD_BACKLIGHT_TIMEOUT     15    // LCD backlight timeout (in seconds))
#define PING_TIMEOUT              200   // Ping test timeout (in ms)
#define UI_STATE_MACHINE_INTERVAL 50    // how often does ui_state_machine run (in ms)
#define CLIENT_READ_TIMEOUT       5     // client read timeout (in seconds)
#define DHCP_CHECKLEASE_INTERVAL  3600L // DHCP check lease interval (in seconds)
#define FLOWPOLL_INTERVAL         5     // flow poll interval (in milli-seconds)
#define CURRPOLL_INTERVAL         20    // current poll interval (in milli-seconds)
#define SENSORPOLL_INTERVAL       5000  // sensor poll interval (in milli-seconds)
// Define buffers: need them to be sufficiently large to cover string option reading
char ether_buffer[ETHER_BUFFER_ALLOC_SIZE]; // HTTP client send/receive buffer
char tmp_buffer[TMP_BUFFER_ALLOC_SIZE];     // scratch buffer

// ====== Object defines ======
OpenSprinkler os; // OpenSprinkler object
ProgramData pd;   // ProgramdData object
NotifQueue notif; // NotifQueue object

/* ====== Robert Hillman (RAH)'s implementation of flow sensor ======
 * flow_begin - time when valve turns on
 * flow_start - time when flow starts being measured (i.e. 2 mins after flow_begin approx
 * flow_stop - time when valve turns off (last rising edge pulse detected before off)
 * flow_gallons - total # of gallons+1 from flow_start to flow_stop
 * flow_last_gpm - last flow rate measured (averaged over flow_gallons) from last valve stopped (used to write to log file). */
uint32_t flow_begin, flow_start, flow_stop, flow_gallons, flow_rt_reset, last_flow_rt;
uint32_t flow_count = 0;
unsigned char prev_flow_state = HIGH;
float flow_last_gpm = 0;
int32_t flow_rt_period = -1;
uint32_t reboot_timer = 0;
unsigned char curr_alert_sid = 0;

void flow_poll() {
	uint32_t curr = millis();

	// Resets counter if timeout occurs
	if (flow_rt_reset && curr > flow_rt_reset) {
		os.flowcount_rt = 0;
		flow_rt_period = -1;
		flow_rt_reset = 0;
	}

	if (flow_rt_period < 0) {
		last_flow_rt = curr;
	}

	#if defined(ESP8266)
	if(os.hw_rev>=2) {
		pinMode(PIN_SENSOR1, INPUT); // Work-around for PIN_SENSOR1 on OS3.2 and above
		pinMode(PIN_SENSOR1, INPUT_PULLUP);
	}
	#endif


	unsigned char curr_flow_state = digitalReadExt(PIN_SENSOR1);
	if((!prev_flow_state) || curr_flow_state) { // only record on falling edge
		prev_flow_state = curr_flow_state;
		return;
	}
	prev_flow_state = curr_flow_state;
	flow_count++;

	/* RAH implementation of flow sensor */
	if (flow_start == 0) {
		flow_gallons = 0;
		flow_start = curr;
	} // if first pulse, record time

	if ((curr-flow_start)<90000) {
		flow_gallons=0;
	} // wait 90 seconds before recording flow_begin
	else {
		if (flow_gallons==1) {
			flow_begin = curr;
		}
	}

	// Use exponential moving average (alpha=0.2) if flow has been previosuly calculated, otherwise just set the value
	uint32_t curr_period = curr - last_flow_rt;
	if (flow_rt_period > 0) {
		flow_rt_period = (curr_period  / 5 + flow_rt_period * 4 / 5);
	} else {
		flow_rt_period = curr_period;
	}

	// calculates the flow rate scaled by the window size to simulated a fixed point number
	if (flow_rt_period > 0) {
		os.flowcount_rt = (uint32_t) (FLOWCOUNT_RT_WINDOW * 1000L / flow_rt_period);
		// Sets the timeout to be 10x the last period
		flow_rt_reset = curr + (curr - last_flow_rt) * 10;
	} else {
		os.flowcount_rt = 0;
		flow_rt_reset = 0;
	}

	last_flow_rt = curr;

	flow_stop = curr; // get time in ms for stop
	flow_gallons++;  // increment gallon count for each poll
	/* End of RAH implementation of flow sensor */
}

#if defined(USE_DISPLAY)
// ====== UI defines ======
static char ui_anim_chars[3] = {'.', 'o', 'O'};

#define UI_STATE_DEFAULT   0
#define UI_STATE_DISP_IP   1
#define UI_STATE_DISP_GW   2
#define UI_STATE_RUNPROG   3

static unsigned char ui_state = UI_STATE_DEFAULT;
static unsigned char ui_state_runprog = 0;

bool ui_confirm(PGM_P str) {
	os.lcd_print_line_clear_pgm(str, 0);
	os.lcd_print_line_clear_pgm(PSTR("(B1:No, B3:Yes)"), 1);
	unsigned char button;
	uint32_t start = millis();
	do {
		button = os.button_read(BUTTON_WAIT_NONE);
		if((button&BUTTON_MASK)==BUTTON_3 && (button&BUTTON_FLAG_DOWN)) return true;
		if((button&BUTTON_MASK)==BUTTON_1 && (button&BUTTON_FLAG_DOWN)) return false;
		delay(10);
	} while(millis() - start < 2500);
	return false;
}

void ui_state_machine() {
	// to avoid ui_state_machine taking too much computation time
	// we run it only every UI_STATE_MACHINE_INTERVAL ms
	static uint32_t last_usm = 0;
	if(millis() - last_usm <= UI_STATE_MACHINE_INTERVAL) { return; }
	last_usm = millis();

	// process screen led
	static uint32_t led_toggle_prev = 0;
	if(led_blink_ms) {
		uint32_t tm = millis();
		if(tm - led_toggle_prev > led_blink_ms) { // overflow proof timeout
			os.toggle_screen_led();
			led_toggle_prev = tm;
		}
	}

	if (!os.button_timeout) {
		os.lcd_set_brightness(0);
		ui_state = UI_STATE_DEFAULT;  // also recover to default state
	}

	// read button, if something is pressed, wait till release
	unsigned char button = os.button_read(BUTTON_WAIT_HOLD);

	if (button & BUTTON_FLAG_DOWN) {  // repond only to button down events
		os.button_timeout = LCD_BACKLIGHT_TIMEOUT;
		os.lcd_set_brightness(1);
	} else {
		return;
	}

	switch(ui_state) {
	case UI_STATE_DEFAULT:
		switch (button & BUTTON_MASK) {
		case BUTTON_1:
			if (button & BUTTON_FLAG_HOLD) {  // holding B1
				if (digitalReadExt(PIN_BUTTON_3)==0) { // if B3 is pressed while holding B1, run a short test (internal test)
					if(!ui_confirm(PSTR("Start 2s test?"))) {ui_state = UI_STATE_DEFAULT; break;}
					manual_start_program(255, 0, QUEUE_OPTION_REPLACE);
				} else if (digitalReadExt(PIN_BUTTON_2)==0) { // if B2 is pressed while holding B1, display gateway IP
					os.lcd.setAutoDisplay(false);
					os.lcd.clear(0, 1);
					os.lcd.setCursor(0, 0);
					#if defined(ESP8266)
						if (useEth) { os.lcd.print(eth.gatewayIP()); }
						else { os.lcd.print(WiFi.gatewayIP()); }
					#elif defined(ESP32)
						if (useEth) { os.lcd.print(ETH.gatewayIP()); }
						else { os.lcd.print(WiFi.gatewayIP()); }
					#else
						route_t route = get_route();
						char str[INET_ADDRSTRLEN];

						inet_ntop(AF_INET, &(route.gateway), str, INET_ADDRSTRLEN);
						os.lcd.print(str);
					#endif
					os.lcd.setCursor(0, 1);
					os.lcd_print_pgm(PSTR("(gwip)"));
					ui_state = UI_STATE_DISP_IP;
					os.lcd.display();
					os.lcd.setAutoDisplay(true);
				} else {  // if no other button is clicked, stop all zones
					if(!ui_confirm(PSTR("Stop all zones?"))) {ui_state = UI_STATE_DEFAULT; break;}
					reset_all_stations();
				}
			} else {  // clicking B1: display device IP and port
				os.lcd.setAutoDisplay(false);
				os.lcd.clear(0, 1);
				os.lcd.setCursor(0, 0);
				#if defined(ESP8266)
					if (useEth) { os.lcd.print(eth.localIP()); }
					else { os.lcd.print(WiFi.localIP()); }
				#elif defined(ESP32)
					if (useEth) { os.lcd.print(ETH.localIP()); }
					else { os.lcd.print(WiFi.localIP()); }
				#else
					route_t route = get_route();
					char str[INET_ADDRSTRLEN];
					in_addr_t ip = get_ip_address(route.iface);

					inet_ntop(AF_INET, &ip, str, INET_ADDRSTRLEN);
					os.lcd.print(str);
				#endif
				os.lcd.setCursor(0, 1);
				os.lcd_print_pgm(PSTR(":"));
				uint16_t httpport = (uint16_t)(os.iopts[IOPT_HTTPPORT_1]<<8) + (uint16_t)os.iopts[IOPT_HTTPPORT_0];
				os.lcd.print(httpport);
				os.lcd_print_pgm(PSTR(" (ip:port)"));
				os.lcd.setCursor(0, 2);
				os.lcd_print_pgm(PSTR("OTC:"));
				switch(otf->getCloudStatus()) {
					case OTF::NOT_ENABLED:
						os.lcd_print_pgm(PSTR(" not enabled"));
						break;
					case OTF::UNABLE_TO_CONNECT:
						os.lcd_print_pgm(PSTR("connecting.."));
						break;
					case OTF::DISCONNECTED:
						os.lcd_print_pgm(PSTR("disconnected"));
						break;
					case OTF::CONNECTED:
						os.lcd_print_pgm(PSTR(" Connected"));
						break;
				}

				ui_state = UI_STATE_DISP_IP;
				os.lcd.display();
				os.lcd.setAutoDisplay(true);
			}
			break;
		case BUTTON_2:
			if (button & BUTTON_FLAG_HOLD) {  // holding B2
				if (digitalReadExt(PIN_BUTTON_1)==0) { // if B1 is pressed while holding B2, display external IP
					os.lcd_print_ip((unsigned char*)(&os.nvdata.external_ip), 1);
					os.lcd.setCursor(0, 1);
					os.lcd_print_pgm(PSTR("(eip)"));
					ui_state = UI_STATE_DISP_IP;
				} else if (digitalReadExt(PIN_BUTTON_3)==0) {  // if B3 is pressed while holding B2, display last successful weather call
					//os.lcd.clear(0, 1);
					os.lcd_print_time(os.checkwt_success_lasttime);
					os.lcd.setCursor(0, 1);
					os.lcd_print_pgm(PSTR("(lswc)"));
					ui_state = UI_STATE_DISP_IP;
				} else {  // if no other button is clicked, reboot
					if(!ui_confirm(PSTR("Reboot device?"))) {ui_state = UI_STATE_DEFAULT; break;}
					os.reboot_dev(REBOOT_CAUSE_BUTTON);
				}
			} else {  // clicking B2: display MAC
				os.lcd.clear(0, 1);
				unsigned char mac[6];
				os.load_hardware_mac(mac, useEth);
				os.lcd_print_mac(mac);
				ui_state = UI_STATE_DISP_GW;
			}
			break;
		case BUTTON_3:
			if (button & BUTTON_FLAG_HOLD) {  // holding B3
				if (digitalReadExt(PIN_BUTTON_1)==0) {  // if B1 is pressed while holding B3, display up time
					os.lcd_print_time(os.powerup_lasttime);
					os.lcd.setCursor(0, 1);
					os.lcd_print_pgm(PSTR("(lupt) cause:"));
					os.lcd.print(os.last_reboot_cause);
					ui_state = UI_STATE_DISP_IP;
				} else if(digitalReadExt(PIN_BUTTON_2)==0) {  // if B2 is pressed while holding B3, reset to AP and reboot
					#if defined(ARDUINO)
					if(!ui_confirm(PSTR("Reset to AP?"))) {ui_state = UI_STATE_DEFAULT; break;}
					os.reset_to_ap();
					#endif
				} else {  // if no other button is clicked, go to Run Program main menu
					os.lcd_print_line_clear_pgm(PSTR("Run a Program:"), 0);
					os.lcd_print_line_clear_pgm(PSTR("Click B3 to list"), 1);
					ui_state = UI_STATE_RUNPROG;
				}
			} else {  // clicking B3: switch board display (cycle through master and all extension boards)
				os.status.display_board = (os.status.display_board + 1) % (os.nboards);
			}
			break;
		}
		break;
	case UI_STATE_DISP_IP:
	case UI_STATE_DISP_GW:
		ui_state = UI_STATE_DEFAULT;
		break;
	case UI_STATE_RUNPROG:
		if ((button & BUTTON_MASK)==BUTTON_3) {
			if (button & BUTTON_FLAG_HOLD) {
				// start
				manual_start_program(ui_state_runprog, 0, QUEUE_OPTION_INSERT_FRONT);
				ui_state = UI_STATE_DEFAULT;
			} else {
				ui_state_runprog = (ui_state_runprog+1) % (pd.nprograms+1);
				os.lcd_print_line_clear_pgm(PSTR("Hold B3 to start"), 0);
				if(ui_state_runprog > 0) {
					ProgramStruct prog;
					pd.read(ui_state_runprog-1, &prog);
					os.lcd_print_line_clear_pgm(PSTR(" "), 1);
					os.lcd.setCursor(0, 1);
					os.lcd.print((int)ui_state_runprog);
					os.lcd_print_pgm(PSTR(". "));
					os.lcd.print(prog.name);
				} else {
					os.lcd_print_line_clear_pgm(PSTR("0. Test (1 min)"), 1);
				}
			}
		}
		break;
	}
}
#endif


// ======================
// Setup Function
// ======================
#if defined(ARDUINO)
void do_setup() {
	/* Clear WDT reset flag. */
	WiFi.persistent(false);
	led_blink_ms = LED_FAST_BLINK;

	DEBUG_BEGIN(115200);
	DEBUG_PRINTLN(F("started"));

	os.begin();          // OpenSprinkler init
	os.options_setup();  // Setup options
#if defined(ARDUINO)
	os.setup_pd_voltage();
#endif

#if defined(USE_DISPLAY)
	os.lcd.clear();
	os.lcd.setCursor(0, 0);
	os.lcd_print_pgm(PSTR("Init sensors..."));
#endif
	Sensor::load_all();

	pd.init();           // ProgramData init

	// set time using RTC if it exists
	if(RTC.exists())	setTime(RTC.get());
	os.lcd_print_time(os.now_tz());  // display time to LCD
	os.powerup_lasttime = os.now_tz();

	if (os.start_network()) {  // initialize network
		os.status.network_fails = 0;
	} else {
		os.status.network_fails = 1;
	}

	os.status.req_network = 0;
	os.status.req_ntpsync = 1;

	os.mqtt.init();
	os.status.req_mqtt_restart = true;

	os.apply_all_station_bits(); // reset station bits

	// because at reboot we don't know if special stations
	// are in OFF state, here we explicitly turn them off
	for(unsigned char sid=0;sid<os.nstations;sid++) {
		os.switch_special_station(sid, 0);
	}

	os.button_timeout = LCD_BACKLIGHT_TIMEOUT;
}

#else
void initialize_otf();

void do_setup() {
	initialiseEpoch();   // initialize the Linux microsecond clock reference
	os.begin();          // OpenSprinkler init
	os.options_setup();  // Setup options

	Sensor::load_all();

	pd.init();           // ProgramData init

	if (os.start_network()) {  // initialize network
		DEBUG_PRINTLN("network established.");
		os.status.network_fails = 0;
	} else {
		DEBUG_PRINTLN("network failed.");
		os.status.network_fails = 1;
	}
	os.status.req_network = 0;

	// because at reboot we don't know if special stations
	// are in OFF state, here we explicitly turn them off
	for(unsigned char sid=0;sid<os.nstations;sid++) {
		os.switch_special_station(sid, 0);
	}

	os.mqtt.init();
	os.status.req_mqtt_restart = true;

	initialize_otf();
}

#endif

static void check_network();
void check_weather();
static bool process_special_program_command(const char*, uint32_t curr_time);
static void perform_ntp_sync();

#if defined(ARDUINO)
void start_server_ap();
void start_server_client();
#if defined(ESP8266)
static Ticker reboot_ticker;
#else
static uint32_t reboot_deadline = 0;
#endif

void reboot_in(uint32_t ms) {
	if(os.state != OS_STATE_WAIT_REBOOT) {
		os.state = OS_STATE_WAIT_REBOOT;
		DEBUG_PRINTLN(F("Prepare to restart..."));
		#if defined(ESP8266)
		reboot_ticker.once_ms(ms, ESP.restart);
		#else
		reboot_deadline = millis() + ms;
		#endif
	}
}
#else
void handle_web_request(char *p);
#endif

uint32_t currpoll_timeout = 0;
void overcurrent_monitor() {
#if defined(ARDUINO)
	// If a zone is turning on, do immediate overcurrent monitoring here for ~50ms
	if (curr_alert_sid) {
		int16_t imax = os.get_imax();
		if(imax > 0) { // disable overcurrent checking if imax==0
			imax += OVERCURRENT_INRUSH_EXTRA; // extra margin for inrush current
			time_os_t tn = os.now_tz();
			unsigned char sid = curr_alert_sid - 1;
			for(unsigned char i = 0; i < 10; i++) {
				uint16_t curr = os.read_current();
				if(curr > (uint16_t)imax) {
					turn_off_running_station_immediate(sid, tn);
					notif.add(NOTIFY_CURR_ALERT, sid, curr, CURR_ALERT_TYPE_OVER_STATION);
					os.status.overcurrent_sid = curr_alert_sid;
					currpoll_timeout += 1000; // delay currpoll_timeout by 1 second to give time for solenoid to reset
					break;
				} else {
					delay(5);
				}
			}
		}
		curr_alert_sid = 0;
	}
#endif
}

/** Main Loop */
void do_loop()
{
	static uint32_t flowpoll_timeout = 0;
	if(os.iopts[IOPT_SENSOR1_TYPE]==SENSOR_TYPE_FLOW) {
	// handle flow sensor using polling. Maximum freq is 1/(2*FLOWPOLL_INTERVAL)
	// e.g. if FLOWPOLL_INTERVAL is 3ms, maximum freq is 166Hz
		uint32_t tm = millis();
		if((int32_t)(tm-flowpoll_timeout) > 0) { // overflow proof timeout
			flowpoll_timeout = tm+FLOWPOLL_INTERVAL;
			flow_poll();
		}
	}

	{
		static uint32_t sensorpoll_timeout = 0;
		uint32_t tm = millis();
		if((int32_t)(tm-sensorpoll_timeout) > 0) {
			sensorpoll_timeout = tm + SENSORPOLL_INTERVAL;
			os.poll_sensors();
		}
	}

	#if defined(ARDUINO)
	{
		uint32_t tn = millis();
		if((int32_t)(tn-currpoll_timeout) > 0) { // overflow proof timeout
			int16_t curr = (int16_t)os.read_current();
			int16_t imax = os.get_imax();
			if((imax > 0) && (curr > imax)) {
				reset_all_stations_immediate(true);
				notif.add(NOTIFY_CURR_ALERT, 0, curr, CURR_ALERT_TYPE_OVER_SYSTEM);
				os.status.overcurrent_sid = 255; // 255 indicates system overcurrent
				currpoll_timeout = tn+1000; // pause currpoll for a second to give time for solenoids to reset
			} else {
				currpoll_timeout = tn+CURRPOLL_INTERVAL;
			}
		}
	}
#endif

	static time_os_t last_time = 0;
	static uint32_t last_minute = 0;

	unsigned char bid, sid, s, pid, qid, gid, bitvalue;
	ProgramStruct prog;

	os.status.mas = os.iopts[IOPT_MASTER_STATION];
	os.status.mas2= os.iopts[IOPT_MASTER_STATION_2];
	os.status.mas3= os.iopts[IOPT_MASTER_STATION_3];
	os.status.mas4= os.iopts[IOPT_MASTER_STATION_4];
	time_os_t curr_time = os.now_tz();

	// ====== Process Ethernet packets ======
	#if defined(ARDUINO)	// Process Ethernet packets for Arduino
	static uint32_t connecting_timeout;
	switch(os.state) {
	case OS_STATE_INITIAL:
		if(useEth) {
			led_blink_ms = 0;
			os.set_screen_led(LOW);
			os.lcd.clear();
			os.save_wifi_ip();
			start_server_client();
			os.state = OS_STATE_CONNECTED;
			connecting_timeout = 0;
		} else if(os.get_wifi_mode()==OS_WIFI_MODE_AP) {
			start_server_ap();
			dns->setErrorReplyCode(DNSReplyCode::NoError);
			dns->start(53, "*", WiFi.softAPIP());
			os.state = OS_STATE_CONNECTED;
			connecting_timeout = 0;
		} else {
			led_blink_ms = LED_SLOW_BLINK;
			#if defined(ESP32)
			if(WiFi.getMode()!=WIFI_STA) WiFi.mode(WIFI_STA);
			os.config_ip();
			#endif
			if(os.sopt_load(SOPT_STA_BSSID_CHL).length()>0 && os.wifi_channel<255) {
				start_network_sta(os.wifi_ssid.c_str(), os.wifi_pass.c_str(), (int32_t)os.wifi_channel, os.wifi_bssid);
			}
			else
				start_network_sta(os.wifi_ssid.c_str(), os.wifi_pass.c_str());
			#if defined(ESP8266)
			os.config_ip();
			#endif
			os.state = OS_STATE_CONNECTING;
			connecting_timeout = millis() + 120000L;
			os.lcd.setCursor(0, -1);
			os.lcd.print(F("Connecting to..."));
			os.lcd.setCursor(0, 2);
			os.lcd.print(os.wifi_ssid);
		}
		break;

	case OS_STATE_TRY_CONNECT:
		led_blink_ms = LED_SLOW_BLINK;
		#if defined(ESP32)
		if(WiFi.getMode()!=WIFI_AP_STA) WiFi.mode(WIFI_AP_STA);
		os.config_ip();
		#endif
		if(os.sopt_load(SOPT_STA_BSSID_CHL).length()>0 && os.wifi_channel<255) {
			start_network_sta_with_ap(os.wifi_ssid.c_str(), os.wifi_pass.c_str(), (int32_t)os.wifi_channel, os.wifi_bssid);
		}
		else
			start_network_sta_with_ap(os.wifi_ssid.c_str(), os.wifi_pass.c_str());
		#if defined(ESP8266)
		os.config_ip();
		#endif
		os.state = OS_STATE_CONNECTED;
		break;

	case OS_STATE_CONNECTING:
		if(WiFi.status() == WL_CONNECTED) {
			led_blink_ms = 0;
			os.set_screen_led(LOW);
			os.lcd.clear();
			os.save_wifi_ip();
			start_server_client();
			os.state = OS_STATE_CONNECTED;
			connecting_timeout = 0;
		} else {
			if((int32_t)((uint32_t)millis()-connecting_timeout)>0) {
				os.state = OS_STATE_INITIAL;
				#if defined(ESP32)
				WiFi.disconnect(false, false);
				#else
				WiFi.disconnect(true);
				#endif
				DEBUG_PRINTLN(F("timeout"));
			}
		}
		break;

	case OS_STATE_WAIT_REBOOT:
		if(dns) dns->processNextRequest();
		if(otf) otf->loop();
		if(update_server) update_server->handleClient();
		#if defined(ESP32)
		if(reboot_deadline && (int32_t)((uint32_t)millis() - reboot_deadline) >= 0) ESP.restart();
		#endif
		break;

	case OS_STATE_CONNECTED:
		if(os.get_wifi_mode() == OS_WIFI_MODE_AP) {
			dns->processNextRequest();
			if(update_server) update_server->handleClient();
			otf->loop();
			connecting_timeout = 0;
			if(os.get_wifi_mode()==OS_WIFI_MODE_STA) {
				// already in STA mode, waiting to reboot
				break;
			}
			if(WiFi.status()==WL_CONNECTED && WiFi.localIP() && reboot_timer!=0) {
				DEBUG_PRINTLN(F("STA connected, set up reboot timer"));
				reboot_timer = os.now_tz() + 10;
				//os.reboot_dev(REBOOT_CAUSE_WIFIDONE);
			}
		} else {
			if(useEth || WiFi.status() == WL_CONNECTED) {
				if(update_server) update_server->handleClient();
				otf->loop(os.network_connected());
				connecting_timeout = 0;
			} else {
				#if defined(ESP32)
				static uint32_t reconnect_at = 0;
				if ((int32_t)((uint32_t)millis() - reconnect_at) >= 0) {
					reconnect_at = millis() + 30000UL;
					os.state = OS_STATE_INITIAL;
				}
				#else
				// ESP8266 handles reconnection internally.
				#endif
			}
		}
		break;
	}

	firmware_update.loop();
	if (!firmware_update.busy() && firmware_update.phase() != FirmwareUpdatePhase::Success)
		ui_state_machine();

#else // Process Ethernet packets for RPI/LINUX
	if(otf) otf->loop();
#if defined(USE_DISPLAY)
	ui_state_machine();
#endif
#endif	// Process Ethernet packets

	// Start up MQTT when we have a network connection
	if (os.status.req_mqtt_restart && os.network_connected()) {
		DEBUG_PRINTLN(F("req_mqtt_restart"));
		os.mqtt.begin();
		os.status.req_mqtt_restart = false;
		os.mqtt.subscribe();
	}
	os.mqtt.loop();

	// The main control loop runs once every second
	if (curr_time != last_time) {

		#if defined(ESP8266)
		if(os.hw_rev>=2) {
			pinMode(PIN_SENSOR1, INPUT_PULLUP); // this seems necessary for OS 3.2
			pinMode(PIN_SENSOR2, INPUT_PULLUP);
		}
		#endif

		last_time = curr_time;
		if (os.button_timeout) os.button_timeout--;

#if defined(USE_DISPLAY)
		if (!ui_state)
			os.lcd_print_time(curr_time);  // print time
#endif

		// ====== Check raindelay status ======
		if (os.status.rain_delayed) {
			if (curr_time >= os.nvdata.rd_stop_time) {  // rain delay is over
				os.raindelay_stop();
			}
		} else {
			if (os.nvdata.rd_stop_time > curr_time) {  // rain delay starts now
				os.raindelay_start();
			}
		}

		// ====== Check controller status changes and write log ======
		if (os.old_status.rain_delayed != os.status.rain_delayed) {
			if (os.status.rain_delayed) {
				// rain delay started, record time
				os.raindelay_on_lasttime = curr_time;
				notif.add(NOTIFY_RAINDELAY, LOGDATA_RAINDELAY, 1);

			} else {
				// rain delay stopped, write log
				write_log(LOGDATA_RAINDELAY, curr_time);
				notif.add(NOTIFY_RAINDELAY, LOGDATA_RAINDELAY, 0);
			}
			os.old_status.rain_delayed = os.status.rain_delayed;
		}

		// ====== Check binary (i.e. rain or soil) sensor status ======
		os.detect_binarysensor_status(curr_time);

		// Sensor active-state change → log + notify, driven by lookup tables
		// instead of 4 duplicated blocks. Each sensor tracks its own
		// prev_active so we don't need a parallel old_status copy.
		for (uint8_t i = 0; i < NUM_SENSORS; i++) {
			if (!sensor_available(i)) continue;
			if (os.sn_sensors[i].prev_active == os.sn_sensors[i].active) continue;
			if (os.sn_sensors[i].active) {
				os.sn_sensors[i].active_lasttime = curr_time;
				notif.add(sensor_notif_bits[i], sensor_log_codes[i], 1);
			} else {
				write_log(sensor_log_codes[i], curr_time);
				notif.add(sensor_notif_bits[i], sensor_log_codes[i], 0);
			}
			os.sn_sensors[i].prev_active = os.sn_sensors[i].active;
		}

		// ===== Check program switch status =====
		unsigned char pswitch = os.detect_programswitch_status(curr_time);
		if(pswitch > 0) {
			reset_all_stations_immediate(); // immediately stop all stations
		}
		for (uint8_t i = 0; i < NUM_SENSORS; i++) {
			if ((pswitch & (1 << i)) && pd.nprograms > i) {
				manual_start_program(i + 1, 0, QUEUE_OPTION_INSERT_FRONT);
			}
		}

		// ====== Schedule program data ======
		uint32_t curr_minute = curr_time / 60;
		boolean match_found = false;
		RuntimeQueueStruct *q;
		// since the granularity of start time is minute
		// we only need to check once every minute
		if (curr_minute != last_minute) {
			last_minute = curr_minute;

			apply_monthly_adjustment(curr_time); // check and apply monthly adjustment here, if it's selected

			// check through all programs
			for(pid=0; pid<pd.nprograms; pid++) {
				pd.read(pid, &prog);	// todo future: reduce load time
				bool will_delete = false;
				unsigned char runcount = prog.check_match(curr_time, &will_delete);
				if(runcount>0) {
					const bool repeated_runonce =
						strncmp_P(prog.name, PSTR(RUNONCE_REPEAT_PREFIX), sizeof(RUNONCE_REPEAT_PREFIX) - 1) == 0;
					const unsigned char queue_pid = repeated_runonce ? RUNONCE_PID : pid + 1;
					const unsigned char notif_pid = repeated_runonce ? RUNONCE_PID : pid;
					// program match found
					unsigned char wl = get_program_water_percent(prog);
					float sensor_adj = get_program_sensor_adj(pid);

					// check and process special program command
					if(process_special_program_command(prog.name, curr_time))	continue;

					// get station ordering
					unsigned char order[os.nstations];
					prog.gen_station_runorder(runcount, order);

					// process all selected stations
					for(unsigned char oi=0;oi<os.nstations;oi++) {
						sid=order[oi];
						bid=sid>>3;
						s=sid&0x07;
						// skip if the station is a master station (because master cannot be scheduled independently
						if (os.is_master_station(sid))
							continue;

						// TODO: compare with old code
						uint32_t dur = prog.durations[sid];
						// if station has non-zero water time and the station is not disabled
						if (dur && !(os.attrib_dis[bid]&(1<<s))) {
							// water time is scaled by watering percentage
							uint32_t water_time = water_time_resolve(dur);

							water_time = water_time_scale(water_time, wl, sensor_adj);
							if (wl < 20 && water_time < 10) { // if water_percentage is less than 20% and water_time is less than 10 seconds, skip watering
								water_time = 0;
							}

							if (water_time) {
								// check if water time is still valid
								// because it may end up being zero after scaling
								q = pd.enqueue();
								if (q) {
									q->st = 0;
									q->dur = water_time;
									q->sid = sid;
									q->pid = queue_pid;
									match_found = true;
								} else {
									// queue is full
								}
							}// if water_time
						}// if prog.durations[sid]
					}// for sid
					if(match_found) {
						notif.add(NOTIFY_PROGRAM_SCHED, notif_pid, prog.use_weather?wl:100, 0, sensor_adj);
					} else {
						// program being skipped e.g. due to 0% watering level
						notif.add(NOTIFY_PROGRAM_SCHED, notif_pid, -1, wt_restricted);
					}
					//delete run-once if on final runtime (stations have already been queued)
					if(will_delete){
						pd.del(pid);
					}
				}// if check_match
			}// for pid

			// calculate start and end time
			if (match_found) {
				schedule_all_stations(curr_time);
			}
		}//if_check_current_minute

		// ====== Run program data ======
		// Check if a program is running currently
		// If so, do station run-time keeping
		if (os.status.program_busy){
			// first, go through run time queue to assign queue elements to stations
			q = pd.queue;
			qid=0;
			for(;q<pd.queue+pd.nqueue;q++,qid++) {
				sid=q->sid;
				unsigned char sqi=pd.station_qid[sid];
				// skip if station is already assigned a queue element
				// and that queue element has an earlier start time
				if(sqi<255 && pd.queue[sqi].st<q->st) continue;
				// otherwise assign the queue element to station
				pd.station_qid[sid]=qid;
			}
			// next, go through the stations and perform time keeping
			for(bid=0;bid<os.nboards; bid++) {
				bitvalue = os.station_bits[bid];
				for(s=0;s<8;s++) {
					unsigned char sid = bid*8+s;

					// skip master stations and any station that's not in the queue
					if (os.is_master_station(sid)) continue;
					if (pd.station_qid[sid]==255) continue;

					q = pd.queue + pd.station_qid[sid];

					// if current station is not running, check if we should turn it on
					if(!((bitvalue >> s) & 1)) {
						if (curr_time >= q->st && curr_time < q->st+q->dur) {
							turn_on_station(sid, q->st+q->dur-curr_time); // the last parameter is expected run time
						} //if curr_time > scheduled_start_time
					} // if current station is not running

					// check if this station should be turned off
					if (q->st > 0) {
						if (curr_time >= q->st+q->dur) {
							turn_off_station(sid, curr_time);
						}
					}
				}//end_s
			}//end_bid

			// finally, go through the queue again and clear up elements marked for removal
			int qi;
			for(qi=pd.nqueue-1;qi>=0;qi--) {
				q=pd.queue+qi;
				if(!q->dur || curr_time >= q->deque_time) {
					pd.dequeue(qi);
				}
			}

			// process dynamic events
			process_dynamic_events(curr_time);

			// activate / deactivate valves
			os.apply_all_station_bits(overcurrent_monitor);

			// check through runtime queue, calculate the last stop time of sequential stations
			memset(pd.last_seq_stop_times, 0, sizeof(uint32_t)*NUM_SEQ_GROUPS);
			time_os_t sst;
			unsigned char re=os.iopts[IOPT_REMOTE_EXT_MODE];
			q = pd.queue;
			for(;q<pd.queue+pd.nqueue;q++) {
				sid = q->sid;
				bid = sid>>3;
				s = sid&0x07;
				gid = os.get_station_gid(sid);
				// check if any sequential station has a valid stop time
				// and the stop time must be larger than curr_time
				sst = q->st + q->dur;
				if (sst>curr_time) {
					// only need to update last_seq_stop_time for sequential stations
					if (os.is_sequential_station(sid) && !re) {
						pd.last_seq_stop_times[gid] = (sst > pd.last_seq_stop_times[gid]) ? sst : pd.last_seq_stop_times[gid];
					}
				}
			}

			// if the runtime queue is empty
			// reset all stations
			if (!pd.nqueue) {
				// turn off all stations
				os.clear_all_station_bits();
				os.apply_all_station_bits();
				pd.reset_runtime(); // reset runtime
				os.status.program_busy = 0; // reset program busy bit
				pd.clear_pause(); // TODO: what if pause hasn't expired and a new program is scheduled to run?

				// log flow sensor reading if flow sensor is used
				if(os.iopts[IOPT_SENSOR1_TYPE]==SENSOR_TYPE_FLOW) {
					write_log(LOGDATA_FLOWSENSE, curr_time);
					notif.add(NOTIFY_FLOWSENSOR, (flow_count>os.flowcount_log_start)?(flow_count-os.flowcount_log_start):0);
				}

				// in case some options have changed while executing the program
				os.status.mas = os.iopts[IOPT_MASTER_STATION]; // update master station
				os.status.mas2= os.iopts[IOPT_MASTER_STATION_2]; // update master2 station
				os.status.mas3= os.iopts[IOPT_MASTER_STATION_3]; // update master3 station
				os.status.mas4= os.iopts[IOPT_MASTER_STATION_4]; // update master4 station
			}
		}//if_some_program_is_running

		// handle master
		for (unsigned char mas = MASTER_1; mas < NUM_MASTER_ZONES; mas++) {

			unsigned char mas_id = os.masters[mas][MASOPT_SID];

			if (mas_id) { // if this master station is set
				int16_t mas_on_adj = os.get_on_adj(mas);
				int16_t mas_off_adj = os.get_off_adj(mas);

				unsigned char masbit = 0;

				for(sid = 0; sid < os.nstations; sid++) {
					// skip if this is the master station
					if (mas_id == sid + 1) continue;

					if(pd.station_qid[sid]==255) continue; // skip if station is not in the queue

					q = pd.queue + pd.station_qid[sid];

					if (os.bound_to_master(q->sid, mas)) {
						// check if timing is within the acceptable range
						if (curr_time >= q->st + mas_on_adj &&
							curr_time <= q->st + q->dur + mas_off_adj) {
							masbit = 1;
							break;
						}
					}
				}

				os.set_station_bit(mas_id - 1, masbit);
			}
		}

		if (os.status.pause_state) {
			if (os.pause_timer > 0) {
				os.pause_timer--;
			} else {
				os.clear_all_station_bits();
				pd.clear_pause();
			}
		}
		// process dynamic events
		process_dynamic_events(curr_time);

		// handle master on / off notif events
		for (unsigned char mas = MASTER_1; mas < NUM_MASTER_ZONES; mas++) {
			unsigned char mas_id = os.masters[mas][MASOPT_SID];
			if (mas_id) { // if this master station is defined
				time_os_t laston = os.masters_last_on[mas];
				unsigned char masbit = os.get_station_bit(mas_id - 1);
				if(!laston && masbit) { // master is about to turn on
					notif.add(NOTIFY_STATION_ON, mas_id - 1, 0);
					os.masters_last_on[mas] = curr_time;
				}
				if(laston > 0 && !masbit) { // master is about to turn off
					notif.add(NOTIFY_STATION_OFF, mas_id - 1, (curr_time>laston) ? (curr_time-laston) : 0);
					os.masters_last_on[mas] = 0;
				}
			}
		}

		// activate/deactivate valves
		os.apply_all_station_bits(overcurrent_monitor);

#if defined(USE_DISPLAY)
		// process LCD display
		if (!ui_state) { os.lcd_print_screen(ui_anim_chars[(uint32_t)curr_time%3]); }
#endif

		// handle reboot request
		// check safe_reboot condition
		if (os.status.safe_reboot && (curr_time > reboot_timer)) {
			// if no program is running at the moment
			if (!os.status.program_busy) {
				// and if no program is scheduled to run in the next minute
				bool willrun = false;
				bool will_delete = false;
				for(pid=0; pid<pd.nprograms; pid++) {
					pd.read(pid, &prog);
					if(prog.check_match(curr_time+60, &will_delete)) {
						willrun = true;
						break;
					}
				}
				if (!willrun) {
					os.reboot_dev(os.nvdata.reboot_cause);
				}
			}
		} else if(reboot_timer && (curr_time > reboot_timer)) {
			os.reboot_dev(REBOOT_CAUSE_TIMER);
		}

		// perform ntp sync
		// instead of using curr_time, which may change due to NTP sync itself
		// we use Arduino's millis() method
		if (curr_time % NTP_SYNC_INTERVAL == 0) os.status.req_ntpsync = 1;
		//if((millis()/1000) % NTP_SYNC_INTERVAL==15) os.status.req_ntpsync = 1;
		perform_ntp_sync();

		// check network connection
		if (curr_time && (curr_time % CHECK_NETWORK_INTERVAL==0))  os.status.req_network = 1;
		check_network();

		// check weather
		check_weather();

		// process notifier events
		if(os.network_connected()) {
			notif.run();
		}

		if(os.weather_update_flag & WEATHER_UPDATE_WL) {
			// at the moment, we only send notification if water level changed
			// the other changes, such as sunrise, sunset changes are ignored for notification
			notif.add(NOTIFY_WEATHER_UPDATE, 0, os.iopts[IOPT_WATER_PERCENTAGE]);
			os.weather_update_flag = 0;
		}
		static unsigned char reboot_notification = 1;
		if(reboot_notification) {
			reboot_notification = 0;
			notif.add(NOTIFY_REBOOT);
		}
	}

		#if !defined(ARDUINO)
		delay(1); // For OSPI/LINUX, sleep 1 ms to minimize CPU usage
	#endif
}

/** Check and process special program command */
static bool process_special_program_command(const char* pname, uint32_t curr_time) {
	if(pname[0]==':') {	// special command start with :
		if(strncmp(pname, ":>reboot_now", 12) == 0) {
			os.status.safe_reboot = 0; // reboot regardless of program status
			reboot_timer = curr_time + 65; // set a timer to reboot in 65 seconds
			// this is to avoid the same command being executed again right after reboot
			return true;
		} else if(strncmp(pname, ":>reboot", 8) == 0) {
			os.status.safe_reboot = 1; // by default reboot should only happen when controller is idle
			reboot_timer = curr_time + 65; // set a timer to reboot in 65 seconds
			// this is to avoid the same command being executed again right after reboot
			return true;
		}
	}
	return false;
}

/** Make weather query */
void check_weather() {
	// do not check weather if
	// - network check has failed, or
	// - the controller is in remote extension mode
	if (os.status.network_fails>0 || os.iopts[IOPT_REMOTE_EXT_MODE]) return;
	if (os.status.program_busy) return;

	if (!os.network_connected()) return;

	time_os_t ntz = os.now_tz();
	if (os.checkwt_success_lasttime && (ntz > os.checkwt_success_lasttime + CHECK_WEATHER_SUCCESS_TIMEOUT)) {
		// if last successful weather call timestamp is more than allowed threshold
		// and if the selected adjustment method is not one of the manual methods
		// reset watering percentage to 100
		os.checkwt_success_lasttime = 0;
		unsigned char method = os.iopts[IOPT_USE_WEATHER];
		if(!(method==WEATHER_METHOD_MANUAL || method==WEATHER_METHOD_AUTORAINDELAY || method==WEATHER_METHOD_MONTHLY)) {
			os.iopts[IOPT_WATER_PERCENTAGE] = 100; // reset watering percentage to 100%
			wt_restricted = 0; // reset wt_rawData, errCode, and md_scales array
			wt_rawData[0] = 0;
			wt_errCode = HTTP_RQT_NOT_RECEIVED;
			md_N = 0;
		}
	} else if (!os.checkwt_lasttime || (ntz > os.checkwt_lasttime + CHECK_WEATHER_TIMEOUT)) {
		os.checkwt_lasttime = ntz;
		#if defined(USE_DISPLAY)
		if (!ui_state) {
			os.lcd_print_line_clear_pgm(PSTR("Check Weather..."),1);
		}
		#endif
		GetWeather();
	}
}

/** Perform network check
 * This function pings the router
 * to check if it's still online.
 * If not, it re-initializes Ethernet controller.
 */
static void check_network() {
	// TODO:
	// nothing to do for other platforms
}

/** Perform NTP sync */
static void perform_ntp_sync() {
	#if defined(ARDUINO)
	// do not perform ntp if this option is disabled, or if a program is currently running
	if (!os.iopts[IOPT_USE_NTP] || os.status.program_busy) return;
	// do not perform ntp if network is not connected
	if (!os.network_connected()) return;

	if (os.status.req_ntpsync) {
		os.status.req_ntpsync = 0;
		if (!ui_state) {
			os.lcd_print_line_clear_pgm(PSTR("NTP Syncing..."),1);
		}
		DEBUG_PRINTLN(F("NTP Syncing..."));
		static uint32_t last_ntp_result = 0;
		uint32_t t = getNtpTime();
		if(last_ntp_result>3 && t>last_ntp_result-3 && t<last_ntp_result+3) {
			DEBUG_PRINTLN(F("error: result too close to last"));
			t = 0;	// invalidate the result
		} else {
			last_ntp_result = t;
		}
		if (t>0) {
			setTime(t);
			RTC.set(t);
			DEBUG_PRINTLN(RTC.get());
		}
	}
#else
	// nothing to do here
	// Linux will do this for you
#endif
}

	#if !defined(ARDUINO) // main function for RPI/LINUX
int main(int argc, char *argv[]) {
	// Disable buffering to work with systemctl journal
	setvbuf(stdout, NULL, _IOLBF, 0);
	printf("Starting OpenSprinkler\n");

	int opt;
	while(-1 != (opt = getopt(argc, argv, "d:"))) {
		switch(opt) {
		case 'd':
			set_data_dir(optarg);
			break;
		default:
			// ignore options we don't understand
			break;
		}
	}

	do_setup();

	while(true) {
		do_loop();
	}
	return 0;
}
#endif
