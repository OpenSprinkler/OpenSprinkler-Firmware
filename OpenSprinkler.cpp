/* OpenSprinkler Unified Firmware
 * Copyright (C) 2015 by Ray Wang (ray@opensprinkler.com)
 *
 * OpenSprinkler library
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

#include "OpenSprinkler.h"
#include "opensprinkler_server.h"
#include "gpio.h"
#ifdef __has_include
	#if __has_include("testmode.h")
		#include "testmode.h"
	#endif
#endif
#include "program.h"
#include "ArduinoJson.hpp"

/** Declare static data members */
sensor_memory_t OpenSprinkler::sensors[64] = {0};
OSMqtt OpenSprinkler::mqtt;
NVConData OpenSprinkler::nvdata;
ConStatus OpenSprinkler::status;
ConStatus OpenSprinkler::old_status;

unsigned char OpenSprinkler::hw_type;
unsigned char OpenSprinkler::hw_rev;
unsigned char OpenSprinkler::nboards;
unsigned char OpenSprinkler::nstations;
unsigned char OpenSprinkler::nsensors;
unsigned char OpenSprinkler::station_bits[MAX_NUM_BOARDS];
unsigned char OpenSprinkler::engage_booster;
uint16_t OpenSprinkler::baseline_current;

SensorState OpenSprinkler::sn_sensors[NUM_SENSORS] = {};

const SensorIoptKeys sensor_iopt_keys[NUM_SENSORS] = {
	{IOPT_SENSOR1_TYPE, IOPT_SENSOR1_OPTION, IOPT_SENSOR1_ON_DELAY, IOPT_SENSOR1_OFF_DELAY},
	{IOPT_SENSOR2_TYPE, IOPT_SENSOR2_OPTION, IOPT_SENSOR2_ON_DELAY, IOPT_SENSOR2_OFF_DELAY},
	{IOPT_SENSOR3_TYPE, IOPT_SENSOR3_OPTION, IOPT_SENSOR3_ON_DELAY, IOPT_SENSOR3_OFF_DELAY},
	{IOPT_SENSOR4_TYPE, IOPT_SENSOR4_OPTION, IOPT_SENSOR4_ON_DELAY, IOPT_SENSOR4_OFF_DELAY},
};

const uint16_t sensor_notif_bits[NUM_SENSORS] = {
	NOTIFY_SENSOR1, NOTIFY_SENSOR2, NOTIFY_SENSOR3, NOTIFY_SENSOR4,
};

const uint8_t sensor_log_codes[NUM_SENSORS] = {
	LOGDATA_SENSOR1, LOGDATA_SENSOR2, LOGDATA_SENSOR3, LOGDATA_SENSOR4,
};

unsigned char sensor_pin(uint8_t i) {
	switch (i) {
		case 0: return PIN_SENSOR1;
		case 1: return PIN_SENSOR2;
	#if defined(ESP8266)
		case 2: return PIN_SENSOR3;
		case 3: return PIN_SENSOR4;
	#endif
	}
	return 255;
}

bool sensor_available(uint8_t i) {
	if (i < 2) return true;
#if defined(ESP8266)
	return OpenSprinkler::hw_rev >= 4;
#else
	return false;
#endif
}

int8_t sensor_index_from_log_code(uint8_t type) {
	for (uint8_t i = 0; i < NUM_SENSORS; i++) {
		if (sensor_log_codes[i] == type) return i;
	}
	if (type == LOGDATA_FLOWSENSE) return 0;
	return -1;
}

time_os_t OpenSprinkler::raindelay_on_lasttime;
uint32_t  OpenSprinkler::pause_timer;

uint32_t   OpenSprinkler::flowcount_log_start;
uint32_t   OpenSprinkler::flowcount_rt;
unsigned char    OpenSprinkler::button_timeout;
time_os_t  OpenSprinkler::checkwt_lasttime;
time_os_t  OpenSprinkler::checkwt_success_lasttime;
time_os_t  OpenSprinkler::powerup_lasttime;
uint8_t OpenSprinkler::last_reboot_cause = REBOOT_CAUSE_NONE;
unsigned char    OpenSprinkler::weather_update_flag;

// todo future: the following attribute bytes are for backward compatibility
unsigned char OpenSprinkler::attrib_mas[MAX_NUM_BOARDS];
unsigned char OpenSprinkler::attrib_mas2[MAX_NUM_BOARDS];
unsigned char OpenSprinkler::attrib_mas3[MAX_NUM_BOARDS];
unsigned char OpenSprinkler::attrib_mas4[MAX_NUM_BOARDS];
unsigned char OpenSprinkler::attrib_igs[NUM_SENSORS][MAX_NUM_BOARDS];
unsigned char OpenSprinkler::attrib_igrd[MAX_NUM_BOARDS];
unsigned char OpenSprinkler::attrib_dis[MAX_NUM_BOARDS];
unsigned char OpenSprinkler::attrib_spe[MAX_NUM_BOARDS];
unsigned char OpenSprinkler::attrib_grp[MAX_NUM_STATIONS];
unsigned char OpenSprinkler::masters[NUM_MASTER_ZONES][NUM_MASTER_OPTS];
time_os_t OpenSprinkler::masters_last_on[NUM_MASTER_ZONES];
RCSwitch OpenSprinkler::rfswitch;

extern char tmp_buffer[];
extern char ether_buffer[];
extern ProgramData pd;
extern const char* user_agent_string;
extern unsigned char curr_alert_sid;

#if defined(USE_DISPLAY)
SSD1306Display OpenSprinkler::lcd(0x3c, SDA, SCL);
#endif

ADS1115 *OpenSprinkler::ads1115_devices[4] = {nullptr};

bool OpenSprinkler::has_ads1115() {
	for (size_t i = 0; i < 4; i++) {
		if (ads1115_devices[i] != nullptr) return true;
	}
	return false;
}

#if defined(ESP8266)
	unsigned char OpenSprinkler::state = OS_STATE_INITIAL;
	unsigned char OpenSprinkler::prev_station_bits[MAX_NUM_BOARDS];
	IOEXP* OpenSprinkler::expanders[MAX_NUM_BOARDS/2];
	IOEXP* OpenSprinkler::mainio; // main controller IO expander object
	IOEXP* OpenSprinkler::drio; // driver board IO expander object
	String OpenSprinkler::wifi_ssid="";
	String OpenSprinkler::wifi_pass="";
	unsigned char OpenSprinkler::wifi_bssid[6]={0};
	unsigned char OpenSprinkler::wifi_channel=255;
	unsigned char OpenSprinkler::wifi_testmode = 0;
	CH224 OpenSprinkler::usbpd;
	uint8_t OpenSprinkler::actual_pd_voltage = 0;
#else
	#if defined(OSPI)
		unsigned char OpenSprinkler::pin_sr_data = PIN_SR_DATA;
	#endif
#endif

OTCConfig OpenSprinkler::otc;

// HTTP port defaults differ by platform. v3 stays on 80; 8080 is reserved
// there for OTA firmware upload. OSPi/DEMO default to 88 -- low enough to be
// unobtrusive in a URL, and both the systemd unit and the container run as
// root, so binding a privileged port is not a problem. Existing installs are
// unaffected: defaults are only consulted when iopts.dat is absent or reset.
#if defined(ESP8266)
#define DEFAULT_HTTPPORT_0 80
#define DEFAULT_HTTPPORT_1 0
#else
#define DEFAULT_HTTPPORT_0 88
#define DEFAULT_HTTPPORT_1 0
#endif

/** Per-option metadata: JSON name, max value, factory default, flags, LCD prompt.
 * Stored in PROGMEM in IOPT_* enum order. To add a new option:
 *   1) add IOPT_* to defines.h enum (before NUM_IOPTS)
 *   2) add a row here in the same position
 * The static_assert below catches order/length drift.
 *
 * Notes on flag use:
 *   IOPT_FLAG_RETIRED      — option is deprecated; skipped in /jo and LCD edit
 *   IOPT_FLAG_SIGNED_TIME  — value uses water_time_encode_signed packing
 *   IOPT_FLAG_READ_ONLY    — /co rejects writes; firmware can still write internally
 *   IOPT_FLAG_HIDDEN_API   — omitted from /jo (e.g., LCD-only or reserved fields)
 */
const IOptDef iopt_defs[NUM_IOPTS] PROGMEM = {
	/* IOPT_FW_VERSION         */ {"fwv",   0,                OS_FW_VERSION,                     IOPT_FLAG_READ_ONLY,    "Firmware version"},
	/* IOPT_TIMEZONE           */ {"tz",    108,              28,                                0,                      "Time zone (GMT):"},
	/* IOPT_USE_NTP            */ {"ntp",   1,                1,                                 0,                      "Enable NTP sync?"},
	/* IOPT_USE_DHCP           */ {"dhcp",  1,                1,                                 0,                      "Enable DHCP?    "},
	/* IOPT_STATIC_IP1         */ {"ip1",   255,              0,                                 0,                      "Static.ip1:     "},
	/* IOPT_STATIC_IP2         */ {"ip2",   255,              0,                                 0,                      "Static.ip2:     "},
	/* IOPT_STATIC_IP3         */ {"ip3",   255,              0,                                 0,                      "Static.ip3:     "},
	/* IOPT_STATIC_IP4         */ {"ip4",   255,              0,                                 0,                      "Static.ip4:     "},
	/* IOPT_GATEWAY_IP1        */ {"gw1",   255,              0,                                 0,                      "Gateway.ip1:    "},
	/* IOPT_GATEWAY_IP2        */ {"gw2",   255,              0,                                 0,                      "Gateway.ip2:    "},
	/* IOPT_GATEWAY_IP3        */ {"gw3",   255,              0,                                 0,                      "Gateway.ip3:    "},
	/* IOPT_GATEWAY_IP4        */ {"gw4",   255,              0,                                 0,                      "Gateway.ip4:    "},
	/* IOPT_HTTPPORT_0         */ {"hp0",   255,              DEFAULT_HTTPPORT_0,                0,                      "HTTP Port:      "},
	/* IOPT_HTTPPORT_1         */ {"hp1",   255,              DEFAULT_HTTPPORT_1,                0,                      "----------------"},
	/* IOPT_HW_VERSION         */ {"hwv",   0,                OS_HW_VERSION,                     IOPT_FLAG_READ_ONLY,    "Hardware version"},
	/* IOPT_EXT_BOARDS         */ {"ext",   MAX_EXT_BOARDS,   0,                                 0,                      "# of exp. board:"},
	/* IOPT_SEQUENTIAL_RETIRED */ {"seq",   1,                1,                                 IOPT_FLAG_RETIRED,      "----------------"},
	/* IOPT_STATION_DELAY_TIME */ {"sdt",   255,              120,                               IOPT_FLAG_SIGNED_TIME,  "Stn. delay (sec)"},
	/* IOPT_MASTER_STATION     */ {"mas",   MAX_NUM_STATIONS, 0,                                 0,                      "Master 1 (Mas1):"},
	/* IOPT_MASTER_ON_ADJ      */ {"mton",  255,              120,                               IOPT_FLAG_SIGNED_TIME,  "Mas1  on adjust:"},
	/* IOPT_MASTER_OFF_ADJ     */ {"mtof",  255,              120,                               IOPT_FLAG_SIGNED_TIME,  "Mas1 off adjust:"},
	/* IOPT_URS_RETIRED        */ {"urs",   255,              0,                                 IOPT_FLAG_RETIRED,      "----------------"},
	/* IOPT_RSO_RETIRED        */ {"rso",   1,                0,                                 IOPT_FLAG_RETIRED,      "----------------"},
	/* IOPT_WATER_PERCENTAGE   */ {"wl",    250,              100,                               0,                      "Watering level: "},
	/* IOPT_DEVICE_ENABLE      */ {"den",   1,                1,                                 0,                      "Device enabled? "},
	/* IOPT_IGNORE_PASSWORD    */ {"ipas",  1,                0,                                 0,                      "Ignore password?"},
	/* IOPT_DEVICE_ID          */ {"devid", 255,              0,                                 0,                      "Device ID:      "},
	/* IOPT_LCD_CONTRAST       */ {"con",   255,              150,                               IOPT_FLAG_HIDDEN_API,   "LCD contrast:   "},
	/* IOPT_LCD_BACKLIGHT      */ {"lit",   255,              100,                               IOPT_FLAG_HIDDEN_API,   "LCD brightness: "},
	/* IOPT_LCD_DIMMING        */ {"dim",   255,              15,                                0,                      "LCD dimming:    "},
	/* IOPT_BOOST_TIME         */ {"bst",   250,              80,                                0,                      "DC boost time:  "},
	/* IOPT_USE_WEATHER        */ {"uwt",   255,              0,                                 0,                      "Weather algo.:  "},
	/* IOPT_NTP_IP1            */ {"ntp1",  255,              0,                                 0,                      "NTP server.ip1: "},
	/* IOPT_NTP_IP2            */ {"ntp2",  255,              0,                                 0,                      "NTP server.ip2: "},
	/* IOPT_NTP_IP3            */ {"ntp3",  255,              0,                                 0,                      "NTP server.ip3: "},
	/* IOPT_NTP_IP4            */ {"ntp4",  255,              0,                                 0,                      "NTP server.ip4: "},
	/* IOPT_ENABLE_LOGGING     */ {"lg",    1,                1,                                 0,                      "Enable logging? "},
	/* IOPT_MASTER_STATION_2   */ {"mas2",  MAX_NUM_STATIONS, 0,                                 0,                      "Master 2 (Mas2):"},
	/* IOPT_MASTER_ON_ADJ_2    */ {"mton2", 255,              120,                               IOPT_FLAG_SIGNED_TIME,  "Mas2  on adjust:"},
	/* IOPT_MASTER_OFF_ADJ_2   */ {"mtof2", 255,              120,                               IOPT_FLAG_SIGNED_TIME,  "Mas2 off adjust:"},
	/* IOPT_FW_MINOR           */ {"fwm",   0,                OS_FW_MINOR,                       IOPT_FLAG_READ_ONLY,    "Firmware minor: "},
	/* IOPT_PULSE_RATE_0       */ {"fpr0",  255,              100,                               0,                      "Pulse rate:     "},
	/* IOPT_PULSE_RATE_1       */ {"fpr1",  255,              0,                                 0,                      "----------------"},
	/* IOPT_REMOTE_EXT_MODE    */ {"re",    1,                0,                                 0,                      "As remote ext.? "},
	/* IOPT_DNS_IP1            */ {"dns1",  255,              8,                                 0,                      "DNS server.ip1: "},
	/* IOPT_DNS_IP2            */ {"dns2",  255,              8,                                 0,                      "DNS server.ip2: "},
	/* IOPT_DNS_IP3            */ {"dns3",  255,              8,                                 0,                      "DNS server.ip3: "},
	/* IOPT_DNS_IP4            */ {"dns4",  255,              8,                                 0,                      "DNS server.ip4: "},
	/* IOPT_SPE_AUTO_REFRESH   */ {"sar",   1,                0,                                 0,                      "Special Refresh?"},
	/* IOPT_NOTIF_ENABLE       */ {"ife",   255,              0,                                 0,                      "Notif Enable:   "},
	/* IOPT_SENSOR1_TYPE       */ {"sn1t",  255,              0,                                 0,                      "Sensor 1 type:  "},
	/* IOPT_SENSOR1_OPTION     */ {"sn1o",  1,                1,                                 0,                      "Normally open?  "},
	/* IOPT_SENSOR2_TYPE       */ {"sn2t",  255,              0,                                 0,                      "Sensor 2 type:  "},
	/* IOPT_SENSOR2_OPTION     */ {"sn2o",  1,                1,                                 0,                      "Normally open?  "},
	/* IOPT_SENSOR1_ON_DELAY   */ {"sn1on", 255,              0,                                 0,                      "Sn1 on adjust:  "},
	/* IOPT_SENSOR1_OFF_DELAY  */ {"sn1of", 255,              0,                                 0,                      "Sn1 off adjust: "},
	/* IOPT_SENSOR2_ON_DELAY   */ {"sn2on", 255,              0,                                 0,                      "Sn2 on adjust:  "},
	/* IOPT_SENSOR2_OFF_DELAY  */ {"sn2of", 255,              0,                                 0,                      "Sn2 off adjust: "},
	/* IOPT_SUBNET_MASK1       */ {"subn1", 255,              255,                               0,                      "Subnet mask1:   "},
	/* IOPT_SUBNET_MASK2       */ {"subn2", 255,              255,                               0,                      "Subnet mask2:   "},
	/* IOPT_SUBNET_MASK3       */ {"subn3", 255,              255,                               0,                      "Subnet mask3:   "},
	/* IOPT_SUBNET_MASK4       */ {"subn4", 255,              0,                                 0,                      "Subnet mask4:   "},
	/* IOPT_FORCE_WIRED        */ {"fwire", 1,                1,                                 0,                      "Force wired?    "},
	/* IOPT_LATCH_ON_VOLTAGE   */ {"laton", 24,               0,                                 0,                      "Latch On Volt.  "},
	/* IOPT_LATCH_OFF_VOLTAGE  */ {"latof", 24,               0,                                 0,                      "Latch Off Volt. "},
	/* IOPT_NOTIF2_ENABLE      */ {"ife2",  255,              0,                                 0,                      "Notif 2 Enable  "},
	/* IOPT_I_MIN_THRESHOLD    */ {"imin",  100,              DEFAULT_UNDERCURRENT_THRESHOLD/10, 0,                      "I min threshold "},
	/* IOPT_I_MAX_LIMIT        */ {"imax",  255,              0,                                 0,                      "I max limit     "},
	/* IOPT_TARGET_PD_VOLTAGE  */ {"tpdv",  210,              DEFAULT_TARGET_PD_VOLTAGE,         0,                      "Target PD Volt. "},
	/* IOPT_RESERVE_7          */ {"resv7", 255,              0,                                 IOPT_FLAG_HIDDEN_API,   "Reserved 7      "},
	/* IOPT_RESERVE_8          */ {"resv8", 255,              0,                                 IOPT_FLAG_HIDDEN_API,   "Reserved 8      "},
	/* IOPT_WIFI_MODE          */ {"wimod", 255,              WIFI_MODE_AP,                      IOPT_FLAG_READ_ONLY,    "WiFi mode?      "},
	/* IOPT_RESET              */ {"reset", 1,                0,                                 IOPT_FLAG_READ_ONLY,    "Factory reset?  "},
	/* IOPT_MASTER_STATION_3   */ {"mas3",  MAX_NUM_STATIONS, 0,                                 0,                      "Master 3 (Mas3):"},
	/* IOPT_MASTER_ON_ADJ_3    */ {"mton3", 255,              120,                               IOPT_FLAG_SIGNED_TIME,  "Mas3  on adjust:"},
	/* IOPT_MASTER_OFF_ADJ_3   */ {"mtof3", 255,              120,                               IOPT_FLAG_SIGNED_TIME,  "Mas3 off adjust:"},
	/* IOPT_MASTER_STATION_4   */ {"mas4",  MAX_NUM_STATIONS, 0,                                 0,                      "Master 4 (Mas4):"},
	/* IOPT_MASTER_ON_ADJ_4    */ {"mton4", 255,              120,                               IOPT_FLAG_SIGNED_TIME,  "Mas4  on adjust:"},
	/* IOPT_MASTER_OFF_ADJ_4   */ {"mtof4", 255,              120,                               IOPT_FLAG_SIGNED_TIME,  "Mas4 off adjust:"},
	/* IOPT_SENSOR3_TYPE       */ {"sn3t",  255,              0,                                 0,                      "Sensor 3 type:  "},
	/* IOPT_SENSOR3_OPTION     */ {"sn3o",  1,                1,                                 0,                      "Normally open?  "},
	/* IOPT_SENSOR3_ON_DELAY   */ {"sn3on", 255,              0,                                 0,                      "Sn3 on adjust:  "},
	/* IOPT_SENSOR3_OFF_DELAY  */ {"sn3of", 255,              0,                                 0,                      "Sn3 off adjust: "},
	/* IOPT_SENSOR4_TYPE       */ {"sn4t",  255,              0,                                 0,                      "Sensor 4 type:  "},
	/* IOPT_SENSOR4_OPTION     */ {"sn4o",  1,                1,                                 0,                      "Normally open?  "},
	/* IOPT_SENSOR4_ON_DELAY   */ {"sn4on", 255,              0,                                 0,                      "Sn4 on adjust:  "},
	/* IOPT_SENSOR4_OFF_DELAY  */ {"sn4of", 255,              0,                                 0,                      "Sn4 off adjust: "},
};

