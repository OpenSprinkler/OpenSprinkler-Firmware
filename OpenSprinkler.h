/* OpenSprinkler Unified Firmware
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

#pragma once

#include "types.h"
#include "defines.h"
#include "util/utils.h"
#include "platform/gpio.h"
#include "drivers/images.h"
#include "services/mqtt.h"
#include "drivers/rf_switch.h"
#include <cmath>
#include <new>

#if defined(ARDUINO)
	#include <Arduino.h>
	#include <Wire.h>
	#include <SPI.h>
	#include "drivers/i2c_rtc.h"

	#include <FS.h>
	#include <LittleFS.h>
	#include <OpenThingsFramework.h>
	#include <DNSServer.h>
	#include "services/espconnect.h"
	#include "drivers/ch224.h"
	#if defined(ESP8266)
		#include <ESP8266WebServer.h>
		#include <ENC28J60lwIP.h>
		#include <W5500lwIP.h>
		#include <Ticker.h>
		#include "services/EMailSender.h"
	#elif defined(ESP32)
		#include <WiFi.h>
		#include <ETH.h>
	#endif

#else // headers for RPI/LINUX
	#include <time.h>
	#include <string.h>
	#include <unistd.h>
	#include <netdb.h>
	#include <sys/stat.h>
	#include "OpenThingsFramework.h"
	#include "etherport.h"
	#include "platform/clock.h"
	#include "services/smtp.h"
#endif // end of headers

#if defined(USE_DISPLAY)
	#include "drivers/ssd1306_display.h"
#endif

#include "sensors/sensor.h"
#include "sensors/aggregate_sensor.h"
#include "sensors/weather_sensor.h"
#include "sensors/system_internal_sensor.h"
#include "sensors/onboard_digital_sensor.h"
#include "drivers/ads1115.h"
#include "sensors/ads1115_sensor.h"

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
		inline IPAddress dnsIP() {
			return IPAddress(dns_getserver(0));
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
	extern bool useEth;
#elif defined(ESP32)
	class WebServer;
	extern WebServer *update_server;
	extern bool useEth;
#else
	// OSPI/Linux specific
#endif

extern OTF::OpenThingsFramework *otf;

/** Non-volatile data structure */
struct NVConData {
	uint16_t sunrise_time;       // sunrise time (in minutes)
	uint16_t sunset_time;        // sunset time (in minutes)
	uint32_t rd_stop_time;       // rain delay stop time
	uint32_t external_ip;        // external ip
	uint8_t  reboot_cause;       // reboot cause
	uint16_t last_sensor_uuid;   // counter for sensor UUID generation; next sensor gets ++this
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
	unsigned char mas3:1; // master 3 binding bit (was reserved[0])
	unsigned char mas4:1; // master 4 binding bit
	unsigned char igs3:1; // ignore sensor 3
	unsigned char igs4:1; // ignore sensor 4
	unsigned char :4;     // remaining bits of this byte, reserved
	unsigned char reserved; // reserved for future use (was reserved[1])
}; // total is 4 bytes

/** Station data structure */
struct StationData {
	char name[STATION_NAME_SIZE];
	StationAttrib attrib;
	unsigned char type; // station type
	unsigned char sped[STATION_SPECIAL_DATA_SIZE]; // special station data
};

/** RF station data structures - Must fit in STATION_SPECIAL_DATA_SIZE */
struct RFStationData {
	unsigned char version;
	unsigned char on[8];
	unsigned char off[8];
	unsigned char timing[4];
	unsigned char protocol[2];
	unsigned char bitlength[2];
};

struct RFStationCode {
	uint32_t on;
	uint32_t off;
	uint16_t timing;
	uint8_t protocol;
	uint8_t bitlength;
};

