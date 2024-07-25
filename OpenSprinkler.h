/* OpenSprinkler Unified (AVR/RPI/BBB/LINUX/ESP8266) Firmware
 * Copyright (C) 2015 by Ray Wang (ray@opensprinkler.com)
 *
 * OpenSprinkler library header file
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


#ifndef _OPENSPRINKLER_H
#define _OPENSPRINKLER_H

#include "types.h"
#include "defines.h"
#include "utils.h"
#include "gpio.h"
#include "images.h"
#include "mqtt.h"

#if defined(ARDUINO) // headers for Arduino
	#include <Arduino.h>
	#include <Wire.h>
	#include <SPI.h>
	#include "I2CRTC.h"

	#if defined(ESP8266) // for ESP8266
		#include <FS.h>
		#include <LittleFS.h>
		#include <ENC28J60lwIP.h>
		#include <W5500lwIP.h>
		#include <RCSwitch.h>
		#include <OpenThingsFramework.h>
		#include <DNSServer.h>
		#include <Ticker.h>
		#include "SSD1306Display.h"
		#include "espconnect.h"
		#include "EMailSender.h"
	#else // for AVR
		#include <SdFat.h>
		#include <Ethernet.h>
		#include "LiquidCrystal.h"
	#endif

#else // headers for RPI/BBB/LINUX
	#include <time.h>
	#include <string.h>
	#include <unistd.h>
	#include <netdb.h>
	#include <sys/stat.h>
	#include "OpenThingsFramework.h"
	#include "etherport.h"
	#include "smtp.h"
#endif // end of headers

#if defined(ARDUINO)
	#if defined(ESP8266)
	extern ESP8266WebServer *update_server;
	extern ENC28J60lwIP enc28j60;
	extern Wiznet5500lwIP w5500;
	struct lwipEth {
		bool isW5500 = false;
		inline boolean config(const IPAddress& local_ip, const IPAddress& arg1, const IPAddress& arg2, const IPAddress& arg3 = IPADDR_NONE, const IPAddress& dns2 = IPADDR_NONE) {
			return (isW5500)?w5500.config(local_ip, arg1, arg2, arg3, dns2) : enc28j60.config(local_ip, arg1, arg2, arg3, dns2);
		}
		inline boolean begin(const uint8_t *macAddress = nullptr) {
			return (isW5500)?w5500.begin(macAddress):enc28j60.begin(macAddress);
		}
		inline IPAddress localIP() {
			return (isW5500)?w5500.localIP():enc28j60.localIP();
		}
		inline IPAddress subnetMask() {
			return (isW5500)?w5500.subnetMask():enc28j60.subnetMask();
		}
		inline IPAddress gatewayIP() {
			return (isW5500)?w5500.gatewayIP():enc28j60.gatewayIP();
		}
		inline void setDefault() {
			(isW5500)?w5500.setDefault():enc28j60.setDefault();
		}
		inline bool connected() {
			return (isW5500)?w5500.connected():enc28j60.connected();
		}
		inline wl_status_t status() {
			return (isW5500)?w5500.status():enc28j60.status();
		}
	};
	extern lwipEth eth;
	#else
		// AVR specific
	#endif
	extern bool useEth;
#else
	// OSPI/Linux specific
#endif

#if defined(USE_OTF)
	extern OTF::OpenThingsFramework *otf;
#else
	extern EthernetServer *m_server;
#endif

/** Non-volatile data structure */
struct NVConData {
	uint16_t sunrise_time;  // sunrise time (in minutes)
	uint16_t sunset_time;   // sunset time (in minutes)
	uint32_t rd_stop_time;  // rain delay stop time
	uint32_t external_ip;   // external ip
	uint8_t  reboot_cause;  // reboot cause
};

struct StationAttrib {  // station attributes
	unsigned char mas:1;
	unsigned char igs:1;  // ignore sensor 1
	unsigned char mas2:1;
	unsigned char dis:1;
	unsigned char seq:1; // this bit is retired and replaced by sequential group id
	unsigned char igs2:1; // ignore sensor 2
	unsigned char igrd:1; // ignore rain delay
	unsigned char igpu:1; // todo: ignore pause

	unsigned char gid;    // sequential group id
	unsigned char reserved[2]; // reserved bytes for the future
}; // total is 4 bytes so far

/** Station data structure */
struct StationData {
	char name[STATION_NAME_SIZE];
	StationAttrib attrib;
	unsigned char type; // station type
	unsigned char sped[STATION_SPECIAL_DATA_SIZE]; // special station data
};