static_assert(sizeof(iopt_defs)/sizeof(iopt_defs[0]) == NUM_IOPTS,
              "iopt_defs out of sync with IOPT_* enum");

// Accessors that hide PROGMEM reads.
uint8_t iopt_get_max(uint8_t oid)   { return pgm_read_byte(&iopt_defs[oid].max_val); }
uint8_t iopt_get_def(uint8_t oid)   { return pgm_read_byte(&iopt_defs[oid].def_val); }
uint8_t iopt_get_flags(uint8_t oid) { return pgm_read_byte(&iopt_defs[oid].flags); }

void iopt_get_json_name(uint8_t oid, char *buf) {
	memcpy_P(buf, iopt_defs[oid].json, 6);
}

void iopt_get_prompt(uint8_t oid, char *buf) {
	memcpy_P(buf, iopt_defs[oid].prompt, 17);
}

/** Integer option values — populated at boot from iopt_defs[].def_val
 * (factory_reset path) or restored from iopts.dat (iopts_load path).
 * iopts.dat file format depends on IOPT_* enum order, which must remain
 * stable across firmware upgrades. */
unsigned char OpenSprinkler::iopts[NUM_IOPTS];


/** String option values (stored in RAM) */
const char *OpenSprinkler::sopts[] = {
	DEFAULT_PASSWORD,
	DEFAULT_LOCATION,
	DEFAULT_JAVASCRIPT_URL,
	DEFAULT_WEATHER_URL,
	DEFAULT_EMPTY_STRING, // SOPT_WEATHER_OPTS
	DEFAULT_EMPTY_STRING, // SOPT_IFTTT_KEY
	DEFAULT_EMPTY_STRING, // SOPT_STA_SSID
	DEFAULT_EMPTY_STRING, // SOPT_STA_PASS
	DEFAULT_EMPTY_STRING, // SOPT_MQTT_OPTS
	DEFAULT_EMPTY_STRING, // SOPT_OTC_OPTS
	DEFAULT_DEVICE_NAME,
	DEFAULT_EMPTY_STRING, // SOPT_STA_BSSID_CHL
	DEFAULT_EMPTY_STRING, // SOPT_EMAIL_OPTS
};

/** Weekday strings (stored in PROGMEM to reduce RAM usage) */
static const char days_str[] PROGMEM =
	"Mon\0"
	"Tue\0"
	"Wed\0"
	"Thu\0"
	"Fri\0"
	"Sat\0"
	"Sun\0";

/** Month name strings (stored in PROGMEM to reduce RAM usage) */
static const char months_str[] PROGMEM =
	"Jan\0"
	"Feb\0"
	"Mar\0"
	"Apr\0"
	"May\0"
	"Jun\0"
	"Jul\0"
	"Aug\0"
	"Sep\0"
	"Oct\0"
	"Nov\0"
	"Dec\0";

#if !defined(ESP8266)
static inline uint32_t now() {
	time_t rawtime;
	time(&rawtime);
	return (uint32_t)rawtime;
}
#endif
/** Calculate local time (UTC time plus time zone offset) */
time_os_t OpenSprinkler::now_tz() {
	return now()+(int32_t)3600/4*(int32_t)(iopts[IOPT_TIMEZONE]-48);
}

#if defined(ESP8266)

bool detect_i2c(int addr) {
	Wire.beginTransmission(addr);
	return (Wire.endTransmission()==0); // successful if received 0
}

/** read hardware MAC into tmp_buffer */
bool OpenSprinkler::load_hardware_mac(unsigned char* buffer, bool wired) {
	WiFi.macAddress((unsigned char*)buffer);
	// if requesting wired Ethernet MAC, flip the last byte to create a modified MAC
	if(wired) buffer[5] = ~buffer[5];
	return true;
}

/** Initialize network with the given mac address and http port */

unsigned char OpenSprinkler::start_network() {
	lcd_print_line_clear_pgm(PSTR("Starting..."), 1);
	uint16_t httpport = (uint16_t)(iopts[IOPT_HTTPPORT_1]<<8) + (uint16_t)iopts[IOPT_HTTPPORT_0];

	if (start_ether()) {
		useEth = true;
		WiFi.mode(WIFI_OFF);
	} else {
		useEth = false;
	}

	if((useEth || get_wifi_mode()==WIFI_MODE_STA) && otc.en>0 && otc.token.length()>=DEFAULT_OTC_TOKEN_LENGTH) {
		otf = new OTF::OpenThingsFramework(httpport, otc.server, otc.port, otc.token, false, ether_buffer, ETHER_BUFFER_SIZE);
		DEBUG_PRINTLN(F("Started OTF with remote connection"));
	} else {
		otf = new OTF::OpenThingsFramework(httpport, ether_buffer, ETHER_BUFFER_SIZE);
		DEBUG_PRINTLN(F("Started OTF with just local connection"));
	}
	extern DNSServer *dns;
	if(get_wifi_mode() == WIFI_MODE_AP) dns = new DNSServer();
	if(update_server) { delete update_server; update_server = NULL; }
	update_server = new ESP8266WebServer(8080);
	DEBUG_PRINT(F("Started update server"));
	return 1;

}

unsigned char OpenSprinkler::start_ether() {
	if(hw_rev<2) return 0;  // ethernet capability is only available when hw_rev>=2
	eth.isW5500 = (hw_rev==2)?false:true; // os 3.2 uses enc28j60 and 3.3 uses w5500

	SPI.begin();
	SPI.setBitOrder(MSBFIRST);
	SPI.setDataMode(SPI_MODE0);
	SPI.setFrequency(ETHER_SPI_CLOCK); // set SPI frequency

	if(eth.isW5500) {
		DEBUG_PRINTLN(F("detect existence of W5500"));
		/* this is copied from w5500.cpp wizchip_sw_reset
		 * perform a software reset and see if we get a correct response
		 * without this, eth.begin will crash if W5500 is not connected
		 * ideally wizchip_sw_reset should return a value but since it doesn't
		 * we have to extract it code here
		 * */
		static const uint8_t AccessModeRead = (0x00 << 2);
		static const uint8_t AccessModeWrite = (0x01 << 2);
		static const uint8_t BlockSelectCReg = (0x00 << 3);
		pinMode(PIN_ETHER_CS, OUTPUT);
		// ==> setMR(MR_RST)
		digitalWrite(PIN_ETHER_CS, LOW);
		SPI.transfer((0x00 & 0xFF00) >> 8);
		SPI.transfer((0x00 & 0x00FF) >> 0);
		SPI.transfer(BlockSelectCReg | AccessModeWrite);
		SPI.transfer(0x80);
		digitalWrite(PIN_ETHER_CS, HIGH);

		// ==> ret = getMR()
		uint8_t ret;
		digitalWrite(PIN_ETHER_CS, LOW);
		SPI.transfer((0x00 & 0xFF00) >> 8);
		SPI.transfer((0x00 & 0x00FF) >> 0);
		SPI.transfer(BlockSelectCReg | AccessModeRead);
		ret = SPI.transfer(0);
		digitalWrite(PIN_ETHER_CS, HIGH);
		if(ret!=0) return 0; // ret is expected to be 0
	} else {
		/* this is copied from enc28j60.cpp geterevid
		 * check to see if the hardware revision number if expected
		 * */
		DEBUG_PRINTLN(F("detect existence of ENC28J60"));
		#define MAADRX_BANK 0x03
		#define EREVID 0x12
		#define ECON1 0x1f

		// ==> setregbank(MAADRX_BANK);
		pinMode(PIN_ETHER_CS, OUTPUT);
		uint8_t r;
		digitalWrite(PIN_ETHER_CS, LOW);
		SPI.transfer(0x00 | (ECON1 & 0x1f));
		r = SPI.transfer(0);
		digitalWrite(PIN_ETHER_CS, HIGH);

		digitalWrite(PIN_ETHER_CS, LOW);
		SPI.transfer(0x40 | (ECON1 & 0x1f));
		SPI.transfer((r & 0xfc) | (MAADRX_BANK & 0x03));
		digitalWrite(PIN_ETHER_CS, HIGH);

		// ==> r = readreg(EREVID);
		digitalWrite(PIN_ETHER_CS, LOW);
		SPI.transfer(0x00 | (EREVID & 0x1f));
		r = SPI.transfer(0);
		digitalWrite(PIN_ETHER_CS, HIGH);
		if(r==0 || r==255) return 0; // r is expected to be a non-255 revision number
	}

	load_hardware_mac((uint8_t*)tmp_buffer, true);
	if (iopts[IOPT_USE_DHCP]==0) { // config static IP before calling eth.begin
		IPAddress staticip(iopts+IOPT_STATIC_IP1);
		IPAddress gateway(iopts+IOPT_GATEWAY_IP1);
		IPAddress dns(iopts+IOPT_DNS_IP1);
		IPAddress subn(iopts+IOPT_SUBNET_MASK1);
		eth.config(staticip, gateway, subn, dns);
	}
	eth.setDefault();
	if(!eth.begin((uint8_t*)tmp_buffer))	return 0;
	lcd_print_line_clear_pgm(PSTR("Start wired link"), 1);
	lcd_print_line_clear_pgm(eth.isW5500 ? PSTR("  [w5500]    ") : PSTR(" [enc28j60]  "), 2);

	uint32_t timeout = millis()+60000; // 60 seconds time out
	unsigned char timecount = 1;
	while (!eth.connected() && (int32_t)((uint32_t)millis()-timeout)<0) { // overflow proof
		DEBUG_PRINT(".");
		lcd.setCursor(13, 2);
		lcd.print(timecount);
		delay(1000);
		timecount++;
	}
	lcd_print_line_clear_pgm(PSTR(""), 2);
	if(eth.connected()) {
		// if wired connection is successful at this point, copy the network ips to config
		if (iopts[IOPT_USE_DHCP]) {
			memcpy(iopts+IOPT_STATIC_IP1, &(eth.localIP()[0]), 4);
			memcpy(iopts+IOPT_GATEWAY_IP1, &(eth.gatewayIP()[0]),4);
			memcpy(iopts+IOPT_DNS_IP1, &(eth.dnsIP()[0]), 4);
			memcpy(iopts+IOPT_SUBNET_MASK1, &(eth.subnetMask()[0]), 4);
			iopts_save();
		}
		return 1;
	} else {
		// if wired connection has failed at this point, return depending on whether the user wants to force wired
		return (iopts[IOPT_FORCE_WIRED] ? 1 : 0);
	}
}

bool OpenSprinkler::network_connected(void) {
	if(useEth)
		return eth.connected();
	else
		return (get_wifi_mode()==WIFI_MODE_STA && WiFi.status()==WL_CONNECTED && state==OS_STATE_CONNECTED);
}

/** Reboot controller */
void OpenSprinkler::reboot_dev(uint8_t cause) {
	lcd_print_line_clear_pgm(PSTR("Rebooting..."), 0);
	if(cause) {
		nvdata.reboot_cause = cause;
		nvdata_save();
	}
	ESP.restart();
}

#else // RPI/LINUX network init functions

#include "etherport.h"
#include <sys/reboot.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include "utils.h"
#include "opensprinkler_server.h"

/** Initialize network with the given mac address and http port */
unsigned char OpenSprinkler::start_network() {
	unsigned int port = (unsigned int)(iopts[IOPT_HTTPPORT_1]<<8) + (unsigned int)iopts[IOPT_HTTPPORT_0];
#if defined(DEMO)
#if defined(HTTP_PORT)
	port = HTTP_PORT;
#else
	port = 80;
#endif
#endif
	if(otc.en>0 && otc.token.length()>=DEFAULT_OTC_TOKEN_LENGTH) {
		otf = new OTF::OpenThingsFramework(port, otc.server.c_str(), otc.port, otc.token.c_str(), false, ether_buffer, ETHER_BUFFER_SIZE);
		DEBUG_PRINT(F("Started OTF with remote connection. Local port is: "));
	} else {
		otf = new OTF::OpenThingsFramework(port, ether_buffer, ETHER_BUFFER_SIZE);
		DEBUG_PRINT(F("Started OTF with just local connection. Local port is: "));
	}
	DEBUG_PRINTLN(port);

	return 1;
}

bool OpenSprinkler::network_connected(void) {
	return true;
}

#if defined(OSPI)
bool detect_i2c(int addr) {
	return Bus.detect(addr)==0;
}
#endif

// Return mac of first recognised interface and fallback to software mac
// Note: on OSPi, operating system handles interface allocation so 'wired' ignored
bool OpenSprinkler::load_hardware_mac(unsigned char* mac, bool wired) {
	const char * if_names[]  = { "eth0", "eth1", "wlan0", "wlan1" };
	struct ifreq ifr;
	int fd;

	// Fallback to asoftware mac if interface not recognised
	mac[0] = 0x00;
	mac[1] = 0x69;
	mac[2] = 0x69;
	mac[3] = 0x2D;
	mac[4] = 0x31;
	mac[5] = iopts[IOPT_DEVICE_ID];

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) return true;

	// Returns the mac address of the first interface if multiple active
	for (unsigned int i = 0; i < sizeof(if_names)/sizeof(const char *); i++) {
		strncpy(ifr.ifr_name, if_names[i], sizeof(ifr.ifr_name));
		if (ioctl(fd, SIOCGIFHWADDR, &ifr) != -1) {
			memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
			break;
		}
	}
	close(fd);
	return true;
}

/** Reboot controller */
void OpenSprinkler::reboot_dev(uint8_t cause) {
	nvdata.reboot_cause = cause;
	nvdata_save();
#if defined(DEMO)
	// do nothing
#else
	sync(); // add sync to prevent file corruption
	reboot(RB_AUTOBOOT);
	// reboot() only returns on failure. In a container it always fails: the
	// default capability set has no CAP_SYS_BOOT and the default seccomp
	// profile gates the syscall behind it. Falling through used to leave
	// reboot_timer armed (main.cpp never clears it), so do_loop() called back
	// in one second later, forever -- an endless nvdata_save() + sync() loop
	// against the data dir. Exit instead: under a restart policy (systemd
	// Restart=always, compose restart: unless-stopped) the supervisor brings
	// the process straight back, which is what "reboot" should mean here.
	// Unconditional, not DEBUG_PRINTLN: that compiles to {} in release builds,
	// and this is precisely the case an operator needs to see in the log.
	fprintf(stderr, "reboot() failed (%s); exiting for the supervisor to restart\n",
	        strerror(errno));
	_exit(0);
#endif
}