struct RFStationDataClassic {
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

// ========================================================================
// Sensor framework (binary sensors SN1-SN4)
// ========================================================================
// Per-sensor state. For sensor 1, type may also be SENSOR_TYPE_FLOW (handled
// outside this struct via flow_count / flow ISR). For sensors 2-4 the type
// is restricted to rain/soil/program switch.
#define NUM_SENSORS 4

struct SensorState {
	time_os_t on_timer;            // when raw input went on; 0 means inactive
	time_os_t off_timer;           // when raw input went off; 0 means inactive
	time_os_t active_lasttime;     // most recent time the sensor became active
	uint8_t   raw         : 1;     // current raw debounced input (post-polarity)
	uint8_t   active      : 1;     // current debounced active state
	uint8_t   prev_active : 1;     // last-cycle active (for state-change detection)
};

// Per-sensor IOPT key lookup. PROGMEM in OpenSprinkler.cpp.
struct SensorIoptKeys {
	uint8_t type, option, on_delay, off_delay;
};
extern const SensorIoptKeys sensor_iopt_keys[NUM_SENSORS];
extern const uint16_t sensor_notif_bits[NUM_SENSORS];   // NOTIFY_SENSOR1..4 bits
extern const uint8_t  sensor_log_codes[NUM_SENSORS];    // LOGDATA_SENSOR1..4 codes

// Helper accessors for sensor metadata. These index iopts[] which is RAM, so
// they're plain inline reads (no pgm_read_byte needed for the iopts side).
unsigned char sensor_pin(uint8_t i);  // implemented in OpenSprinkler.cpp
unsigned char sensor_pullup_pin(uint8_t i);
bool sensor_available(uint8_t i);     // true if the physical SN input exists
int8_t sensor_index_from_log_code(uint8_t type);

/** Volatile controller status bits */
struct ConStatus {
	unsigned char enabled:1;         // operation enable (when set, controller operation is enabled)
	unsigned char rain_delayed:1;    // rain delay bit (when set, rain delay is applied)
	unsigned char program_busy:1;    // HIGH means a program is being executed currently
	unsigned char has_curr_sense:1;  // HIGH means the controller has a current sensing pin
	unsigned char safe_reboot:1;     // HIGH means a safe reboot has been marked
	unsigned char req_ntpsync:1;     // request ntpsync
	unsigned char req_network:1;     // request check network
	unsigned char display_board:5;   // the board that is being displayed onto the lcd
	unsigned char network_fails:3;   // number of network fails
	unsigned char mas:8;             // master station index
	unsigned char mas2:8;            // master2 station index
	unsigned char mas3:8;            // master3 station index
	unsigned char mas4:8;            // master4 station index
	unsigned char req_mqtt_restart:1;// request mqtt restart
	unsigned char pause_state:1;     // pause station runs
	unsigned char overcurrent_sid:8; // overcurrent sid (0: no overcurrent; 1~254: overcurrent caused by opening zone; 255: system overcurrent)
	// Sensor raw/active state lives in OpenSprinkler::sn_sensors[] (per-sensor SensorState).
};

/** OTF configuration */
struct OTCConfig {
	unsigned char en;
	String token;
	String server;
	uint32_t port;
};

// ========================================================================
// IOPT metadata table
// ========================================================================
// Per-option metadata flags. The flags byte is reserved for future expansion.
#define IOPT_FLAG_RETIRED      0x01  // skipped in /jo, /co, and LCD edit
#define IOPT_FLAG_SIGNED_TIME  0x02  // value uses water_time_encode_signed
#define IOPT_FLAG_READ_ONLY    0x04  // /co rejects writes; reads pass through
#define IOPT_FLAG_HIDDEN_API   0x08  // omitted from /jo (still editable on LCD)

// Per-option flash-resident metadata. One entry per IOPT_* in enum order.
struct IOptDef {
	char json[6];        // JSON name, up to 5 chars plus NUL
	uint8_t max_val;     // permitted maximum (also used by LCD edit clamp)
	uint8_t def_val;     // factory-default value
	uint8_t flags;       // IOPT_FLAG_*
	char prompt[17];     // LCD prompt, up to 16 chars plus NUL
};                       // flash-resident only

extern const IOptDef iopt_defs[NUM_IOPTS] PROGMEM;

// Accessors that hide PROGMEM reads.
uint8_t iopt_get_max(uint8_t oid);
uint8_t iopt_get_def(uint8_t oid);
uint8_t iopt_get_flags(uint8_t oid);
void iopt_get_json_name(uint8_t oid, char *buf);  // buf size >= 6
void iopt_get_prompt(uint8_t oid, char *buf);     // buf size >= 17

class OpenSprinkler {
public:

	// data members
#if defined(USE_DISPLAY)
	static SSD1306Display lcd;  // 128x64 OLED display
#endif

	static ADS1115 *ads1115_devices[4];

	// True if at least one ADS1115 chip was detected at boot (or always true on
	// DEMO/SIM where the mock backend is unconditionally instantiated).
	static bool has_ads1115();

	union SensorUnion {
		ADS1115Sensor ads1115;
		AggregateSensor aggregate;
		WeatherSensor weather;
		SystemInternalSensor system_internal;
		OnboardDigitalSensor onboard_digital;
	};
	static sensor_memory_t sensors[MAX_SENSORS];

#if defined(OSPI)
	static unsigned char pin_sr_data;  // RPi shift register data pin to handle RPi rev. 1
#endif

	static OSMqtt mqtt;

	static NVConData nvdata;
	static ConStatus status;
	static ConStatus old_status;
	static unsigned char nboards, nstations, nsensors;
	static unsigned char hw_type;  // hardware type
	static unsigned char hw_rev;   // hardware minor

	static unsigned char iopts[]; // integer options
	static const char*sopts[]; // string options
	static unsigned char station_bits[];     // station activation bits. each byte corresponds to a board (8 stations)
																	// first byte-> master controller, second byte-> ext. board 1, and so on
	// Note: the following attribute bytes are for backward compatibility
	static unsigned char attrib_mas[];
	static unsigned char attrib_mas2[];
	static unsigned char attrib_mas3[];
	static unsigned char attrib_mas4[];
	// Per-sensor per-board ignore mask. attrib_igs[i] is for sensor i+1.
	static unsigned char attrib_igs[NUM_SENSORS][MAX_NUM_BOARDS];
	static unsigned char attrib_igrd[];
	static unsigned char attrib_dis[];
	static unsigned char attrib_spe[];
	static unsigned char attrib_grp[];
	static unsigned char masters[NUM_MASTER_ZONES][NUM_MASTER_OPTS];
	static time_os_t masters_last_on[NUM_MASTER_ZONES];

	// Per-sensor state (timers + raw/active bits). Replaces the 12 separate
	// per-sensor timers and the 8 per-sensor bit fields that used to live in
	// ConStatus. Access via os.sn_sensors[i].active / .raw / .on_timer / etc.
	static SensorState sn_sensors[NUM_SENSORS];

	// variables for time keeping
	static time_os_t raindelay_on_lasttime;  // time when the most recent rain delay started
	static uint32_t pause_timer; // count down timer in paused state
	static uint32_t flowcount_rt;     // flow count (for computing real-time flow rate)
	static uint32_t flowcount_log_start; // starting flow count (for logging)

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
	static int16_t get_imin();
	static int16_t get_imax();
	static unsigned char is_running(unsigned char sid);
	static unsigned char get_station_gid(unsigned char sid);
	static void set_station_gid(unsigned char sid, unsigned char gid);

	//static StationAttrib get_station_attrib(unsigned char sid); // get station attribute
	static void attribs_save(); // repackage attrib bits and save (backward compatibility)
	static void attribs_load(); // load and repackage attrib bits (backward compatibility)
	static bool parse_rfstation_code(RFStationData *data, RFStationCode *code); // parse rf code into on/off/time sections
	static void switch_rfstation(RFStationData *data, bool turnon);  // switch rf station
	static void switch_remotestation(RemoteIPStationData *data, bool turnon, uint32_t dur=0); // switch remote IP station
	static void switch_remotestation(RemoteOTCStationData *data, bool turnon, uint32_t dur=0); // switch remote OTC station
	static void switch_gpiostation(GPIOStationData *data, bool turnon); // switch gpio station
	static void switch_httpstation(HTTPStationData *data, bool turnon, bool usessl=false); // switch http station

	// -- options and data storeage
	static void nvdata_load();
	static void nvdata_save();

	static void options_setup();
	static void pre_factory_reset();
	static void factory_reset();
	static void load_iopt_defaults();   // populate iopts[] from iopt_defs[].def_val
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

	static uint16_t read_current(bool use_ema=false); // read current sensing value. use_ema uses exponential moving average for filtering
	static uint16_t baseline_current; // resting state current