/** RF station data structures - Must fit in STATION_SPECIAL_DATA_SIZE */
struct RFStationData {
	unsigned char on[6];
	unsigned char off[6];
	unsigned char timing[4];
};

/** Remote station data structures - Must fit in STATION_SPECIAL_DATA_SIZE */
struct RemoteIPStationData {
	unsigned char ip[8];
	unsigned char port[4];
	unsigned char sid[2];
};

/** Remote OTC station data structures - Must fit in STATION_SPECIAL_DATA_SIZE */
struct RemoteOTCStationData {
	unsigned char token[DEFAULT_OTC_TOKEN_LENGTH+1];
	unsigned char sid[2];
};

/** GPIO station data structures - Must fit in STATION_SPECIAL_DATA_SIZE */
struct GPIOStationData {
	unsigned char pin[2];
	unsigned char active;
};

/** HTTP station data structures - Must fit in STATION_SPECIAL_DATA_SIZE */
struct HTTPStationData {
	unsigned char data[STATION_SPECIAL_DATA_SIZE];
};

/** Volatile controller status bits */
struct ConStatus {
	unsigned char enabled:1;         // operation enable (when set, controller operation is enabled)
	unsigned char rain_delayed:1;    // rain delay bit (when set, rain delay is applied)
	unsigned char sensor1:1;         // sensor1 status bit (when set, sensor1 on is detected)
	unsigned char program_busy:1;    // HIGH means a program is being executed currently
	unsigned char has_curr_sense:1;  // HIGH means the controller has a current sensing pin
	unsigned char safe_reboot:1;     // HIGH means a safe reboot has been marked
	unsigned char req_ntpsync:1;     // request ntpsync
	unsigned char req_network:1;     // request check network
	unsigned char display_board:5;   // the board that is being displayed onto the lcd
	unsigned char network_fails:3;   // number of network fails
	unsigned char mas:8;             // master station index
	unsigned char mas2:8;            // master2 station index
	unsigned char sensor2:1;         // sensor2 status bit (when set, sensor2 on is detected)
	unsigned char sensor1_active:1;  // sensor1 active bit (when set, sensor1 is activated)
	unsigned char sensor2_active:1;  // sensor2 active bit (when set, sensor2 is activated)
	unsigned char req_mqtt_restart:1;// request mqtt restart
	unsigned char pause_state:1;     // pause station runs
};

/** OTF configuration */
struct OTCConfig {
	unsigned char en;
	String token;
	String server;
	uint32_t port;
};

extern const char iopt_json_names[];
extern const uint8_t iopt_max[];

class OpenSprinkler {
public:

	// data members
#if defined(ESP8266)
	static SSD1306Display lcd;  // 128x64 OLED display
#elif defined(ARDUINO)
	static LiquidCrystal lcd;   // 16x2 character LCD
#else
	// todo: LCD define for RPI/BBB
#endif

#if defined(OSPI)
	static unsigned char pin_sr_data;  // RPi shift register data pin to handle RPi rev. 1
#endif

	static OSMqtt mqtt;

	static NVConData nvdata;
	static ConStatus status;
	static ConStatus old_status;
	static unsigned char nboards, nstations;
	static unsigned char hw_type;  // hardware type
	static unsigned char hw_rev;   // hardware minor

	static unsigned char iopts[]; // integer options
	static const char*sopts[]; // string options
	static unsigned char station_bits[];     // station activation bits. each byte corresponds to a board (8 stations)
																	// first byte-> master controller, second byte-> ext. board 1, and so on
	// todo future: the following attribute bytes are for backward compatibility
	static unsigned char attrib_mas[];
	static unsigned char attrib_igs[];
	static unsigned char attrib_mas2[];
	static unsigned char attrib_igs2[];
	static unsigned char attrib_igrd[];
	static unsigned char attrib_dis[];
	static unsigned char attrib_spe[];
	static unsigned char attrib_grp[];
	static unsigned char masters[NUM_MASTER_ZONES][NUM_MASTER_OPTS];