/** Launch update script */
void OpenSprinkler::update_dev() {
	char cmd[500];
	snprintf(cmd, 500, "cd %s && ./updater.sh", get_data_dir());
	system(cmd);
}
#endif // end network init functions

/** Initialize LCD */
#if defined(USE_DISPLAY)
void OpenSprinkler::lcd_start() {
	// initialize SSD1306
	lcd.init();
	lcd.begin();
	flash_screen();
}
#endif

//extern void flow_isr();

/** Initialize pins, controller variables, LCD */
void OpenSprinkler::begin() {
	hw_type = HW_TYPE_UNKNOWN;
	hw_rev = 0;

#if defined(ESP8266)
	Wire.begin(); // init I2C
	/* detect hardware revision type */
	if(detect_i2c(MAIN_I2CADDR)) {	// check if main PCF8574 exists
		/* assign revision 0 pins */
		PIN_BUTTON_1 = V0_PIN_BUTTON_1;
		PIN_BUTTON_2 = V0_PIN_BUTTON_2;
		PIN_BUTTON_3 = V0_PIN_BUTTON_3;
		PIN_RFRX = V0_PIN_RFRX;
		PIN_RFTX = V0_PIN_RFTX;
		PIN_BOOST = V0_PIN_BOOST;
		PIN_BOOST_EN = V0_PIN_BOOST_EN;
		PIN_SENSOR1 = V0_PIN_SENSOR1;
		PIN_SENSOR2 = V0_PIN_SENSOR2;

		/* check hardware type */
		if(detect_i2c(ACDR_I2CADDR)) {
			hw_type = HW_TYPE_AC;
			drio = new PCF8574(ACDR_I2CADDR);
		} else if(detect_i2c(DCDR_I2CADDR)) {
			hw_type = HW_TYPE_DC;
			drio = new PCF8574(DCDR_I2CADDR);
		} else if(detect_i2c(LADR_I2CADDR)) {
			hw_type = HW_TYPE_LATCH;
			drio = new PCF8574(LADR_I2CADDR);
		} else {
			hw_type = HW_TYPE_UNKNOWN;
			drio = new PCF8574(ACDR_I2CADDR);
		}

		mainio = new PCF8574(MAIN_I2CADDR);
		mainio->i2c_write(0, 0x0F); // set lower four bits of main PCF8574 (8-ch) to high

		digitalWriteExt(V0_PIN_PWR_TX, 1); // turn on TX power
		digitalWriteExt(V0_PIN_PWR_RX, 1); // turn on RX power
		pinModeExt(PIN_BUTTON_2, INPUT_PULLUP);
		digitalWriteExt(PIN_BOOST, LOW);
		digitalWriteExt(PIN_BOOST_EN, LOW);
		digitalWriteExt(PIN_LATCH_COM, LOW);

	} else {

		pinMode(16, INPUT);
		if(digitalRead(16)==LOW) {
			// revision 1
			hw_rev = 1;
			if(detect_i2c(ACDR_I2CADDR)) {
				hw_type = HW_TYPE_AC;
				drio = new PCA9555(ACDR_I2CADDR);
			} else if(detect_i2c(DCDR_I2CADDR)) {
				hw_type = HW_TYPE_DC;
				drio = new PCA9555(DCDR_I2CADDR);
			} else if(detect_i2c(LADR_I2CADDR)) {
				hw_type = HW_TYPE_LATCH;
				drio = new PCA9555(LADR_I2CADDR);
			} else {
				hw_type = HW_TYPE_UNKNOWN;
				drio = new PCA9555(ACDR_I2CADDR);
			}
			mainio = drio;

			mainio->i2c_write(NXP_CONFIG_REG, V1_IO_CONFIG);
			mainio->i2c_write(NXP_OUTPUT_REG, V1_IO_OUTPUT);

			PIN_BUTTON_1 = V1_PIN_BUTTON_1;
			PIN_BUTTON_2 = V1_PIN_BUTTON_2;
			PIN_BUTTON_3 = V1_PIN_BUTTON_3;
			PIN_RFRX = V1_PIN_RFRX;
			PIN_RFTX = V1_PIN_RFTX;
			PIN_IOEXP_INT = V1_PIN_IOEXP_INT;
			PIN_BOOST = V1_PIN_BOOST;
			PIN_BOOST_EN = V1_PIN_BOOST_EN;
			PIN_LATCH_COM = V1_PIN_LATCH_COM;
			PIN_SENSOR1 = V1_PIN_SENSOR1;
			PIN_SENSOR2 = V1_PIN_SENSOR2;
		} else { // revision 2 and above
			// conditions for revision 4:
			// * has I2C EEPROM @ 0x52? --> OS 3.4 AC (skipping 0x51 due to conflict with PCF8563)
			// * has CH224A/Q @ both 0x22, 0x23? --> OS 3.4 DC
			bool has_eeprom_2 = detect_i2c(EEPROM_I2CADDR+2);
			bool has_ch224_0 = detect_i2c(CH224_I2CADDR);
			bool has_ch224_1 = detect_i2c(CH224_I2CADDR+1);
			if(has_eeprom_2 || (has_ch224_0 && has_ch224_1)) {
				hw_rev = 4;
				drio = new PCA9555(ACDR_I2CADDR); // all OS 3.4 models have IOEXP at 0x21 due to address conflicts with CH224
				mainio = drio;
				if(has_ch224_0 && has_ch224_1) {
					hw_type = HW_TYPE_DC;
					usbpd.begin();
				} else {
					hw_type = HW_TYPE_AC;
				}
			} else {
				if(detect_i2c(EEPROM_I2CADDR)) { // revision 3 has an I2C EEPROM at this address
					hw_rev = 3;
				} else {
					hw_rev = 2;
				}
				if(detect_i2c(ACDR_I2CADDR)) {
					hw_type = HW_TYPE_AC;
					drio = new PCA9555(ACDR_I2CADDR);
				} else if(detect_i2c(DCDR_I2CADDR)) {
					hw_type = HW_TYPE_DC;
					drio = new PCA9555(DCDR_I2CADDR);
				} else if(detect_i2c(LADR_I2CADDR)) {
					hw_type = HW_TYPE_LATCH;
					drio = new PCA9555(LADR_I2CADDR);
				} else {
					hw_type = HW_TYPE_UNKNOWN;
					drio = new PCA9555(ACDR_I2CADDR);
				}
			}
			mainio = drio;

			mainio->i2c_write(NXP_CONFIG_REG, V2_IO_CONFIG);
			mainio->i2c_write(NXP_OUTPUT_REG, V2_IO_OUTPUT);

			PIN_BUTTON_1 = V2_PIN_BUTTON_1;
			PIN_BUTTON_2 = V2_PIN_BUTTON_2;
			PIN_BUTTON_3 = V2_PIN_BUTTON_3;
			PIN_RFTX = V2_PIN_RFTX;
			PIN_BOOST = V2_PIN_BOOST;
			PIN_BOOST_EN = V2_PIN_BOOST_EN;
			PIN_LATCH_COMK = V2_PIN_LATCH_COMK; // os3.2latch uses H-bridge separate cathode and anode design
			PIN_LATCH_COMA = V2_PIN_LATCH_COMA;
			PIN_SENSOR1 = V2_PIN_SENSOR1;
			PIN_SENSOR2 = V2_PIN_SENSOR2;
			if(hw_rev == 4) {
				// SN3/SN4 are wired to IO expander pins on OS 3.4 only.
				// Earlier rev2/rev3 boards leave PIN_SENSOR3/4 at the default 255 sentinel.
				PIN_SENSOR3 = V2_PIN_SENSOR3;
				PIN_SENSOR4 = V2_PIN_SENSOR4;
			}
		}
	}

	/* detect expanders */
	for(unsigned char i=0;i<(MAX_NUM_BOARDS)/2;i++)
		expanders[i] = NULL;
	detect_expanders();

#endif

#if defined(OSPI)
	Bus.begin(); // init I2C for OSPI

	// Configure the station shift register before loading the initial all-off state.
	pinMode(PIN_SR_OE, OUTPUT);
	digitalWrite(PIN_SR_OE, HIGH); // disable outputs during setup
	pinMode(PIN_SR_LATCH, OUTPUT);
	digitalWrite(PIN_SR_LATCH, HIGH);
	pinMode(PIN_SR_CLOCK, OUTPUT);

	pin_sr_data = PIN_SR_DATA;
	unsigned int rev = detect_rpi_rev();
	if (rev == 0x0002 || rev == 0x0003) {
		pin_sr_data = PIN_SR_DATA_ALT; // RPi 1 revision 1 boards
	}
	pinMode(pin_sr_data, OUTPUT);

	pinModeExt(PIN_BUTTON_1, INPUT_PULLUP);
	pinModeExt(PIN_BUTTON_2, INPUT_PULLUP);
	pinModeExt(PIN_BUTTON_3, INPUT_PULLUP);
#endif

	// init masters_last_on array
	memset(masters_last_on, 0, sizeof(masters_last_on));
	// Reset all stations
	clear_all_station_bits();
	apply_all_station_bits();

#if defined(ESP8266)
	// Unavailable sensor pins remain 255, which pinModeExt safely ignores.
	pinModeExt(PIN_SENSOR1, INPUT_PULLUP);
	pinModeExt(PIN_SENSOR2, INPUT_PULLUP);
	pinModeExt(PIN_SENSOR3, INPUT_PULLUP);
	pinModeExt(PIN_SENSOR4, INPUT_PULLUP);

#else
	// pull shift register OE low to enable output
	digitalWrite(PIN_SR_OE, LOW);
	// Rain sensor port set up
	pinMode(PIN_SENSOR1, INPUT_PULLUP);
	pinMode(PIN_SENSOR2, INPUT_PULLUP);
#endif

	// Default controller status variables
	// Static variables are assigned 0 by default
	// so only need to initialize non-zero ones
	status.enabled = 1;
	status.safe_reboot = 0;

	old_status = status;

	nvdata.sunrise_time = 360;  // 6:00am default sunrise
	nvdata.sunset_time = 1080;  // 6:00pm default sunset
	nvdata.reboot_cause = REBOOT_CAUSE_POWERON;

	nboards = 1;
	nstations = nboards*8;

	// set rf data pin, unless it is not being used
	if(PIN_RFTX != 255) {
		pinModeExt(PIN_RFTX, OUTPUT);
		digitalWriteExt(PIN_RFTX, LOW);
	}

#if defined(ESP8266)
		status.has_curr_sense = 1;  // OS3.0 has current sensing capacility
		// measure baseline current
		baseline_current = 80;
#endif

#if defined(USE_DISPLAY)
	lcd_start();
	lcd.createChar(ICON_ETHER_CONNECTED, _iconimage_ether_connected);
	lcd.createChar(ICON_ETHER_DISCONNECTED, _iconimage_ether_disconnected);
	lcd.createChar(ICON_WIFI_CONNECTED, _iconimage_wifi_connected);
	lcd.createChar(ICON_WIFI_DISCONNECTED, _iconimage_wifi_disconnected);
	lcd.createChar(ICON_REMOTEXT, _iconimage_remotext);
	lcd.createChar(ICON_RAINDELAY, _iconimage_raindelay);
	lcd.createChar(ICON_RAIN, _iconimage_rain);
	lcd.createChar(ICON_SOIL, _iconimage_soil);
#endif

#if defined(ESP8266)
	lcd.setCursor(0,0);
	lcd.print(F("Init file system"));
	lcd.setCursor(0,1);
	if(!LittleFS.begin()) {
		// !!! flash init failed, stall as we cannot proceed
		lcd.setCursor(0, 0);
		lcd_print_pgm(PSTR("Error Code: 0x2D"));
		delay(5000);
	}

	state = OS_STATE_INITIAL;

	// set button pins
	// enable internal pullup
	pinModeExt(PIN_BUTTON_1, INPUT_PULLUP);
	pinModeExt(PIN_BUTTON_2, INPUT_PULLUP);
	pinModeExt(PIN_BUTTON_3, INPUT_PULLUP);

	// detect and check RTC type
	RTC.detect();

#else
	//DEBUG_PRINTLN(get_runtime_path());
#endif

	for (size_t i = 0; i < 4; i++) {
		uint8_t address = 0x48 + i;
#if defined(ADS1115_HARDWARE)
		if (detect_i2c(address)) {
			ads1115_devices[i] = new ADS1115(address);
		}
#else
		// DEMO/SIM: instantiate all four mock chips so all 16 channels are available
		ads1115_devices[i] = new ADS1115(address);
#endif
	}
}

#if defined(ESP8266)
/** Setup PD voltage
 *
 */
void OpenSprinkler::setup_pd_voltage() {
	actual_pd_voltage = 0;
	if(!(hw_rev==4 && hw_type==HW_TYPE_DC)) return;
	if(usbpd.update_power_data()) {
		uint16_t tpdv = iopts[IOPT_TARGET_PD_VOLTAGE];
		if(tpdv < 50) tpdv = DEFAULT_TARGET_PD_VOLTAGE; // anything below 5.0V will be force converted to default tpdv
		usbpd.request_voltage_closest(tpdv*100);
		delay(200);
		actual_pd_voltage = usbpd.get_output_voltage_mv()/100; // read back the actual voltage
	} else {
		// the power source does not support PD
	}
}

/** LATCH boost voltage
 *
 */
void OpenSprinkler::latch_boost(int8_t volt) {
	// if volt is negative or larger than max volt, ignore it and boost according to BOOST_TIME only
	if(volt<0 || volt>iopt_get_max(IOPT_LATCH_ON_VOLTAGE)) {
		digitalWriteExt(PIN_BOOST, HIGH);      // enable boost converter
		delay((int)iopts[IOPT_BOOST_TIME]<<2); // wait for booster to charge
		digitalWriteExt(PIN_BOOST, LOW);       // disable boost converter
	} else {
		if(volt == 0) volt = DEFAULT_LATCH_BOOST_VOLTAGE;
		// boost to specified volt, up to time specified by BOOST_TIME
		uint16_t top = (uint16_t)(volt * 19.25f); // ADC = 1024 * volt * 1.5k / 79.8k
		if(analogRead(PIN_CURR_SENSE)>=top) return; // if the voltage has already reached top, return right away
		uint32_t boost_timeout = millis() + (iopts[IOPT_BOOST_TIME]<<2);
		digitalWriteExt(PIN_BOOST, HIGH);
		// boost until either top voltage is reached or boost timeout is reached
		while((int32_t)((uint32_t)millis()-boost_timeout)<0 && analogRead(PIN_CURR_SENSE)<top) { // overflow proof
			delay(5);
		}
		digitalWriteExt(PIN_BOOST, LOW);
	}
}

/** Set all zones (for LATCH controller)
 *  This function sets all zone pins (including COM) to a specified value
 */
void OpenSprinkler::latch_setallzonepins(unsigned char value) {
	digitalWriteExt(PIN_LATCH_COM, value);  // set latch com pin
	// Handle driver board (on main controller)
	if(drio->type==IOEXP_TYPE_9555) { // LATCH contorller only uses PCA9555, no other type
		uint16_t reg = drio->i2c_read(NXP_OUTPUT_REG);  // read current output reg value
		if(value) reg |= 0x00FF;  // first 8 zones are the lowest 8 bits of main driver board
		else reg &= 0xFF00;
		drio->i2c_write(NXP_OUTPUT_REG, reg); // write value to register
	}
	// Handle all expansion boards
	for(unsigned char i=0;i<MAX_EXT_BOARDS/2;i++) {
		if(expanders[i]->type==IOEXP_TYPE_9555) {
			expanders[i]->i2c_write(NXP_OUTPUT_REG, value?0xFFFF:0x0000);
		}
	}
}

void OpenSprinkler::latch_disable_alloutputs_v2() {
	digitalWriteExt(PIN_LATCH_COMA, LOW);
	digitalWriteExt(PIN_LATCH_COMK, LOW);

	// latch v2 has a pca9555 the lowest 8 bits of which control all h-bridge anode pins
	drio->i2c_write(NXP_OUTPUT_REG, drio->i2c_read(NXP_OUTPUT_REG) & 0xFF00);
	// latch v2 has a 74hc595 which controls all h-bridge cathode pins
	drio->shift_out(V2_PIN_SRLAT, V2_PIN_SRCLK, V2_PIN_SRDAT, 0x00);

	// todo: handle latch expander
}

/** Set one zone (for LATCH controller)
 *  This function sets one specified zone pin to a specified value
 */
void OpenSprinkler::latch_setzonepin(unsigned char sid, unsigned char value) {
	if(sid<8) { // on main controller
		if(drio->type==IOEXP_TYPE_9555) { // LATCH contorller only uses PCA9555, no other type
			uint16_t reg = drio->i2c_read(NXP_OUTPUT_REG);  // read current output reg value
			if(value) reg |= (1<<sid);
			else reg &= (~(1<<sid));
			drio->i2c_write(NXP_OUTPUT_REG, reg);  // write value to register
		}
	} else {  // on expander
		unsigned char bid=(sid-8)>>4;
		uint16_t s=(sid-8)&0x0F;
		if(expanders[bid]->type==IOEXP_TYPE_9555) {
			uint16_t reg = expanders[bid]->i2c_read(NXP_OUTPUT_REG);  // read current output reg value
			if(value) reg |= (1<<s);
			else reg &= (~(1<<s));
			expanders[bid]->i2c_write(NXP_OUTPUT_REG, reg);
		}
	}
}