	static int detect_exp();      // detect the number of expansion boards
	static unsigned char weekday_today();  // returns index of today's weekday (Monday is 0)

	static unsigned char set_station_bit(unsigned char sid, unsigned char value, uint32_t dur=0); // set station bit of one station (sid->station index, value->0/1)
	static unsigned char get_station_bit(unsigned char sid); // get station bit of one station (sid->station index)
	static void switch_special_station(unsigned char sid, unsigned char value, uint32_t dur=0); // swtich special station
	static void clear_all_station_bits(); // clear all station bits
	static void apply_all_station_bits(void (*post_activation_callback)()=NULL); // apply all station bits (activate/deactive values)

	static int8_t send_http_request(uint32_t ip4, uint16_t port, char* p, void(*callback)(char*)=NULL, bool usessl=false, uint16_t timeout=5000);
	static int8_t send_http_request(const char* server, uint16_t port, char* p, void(*callback)(char*)=NULL, bool usessl=false, uint16_t timeout=5000);
	static int8_t send_http_request(char* server_with_port, char* p, void(*callback)(char*)=NULL, bool usessl=false, uint16_t timeout=5000);

	static OTCConfig otc;

	// -- Sensor functions
    void log_sensor(uint8_t sid, float value);
    static void poll_sensors();
    static float get_sensor_weather_data(WeatherAction action);
	// -- LCD functions
#if defined(USE_DISPLAY)
	static void lcd_print_time(time_os_t t);  // print current time
	static void lcd_print_ip(const unsigned char *ip, unsigned char endian);  // print ip
	static void lcd_print_mac(const unsigned char *mac);  // print mac
	static void lcd_print_update(const char *message, int16_t percent); // percent < 0 means indeterminate
	static void lcd_print_screen(char c);  // print station bits of the board selected by display_board
	static void lcd_print_version(unsigned char v);  // print version number
	static void lcd_set_brightness(unsigned char value=1);
	static void lcd_set_contrast();
	static void flash_screen();
	static void toggle_screen_led();
	static void set_screen_led(unsigned char status);

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
#endif

#if defined(ARDUINO) // LCD functions for Arduino
	static void lcd_print_pgm(PGM_P str); // ESP8266 does not allow PGM_P followed by PROGMEM
	static void lcd_print_line_clear_pgm(PGM_P str, unsigned char line);

	static IOEXP *drio;
	static IOEXP *expanders[];
	static CH224 usbpd;
	static uint8_t actual_pd_voltage;

	static void detect_expanders();
	static unsigned char get_wifi_mode() { if (useEth) return OS_WIFI_MODE_STA; else return wifi_testmode ? OS_WIFI_MODE_STA : iopts[IOPT_WIFI_MODE];}
	static unsigned char wifi_testmode;
	static String wifi_ssid, wifi_pass;
	static unsigned char wifi_bssid[6], wifi_channel;
	static void config_ip();
	static void save_wifi_ip();
	static void reset_to_ap();
	static unsigned char state;
	static void setup_pd_voltage();

#else
	static void lcd_print_pgm(const char *str);
	static void lcd_print_line_clear_pgm(const char *str, unsigned char line);
#endif // LCD functions for Arduino

private:
#if defined(USE_DISPLAY)  // LCD functions
	static void lcd_print_option(int i);  // print an option to the lcd
	static void lcd_print_2digit(int v);  // print a integer in 2 digits
	static void lcd_start();
	static unsigned char button_read_busy(unsigned char pin_butt, unsigned char waitmode, unsigned char butt, unsigned char is_holding);
#endif // LCD functions

#if defined(ESP8266)
	static void latch_boost(int8_t volt=-1);
	static void latch_open(unsigned char sid);
	static void latch_close(unsigned char sid);
	static void latch_setzonepin(unsigned char sid, unsigned char value);
	static void latch_setallzonepins(unsigned char value);
	static void latch_disable_alloutputs_v2();
	static void latch_setzoneoutput_v2(unsigned char sid, unsigned char A, unsigned char K);
	static void latch_apply_all_station_bits();
	static unsigned char prev_station_bits[];
#endif // LCD functions
	static unsigned char engage_booster;
	static RCSwitch rfswitch;

	static void parse_otc_config();
};