	// variables for time keeping
	static time_os_t sensor1_on_timer;  // time when sensor1 is detected on last time
	static time_os_t sensor1_off_timer; // time when sensor1 is detected off last time
	static time_os_t sensor1_active_lasttime; // most recent time sensor1 is activated
	static time_os_t sensor2_on_timer;  // time when sensor2 is detected on last time
	static time_os_t sensor2_off_timer; // time when sensor2 is detected off last time
	static time_os_t sensor2_active_lasttime; // most recent time sensor1 is activated
	static time_os_t raindelay_on_lasttime;  // time when the most recent rain delay started
	static ulong pause_timer; // count down timer in paused state
	static ulong flowcount_rt;     // flow count (for computing real-time flow rate)
	static ulong flowcount_log_start; // starting flow count (for logging)

	static unsigned char  button_timeout;    // button timeout
	static time_os_t checkwt_lasttime;  // time when weather was checked
	static time_os_t checkwt_success_lasttime; // time when weather check was successful
	static time_os_t powerup_lasttime;  // time when controller is powered up most recently
	static uint8_t last_reboot_cause;  // last reboot cause
	static unsigned char  weather_update_flag;
	// member functions
	// -- setup
	static void update_dev();  // update software for Linux instances
	static void reboot_dev(uint8_t);  // reboot the microcontroller
	static void begin();  // initialization, must call this function before calling other functions
	static unsigned char start_network();  // initialize network with the given mac and port
	static unsigned char start_ether();  // initialize ethernet with the given mac and port
	static bool network_connected();  // check if the network is up
	static bool load_hardware_mac(unsigned char* buffer, bool wired=false);  // read hardware mac address
	static time_os_t now_tz();
	// -- station names and attributes
	static void get_station_data(unsigned char sid, StationData* data); // get station data
	static void set_station_data(unsigned char sid, StationData* data); // set station data
	static void get_station_name(unsigned char sid, char buf[]); // get station name
	static void set_station_name(unsigned char sid, char buf[]); // set station name
	static unsigned char get_station_type(unsigned char sid); // get station type
	static unsigned char is_sequential_station(unsigned char sid);
	static unsigned char is_master_station(unsigned char sid);
	static unsigned char bound_to_master(unsigned char sid, unsigned char mas);
	static unsigned char get_master_id(unsigned char mas);
	static int16_t get_on_adj(unsigned char mas);
	static int16_t get_off_adj(unsigned char mas);
	static unsigned char is_running(unsigned char sid);
	static unsigned char get_station_gid(unsigned char sid);
	static void set_station_gid(unsigned char sid, unsigned char gid);

	//static StationAttrib get_station_attrib(unsigned char sid); // get station attribute
	static void attribs_save(); // repackage attrib bits and save (backward compatibility)
	static void attribs_load(); // load and repackage attrib bits (backward compatibility)
	static uint16_t parse_rfstation_code(RFStationData *data, ulong *on, ulong *off); // parse rf code into on/off/time sections
	static void switch_rfstation(RFStationData *data, bool turnon);  // switch rf station
	static void switch_remotestation(RemoteIPStationData *data, bool turnon, uint16_t dur=0); // switch remote IP station
	static void switch_remotestation(RemoteOTCStationData *data, bool turnon, uint16_t dur=0); // switch remote OTC station
	static void switch_gpiostation(GPIOStationData *data, bool turnon); // switch gpio station
	static void switch_httpstation(HTTPStationData *data, bool turnon, bool usessl=false); // switch http station
	
	// -- options and data storeage
	static void nvdata_load();
	static void nvdata_save();

	static void options_setup();
	static void pre_factory_reset();
	static void factory_reset();
	static void iopts_load();
	static void iopts_save();
	static bool sopt_save(unsigned char oid, const char *buf);
	static void sopt_load(unsigned char oid, char *buf, uint16_t maxlen=MAX_SOPTS_SIZE);
	static String sopt_load(unsigned char oid);
	static void populate_master();
	static unsigned char password_verify(const char *pw);  // verify password

	// -- controller operation
	static void enable();   // enable controller operation
	static void disable();  // disable controller operation, all stations will be closed immediately
	static void raindelay_start();  // start raindelay
	static void raindelay_stop();   // stop rain delay
	static void detect_binarysensor_status(time_os_t curr_time);// update binary (rain, soil) sensor status
	static unsigned char detect_programswitch_status(time_os_t curr_time); // get program switch status
	static void sensor_resetall();

	static uint16_t read_current(); // read current sensing value
	static uint16_t baseline_current; // resting state current

	static int detect_exp();      // detect the number of expansion boards
	static unsigned char weekday_today();  // returns index of today's weekday (Monday is 0)