void OpenSprinkler::latch_setzoneoutput_v2(unsigned char sid, unsigned char A, unsigned char K) {
	if(A==HIGH && K==HIGH) return; // A and K must not be HIGH at the same time

	if(sid<8) { // on main controller
		// v2 latch driver has one PCA9555, the lowest 8-bits of which control all anode pins
		// and one 74HC595, which controls all cathod pins
		uint16_t reg = drio->i2c_read(NXP_OUTPUT_REG);
		if(A) reg |= (1<<sid); // lowest 8 bits of 9555 control output anodes
		else reg &= (~(1<<sid));
		drio->i2c_write(NXP_OUTPUT_REG, reg);

		drio->shift_out(V2_PIN_SRLAT, V2_PIN_SRCLK, V2_PIN_SRDAT, K ? (1<<sid) : 0);

	} else { // on expander
		// todo: handle latch expander
	}
}

/** LATCH open / close a station
 *
 */
void OpenSprinkler::latch_open(unsigned char sid) {
	if(hw_rev>=2) {
		DEBUG_PRINTLN(F("latch_open_v2"));
		latch_disable_alloutputs_v2(); // disable all output pins
		DEBUG_PRINTLN(F("boost on voltage: "));
		DEBUG_PRINTLN(iopts[IOPT_LATCH_ON_VOLTAGE]);
		latch_boost(iopts[IOPT_LATCH_ON_VOLTAGE]); // generate boost voltage
		digitalWriteExt(PIN_LATCH_COMA, HIGH); // enable COM+
		latch_setzoneoutput_v2(sid, LOW, HIGH); // enable sid-
		digitalWriteExt(PIN_BOOST_EN, HIGH); // enable output path
		delay(150);
		digitalWriteExt(PIN_BOOST_EN, LOW); // disabled output boosted voltage path
		latch_disable_alloutputs_v2(); // disable all output pins
	} else {
		latch_boost();  // boost voltage
		latch_setallzonepins(HIGH);  // set all switches to HIGH, including COM
		latch_setzonepin(sid, LOW); // set the specified switch to LOW
		delay(1); // delay 1 ms for all gates to stablize
		digitalWriteExt(PIN_BOOST_EN, HIGH); // dump boosted voltage
		delay(100);                          // for 100ms
		latch_setzonepin(sid, HIGH);  // set the specified switch back to HIGH
		digitalWriteExt(PIN_BOOST_EN, LOW);  // disable boosted voltage
	}
}

void OpenSprinkler::latch_close(unsigned char sid) {
	if(hw_rev>=2) {
		DEBUG_PRINTLN(F("latch_close_v2"));
		latch_disable_alloutputs_v2(); // disable all output pins
		DEBUG_PRINTLN(F("boost off voltage: "));
		DEBUG_PRINTLN(iopts[IOPT_LATCH_OFF_VOLTAGE]);
		latch_boost(iopts[IOPT_LATCH_OFF_VOLTAGE]); // generate boost voltage
		latch_setzoneoutput_v2(sid, HIGH, LOW); // enable sid+
		digitalWriteExt(PIN_LATCH_COMK, HIGH); // enable COM-
		digitalWriteExt(PIN_BOOST_EN, HIGH); // enable output path
		delay(150);
		digitalWriteExt(PIN_BOOST_EN, LOW); // disable output boosted voltage path
		latch_disable_alloutputs_v2(); // disable all output pins
	} else {
		latch_boost();  // boost voltage
		latch_setallzonepins(LOW);  // set all switches to LOW, including COM
		latch_setzonepin(sid, HIGH);// set the specified switch to HIGH
		delay(1); // delay 1 ms for all gates to stablize
		digitalWriteExt(PIN_BOOST_EN, HIGH); // dump boosted voltage
		delay(100);                          // for 100ms
		latch_setzonepin(sid, LOW);  // set the specified switch back to LOW
		digitalWriteExt(PIN_BOOST_EN, LOW);  // disable boosted voltage
		latch_setallzonepins(HIGH);  // set all switches back to HIGH
	}
}

/**
 * LATCH version of apply_all_station_bits
 */
void OpenSprinkler::latch_apply_all_station_bits() {
	if(hw_type==HW_TYPE_LATCH && engage_booster) {
		for(unsigned char i=0;i<nstations;i++) {
			unsigned char bid=i>>3;
			unsigned char s=i&0x07;
			unsigned char mask=(unsigned char)1<<s;
			if(station_bits[bid] & mask) {
				if(prev_station_bits[bid] & mask) continue; // already set
				latch_open(i);
			} else {
				if(!(prev_station_bits[bid] & mask)) continue; // already reset
				latch_close(i);
			}
		}
		engage_booster = 0;
		memcpy(prev_station_bits, station_bits, MAX_NUM_BOARDS);
	}
}
#endif

/** Apply all station bits
 * !!! This will activate/deactivate valves !!!
 */
void OpenSprinkler::apply_all_station_bits(void (*post_activation_callback)()) {

#if defined(ESP8266)
	if(hw_type==HW_TYPE_LATCH) {
		// if controller type is latching, the control mechanism is different
		// hence will be handled separately
		latch_apply_all_station_bits();
	} else {
		// Handle DC booster
		if(hw_type==HW_TYPE_DC && engage_booster) {
			// for DC controller: boost voltage and enable output path
			digitalWriteExt(PIN_BOOST_EN, LOW);  // disfable output path
			digitalWriteExt(PIN_BOOST, HIGH);    // enable boost converter
			delay((int)iopts[IOPT_BOOST_TIME]<<2);  // wait for booster to charge
			digitalWriteExt(PIN_BOOST, LOW);  // disable boost converter
			digitalWriteExt(PIN_BOOST_EN, HIGH);  // enable output path
			engage_booster = 0;
		}

		// Handle driver board (on main controller)
		if(drio->type==IOEXP_TYPE_9555) {
			/* revision >= 1 uses PCA9555 with active high logic */
			uint16_t reg = drio->i2c_read(NXP_OUTPUT_REG);  // read current output reg value
			reg = (reg&0xFF00) | station_bits[0]; // output channels are the low 8-bit
			drio->i2c_write(NXP_OUTPUT_REG, reg); // write value to register
		} else if(drio->type==IOEXP_TYPE_8574) {
			/* revision 0 uses PCF8574 with active low logic, so all bits must be flipped */
			drio->i2c_write(NXP_OUTPUT_REG, ~station_bits[0]);
		}

		// Handle expansion boards
		for(int i=0;i<MAX_EXT_BOARDS/2;i++) {
			uint16_t data = station_bits[i*2+2];
			data = (data<<8) + station_bits[i*2+1];
			if(expanders[i]->type==IOEXP_TYPE_9555) {
				expanders[i]->i2c_write(NXP_OUTPUT_REG, data);
			} else {
				expanders[i]->i2c_write(NXP_OUTPUT_REG, ~data);
			}
		}
	}

#else
	digitalWrite(PIN_SR_LATCH, LOW);
	unsigned char bid, s, sbits;

	// Shift out all station bit values
	// from the highest bit to the lowest
	for(bid=0;bid<=MAX_EXT_BOARDS;bid++) {
		if (status.enabled) // TODO: checking enabled bit here is inconsistent with Arduino implementation
			sbits = station_bits[MAX_EXT_BOARDS-bid];
		else
			sbits = 0;

		for(s=0;s<8;s++) {
			digitalWrite(PIN_SR_CLOCK, LOW);
	#if defined(OSPI) // if OSPI, use dynamically assigned pin_sr_data
			digitalWrite(pin_sr_data, (sbits & ((unsigned char)1<<(7-s))) ? HIGH : LOW );
	#else
			digitalWrite(PIN_SR_DATA, (sbits & ((unsigned char)1<<(7-s))) ? HIGH : LOW );
	#endif
			digitalWrite(PIN_SR_CLOCK, HIGH);
		}
	}
	digitalWrite(PIN_SR_LATCH, HIGH);

#endif

	// If a post activation callback function is defined, call it here
	if(post_activation_callback) post_activation_callback();

	if(iopts[IOPT_SPE_AUTO_REFRESH]) {
		// handle refresh of RF and remote stations
		// we refresh the station that's next in line
		static unsigned char next_sid_to_refresh = MAX_NUM_STATIONS>>1;
		static unsigned char lastnow = 0;
		time_os_t curr_time = now_tz();
		unsigned char _now = (curr_time & 0xFF);
		if (lastnow != _now) {  // perform this no more than once per second
			lastnow = _now;
			next_sid_to_refresh = (next_sid_to_refresh+1) % MAX_NUM_STATIONS;
			unsigned char bid=next_sid_to_refresh>>3,s=next_sid_to_refresh&0x07;
			if(os.attrib_spe[bid]&(1<<s)) { // check if this is a special station
				bid=next_sid_to_refresh>>3;
				s=next_sid_to_refresh&0x07;
				bool on = (station_bits[bid]>>s)&0x01;
				uint32_t dur = 0;
				if(on) {
					unsigned char sqi=pd.station_qid[next_sid_to_refresh];
					RuntimeQueueStruct *q=pd.queue+sqi;
					if(sqi<255 && q->st>0 && q->st+q->dur>curr_time) {
						dur = q->st+q->dur-curr_time;
					}
				}
				switch_special_station(next_sid_to_refresh, on, dur);
			}
		}
	}
}

/** Read rain/soil sensor status across all binary sensors. */
void OpenSprinkler::detect_binarysensor_status(time_os_t curr_time) {
	// option byte: 0 = normally closed, 1 = normally open
	for (uint8_t i = 0; i < NUM_SENSORS; i++) {
		if (!sensor_available(i)) continue;
		uint8_t type = iopts[sensor_iopt_keys[i].type];
		if (type != SENSOR_TYPE_RAIN && type != SENSOR_TYPE_SOIL) continue;

		// SN1/SN2 GPIO pins need INPUT_PULLUP on OS 3.2+. SN3/SN4 are on
		// the I/O expander where pinMode() is a no-op anyway, so the call
		// is safe regardless.
		if (i < 2 && hw_rev >= 2) pinMode(sensor_pin(i), INPUT_PULLUP);

		unsigned char val = digitalReadExt(sensor_pin(i));
		sn_sensors[i].raw = (val == iopts[sensor_iopt_keys[i].option]) ? 0 : 1;

		if (sn_sensors[i].raw) {
			if (!sn_sensors[i].on_timer) {
				// add minimum of 5 seconds on delay
				uint32_t delay_time = (uint32_t)iopts[sensor_iopt_keys[i].on_delay] * 60;
				sn_sensors[i].on_timer = curr_time + (delay_time > 5 ? delay_time : 5);
				sn_sensors[i].off_timer = 0;
			} else if (curr_time > sn_sensors[i].on_timer) {
				sn_sensors[i].active = 1;
			}
		} else {
			if (!sn_sensors[i].off_timer) {
				uint32_t delay_time = (uint32_t)iopts[sensor_iopt_keys[i].off_delay] * 60;
				sn_sensors[i].off_timer = curr_time + (delay_time > 5 ? delay_time : 5);
				sn_sensors[i].on_timer = 0;
			} else if (curr_time > sn_sensors[i].off_timer) {
				sn_sensors[i].active = 0;
			}
		}
	}
}

/** Return program switch status; bit i corresponds to sensor i+1.
 * 4-sample debounce: triggers on pattern 0011 (two consecutive lows
 * followed by two consecutive highs). */
unsigned char OpenSprinkler::detect_programswitch_status(time_os_t curr_time) {
	(void)curr_time;
	static unsigned char sensor_hist[NUM_SENSORS] = {0};
	unsigned char ret = 0;
	for (uint8_t i = 0; i < NUM_SENSORS; i++) {
		if (!sensor_available(i)) continue;
		if (iopts[sensor_iopt_keys[i].type] != SENSOR_TYPE_PSWITCH) continue;

		if (i < 2 && hw_rev >= 2) pinMode(sensor_pin(i), INPUT_PULLUP);

		sn_sensors[i].raw = (digitalReadExt(sensor_pin(i)) != iopts[sensor_iopt_keys[i].option]);
		sensor_hist[i] = (sensor_hist[i] << 1) | sn_sensors[i].raw;
		if ((sensor_hist[i] & 0b1111) == 0b0011) {
			ret |= (1 << i);
		}
	}
	return ret;
}

void OpenSprinkler::sensor_resetall() {
	for (uint8_t i = 0; i < NUM_SENSORS; i++) {
		sn_sensors[i].on_timer = 0;
		sn_sensors[i].off_timer = 0;
		sn_sensors[i].active_lasttime = 0;
		sn_sensors[i].active = 0;
		sn_sensors[i].prev_active = 0;
	}
}

/** Read current sensing value
 * OpenSprinkler 2.3 and above have a 0.2 ohm current sensing resistor.
 * Therefore the conversion from analog reading to milli-amp is:
 * (r/1024)*3.3*1000/0.2 (DC-powered controller)
 * AC-powered controller has a built-in precision rectifier to sense
 * the peak AC current. Therefore the actual current is discounted by 0.707
 * ESP8266's analog reference voltage is 1.0 instead of 3.3, therefore
 * it's further discounted by 1/3.3
 */
#if defined(ESP8266)
uint16_t OpenSprinkler::read_current(bool use_ema) {
	static uint16_t ema = 0; // exponential moving average
	static float scale = -1;
	if(scale < 0) { // assign scale upon first call of this function
		if (hw_type == HW_TYPE_DC) {
			#if defined(ESP8266)
			scale = 4.88;
			#else
			scale = 16.11;
			#endif
		} else if (hw_type == HW_TYPE_AC) {
			#if defined(ESP8266)
			scale = 3.45;
			#else
			scale = 11.39;
			#endif
		} else {
			scale = 0.0;  // for other controllers, current is 0
		}
	}
	uint16_t curr = analogRead(PIN_CURR_SENSE)*scale;
	ema = curr / 5 + ema * 4 / 5; // using alpha=0.2 for exponential moving average
	return use_ema ? ema : curr;
}
#endif

/** Read the number of 8-station expansion boards */
// Arduino has capability to detect number of expansion boards
int OpenSprinkler::detect_exp() {
#if defined(ESP8266)
	// detect the highest expansion board index
	int n;
	for(n=4;n>=0;n--) {
		if(detect_i2c(EXP_I2CADDR_BASE+n)) break;
	}
	return (n+1)*2;
#else
	return -1;
#endif
}

/** Convert hex code to uint32_t integer */
static uint32_t hex2uint32_t(unsigned char *code, unsigned char len) {
	char c;
	uint32_t v = 0;
	for(unsigned char i=0;i<len;i++) {
		c = code[i];
		v <<= 4;
		if(c>='0' && c<='9') {
			v += (c-'0');
		} else if (c>='A' && c<='F') {
			v += 10 + (c-'A');
		} else if (c>='a' && c<='f') {
			v += 10 + (c-'a');
		} else {
			return 0;
		}
	}
	return v;
}

/** Parse RF  station data into code */
bool OpenSprinkler::parse_rfstation_code(RFStationData *data, RFStationCode *code) {
	if(!code) return false;
	code->timing = 0; // temporarily set it to 0
	if(data->version=='H') {
		// this is version G rf code data (25 bytes long including version signature at the beginning)
		code->on = hex2uint32_t(data->on, sizeof(data->on));
		code->off = hex2uint32_t(data->off, sizeof(data->off));
		code->timing = hex2uint32_t(data->timing, sizeof(data->timing));
		code->protocol = hex2uint32_t(data->protocol, sizeof(data->protocol));
		code->bitlength = hex2uint32_t(data->bitlength, sizeof(data->bitlength));
	} else {
		// this is classic rf code data (16 bytes long, assuming protocol=1 and bitlength=24)
		RFStationDataClassic *classic = (RFStationDataClassic*)data;
		code->on = hex2uint32_t(classic->on, sizeof(classic->on));
		code->off = hex2uint32_t(classic->off, sizeof(classic->off));
		code->timing = hex2uint32_t(classic->timing, sizeof(classic->timing));
		code->protocol = 1;
		code->bitlength = 24;
	}
	if(!code->timing) return false;
	return true;
}

/** Get station data */
void OpenSprinkler::get_station_data(unsigned char sid, StationData* data) {
	file_read_block(STATIONS_FILENAME, data, (uint32_t)sid*sizeof(StationData), sizeof(StationData));
}

/** Set station data */
/*
void OpenSprinkler::set_station_data(unsigned char sid, StationData* data) {
	file_write_block(STATIONS_FILENAME, data, (uint32_t)sid*sizeof(StationData), sizeof(StationData));
}
*/

/** Get station name */
void OpenSprinkler::get_station_name(unsigned char sid, char tmp[]) {
	tmp[STATION_NAME_SIZE]=0;
	file_read_block(STATIONS_FILENAME, tmp, (uint32_t)sid*sizeof(StationData)+offsetof(StationData, name), STATION_NAME_SIZE);
}

/** Set station name */
void OpenSprinkler::set_station_name(unsigned char sid, char tmp[]) {
	tmp[STATION_NAME_SIZE]=0;
	char n0[STATION_NAME_SIZE+1];
	get_station_name(sid, n0);
	size_t len = strlen(n0);
	if(len!=strlen(tmp) || memcmp(n0, tmp, len)!=0) { // only write if the name has changed
		file_write_block(STATIONS_FILENAME, tmp, (uint32_t)sid*sizeof(StationData)+offsetof(StationData, name), STATION_NAME_SIZE);
	}
}

/** Get station type */
unsigned char OpenSprinkler::get_station_type(unsigned char sid) {
	return file_read_byte(STATIONS_FILENAME, (uint32_t)sid*sizeof(StationData)+offsetof(StationData, type));
}

unsigned char OpenSprinkler::is_sequential_station(unsigned char sid) {
	return attrib_grp[sid] < NUM_SEQ_GROUPS;
}

unsigned char OpenSprinkler::is_master_station(unsigned char sid) {
	for (unsigned char mas = 0; mas < NUM_MASTER_ZONES; mas++) {
		if (get_master_id(mas) && (get_master_id(mas) - 1 == sid)) {
			return 1;
		}
	}
	return 0;
}

unsigned char OpenSprinkler::is_running(unsigned char sid) {
	return station_bits[(sid >> 3)] >> (sid & 0x07) & 1;
}

unsigned char OpenSprinkler::get_master_id(unsigned char mas) {
	return masters[mas][MASOPT_SID];
}

int16_t OpenSprinkler::get_on_adj(unsigned char mas) {
	int16_t onadj = water_time_decode_signed(masters[mas][MASOPT_ON_ADJ]);
	return onadj ? onadj : -1; // if on adj is 0, modify it to -1 to stagger with station
}

int16_t OpenSprinkler::get_off_adj(unsigned char mas) {
	int16_t offadj = water_time_decode_signed(masters[mas][MASOPT_OFF_ADJ]);
	return offadj ? offadj : 1; // if off adj is 0, modify it to +1 to stagger with station
}

int16_t OpenSprinkler::get_imin() {
	return iopts[IOPT_I_MIN_THRESHOLD]*10;
}

int16_t OpenSprinkler::get_imax() {
	unsigned char i = iopts[IOPT_I_MAX_LIMIT];
	if(hw_type == HW_TYPE_DC) {
		return (i == 0) ? (DEFAULT_OVERCURRENT_LIMIT+OVERCURRENT_DC_EXTRA) : (i == 255 ? -1 : i*10);
	}
	return (i == 0) ? DEFAULT_OVERCURRENT_LIMIT : (i == 255 ? -1 : i*10);
}

unsigned char OpenSprinkler::bound_to_master(unsigned char sid, unsigned char mas) {
	unsigned char bid = sid >> 3;
	unsigned char s = sid & 0x07;
	unsigned char attributes = 0;

	switch (mas) {
		case MASTER_1:
			attributes = attrib_mas[bid];
			break;
		case MASTER_2:
			attributes = attrib_mas2[bid];
			break;
		case MASTER_3:
			attributes = attrib_mas3[bid];
			break;
		case MASTER_4:
			attributes = attrib_mas4[bid];
			break;
		default:
			break;
	}

	return attributes & (1 << s);
}

unsigned char OpenSprinkler::get_station_gid(unsigned char sid) {
	return attrib_grp[sid];
}

void OpenSprinkler::set_station_gid(unsigned char sid, unsigned char gid) {
	attrib_grp[sid] = gid;
}

/** Save all station attribs to file (backward compatibility) */
void OpenSprinkler::attribs_save() {
	// re-package attribute bits and save
	unsigned char bid, s, sid=0;
	StationAttrib at, at0;
	memset(&at, 0, sizeof(StationAttrib));
	unsigned char ty = STN_TYPE_STANDARD, ty0;
	for(bid=0;bid<MAX_NUM_BOARDS && sid<nstations;bid++) {
		for(s=0;s<8 && sid<nstations;s++,sid++) {
			at.mas = (attrib_mas[bid]>>s) & 1;
			at.igs = (attrib_igs[0][bid]>>s) & 1;
			at.mas2= (attrib_mas2[bid]>>s)& 1;
			at.igs2= (attrib_igs[1][bid]>>s) & 1;
			at.igs3= (attrib_igs[2][bid]>>s) & 1;
			at.igs4= (attrib_igs[3][bid]>>s) & 1;
			at.igrd= (attrib_igrd[bid]>>s) & 1;
			at.dis = (attrib_dis[bid]>>s) & 1;
			at.mas3= (attrib_mas3[bid]>>s) & 1;
			at.mas4= (attrib_mas4[bid]>>s) & 1;
			at.gid = get_station_gid(sid);
			set_station_gid(sid, at.gid);

			// only write if content has changed: this is important for LittleFS as otherwise the overhead is too large
			file_read_block(STATIONS_FILENAME, &at0, (uint32_t)sid*sizeof(StationData)+offsetof(StationData, attrib), sizeof(StationAttrib));
			if(memcmp(&at,&at0,sizeof(StationAttrib))!=0) {
				file_write_block(STATIONS_FILENAME, &at, (uint32_t)sid*sizeof(StationData)+offsetof(StationData, attrib), sizeof(StationAttrib)); // attribte bits are 1 byte long
			}
			if(attrib_spe[bid]>>s==0) {
				// if station special bit is 0, make sure to write type STANDARD
				// only write if content has changed
				file_read_block(STATIONS_FILENAME, &ty0, (uint32_t)sid*sizeof(StationData)+offsetof(StationData, type), 1);
				if(ty!=ty0) {
					file_write_block(STATIONS_FILENAME, &ty, (uint32_t)sid*sizeof(StationData)+offsetof(StationData, type), 1); // attribte bits are 1 byte long
				}
			}
		}
	}
}

/** Load all station attribs from file (backward compatibility) */
void OpenSprinkler::attribs_load() {
	// load and re-package attributes
	unsigned char bid, s, sid=0;
	StationAttrib at;
	unsigned char ty;
	memset(attrib_mas, 0, nboards);
	memset(attrib_igs, 0, sizeof(attrib_igs));
	memset(attrib_mas2, 0, nboards);
	memset(attrib_mas3, 0, nboards);
	memset(attrib_mas4, 0, nboards);
	memset(attrib_igrd, 0, nboards);
	memset(attrib_dis, 0, nboards);
	memset(attrib_spe, 0, nboards);
	memset(attrib_grp, 0, MAX_NUM_STATIONS);

	for(bid=0;bid<MAX_NUM_BOARDS;bid++) {
		for(s=0;s<8;s++,sid++) {
			file_read_block(STATIONS_FILENAME, &at, (uint32_t)sid*sizeof(StationData)+offsetof(StationData, attrib), sizeof(StationAttrib));
			attrib_mas[bid] |= (at.mas<<s);
			attrib_igs[0][bid] |= (at.igs<<s);
			attrib_mas2[bid]|= (at.mas2<<s);
			attrib_mas3[bid]|= (at.mas3<<s);
			attrib_mas4[bid]|= (at.mas4<<s);
			attrib_igs[1][bid] |= (at.igs2<<s);
			attrib_igs[2][bid] |= (at.igs3<<s);
			attrib_igs[3][bid] |= (at.igs4<<s);
			attrib_igrd[bid]|= (at.igrd<<s);
			attrib_dis[bid] |= (at.dis<<s);
			attrib_grp[sid] = at.gid;
			file_read_block(STATIONS_FILENAME, &ty, (uint32_t)sid*sizeof(StationData)+offsetof(StationData, type), 1);
			if(ty!=STN_TYPE_STANDARD) {
				attrib_spe[bid] |= (1<<s);
			}
		}
	}
}

/** verify if a string matches password */
unsigned char OpenSprinkler::password_verify(const char *pw) {
	return (file_cmp_block(SOPTS_FILENAME, pw, SOPT_PASSWORD*MAX_SOPTS_SIZE)==0) ? 1 : 0;
}

// ==================
// Schedule Functions
// ==================

/** Index of today's weekday (Monday is 0) */
unsigned char OpenSprinkler::weekday_today() {
	//return ((unsigned char)weekday()+5)%7; // Time::weekday() assumes Sunday is 1
#if defined(ESP8266)
	uint32_t wd = now_tz() / 86400L;
	return (wd+3) % 7;	// Jan 1, 1970 is a Thursday
#else
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	return (tm->tm_wday+6) % 7;
#endif
}

/** Switch special station */
void OpenSprinkler::switch_special_station(unsigned char sid, unsigned char value, uint32_t dur) {
	// check if this is a special station
	unsigned char bid=sid>>3,s=sid&0x07;
	if(!(os.attrib_spe[bid]&(1<<s))) return; // if this is not a special stations
	unsigned char stype = get_station_type(sid);
	if(stype!=STN_TYPE_STANDARD) {
		// read station data
		StationData *pdata=(StationData*) tmp_buffer;
		get_station_data(sid, pdata);
		switch(stype) {

		case STN_TYPE_RF:
			switch_rfstation((RFStationData *)pdata->sped, value);
			break;

		case STN_TYPE_REMOTE_IP:
			switch_remotestation((RemoteIPStationData *)pdata->sped, value, dur);
			break;

		case STN_TYPE_REMOTE_OTC:
			switch_remotestation((RemoteOTCStationData *)pdata->sped, value, dur);
			break;

		case STN_TYPE_GPIO:
			switch_gpiostation((GPIOStationData *)pdata->sped, value);
			break;

		case STN_TYPE_HTTP:
			switch_httpstation((HTTPStationData *)pdata->sped, value, false);
			break;

		case STN_TYPE_HTTPS:
			switch_httpstation((HTTPStationData *)pdata->sped, value, true);
			break;

		}
	}
}

/** Set station bit
 * This function sets/resets the corresponding station bit variable
 * You have to call apply_all_station_bits next to apply the bits
 * (which results in physical actions of opening/closing valves).
 */
unsigned char OpenSprinkler::set_station_bit(unsigned char sid, unsigned char value, uint32_t dur) {
	unsigned char *data = station_bits+(sid>>3);  // pointer to the station byte
	unsigned char mask = (unsigned char)1<<(sid&0x07); // mask
	if (value) {
		if((*data)&mask) return 0;  // if bit is already set, return no change
		else {
			(*data) = (*data) | mask;
			engage_booster = true; // if bit is changing from 0 to 1, set engage_booster
			curr_alert_sid = sid+1; // record the zone that's turning on (starting from 1)
			switch_special_station(sid, 1, dur); // handle special stations
			return 1;
		}
	} else {
		if(!((*data)&mask)) return 0; // if bit is already reset, return no change
		else {
			(*data) = (*data) & (~mask);
			if(hw_type == HW_TYPE_LATCH) {
				engage_booster = true;  // if LATCH controller, engage booster when bit changes
			}
			switch_special_station(sid, 0); // handle special stations
			return 255;
		}
	}
	return 0;
}

unsigned char OpenSprinkler::get_station_bit(unsigned char sid) {
	unsigned char *data = station_bits+(sid>>3); // pointer to the station byte
	unsigned char mask = (unsigned char)1<<(sid&0x07); // mask
	if ((*data)&mask) return 1;
	else return 0;
}

/** Clear all station bits */
void OpenSprinkler::clear_all_station_bits() {
	unsigned char sid;
	for(sid=0;sid<MAX_NUM_STATIONS;sid++) {
		set_station_bit(sid, 0);
	}
}

/** Switch RF station
 * This function takes a RF code,
 * parses it into signals and timing,
 * and sends it out through RF transmitter.
 */
void OpenSprinkler::switch_rfstation(RFStationData *data, bool turnon) {
	RFStationCode code;
	if(!parse_rfstation_code(data, &code)) return; // return if the timing parameter is 0

	if(PIN_RFTX == 255) return; // ignore RF station if RF pin disabled

	rfswitch.enableTransmit(PIN_RFTX);
	rfswitch.setProtocol(code.protocol);
	rfswitch.setPulseLength(code.timing);
	rfswitch.send(turnon ? code.on : code.off, code.bitlength);
}

/** Switch GPIO station
 * Special data for GPIO Station is three bytes of ascii decimal (not hex)
 * First two bytes are zero padded GPIO pin number.
 * Third byte is either 0 or 1 for active low (GND) or high (+5V) relays
 */
void OpenSprinkler::switch_gpiostation(GPIOStationData *data, bool turnon) {
	unsigned char gpio = (data->pin[0] - '0') * 10 + (data->pin[1] - '0');
	unsigned char activeState = data->active - '0';

	pinMode(gpio, OUTPUT);
	if (turnon)
		digitalWrite(gpio, activeState);
	else
		digitalWrite(gpio, 1-activeState);
}

/** Callback function for switching remote station */
void default_http_callback(char* buffer) {

	DEBUG_PRINTLN(buffer);

}

int8_t OpenSprinkler::send_http_request(const char* server, uint16_t port, char* p, void(*callback)(char*), bool usessl, uint16_t timeout) {

	if(server == NULL || server[0]==0 || port==0 ) { // sanity checking
		DEBUG_PRINTLN("server:port is invalid!");
		return HTTP_RQT_CONNECT_ERR;
	}
#if defined(ESP8266)

	Client *client = NULL;
	if(usessl) {
		WiFiClientSecure *_c = new WiFiClientSecure();
		_c->setInsecure();
		bool mfln = _c->probeMaxFragmentLength(server, port, 512);
		DEBUG_PRINTF("MFLN supported: %s\n", mfln ? "yes" : "no");
		if (mfln) {
			_c->setBufferSizes(512, 512);
		} else {
			_c->setBufferSizes(2048, 2048);
		}
		client = _c;
	} else {
		client = new WiFiClient();
	}

	#define HTTP_CONNECT_NTRIES 3
	unsigned char tries = 0;
	do {
		DEBUG_PRINT(server);
		DEBUG_PRINT(":");
		DEBUG_PRINT(port);
		DEBUG_PRINT("(");
		DEBUG_PRINT(tries);
		DEBUG_PRINTLN(")");
		if(client->connect(server, port)==1) break;
		tries++;
	} while(tries<HTTP_CONNECT_NTRIES);

	if(tries==HTTP_CONNECT_NTRIES) {
		DEBUG_PRINTLN(F("failed."));
		client->stop();
		delete client;
		return HTTP_RQT_CONNECT_ERR;
	}
#else
	EthernetClient *client = NULL;

	if (usessl) {
		client = new EthernetClientSsl();
	} else {
		client = new EthernetClient();
	}

	DEBUG_PRINT(server);
	DEBUG_PRINT(":");
	DEBUG_PRINTLN(port);
	if(!client->connect(server, port)) {
		DEBUG_PRINT(F("failed."));
		client->stop();
		delete client;
		return HTTP_RQT_CONNECT_ERR;
	}

#endif

	uint16_t len = strlen(p);
	if(len > ETHER_BUFFER_SIZE) len = ETHER_BUFFER_SIZE;
	if(client->connected()) {
		client->write((uint8_t *)p, len);
	} else {
		DEBUG_PRINTLN(F("client no longer connected"));
	}
	memset(ether_buffer, 0, ETHER_BUFFER_SIZE);
	uint32_t stoptime = millis()+timeout;

	int pos = 0;
#if defined(ESP8266)
	// with ESP8266 core 3.0.2, client->connected() is not always true even if there is more data
	// so this loop is going to take longer than it should be
	// todo: can consider using HTTPClient for ESP8266
	while(true) {
		int nbytes = client->available();
		if(nbytes>0) {
			int remaining = ETHER_BUFFER_SIZE-1-pos;
			if(remaining<=0) break;
			if(nbytes>remaining) nbytes=remaining;
			int bytes_read = client->read((uint8_t*)ether_buffer+pos, nbytes);
			if(bytes_read>0) pos+=bytes_read;
		}
		if((int32_t)((uint32_t)millis()-stoptime)>0) { // overflow proof
			DEBUG_PRINTLN(F("host timeout occured"));
			//return HTTP_RQT_TIMEOUT; // instead of returning with timeout, we'll work with data received so far
			break;
		}
		if(!client->connected() && !client->available()) {
			//DEBUG_PRINTLN(F("host disconnected"));
			break;
		}
	}
#else
	int bytes_read = client->read((uint8_t *)ether_buffer+pos, ETHER_BUFFER_SIZE-1);
	if(bytes_read>0) pos += bytes_read;

#endif
	ether_buffer[pos]=0; // properly end buffer with 0
	client->stop();
	delete client;
	if(strlen(ether_buffer)==0) return HTTP_RQT_EMPTY_RETURN;
	if(callback) callback(ether_buffer);
	return HTTP_RQT_SUCCESS;
}