	static unsigned char set_station_bit(unsigned char sid, unsigned char value, uint16_t dur=0); // set station bit of one station (sid->station index, value->0/1)
	static unsigned char get_station_bit(unsigned char sid); // get station bit of one station (sid->station index)
	static void switch_special_station(unsigned char sid, unsigned char value, uint16_t dur=0); // swtich special station
	static void clear_all_station_bits(); // clear all station bits
	static void apply_all_station_bits(); // apply all station bits (activate/deactive values)

	static int8_t send_http_request(uint32_t ip4, uint16_t port, char* p, void(*callback)(char*)=NULL, bool usessl=false, uint16_t timeout=5000);
	static int8_t send_http_request(const char* server, uint16_t port, char* p, void(*callback)(char*)=NULL, bool usessl=false, uint16_t timeout=5000);
	static int8_t send_http_request(char* server_with_port, char* p, void(*callback)(char*)=NULL, bool usessl=false, uint16_t timeout=5000);
	
	#if defined(USE_OTF)
	static OTCConfig otc;
	#endif

	// -- LCD functions
#if defined(ARDUINO) // LCD functions for Arduino
	#if defined(ESP8266)
	static void lcd_print_pgm(PGM_P str); // ESP8266 does not allow PGM_P followed by PROGMEM
	static void lcd_print_line_clear_pgm(PGM_P str, unsigned char line);
	#else
	static void lcd_print_pgm(PGM_P PROGMEM str);  // print a program memory string
	static void lcd_print_line_clear_pgm(PGM_P PROGMEM str, unsigned char line);
	#endif
	static void lcd_print_time(time_os_t t);  // print current time
	static void lcd_print_ip(const unsigned char *ip, unsigned char endian);  // print ip
	static void lcd_print_mac(const unsigned char *mac);  // print mac
	static void lcd_print_screen(char c);  // print station bits of the board selected by display_board
	static void lcd_print_version(unsigned char v);  // print version number

	static String time2str(uint32_t t) {
		uint16_t h = hour(t);
		uint16_t m = minute(t);
		uint16_t s = second(t);
		String str = "";
		str+=h/10;
		str+=h%10;
		str+=":";
		str+=m/10;
		str+=m%10;
		str+=":";
		str+=s/10;
		str+=s%10;
		return str;
	}
	// -- UI and buttons
	static unsigned char button_read(unsigned char waitmode); // Read button value. options for 'waitmodes' are:
																					// BUTTON_WAIT_NONE, BUTTON_WAIT_RELEASE, BUTTON_WAIT_HOLD
																					// return values are 'OR'ed with flags
																					// check defines.h for details

	// -- UI functions --
	static void ui_set_options(int oid);		// ui for setting options (oid-> starting option index)
	static void lcd_set_brightness(unsigned char value=1);
	static void lcd_set_contrast();

	#if defined(ESP8266)
	static IOEXP *mainio, *drio;
	static IOEXP *expanders[];
	static RCSwitch rfswitch;
	static void detect_expanders();
	static void flash_screen();
	static void toggle_screen_led();
	static void set_screen_led(unsigned char status);
	static unsigned char get_wifi_mode() { if (useEth) return WIFI_MODE_STA; else return wifi_testmode ? WIFI_MODE_STA : iopts[IOPT_WIFI_MODE];}
	static unsigned char wifi_testmode;
	static String wifi_ssid, wifi_pass;
	static unsigned char wifi_bssid[6], wifi_channel;
	static void config_ip();
	static void save_wifi_ip();
	static void reset_to_ap();
	static unsigned char state;
	#endif

private:
	static void lcd_print_option(int i);  // print an option to the lcd
	static void lcd_print_2digit(int v);  // print a integer in 2 digits
	static void lcd_start();
	static unsigned char button_read_busy(unsigned char pin_butt, unsigned char waitmode, unsigned char butt, unsigned char is_holding);

	#if defined(ESP8266)
	static void latch_boost();
	static void latch_open(unsigned char sid);
	static void latch_close(unsigned char sid);
	static void latch_setzonepin(unsigned char sid, unsigned char value);
	static void latch_setallzonepins(unsigned char value);
	static void latch_disable_alloutputs_v2();
	static void latch_setzoneoutput_v2(unsigned char sid, unsigned char A, unsigned char K);
	static void latch_apply_all_station_bits();
	static unsigned char prev_station_bits[];
	#endif
#endif // LCD functions
	static unsigned char engage_booster;

	#if defined(USE_OTF)
	static void parse_otc_config();
	#endif
};

#endif  // _OPENSPRINKLER_H