int8_t OpenSprinkler::send_http_request(uint32_t ip4, uint16_t port, char* p, void(*callback)(char*), bool usessl, uint16_t timeout) {
	char server[20];
	unsigned char ip[4];
	ip[0] = ip4>>24;
	ip[1] = (ip4>>16)&0xff;
	ip[2] = (ip4>>8)&0xff;
	ip[3] = ip4&0xff;
	snprintf(server, 20, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
	return send_http_request(server, port, p, callback, usessl, timeout);
}

int8_t OpenSprinkler::send_http_request(char* server_with_port, char* p, void(*callback)(char*), bool usessl, uint16_t timeout) {
	char * server = strtok(server_with_port, ":");
	char * port = strtok(NULL, ":");
	return send_http_request(server, (port==NULL)?80:atoi(port), p, callback, usessl, timeout);
}

/** Switch remote IP station
 * This function takes a remote station code,
 * parses it into remote IP, port, station index,
 * and makes a HTTP GET request.
 * The remote controller is assumed to have the same
 * password as the main controller
 */
void OpenSprinkler::switch_remotestation(RemoteIPStationData *data, bool turnon, uint32_t dur) {
	RemoteIPStationData copy;
	memcpy((char*)&copy, (char*)data, sizeof(RemoteIPStationData));

	uint32_t ip4 = hex2uint32_t(copy.ip, sizeof(copy.ip));
	uint16_t port = (uint16_t)hex2uint32_t(copy.port, sizeof(copy.port));

	unsigned char ip[4];
	ip[0] = ip4>>24;
	ip[1] = (ip4>>16)&0xff;
	ip[2] = (ip4>>8)&0xff;
	ip[3] = ip4&0xff;

	char *p = tmp_buffer;
	BufferFiller bf = BufferFiller(p, TMP_BUFFER_ALLOC_SIZE);
	// if turning on the zone and duration is defined, give duration as the timer value
	// otherwise:
	//   if autorefresh is defined, we give a fixed duration each time, and auto refresh will renew it periodically
	//   if no auto refresh, we will give the maximum allowed duration, and station will be turned off when off command is sent
	uint32_t timer = 0;
	if(turnon) {
		if(dur>0) {
			timer = dur > MAX_PROGRAMMED_DURATION ? MAX_PROGRAMMED_DURATION : dur;
		} else {
			timer = iopts[IOPT_SPE_AUTO_REFRESH]?4*MAX_NUM_STATIONS:MAX_PROGRAMMED_DURATION;
		}
	}
	bf.emit_p(PSTR("GET /cm?pw=$O&sid=$D&en=$D&t=$L"),
						SOPT_PASSWORD,
						(int)hex2uint32_t(copy.sid, sizeof(copy.sid)),
						turnon, (uint32_t)timer);
	bf.emit_p(PSTR(" HTTP/1.0\r\nHOST: $D.$D.$D.$D\r\n"),
						ip[0],ip[1],ip[2],ip[3]);

	bf.emit_p(PSTR("User-Agent: $S\r\n\r\n"), user_agent_string);

	char server[20];
	snprintf(server, 20, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
	send_http_request(server, port, p, default_http_callback);
}

/** Switch remote OTC station
 * This function takes a remote station code,
 * parses it into OTC token and station index,
 * and makes a HTTPS GET request.
 * The remote controller is assumed to have the same
 * password as the main controller
 */
void OpenSprinkler::switch_remotestation(RemoteOTCStationData *data, bool turnon, uint32_t dur) {
	RemoteOTCStationData copy;
	memcpy((char*)&copy, (char*)data, sizeof(RemoteOTCStationData));
	copy.token[sizeof(copy.token)-1] = 0; // ensure the string ends properly
	char *p = tmp_buffer;
	BufferFiller bf = BufferFiller(p, TMP_BUFFER_ALLOC_SIZE);
	// if turning on the zone and duration is defined, give duration as the timer value
	// otherwise:
	//   if autorefresh is defined, we give a fixed duration each time, and auto refresh will renew it periodically
	//   if no auto refresh, we will give the maximum allowed duration, and station will be turned off when off command is sent
	uint32_t timer = 0;
	if(turnon) {
		if(dur>0) {
			timer = dur > MAX_PROGRAMMED_DURATION ? MAX_PROGRAMMED_DURATION : dur;
		} else {
			timer = iopts[IOPT_SPE_AUTO_REFRESH]?4*MAX_NUM_STATIONS:MAX_PROGRAMMED_DURATION;
		}
	}
	bf.emit_p(PSTR("GET /forward/v1/$S/cm?pw=$O&sid=$D&en=$D&t=$L"),
						copy.token,
						SOPT_PASSWORD,
						(int)hex2uint32_t(copy.sid, sizeof(copy.sid)),
						turnon, (uint32_t)timer);
	bf.emit_p(PSTR(" HTTP/1.0\r\nHOST: $S\r\nConnection:close\r\n"), DEFAULT_OTC_SERVER_APP);

	bf.emit_p(PSTR("User-Agent: $S\r\n\r\n"), user_agent_string);

	send_http_request(DEFAULT_OTC_SERVER_APP, DEFAULT_OTC_PORT_APP, p, default_http_callback, true);
}

/** Switch http(s) station
 * This function takes an http(s) station code,
 * parses it into a server name and two HTTP GET requests.
 */
void OpenSprinkler::switch_httpstation(HTTPStationData *data, bool turnon, bool usessl) {

	HTTPStationData copy;
	// make a copy of the HTTP station data and work with it
	memcpy((char*)&copy, (char*)data, sizeof(HTTPStationData));
	char * server = strtok((char *)copy.data, ",");
	char * port = strtok(NULL, ",");
	char * on_cmd = strtok(NULL, ",");
	char * off_cmd = strtok(NULL, ",");
	char * cmd = turnon ? on_cmd : off_cmd;

	char *p = tmp_buffer;
	BufferFiller bf = BufferFiller(p, TMP_BUFFER_ALLOC_SIZE);

	if(cmd==NULL || server==NULL) return; // proceed only if cmd and server are valid

	bf.emit_p(PSTR("GET /$S HTTP/1.0\r\nHOST: $S\r\n"), cmd, server);
	bf.emit_p(PSTR("User-Agent: $S\r\n\r\n"), user_agent_string);

	send_http_request(server, atoi(port), p, default_http_callback, usessl);
}

/** Prepare factory reset */
void OpenSprinkler::pre_factory_reset() {
	// for ESP8266: wipe out flash
	#if defined(ESP8266)
	lcd_print_line_clear_pgm(PSTR("Wiping flash.."), 0);
	lcd_print_line_clear_pgm(PSTR("Please Wait..."), 1);
	LittleFS.format();
	#else
	// remove 'done' file as an indicator for reset
	// todo os2.3 and ospi: delete log files and/or wipe SD card
	remove_file(DONE_FILENAME);
	#endif
}

/** Populate the in-RAM iopts[] from the PROGMEM iopt_defs[].def_val table.
 * Called from factory_reset before iopts_save, and as a safety net when
 * iopts.dat is missing or corrupt. */
void OpenSprinkler::load_iopt_defaults() {
	for (uint8_t i = 0; i < NUM_IOPTS; i++) {
		iopts[i] = iopt_get_def(i);
	}
}

/** Factory reset */
void OpenSprinkler::factory_reset() {
#if defined(ESP8266)
	lcd_print_line_clear_pgm(PSTR("Factory reset"), 0);
	lcd_print_line_clear_pgm(PSTR("Please Wait..."), 1);
#else
	DEBUG_PRINT("factory reset...");
#endif

	// 1. populate iopts[] with factory defaults from iopt_defs[], then persist
	load_iopt_defaults();
	iopts_save();
	// reset string options by first wiping the file clean then write default values
	memset(tmp_buffer, 0, MAX_SOPTS_SIZE);
	for(int i=0; i<NUM_SOPTS; i++) {
		file_write_block(SOPTS_FILENAME, tmp_buffer, (uint32_t)MAX_SOPTS_SIZE*i, MAX_SOPTS_SIZE);
	}
	for(int i=0; i<NUM_SOPTS; i++) {
		sopt_save(i, sopts[i]);
	}

	// 2. write default station data
	StationData *pdata=(StationData*)tmp_buffer;
	pdata->name[0]='S';
	pdata->name[3]=0;
	pdata->name[4]=0;
	StationAttrib at;
	memset(&at, 0, sizeof(StationAttrib));
	at.mas=1;
	pdata->attrib=at; // mas:1
	pdata->type=STN_TYPE_STANDARD;
	pdata->sped[0]='0';
	pdata->sped[1]=0;
	for(int i=0; i<MAX_NUM_STATIONS; i++) {
		int sid=i+1;
		if(i<99) {
			pdata->name[1]='0'+(sid/10); // default station name
			pdata->name[2]='0'+(sid%10);
		} else {
			pdata->name[1]='0'+(sid/100);
			pdata->name[2]='0'+((sid%100)/10);
			pdata->name[3]='0'+(sid%10);
		}
		file_write_block(STATIONS_FILENAME, pdata, sizeof(StationData)*i, sizeof(StationData));
	}

	attribs_load(); // load and repackage attrib bits (for backward compatibility)

	// 3. write non-volatile controller status
	nvdata.reboot_cause = REBOOT_CAUSE_RESET;
	nvdata_save();
	last_reboot_cause = nvdata.reboot_cause;

	// 4. write program data: just need to write a program counter: 0
	file_write_byte(PROG_FILENAME, 0, 0);

	// remove all sensor files, so they will be re-created during loading
	remove_file(SENSORS_FILENAME);
	remove_sensor_log();
	remove_file(SENADJ_FILENAME);

	// 5. write 'done' file
	file_write_byte(DONE_FILENAME, 0, 1);
}

/** Parse OTC configuration */
void OpenSprinkler::parse_otc_config() {
	ArduinoJson::JsonDocument doc; // make sure this has the same scope as server and token
	const char *server = NULL;
	const char *token = NULL;
	int port = DEFAULT_OTC_PORT_DEV;
	int en = 0;

	char *config = tmp_buffer + 1;
	sopt_load(SOPT_OTC_OPTS, config);
	if (*config != 0) {
		// Add the wrapping curly braces to the string
		config = tmp_buffer;
		config[0] = '{';
		int len = strlen(config);
		config[len] = '}';
		config[len+1] = 0;

		ArduinoJson::DeserializationError error = ArduinoJson::deserializeJson(doc, config);

		// Test the parsing otherwise parse
		if (error) {
				DEBUG_PRINT(F("otf: deserializeJson() failed: "));
				DEBUG_PRINTLN(error.c_str());
		} else {
				en = doc["en"];
				token = doc["token"];
				server = doc["server"];
				port = doc["port"];
		}
	}

	otc.en = en;
	otc.token = token ? String(token) : "";
	otc.server = server ? String(server) : "";
	otc.port = port;
}

/** Setup function for options */
void OpenSprinkler::options_setup() {

	// Check reset conditions:
	if (file_read_byte(IOPTS_FILENAME, IOPT_FW_VERSION)!=OS_FW_VERSION ||  // fw major version has changed
			!file_exists(DONE_FILENAME)) {  // done file doesn't exist

		factory_reset();

	} else	{

		iopts_load();
		nvdata_load();
		last_reboot_cause = nvdata.reboot_cause;
		nvdata.reboot_cause = REBOOT_CAUSE_POWERON;
		nvdata_save();
		#if defined(ESP8266)
		wifi_ssid = sopt_load(SOPT_STA_SSID);
		wifi_pass = sopt_load(SOPT_STA_PASS);
		sopt_load(SOPT_STA_BSSID_CHL, tmp_buffer);
		if(tmp_buffer[0]!=0) {
			char *mac = strchr(tmp_buffer, '@');
			if(mac!=NULL && isValidMAC(tmp_buffer)) { // check if bssid is valid MAC
				*mac=0; // terminate MAC string
				int chl = atoi(mac+1);
				if(chl>=0 && chl<=255) {
					str2mac(tmp_buffer, wifi_bssid);
					wifi_channel = chl;
				}
			}
		}
		#endif
		parse_otc_config();

		attribs_load();
	}

#if defined(ESP8266)	// handle buttons
	unsigned char button = button_read(BUTTON_WAIT_NONE);

	switch(button & BUTTON_MASK) {

	case BUTTON_1:
		// if BUTTON_1 is pressed during startup, go to 'reset all options'
		ui_set_options(IOPT_RESET);
		if (iopts[IOPT_RESET]) {
			pre_factory_reset();
			reboot_dev(REBOOT_CAUSE_RESET);
		}
		break;

	case BUTTON_2:
		// if BUTTON_2 is pressed during startup, go to Test OS mode
		// only available for OS 3.0
		lcd_print_line_clear_pgm(PSTR("===Test Mode==="), 0);
		lcd_print_line_clear_pgm(PSTR("  B3:proceed"), 1);
		do {
			button = button_read(BUTTON_WAIT_NONE);
		} while(!((button&BUTTON_MASK)==BUTTON_3 && (button&BUTTON_FLAG_DOWN)));
		// set test mode parameters

		//iopts[IOPT_WIFI_MODE] = WIFI_MODE_STA;
		wifi_testmode = 1;
		#if defined(TESTMODE_SSID)
		wifi_ssid = TESTMODE_SSID;
		wifi_pass = TESTMODE_PASS;
		#else
		wifi_ssid = "ostest";
		wifi_pass = "opendoor";
		#endif
		button = 0;
		break;

	case BUTTON_3:
		// if BUTTON_3 is pressed during startup, enter Setup option mode
		lcd_print_line_clear_pgm(PSTR("==Set Options=="), 0);
		delay(DISPLAY_MSG_MS);
		lcd_print_line_clear_pgm(PSTR("B1/B2:+/-, B3:->"), 0);
		lcd_print_line_clear_pgm(PSTR("Hold B3 to save"), 1);
		do {
			button = button_read(BUTTON_WAIT_NONE);
		} while (!(button & BUTTON_FLAG_DOWN));
		lcd.clear();
		ui_set_options(0);
		if (iopts[IOPT_RESET]) {
			pre_factory_reset();
			reboot_dev(REBOOT_CAUSE_RESET);
		}
		break;
	}

	// turn on LCD backlight and contrast
	lcd_set_brightness();
	lcd_set_contrast();
#endif
}

/** Load non-volatile controller status data from file */
void OpenSprinkler::nvdata_load() {
	file_read_block(NVCON_FILENAME, &nvdata, 0, sizeof(NVConData));
	old_status = status;
}

/** Save non-volatile controller status data */
void OpenSprinkler::nvdata_save() {
	file_write_block(NVCON_FILENAME, &nvdata, 0, sizeof(NVConData));
}

void parse_wto(char* wto);

/** Load integer options from file */
void OpenSprinkler::iopts_load() {
	// Seed iopts[] with factory defaults first. This way, any options not
	// yet present in iopts.dat (e.g., newly added options after a firmware
	// upgrade) keep their compile-time defaults rather than reading as 0.
	load_iopt_defaults();
	os_file_type f = file_open(IOPTS_FILENAME, FileOpenMode::Read);
	uint32_t load_count = f ? file_size(f) : 0;
	if (f) file_close(f);
	if (load_count > NUM_IOPTS) load_count = NUM_IOPTS;
	file_read_block(IOPTS_FILENAME, iopts, 0, load_count);
	nboards = iopts[IOPT_EXT_BOARDS]+1;
	nstations = nboards * 8;
	status.enabled = iopts[IOPT_DEVICE_ENABLE];
	iopts[IOPT_FW_VERSION] = OS_FW_VERSION;
	iopts[IOPT_FW_MINOR] = OS_FW_MINOR;
	/* Reject the former default 50.97.210.169 NTP IP address as
		* it no longer works, yet is carried on by people's saved
		* configs when they upgrade from older versions.
		* IOPT_NTP_IP1 = 0 leads to the new good default behavior. */
	if (iopts[IOPT_NTP_IP1] == 50 && iopts[IOPT_NTP_IP2] == 97 &&
			iopts[IOPT_NTP_IP3] == 210 && iopts[IOPT_NTP_IP4] == 169) {
			iopts[IOPT_NTP_IP1] = 0;
			iopts[IOPT_NTP_IP2] = 0;
			iopts[IOPT_NTP_IP3] = 0;
			iopts[IOPT_NTP_IP4] = 0;
	}
	populate_master();
	sopt_load(SOPT_WEATHER_OPTS, tmp_buffer+1); // Leave room for curly brace
	parse_wto(tmp_buffer);
	// California restriction is now indicated in wto and no longer by the highest bit of uwt. So we force that bit to 0
	iopts[IOPT_USE_WEATHER] &= 0x7F;
}

void OpenSprinkler::populate_master() {
	masters[MASTER_1][MASOPT_SID] = iopts[IOPT_MASTER_STATION];
	masters[MASTER_1][MASOPT_ON_ADJ] = iopts[IOPT_MASTER_ON_ADJ];
	masters[MASTER_1][MASOPT_OFF_ADJ] = iopts[IOPT_MASTER_OFF_ADJ];

	masters[MASTER_2][MASOPT_SID] = iopts[IOPT_MASTER_STATION_2];
	masters[MASTER_2][MASOPT_ON_ADJ] = iopts[IOPT_MASTER_ON_ADJ_2];
	masters[MASTER_2][MASOPT_OFF_ADJ] = iopts[IOPT_MASTER_OFF_ADJ_2];

	masters[MASTER_3][MASOPT_SID] = iopts[IOPT_MASTER_STATION_3];
	masters[MASTER_3][MASOPT_ON_ADJ] = iopts[IOPT_MASTER_ON_ADJ_3];
	masters[MASTER_3][MASOPT_OFF_ADJ] = iopts[IOPT_MASTER_OFF_ADJ_3];

	masters[MASTER_4][MASOPT_SID] = iopts[IOPT_MASTER_STATION_4];
	masters[MASTER_4][MASOPT_ON_ADJ] = iopts[IOPT_MASTER_ON_ADJ_4];
	masters[MASTER_4][MASOPT_OFF_ADJ] = iopts[IOPT_MASTER_OFF_ADJ_4];
}

/** Save integer options to file */
void OpenSprinkler::iopts_save() {
	file_write_block(IOPTS_FILENAME, iopts, 0, NUM_IOPTS);
	nboards = iopts[IOPT_EXT_BOARDS]+1;
	nstations = nboards * 8;
	status.enabled = iopts[IOPT_DEVICE_ENABLE];
}

/** Load a string option from file */
void OpenSprinkler::sopt_load(unsigned char oid, char *buf, uint16_t maxlen) {
	if(maxlen>MAX_SOPTS_SIZE) maxlen = MAX_SOPTS_SIZE; // cap maxlen
	file_read_block(SOPTS_FILENAME, buf, MAX_SOPTS_SIZE*oid, maxlen);
	buf[maxlen]=0;  // ensure the string ends properly
}

/** Load a string option from file, return String */
String OpenSprinkler::sopt_load(unsigned char oid) {
	sopt_load(oid, tmp_buffer);
	String str = tmp_buffer;
	return str;
}

/** Save a string option to file */
bool OpenSprinkler::sopt_save(unsigned char oid, const char *buf) {
	// smart save: if value hasn't changed, don't write
	if(file_cmp_block(SOPTS_FILENAME, buf, (uint32_t)MAX_SOPTS_SIZE*oid)==0) return false;
	int len = strlen(buf);
	if(len>=MAX_SOPTS_SIZE) {
		file_write_block(SOPTS_FILENAME, buf, (uint32_t)MAX_SOPTS_SIZE*oid, MAX_SOPTS_SIZE);
	} else {
		// copy ending 0 too
		file_write_block(SOPTS_FILENAME, buf, (uint32_t)MAX_SOPTS_SIZE*oid, len+1);
	}
	return true;
}

// ==============================
// Controller Operation Functions
// ==============================

/** Enable controller operation */
void OpenSprinkler::enable() {
	status.enabled = 1;
	iopts[IOPT_DEVICE_ENABLE] = 1;
	iopts_save();
}

/** Disable controller operation */
void OpenSprinkler::disable() {
	status.enabled = 0;
	iopts[IOPT_DEVICE_ENABLE] = 0;
	iopts_save();
}

/** Start rain delay */
void OpenSprinkler::raindelay_start() {
	status.rain_delayed = 1;
	nvdata_save();
}

/** Stop rain delay */
void OpenSprinkler::raindelay_stop() {
	status.rain_delayed = 0;
	nvdata.rd_stop_time = 0;
	nvdata_save();
}

/** Sensor functions */

void list_all_files() {
#if defined(ESP8266)
    Serial.println(PSTR("\n--- Flash File System Map ---"));
    Serial.printf("%-30s %10s\n", "Filename", "Size (B)");
    Serial.println(PSTR("------------------------------------------"));

    uint32_t totalUsed = 0;
    uint32_t fileCount = 0;

    // Root directory (skip directory entries — they appear as dot-less names with size 0)
    Dir dir = LittleFS.openDir("/");
    while (dir.next()) {
        if (dir.fileName().indexOf('.') < 0) continue;
        Serial.printf("%-30s %10u\n", ("/" + dir.fileName()).c_str(), dir.fileSize());
        totalUsed += dir.fileSize();
        fileCount++;
    }

    // /logs/ subdirectory
    uint32_t logUsed = 0;
    uint32_t logCount = 0;
    Dir logdir = LittleFS.openDir("/logs/");
    while (logdir.next()) {
        String fullname = "/logs/" + logdir.fileName();
        Serial.printf("%-30s %10u\n", fullname.c_str(), logdir.fileSize());
        logUsed += logdir.fileSize();
        logCount++;
    }
    totalUsed += logUsed;
    fileCount += logCount;

    // Filesystem totals
    FSInfo fs_info;
    LittleFS.info(fs_info);

    Serial.println(PSTR("------------------------------------------"));
    Serial.printf("Total Files: %u  (logs/: %u)\n", fileCount, logCount);
    Serial.printf("Total Size:  %u bytes  (logs/: %u bytes)\n", totalUsed, logUsed);
    Serial.printf("FS Total:    %u bytes\n", fs_info.totalBytes);
    Serial.printf("FS Used:     %u bytes\n", fs_info.usedBytes);
    Serial.printf("FS Free:     %u bytes\n", fs_info.totalBytes - fs_info.usedBytes);
    Serial.printf("Block Size:  %u bytes\n", fs_info.blockSize);
    Serial.println(PSTR("------------------------------------------\n"));
#endif
}


void OpenSprinkler::log_sensor(uint8_t sid, float value) {
	if (sid >= nsensors || sensors[sid].uuid == SENSOR_UUID_NONE) return;

	// Read central header; create/recreate if missing or version mismatch
	SensorLogHeader hdr = {};
	bool hdr_valid = false;
	os_file_type hfile = open_sensor_log_header(FileOpenMode::Read);
	if (hfile) {
		int n = file_read(hfile, &hdr, sizeof(hdr));
		file_close(hfile);
		hdr_valid = (n == (int)sizeof(hdr) &&
		             hdr.magic == SENSOR_LOG_MAGIC &&
		             hdr.version == SENSOR_LOG_VERSION &&
		             hdr.max_files == SENSOR_LOG_MAX_FILES &&
		             hdr.records_per_file == SENSOR_LOG_RECORDS_PER_FILE);
	}

	if (!hdr_valid) {
		// First use or firmware upgrade: ensure directory exists, wipe stale data files, write fresh header
		ensure_log_dir();
		char fname[24];
		for (uint16_t i = 0; i < SENSOR_LOG_MAX_FILES; i++) {
			get_sensor_log_filename(fname, i);
			remove_file(fname);
		}
		hdr = {};
		hdr.magic            = SENSOR_LOG_MAGIC;
		hdr.version          = SENSOR_LOG_VERSION;
		hdr.max_files        = SENSOR_LOG_MAX_FILES;
		hdr.records_per_file = SENSOR_LOG_RECORDS_PER_FILE;
		hfile = open_sensor_log_header(FileOpenMode::WriteTruncate);
		if (!hfile) {
			DEBUG_PRINTLN("Failed to create sensor log header");
			return;
		}
		file_write(hfile, &hdr, sizeof(hdr));
		file_close(hfile);
	}

	// Open current data file for appending; infer record count from file size
	os_file_type dfile = open_sensor_log(hdr.cur_file, FileOpenMode::Append);
	if (!dfile) {
		DEBUG_PRINTLN("Failed to open sensor log data file");
		return;
	}
	uint32_t count = file_size(dfile) / sizeof(SensorLogRecord);

	if (count >= hdr.records_per_file) {
		// Current file is full — rotate to next slot
		file_close(dfile);
		hdr.cur_file = (uint16_t)((hdr.cur_file + 1) % hdr.max_files);
		if (hdr.cur_file == 0 && !hdr.wrapped) hdr.wrapped = 1;

		// Evict the file at the new slot (oldest data)
		remove_sensor_log(hdr.cur_file);

		// Persist updated header (only written on rotation, not on every record)
		hfile = open_sensor_log_header(FileOpenMode::WriteTruncate);
		if (hfile) { file_write(hfile, &hdr, sizeof(hdr)); file_close(hfile); }

		dfile = open_sensor_log(hdr.cur_file, FileOpenMode::Append);
		if (!dfile) {
			DEBUG_PRINTLN("Failed to open new sensor log data file");
			return;
		}
	}

	SensorLogRecord rec = {};
	rec.timestamp = now();
	rec.value     = value;
	rec.uuid      = sensors[sid].uuid;
	file_write(dfile, &rec, sizeof(rec));
	file_close(dfile);
}

void OpenSprinkler::poll_sensors() {
	for (uint8_t i = 0; i < nsensors; i++) {
		sensor_memory_t &mem = sensors[i];
		if (!mem.interval || !(mem.flag & (1 << SENSOR_FLAG_ENABLE))) continue;

		if ((int32_t)((uint32_t)millis() - mem.next_update) <= 0) continue;

		Sensor *sensor = Sensor::get(i);
		if (!sensor) {
			// Can't load sensor from disk — value is now stale
			mem.status |= SENSOR_STATUS_STALE;
			continue;
		}

		uint8_t new_status = 0;
		float new_value = sensor->get_new_value(&new_status);

		if (new_status & SENSOR_STATUS_ERROR) {
			// Hardware fault — preserve last good value but flag error; clear stale
			mem.status = (mem.status & SENSOR_STATUS_VALID) | SENSOR_STATUS_ERROR;
		} else {
			mem.value = new_value;
			mem.status = new_status; // VALID + CLAMPED_* as appropriate; clears ERROR/STALE
		}
		mem.next_update = millis() + (mem.interval * 1000 * 60);

		// Log only a value produced by this read. mem.value may still contain the
		// previous good reading when the current attempt reports an error.
		if ((mem.flag & (1 << SENSOR_FLAG_LOG)) && (new_status & SENSOR_STATUS_VALID)) {
			os.log_sensor(i, mem.value);
		}
	}
}

float OpenSprinkler::get_sensor_weather_data(WeatherAction action) {
	return NAN; // TODO make function for WeatherSensor
}

/** LCD and button functions */
#if defined(USE_DISPLAY)
#if defined(ESP8266)		// Arduino LCD and button functions
/** print a program memory string */
void OpenSprinkler::lcd_print_pgm(PGM_P str) {
	uint8_t c;
	while((c=pgm_read_byte(str++))!= '\0') {
		lcd.print((char)c);
	}
}

/** print a program memory string to a given line with clearing */
void OpenSprinkler::lcd_print_line_clear_pgm(PGM_P str, unsigned char line) {
	lcd.setCursor(0, line);
	uint8_t c;
	uint8_t cnt = 0;
	while((c=pgm_read_byte(str++))!= '\0' && cnt < 16) {
		lcd.print((char)c);
		cnt++;
	}
	for(; cnt < 16; cnt++) lcd_print_pgm(PSTR(" "));
}

#else
void OpenSprinkler::lcd_print_pgm(const char *str) {
	lcd.print(str);
}
void OpenSprinkler::lcd_print_line_clear_pgm(const char *str, uint8_t line) {
	char buf[17];
	uint8_t c;
	uint8_t cnt = 0;
	while((c=*str++)!= '\0' && cnt<16) {
		buf[cnt] = c;
		cnt++;
	}

	for(int i=cnt; i<16; i++) buf[i] = ' ';
	buf[16] = '\0';

	lcd.setCursor(0, line);
	lcd.print(buf);
}

#define PGSTR(s) s
#endif

void OpenSprinkler::lcd_print_2digit(int v)
{
	lcd.print((int)(v/10));
	lcd.print((int)(v%10));
}

/** print time to a given line */
void OpenSprinkler::lcd_print_time(time_os_t t)
{
	lcd.setAutoDisplay(false);
	lcd.setCursor(0, 0);
	lcd_print_2digit(hour(t));
	lcd_print_pgm(PSTR(":"));
	lcd_print_2digit(minute(t));
	lcd_print_pgm(PSTR(" "));
	// each weekday string has 3 characters + ending 0
	lcd_print_pgm(days_str+4*weekday_today());
	lcd_print_pgm(PSTR(" "));
	lcd_print_pgm(months_str+4*(month(t)-1));
	lcd_print_pgm(PSTR("-"));
	lcd_print_2digit(day(t));
	lcd.display();
	lcd.setAutoDisplay(true);
}

/** print ip address */
void OpenSprinkler::lcd_print_ip(const unsigned char *ip, unsigned char endian) {
	lcd.clear(0, 1);
	lcd.setAutoDisplay(false);
	lcd.setCursor(0, 0);
	for (unsigned char i=0; i<4; i++) {
		lcd.print(endian ? (int)ip[3-i] : (int)ip[i]);

		if(i<3) {
			lcd_print_pgm(PSTR("."));
		}
	}
	lcd.display();
	lcd.setAutoDisplay(true);
}

/** print mac address */
void OpenSprinkler::lcd_print_mac(const unsigned char *mac) {
	lcd.setAutoDisplay(false); // reduce screen drawing time by turning off display() when drawing individual characters
	lcd.setCursor(0, 0);
	for(unsigned char i=0; i<6; i++) {
		if(i) {
			lcd_print_pgm(PSTR("-"));
		}

		lcd.print((mac[i]>>4), HEX);
		lcd.print((mac[i]&0x0F), HEX);
		if(i==4) lcd.setCursor(0, 1);
	}
	#if defined(ESP8266)
	if(useEth) {
		lcd_print_pgm(PSTR(" (Ether MAC)"));
	} else {
		lcd_print_pgm(PSTR(" (WiFi MAC)"));
	}
	#else
		lcd_print_pgm(PSTR(" (MAC)"));
	#endif

	lcd.clear(2, 2);
	lcd.setCursor(0, 2);
	#if defined(ESP8266)
		lcd.print((char)('0' + (OS_HW_VERSION / 10)));
		lcd.print('.');
		lcd.print(hw_rev);
		switch (hw_type) {
		case HW_TYPE_DC:
			lcd_print_pgm(PSTR(" DC, "));
			break;
		case HW_TYPE_LATCH:
			lcd_print_pgm(PSTR(" LA, "));
			break;
		default:
			lcd_print_pgm(PSTR(" AC, "));
		}
	#else
		lcd_print_pgm(PSTR("OSPi, "));
	#endif
	lcd.print((char)('0' + (OS_FW_VERSION / 100)));
	lcd.print('.');
	lcd.print((char)('0' + ((OS_FW_VERSION / 10) % 10)));
	lcd.print('.');
	lcd.print((char)('0' + (OS_FW_VERSION % 10)));
	lcd.print('(');
	lcd.print(OS_FW_MINOR);
	lcd.print(')');

	lcd.display();
	lcd.setAutoDisplay(true);
}

/** print station bits */
void OpenSprinkler::lcd_print_screen(char c) {
	lcd.setAutoDisplay(false); // reduce screen drawing time by turning off display() when drawing individual characters
	lcd.setCursor(0, 1);
	if (status.display_board == 0) {
		lcd.print(F("MC:"));  // Master controller is display as 'MC'
	}	else {
		lcd.print(F("E"));
		lcd.print((int)status.display_board);
		lcd.print(F(":"));  // extension boards are displayed as E1, E2...
	}
	if (!status.enabled) {
		lcd.print(F("-Disabled!-"));
	} else {
		unsigned char bitvalue = station_bits[status.display_board];
		for (unsigned char s=0; s<8; s++) {
			unsigned char sid = (unsigned char)status.display_board<<3;
			sid += (s+1);
			if (sid == iopts[IOPT_MASTER_STATION]) {
				lcd.print((bitvalue&1) ? c : 'M'); // print master station
			} else if (sid == iopts[IOPT_MASTER_STATION_2]) {
				lcd.print((bitvalue&1) ? c : 'N'); // print master2 station
			} else if (sid == iopts[IOPT_MASTER_STATION_3]) {
				lcd.print((bitvalue&1) ? c : 'U'); // print master3 station
			} else if (sid == iopts[IOPT_MASTER_STATION_4]) {
				lcd.print((bitvalue&1) ? c : 'V'); // print master4 station
			} else {
				lcd.print((bitvalue&1) ? c : '_');
			}
			bitvalue >>= 1;
		}
	}
	//lcd.print(F("    "));

	// Per-sensor icon on the status row (row 1). Loop instead of 4× duplicated
	// switches. Pswitch uses 'P'/'p' for all sensors. SENSOR_TYPE_FLOW is only
	// reachable for SN1 (the others can't be configured as flow), but the
	// case is harmless on the others — they'll never hit it.
	static const uint8_t sensor_lcd_cols[NUM_SENSORS] = {
		LCD_CURSOR_SENSOR1, LCD_CURSOR_SENSOR2, LCD_CURSOR_SENSOR3, LCD_CURSOR_SENSOR4
	};
	for (uint8_t i = 0; i < NUM_SENSORS; i++) {
		lcd.setCursor(sensor_lcd_cols[i], 1);
		switch(iopts[sensor_iopt_keys[i].type]) {
			case SENSOR_TYPE_RAIN:
				lcd.write(sn_sensors[i].active?ICON_RAIN:(sn_sensors[i].raw?'R':'r'));
				break;
			case SENSOR_TYPE_SOIL:
				lcd.write(sn_sensors[i].active?ICON_SOIL:(sn_sensors[i].raw?'S':'s'));
				break;
			case SENSOR_TYPE_FLOW:
				lcd.write(flowcount_rt>0?'F':'f');
				break;
			case SENSOR_TYPE_PSWITCH:
				lcd.write(sn_sensors[i].raw?'P':'p');
				break;
			default:
				lcd.write(' ');
				break;
		}
	}

	lcd.setCursor(LCD_CURSOR_NETWORK, 1);
#if defined(ESP8266)
	if(useEth) {
		lcd.write(eth.connected()?ICON_ETHER_CONNECTED:ICON_ETHER_DISCONNECTED);
	}
	else {
		lcd.write(WiFi.status()==WL_CONNECTED?ICON_WIFI_CONNECTED:ICON_WIFI_DISCONNECTED);
	}
#else
	lcd.write(status.network_fails>2?ICON_ETHER_DISCONNECTED:ICON_ETHER_CONNECTED);  // if network failure detection is more than 2, display disconnect icon
#endif

	#if defined(ESP8266)
	if(useEth || (get_wifi_mode()==WIFI_MODE_STA && WiFi.status()==WL_CONNECTED && WiFi.localIP())) {
	#else
	{
	#endif
		lcd.setCursor(0, -1);
		if(status.overcurrent_sid > 0) {
			lcd.print(F("<!OVERCURRENT!> "));
		} else if(status.rain_delayed) {
			lcd.print(F("<Rain Delay On> "));
		} else if(status.pause_state) {
			lcd.print(F("<Program Paused>"));
		} else if(status.program_busy) {
			lcd.print(F("<Running Zones> "));
		} else {
			lcd.print(F(" (System Idle)  "));
		}

	#if defined(ESP8266)
		lcd.setCursor(2, 2);
		if(status.program_busy && !status.pause_state) {
			//lcd.print(F("Curr: "));
			lcd.print(read_current(true));
			lcd.print(F(" mA      "));
		} else {
	#else
		{
	#endif
			lcd.clear(2, 2);
		}
	}

	// Remote Extension and Rain Delay icons live on the bottom row, freeing
	// the status-row space for SN3/SN4. Drawn AFTER the current-display block
	// above so they overwrite that block's trailing spaces / clear.
	lcd.setCursor(LCD_CURSOR_REMOTEXT, 2);
	lcd.write(iopts[IOPT_REMOTE_EXT_MODE]?ICON_REMOTEXT:' ');

	lcd.setCursor(LCD_CURSOR_RAINDELAY, 2);
	lcd.write((status.rain_delayed || status.pause_state)?ICON_RAINDELAY:' ');

	lcd.display();
	lcd.setAutoDisplay(true);

}

/** print a version number */
void OpenSprinkler::lcd_print_version(unsigned char v) {
	if(v > 99) {
		lcd.print(v/100);
		lcd.print(".");
	}
	if(v>9) {
		lcd.print((v/10)%10);
		lcd.print(".");
	}
	lcd.print(v%10);
}

/** print an option value */
void OpenSprinkler::lcd_print_option(int i) {
	// each prompt string takes 16 characters
	iopt_get_prompt(i, tmp_buffer);
	lcd.setCursor(0, 0);
	lcd.print(tmp_buffer);
	lcd_print_line_clear_pgm(PSTR(""), 1);
	lcd.setCursor(0, 1);
	int tz;
	switch(i) {
	case IOPT_HW_VERSION:
		lcd.print("v");
	case IOPT_FW_VERSION:
		lcd_print_version(iopts[i]);
		break;
	case IOPT_TIMEZONE: // if this is the time zone option, do some conversion
		tz = (int)iopts[i]-48;
		if (tz>=0) lcd_print_pgm(PSTR("+"));
		else {lcd_print_pgm(PSTR("-")); tz=-tz;}
		lcd.print(tz/4); // print integer portion
		lcd_print_pgm(PSTR(":"));
		tz = (tz%4)*15;
		if (tz==0)	lcd_print_pgm(PSTR("00"));
		else {
			lcd.print(tz);	// print fractional portion
		}
		break;
	case IOPT_MASTER_ON_ADJ:
	case IOPT_MASTER_ON_ADJ_2:
	case IOPT_MASTER_ON_ADJ_3:
	case IOPT_MASTER_ON_ADJ_4:
	case IOPT_MASTER_OFF_ADJ:
	case IOPT_MASTER_OFF_ADJ_2:
	case IOPT_MASTER_OFF_ADJ_3:
	case IOPT_MASTER_OFF_ADJ_4:
	case IOPT_STATION_DELAY_TIME:
		{
		int16_t t=water_time_decode_signed(iopts[i]);
		if(t>=0)	lcd_print_pgm(PSTR("+"));
		lcd.print(t);
		}
		break;
	case IOPT_HTTPPORT_0:
		lcd.print((unsigned int)(iopts[i+1]<<8)+iopts[i]);
		break;
	case IOPT_PULSE_RATE_0:
		{
		uint16_t fpr = (unsigned int)(iopts[i+1]<<8)+iopts[i];
		lcd.print(fpr/100);
		lcd_print_pgm(PSTR("."));
		lcd.print((fpr/10)%10);
		lcd.print(fpr%10);
		}
		break;
	case IOPT_LCD_CONTRAST:
		lcd_set_contrast();
		lcd.print((int)iopts[i]);
		break;
	case IOPT_LCD_BACKLIGHT:
		lcd_set_brightness();
		lcd.print((int)iopts[i]);
		break;
	case IOPT_BOOST_TIME:
		#if defined(ESP8266)
		if(hw_type==HW_TYPE_AC) {
			lcd.print('-');
		} else {
			lcd.print((int)iopts[i]*4);
			lcd_print_pgm(PSTR(" ms"));
		}
		#else
		lcd.print('-');
		#endif
		break;
	case IOPT_I_MIN_THRESHOLD:
	case IOPT_I_MAX_LIMIT:
		#if defined(ESP8266)
		lcd.print((int)iopts[i]*10);
		lcd_print_pgm(PSTR(" mA"));
		#else
		lcd.print('-');
		#endif
		break;
	case IOPT_LATCH_ON_VOLTAGE:
	case IOPT_LATCH_OFF_VOLTAGE:
		#if defined(ESP8266)
		if(hw_type==HW_TYPE_LATCH) {
			lcd.print((int)iopts[i]);
			lcd.print('V');
		} else {
			lcd.print('-');
		}
		#else
		lcd.print('-');
		#endif
		break;
	case IOPT_TARGET_PD_VOLTAGE:
		#if defined(ESP8266)
		if(hw_rev == 4 && hw_type==HW_TYPE_DC) {
			lcd.print(iopts[i]/10);
			lcd.print('.');
			lcd.print(iopts[i]%10);
			lcd.print('V');
		} else {
			lcd.print('-');
		}
		#else
		lcd.print('-');
		#endif
		break;
	default:
		// if this is a boolean option
		if (iopt_get_max(i)==1)
			lcd_print_pgm(iopts[i] ? PSTR("Yes") : PSTR("No"));
		else
			lcd.print((int)iopts[i]);
		break;
	}
	if (i==IOPT_WATER_PERCENTAGE)  lcd_print_pgm(PSTR("%"));
	else if (i==IOPT_MASTER_ON_ADJ || i==IOPT_MASTER_OFF_ADJ ||
	         i==IOPT_MASTER_ON_ADJ_2 || i==IOPT_MASTER_OFF_ADJ_2 ||
	         i==IOPT_MASTER_ON_ADJ_3 || i==IOPT_MASTER_OFF_ADJ_3 ||
	         i==IOPT_MASTER_ON_ADJ_4 || i==IOPT_MASTER_OFF_ADJ_4)
		lcd_print_pgm(PSTR(" sec"));

}


/** Button functions */
/** wait for button */
unsigned char OpenSprinkler::button_read_busy(unsigned char pin_butt, unsigned char waitmode, unsigned char butt, unsigned char is_holding) {

	int hold_time = 0;

	if (waitmode==BUTTON_WAIT_NONE || (waitmode == BUTTON_WAIT_HOLD && is_holding)) {
		if (digitalReadExt(pin_butt) != 0) return BUTTON_NONE;
		return butt | (is_holding ? BUTTON_FLAG_HOLD : 0);
	}

	while (digitalReadExt(pin_butt) == 0 &&
				 (waitmode == BUTTON_WAIT_RELEASE || (waitmode == BUTTON_WAIT_HOLD && hold_time<BUTTON_HOLD_MS))) {
		delay(BUTTON_DELAY_MS);
		hold_time += BUTTON_DELAY_MS;
	}
	if (is_holding || hold_time >= BUTTON_HOLD_MS)
		butt |= BUTTON_FLAG_HOLD;
	return butt;

}

/** read button and returns button value 'OR'ed with flag bits */
unsigned char OpenSprinkler::button_read(unsigned char waitmode)
{
	static unsigned char old = BUTTON_NONE;
	unsigned char curr = BUTTON_NONE;
	unsigned char is_holding = (old&BUTTON_FLAG_HOLD);

	delay(BUTTON_DELAY_MS);

	if (digitalReadExt(PIN_BUTTON_1) == 0) {
		curr = button_read_busy(PIN_BUTTON_1, waitmode, BUTTON_1, is_holding);
	} else if (digitalReadExt(PIN_BUTTON_2) == 0) {
		curr = button_read_busy(PIN_BUTTON_2, waitmode, BUTTON_2, is_holding);
	} else if (digitalReadExt(PIN_BUTTON_3) == 0) {
		curr = button_read_busy(PIN_BUTTON_3, waitmode, BUTTON_3, is_holding);
	}

	// set flags in return value
	unsigned char ret = curr;
	if (!(old&BUTTON_MASK) && (curr&BUTTON_MASK))
		ret |= BUTTON_FLAG_DOWN;
	if ((old&BUTTON_MASK) && !(curr&BUTTON_MASK))
		ret |= BUTTON_FLAG_UP;

	old = curr;

	return ret;
}

#if defined(ESP8266)

/** user interface for setting options during startup */
void OpenSprinkler::ui_set_options(int oid)
{
	boolean finished = false;
	unsigned char button;
	int i=oid;

	while(!finished) {
		button = button_read(BUTTON_WAIT_HOLD);

		switch (button & BUTTON_MASK) {
		case BUTTON_1:
			if (i==IOPT_FW_VERSION || i==IOPT_HW_VERSION || i==IOPT_FW_MINOR ||
					i==IOPT_HTTPPORT_0 || i==IOPT_HTTPPORT_1 ||
					i==IOPT_PULSE_RATE_0 || i==IOPT_PULSE_RATE_1 ||
					i==IOPT_WIFI_MODE) break; // ignore non-editable options
			if (iopt_get_max(i) != iopts[i]) iopts[i] ++;
			break;

		case BUTTON_2:
			if (i==IOPT_FW_VERSION || i==IOPT_HW_VERSION || i==IOPT_FW_MINOR ||
					i==IOPT_HTTPPORT_0 || i==IOPT_HTTPPORT_1 ||
					i==IOPT_PULSE_RATE_0 || i==IOPT_PULSE_RATE_1 ||
					i==IOPT_WIFI_MODE) break; // ignore non-editable options
			if (iopts[i] != 0) iopts[i] --;
			break;

		case BUTTON_3:
			if (!(button & BUTTON_FLAG_DOWN)) break;
			if (button & BUTTON_FLAG_HOLD) {
				// long press, save options
				iopts_save();
				finished = true;
			}
			else {
				// click, move to the next option
				if (i==IOPT_USE_DHCP && iopts[i]) i += 9; // if use DHCP, skip static ip set
				else if (i==IOPT_HTTPPORT_0) i+=2; // skip IOPT_HTTPPORT_1
				else if (i==IOPT_PULSE_RATE_0) i+=2; // skip IOPT_PULSE_RATE_1
				else if (i==IOPT_MASTER_STATION   && iopts[i]==0) i+=3; // if not using master, skip on/off adjust
				else if (i==IOPT_MASTER_STATION_2 && iopts[i]==0) i+=3; // if not using master2, skip on/off adjust
				else if (i==IOPT_MASTER_STATION_3 && iopts[i]==0) i+=3; // if not using master3, skip on/off adjust
				else if (i==IOPT_MASTER_STATION_4 && iopts[i]==0) i+=3; // if not using master4, skip on/off adjust
				else	{
					i = (i+1) % NUM_IOPTS;
				}
				if(i==IOPT_SEQUENTIAL_RETIRED) i++;
				if(i==IOPT_URS_RETIRED) i++;
				if(i==IOPT_RSO_RETIRED) i++;
				if (hw_type==HW_TYPE_AC && i==IOPT_BOOST_TIME) i++;	// skip boost time for non-DC controller
				if (i==IOPT_LATCH_ON_VOLTAGE && hw_type!=HW_TYPE_LATCH) i+= 2; // skip latch voltage defs for non-latch controllers
				if (i==IOPT_TARGET_PD_VOLTAGE && !(hw_rev==4 && hw_type==HW_TYPE_DC)) i++; // skip target pd voltage if not 3.4 or not DC type
				else if (lcd.type()==LCD_I2C && i==IOPT_LCD_CONTRAST) i+=3;
				// string options are not editable
			}
			break;
		}

		if (button != BUTTON_NONE) {
			lcd_print_option(i);
		}
	}
	lcd.noBlink();
}
#endif  // end of LCD and button functions

/** Set LCD contrast (using PWM) */
void OpenSprinkler::lcd_set_contrast() {
}

/** Set LCD brightness (using PWM) */
void OpenSprinkler::lcd_set_brightness(unsigned char value) {
	if (value) {lcd.displayOn();lcd.setBrightness(255); }
	else {
		if(iopts[IOPT_LCD_DIMMING]==0) lcd.displayOff();
		else { lcd.displayOn();lcd.setBrightness(iopts[IOPT_LCD_DIMMING]); }
	}
}

#if defined(USE_DISPLAY)
#include "images.h"
void OpenSprinkler::flash_screen() {
	lcd.drawXbm(0, 0, OpenSprinkler_Logo_width, OpenSprinkler_Logo_height,
		(const unsigned char*)OpenSprinkler_Logo_image);

	lcd.setCursor(2, 1);
	lcd.print(F("FW "));
	lcd.print((char)('0' + (OS_FW_VERSION / 100)));
	lcd.print('.');
	lcd.print((char)('0' + ((OS_FW_VERSION / 10) % 10)));
	lcd.print('.');
	lcd.print((char)('0' + (OS_FW_VERSION % 10)));
	lcd.print('(');
	lcd.print(OS_FW_MINOR);
	lcd.print(')');

	#if defined(OSPI)
	lcd.setCursor(3, 2);
	lcd.print(F("HW OSPi AC"));
	#else
	lcd.setCursor((hw_type == HW_TYPE_LATCH) ? 2 : 3, 2);
	lcd.print(F("HW "));
	lcd.print((char)('0' + (OS_HW_VERSION / 10)));
	lcd.print('.');
	#if defined(ESP8266)
	lcd.print(hw_rev);
	#else
	lcd.print((char)('0' + (OS_HW_VERSION % 10)));
	#endif
	switch (hw_type) {
	case HW_TYPE_DC:
		lcd.print(F(" DC"));
		break;
	case HW_TYPE_LATCH:
		lcd.print(F(" LATCH"));
		break;
	default:
		lcd.print(F(" AC"));
	}
	#endif

	lcd.display();
	delay(2000);
	lcd.clear();
	lcd.display();
}

void OpenSprinkler::toggle_screen_led() {
	static unsigned char status = 0;
	status = 1-status;
	set_screen_led(!status);
}

void OpenSprinkler::set_screen_led(unsigned char status) {
	lcd.setColor(status ? WHITE : BLACK);
	lcd.fillCircle(122, 58, 4);
	lcd.display();
	lcd.setColor(WHITE);
}

#endif

#endif

#if defined(ESP8266)

void OpenSprinkler::reset_to_ap() {
	iopts[IOPT_WIFI_MODE] = WIFI_MODE_AP;
	iopts_save();
	reboot_dev(REBOOT_CAUSE_RSTAP);
}

void OpenSprinkler::config_ip() {
	if(iopts[IOPT_USE_DHCP] == 0) {
		unsigned char *_ip = iopts+IOPT_STATIC_IP1;
		IPAddress dvip(_ip[0], _ip[1], _ip[2], _ip[3]);
		if(dvip==(uint32_t)0x00000000) return;

		_ip = iopts+IOPT_GATEWAY_IP1;
		IPAddress gwip(_ip[0], _ip[1], _ip[2], _ip[3]);
		if(gwip==(uint32_t)0x00000000) return;

		_ip = iopts+IOPT_SUBNET_MASK1;
		IPAddress subn(_ip[0], _ip[1], _ip[2], _ip[3]);
		if(subn==(uint32_t)0x00000000) return;

		_ip = iopts+IOPT_DNS_IP1;
		IPAddress dnsip(_ip[0], _ip[1], _ip[2], _ip[3]);

		WiFi.config(dvip, gwip, subn, dnsip);
	}
}

void OpenSprinkler::save_wifi_ip() {
	// todo: handle wired ethernet
	if(iopts[IOPT_USE_DHCP] && WiFi.status() == WL_CONNECTED) {
		memcpy(iopts+IOPT_STATIC_IP1, &(WiFi.localIP()[0]), 4);
		memcpy(iopts+IOPT_GATEWAY_IP1, &(WiFi.gatewayIP()[0]),4);
		memcpy(iopts+IOPT_DNS_IP1, &(WiFi.dnsIP()[0]), 4);
		memcpy(iopts+IOPT_SUBNET_MASK1, &(WiFi.subnetMask()[0]), 4);
		iopts_save();
	}
}

void OpenSprinkler::detect_expanders() {
	for(unsigned char i=0;i<(MAX_NUM_BOARDS)/2;i++) {
		unsigned char address = EXP_I2CADDR_BASE+i;
		unsigned char type = IOEXP::detectType(address);
		if(expanders[i]!=NULL) delete expanders[i];
		if(type==IOEXP_TYPE_9555) {
			expanders[i] = new PCA9555(address);
			expanders[i]->i2c_write(NXP_CONFIG_REG, 0); // set all channels to output
		} else if(type==IOEXP_TYPE_8575){
			expanders[i] = new PCF8575(address);
		} else {
			expanders[i] = new IOEXP(address);
		}
	}
}
#endif
