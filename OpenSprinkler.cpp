/* OpenSprinkler Unified (AVR/RPI/BBB/LINUX/ESP8266/ESP32) Firmware
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
#include "testmode.h"
#include "program.h"

/** Declare static data members */
OSMqtt OpenSprinkler::mqtt;
NVConData OpenSprinkler::nvdata;
ConStatus OpenSprinkler::status;
ConStatus OpenSprinkler::old_status;

byte OpenSprinkler::hw_type;
byte OpenSprinkler::hw_rev;
byte OpenSprinkler::nboards;
byte OpenSprinkler::nstations;
byte OpenSprinkler::station_bits[MAX_NUM_BOARDS];
byte OpenSprinkler::engage_booster;
uint16_t OpenSprinkler::baseline_current;

ulong OpenSprinkler::sensor1_on_timer;
ulong OpenSprinkler::sensor1_off_timer;
ulong OpenSprinkler::sensor1_active_lasttime;
ulong OpenSprinkler::sensor2_on_timer;
ulong OpenSprinkler::sensor2_off_timer;
ulong OpenSprinkler::sensor2_active_lasttime;
ulong OpenSprinkler::raindelay_on_lasttime;
ulong OpenSprinkler::pause_timer;

ulong OpenSprinkler::flowcount_log_start;
ulong OpenSprinkler::flowcount_rt;
byte OpenSprinkler::button_timeout;
#if defined(ESP32)
bool OpenSprinkler::lcd_dimmed = false;
#endif
#if defined(USE_ROTARY_ENCODER)
volatile int OpenSprinkler::encoderPos = 0;
volatile int OpenSprinkler::lastEncoderPos = 0;
volatile bool OpenSprinkler::buttonPressed = false;

byte OpenSprinkler::current_menu_item = 0;
byte OpenSprinkler::current_submenu_item = 0;
#endif
ulong OpenSprinkler::checkwt_lasttime;
ulong OpenSprinkler::checkwt_success_lasttime;
ulong OpenSprinkler::powerup_lasttime;
uint8_t OpenSprinkler::last_reboot_cause = REBOOT_CAUSE_NONE;
byte OpenSprinkler::weather_update_flag;

// todo future: the following attribute bytes are for backward compatibility
byte OpenSprinkler::attrib_mas[MAX_NUM_BOARDS];
byte OpenSprinkler::attrib_igs[MAX_NUM_BOARDS];
byte OpenSprinkler::attrib_mas2[MAX_NUM_BOARDS];
byte OpenSprinkler::attrib_igs2[MAX_NUM_BOARDS];
byte OpenSprinkler::attrib_igrd[MAX_NUM_BOARDS];
byte OpenSprinkler::attrib_dis[MAX_NUM_BOARDS];
byte OpenSprinkler::attrib_spe[MAX_NUM_BOARDS];
byte OpenSprinkler::attrib_grp[MAX_NUM_STATIONS];
byte OpenSprinkler::masters[NUM_MASTER_ZONES][NUM_MASTER_OPTS];

extern char tmp_buffer[];
extern char ether_buffer[];
extern ProgramData pd;

extern void start_server_ap();

#if defined(ESP8266) || defined(ESP32) 
	
	//DEBUG_PRINTLN(F("I2C Init with pins "));
	//DEBUG_PRINT(DEBUG_TOSTRING(SDA_PIN));
	//DEBUG_PRINT(" ");
	//DEBUG_PRINT(SCL);
	//DEBUG_PRINTLN(F(" with LCD @ "));
	//DEBUG_PRINTLN(LCD_I2CADDR);
	
	#if defined(LCD_SH1106)
	SH1106Display OpenSprinkler::lcd(LCD_I2CADDR, SDA_PIN, SCL_PIN);
	#else
	SSD1306Display OpenSprinkler::lcd(LCD_I2CADDR, SDA_PIN, SCL_PIN);
	#endif 
	byte OpenSprinkler::state = OS_STATE_INITIAL;
	byte OpenSprinkler::prev_station_bits[MAX_NUM_BOARDS];
	IOEXP* OpenSprinkler::expanders[MAX_NUM_BOARDS/2];
	IOEXP* OpenSprinkler::mainio; // main controller IO expander object
	IOEXP* OpenSprinkler::drio; // driver board IO expander object
	RCSwitch OpenSprinkler::rfswitch;
	OTCConfig OpenSprinkler::otc;

	String OpenSprinkler::wifi_ssid="";
	String OpenSprinkler::wifi_pass="";
	byte OpenSprinkler::wifi_bssid[6]={0};
	byte OpenSprinkler::wifi_channel=255;
	byte OpenSprinkler::wifi_testmode = 0;
#elif defined(ARDUINO)
	LiquidCrystal OpenSprinkler::lcd;
	extern SdFat sd;
#else
	#if defined(OSPI)
		byte OpenSprinkler::pin_sr_data = PIN_SR_DATA;
	#endif
	// todo future: LCD define for Linux-based systems
#endif

/** Option json names (stored in PROGMEM to reduce RAM usage) */
// IMPORTANT: each json name is strictly 5 characters
// with 0 fillings if less
#define OP_JSON_NAME_STEPSIZE 5
// for Integer options
const char iopt_json_names[] PROGMEM =
	"fwv\0\0"
	"tz\0\0\0"
	"ntp\0\0"
	"dhcp\0"
	"ip1\0\0"
	"ip2\0\0"
	"ip3\0\0"
	"ip4\0\0"
	"gw1\0\0"
	"gw2\0\0"
	"gw3\0\0"
	"gw4\0\0"
	"hp0\0\0"
	"hp1\0\0"
	"hwv\0\0"
	"ext\0\0"
	"seq\0\0"
	"sdt\0\0"
	"mas\0\0"
	"mton\0"
	"mtof\0"
	"urs\0\0"
	"rso\0\0"
	"wl\0\0\0"
	"den\0\0"
	"ipas\0"
	"devid"
	"con\0\0"
	"lit\0\0"
	"dim\0\0"
	"bst\0\0"
	"uwt\0\0"
	"ntp1\0"
	"ntp2\0"
	"ntp3\0"
	"ntp4\0"
	"lg\0\0\0"
	"mas2\0"
	"mton2"
	"mtof2"
	"fwm\0\0"
	"fpr0\0"
	"fpr1\0"
	"re\0\0\0"
	"dns1\0"
	"dns2\0"
	"dns3\0"
	"dns4\0"
	"sar\0\0"
	"ife\0\0"
	"sn1t\0"
	"sn1o\0"
	"sn2t\0"
	"sn2o\0"
	"sn1on"
	"sn1of"
	"sn2on"
	"sn2of"
	"subn1"
	"subn2"
	"subn3"
	"subn4"
	"wimod"
	"reset"
	;

/** Option prompts (stored in PROGMEM to reduce RAM usage) */
// Each string is strictly 16 characters
// with SPACE fillings if less
const char iopt_prompts[] PROGMEM =
	"Firmware version"
	"Time zone (GMT):"
	"Enable NTP sync?"
	"Enable DHCP?    "
	"Static.ip1:     "
	"Static.ip2:     "
	"Static.ip3:     "
	"Static.ip4:     "
	"Gateway.ip1:    "
	"Gateway.ip2:    "
	"Gateway.ip3:    "
	"Gateway.ip4:    "
	"HTTP Port:      "
	"----------------"
	"Hardware version"
	"# of exp. board:"
	"----------------"
	"Stn. delay (sec)"
	"Master 1 (Mas1):"
	"Mas1  on adjust:"
	"Mas1 off adjust:"
	"----------------"
	"----------------"
	"Watering level: "
	"Device enabled? "
	"Ignore password?"
	"Device ID:      "
	"LCD contrast:   "
	"LCD brightness: "
	"LCD dimming:    "
	"DC boost time:  "
	"Weather algo.:  "
	"NTP server.ip1: "
	"NTP server.ip2: "
	"NTP server.ip3: "
	"NTP server.ip4: "
	"Enable logging? "
	"Master 2 (Mas2):"
	"Mas2  on adjust:"
	"Mas2 off adjust:"
	"Firmware minor: "
	"Pulse rate:     "
	"----------------"
	"As remote ext.? "
	"DNS server.ip1: "
	"DNS server.ip2: "
	"DNS server.ip3: "
	"DNS server.ip4: "
	"Special Refresh?"
	"IFTTT Enable:   "
	"Sensor 1 type:  "
	"Normally open?  "
	"Sensor 2 type:  "
	"Normally open?  "
	"Sn1 on adjust:  "
	"Sn1 off adjust: "
	"Sn2 on adjust:  "
	"Sn2 off adjust: "
	"Subnet mask1:   "
	"Subnet mask2:   "
	"Subnet mask3:   "
	"Subnet mask4:   "
	"WiFi mode?      "
	"Factory reset?  ";

// string options do not have prompts

/** Option maximum values (stored in PROGMEM to reduce RAM usage) */
const byte iopt_max[] PROGMEM = {
	0,
	108,
	1,
	1,
	255,
	255,
	255,
	255,
	255,
	255,
	255,
	255,
	255,
	255,
	0,
	MAX_EXT_BOARDS,
	1,
	255,
	MAX_NUM_STATIONS,
	255,
	255,
	255,
	1,
	250,
	1,
	1,
	255,
	255,
	255,
	255,
	250,
	255,
	255,
	255,
	255,
	255,
	1,
	MAX_NUM_STATIONS,
	255,
	255,
	0,
	255,
	255,
	1,
	255,
	255,
	255,
	255,
	1,
	255,
	255,
	1,
	255,
	1,
	255,
	255,
	255,
	255,
	255,
	255,
	255,
	255,
	255,
	1
};

// string options do not have maximum values

/** Integer option values (stored in RAM) */
byte OpenSprinkler::iopts[] = {
	OS_FW_VERSION, // firmware version
	28, // default time zone: GMT-5
	1,  // 0: disable NTP sync, 1: enable NTP sync
	1,  // 0: use static ip, 1: use dhcp
	0,  // this and next 3 bytes define static ip
	0,
	0,
	0,
	0,  // this and next 3 bytes define static gateway ip
	0,
	0,
	0,
#if defined(ARDUINO)  // on AVR, the default HTTP port is 80
	80, // this and next byte define http port number
	0,
#else // on RPI/BBB/LINUX, the default HTTP port is 8080
	144,// this and next byte define http port number
	31,
#endif
	OS_HW_VERSION,
	0,  // number of 8-station extension board. 0: no extension boards
	1,  // the option 'sequential' is now retired
	120,// station delay time (-10 minutes to 10 minutes).
	0,  // index of master station. 0: no master station
	120,// master on time adjusted time (-10 minutes to 10 minutes)
	120,// master off adjusted time (-10 minutes to 10 minutes)
	0,  // urs (retired)
	0,  // rso (retired)
	100,// water level (default 100%),
	1,  // device enable
	0,  // 1: ignore password; 0: use password
	0,  // device id
	150,// lcd contrast
	100,// lcd backlight
	#if defined(ESP32)
	40,
	#else
	15, // lcd dimming
	#endif
	80, // boost time (only valid to DC and LATCH type)
	0,  // weather algorithm (0 means not using weather algorithm)
	0,  // this and the next three bytes define the ntp server ip
	0,
	0,
	0,
	1,  // enable logging: 0: disable; 1: enable.
	0,  // index of master2. 0: no master2 station
	120,// master2 on adjusted time
	120,// master2 off adjusted time
	OS_FW_MINOR, // firmware minor version
	100,// this and next byte define flow pulse rate (100x)
	0,  // default is 1.00 (100)
	0,  // set as remote extension
	8,  // this and the next three bytes define the custom dns server ip
	8,
	8,
	8,
	0,  // special station auto refresh
	0,  // ifttt enable bits
	0,  // sensor 1 type (see SENSOR_TYPE macro defines)
	1,  // sensor 1 option. 0: normally closed; 1: normally open.	default 1.
	0,  // sensor 2 type
	1,  // sensor 2 option. 0: normally closed; 1: normally open. default 1.
	0,  // sensor 1 on delay
	0,  // sensor 1 off delay
	0,  // sensor 2 on delay
	0,  // sensor 2 off delay
	255,// subnet mask 1
	255,// subnet mask 2
	255,// subnet mask 3
	0,
	WIFI_MODE_AP, // wifi mode
	0   // reset
};

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
};

#if defined(BOOT_MENU_V2)

// Submenu for first menu set
MenuItem bootMenuSetup[] = {
  {"Submenu Item 1.1", nullptr, 0, -1,nullptr,false},
  {"Submenu Item 1.2", nullptr, 0, -1,nullptr,false},
  {"Submenu Item 1.3", nullptr, 0, -1,nullptr,false},
  {"<== Back ", nullptr, 0, -1,nullptr,false},
};

// First menu set
MenuItem bootMenu[] = {
  {" === Setup ===", bootMenuSetup, sizeof(bootMenuSetup) / sizeof(bootMenuSetup[0]), -1, nullptr, false},
  {"Device PW Reset?", nullptr, 0, -1,nullptr,false},
  {" Factory reset?", nullptr, 0, -1,OpenSprinkler::pre_factory_reset,true},
  {" Internal test", nullptr, 0, -1,OpenSprinkler::set_test_mode,false},
  {"  == Exit ==", nullptr, 0, -1,nullptr,false},
};

#endif

/** Weekday strings (stored in PROGMEM to reduce RAM usage) */
static const char days_str[] PROGMEM =
	"Mon\0"
	"Tue\0"
	"Wed\0"
	"Thu\0"
	"Fri\0"
	"Sat\0"
	"Sun\0";

/** Calculate local time (UTC time plus time zone offset) */
time_t OpenSprinkler::now_tz() {
	return now()+(int32_t)3600/4*(int32_t)(iopts[IOPT_TIMEZONE]-48);
}

#if defined(ARDUINO)

bool detect_i2c(int addr) {
	Wire.beginTransmission(addr);
	return (Wire.endTransmission()==0); // successful if received 0
}

/** read hardware MAC into tmp_buffer */
#define MAC_CTRL_ID 0x50
bool OpenSprinkler::load_hardware_mac(byte* buffer, bool wired) {
#if defined(ESP8266) || defined(ESP32)
	WiFi.macAddress((byte*)buffer);
	// if requesting wired Ethernet MAC, flip the last byte to create a modified MAC
	if(wired) buffer[5] = ~buffer[5];
	return true;
#else
	// initialize the buffer by assigning software mac
	buffer[0] = 0x00;
	buffer[1] = 0x69;
	buffer[2] = 0x69;
	buffer[3] = 0x2D;
	buffer[4] = 0x31;
	buffer[5] = iopts[IOPT_DEVICE_ID];
	if (detect_i2c(MAC_CTRL_ID)==false)	return false;

	Wire.beginTransmission(MAC_CTRL_ID);
	Wire.write(0xFA); // The address of the register we want
	Wire.endTransmission(); // Send the data
	if(Wire.requestFrom(MAC_CTRL_ID, 6) != 6) return false;	// if not enough data, return false
	for(byte ret=0;ret<6;ret++) {
		buffer[ret] = Wire.read();
	}
	return true;
#endif
}

void(* resetFunc) (void) = 0; // AVR software reset function

/** Initialize network with the given mac address and http port */

byte OpenSprinkler::start_network() {
	lcd_print_line_clear_pgm(PSTR("Starting..."), 1);
	uint16_t httpport = (uint16_t)(iopts[IOPT_HTTPPORT_1]<<8) + (uint16_t)iopts[IOPT_HTTPPORT_0];

#ifdef ENABLE_DEBUG
#if defined(ESP32)
  //DEBUG_PRINTLN(F("ESP32 FS dir:"));
  ESP32_FS_list_dir();
  DEBUG_PRINTLN(F("Starting network"));
#endif //ESP32
#endif 

#if defined(ESP8266) || defined(ESP32)

	#if defined(ESP32)
		useEth = false;
	#else
	if (start_ether()) {
		useEth = true;
	} else {
		useEth = false;
	}
	#endif

	DEBUG_PRINT("ETH enabled: ");
	DEBUG_PRINTLN(useEth);
	DEBUG_PRINT("Wifi mode: ");
	DEBUG_PRINTLN(( get_wifi_mode() == WIFI_MODE_STA ) ?  F("STA") : F("AP"));
	
	// FIXME, just for testing
	::start_server_ap();

	if((useEth || get_wifi_mode()==WIFI_MODE_STA) && otc.en>0 && otc.token.length()>=32) {
		otf = new OTF::OpenThingsFramework(httpport, otc.server, otc.port, otc.token, false, ether_buffer, ETHER_BUFFER_SIZE);
		DEBUG_PRINTLN(F("Started OTF with remote connection"));
	} else {
		DEBUG_PRINT(F("OTF start with http_port "));
		DEBUG_PRINTLN(httpport);
		otf = new OTF::OpenThingsFramework(httpport, ether_buffer, ETHER_BUFFER_SIZE);
		DEBUG_PRINTLN(F("Started OTF with just local connection"));
	}
	DEBUG_PRINTLN("DNSServer start");
	extern DNSServer *dns;
	if(get_wifi_mode() == WIFI_MODE_AP) dns = new DNSServer();
	if(update_server) { delete update_server; update_server = NULL; }
	DEBUG_PRINTLN(F("Setting update server"));
	#if defined(ESP8266)
	update_server = new ESP8266WebServer(8080);
	#elif defined(ESP32)
	//DEBUG_PRINTLN(F("Starting MDNS service"));
	//MDNS.addService("_http", "_tcp", 8080 );
	//MDNS.addServiceTxt("_http", "_tcp", "path", "/");
	//      MDNS.addService("_http", "_udp", 80 );
	update_server = new WebServer(8080);
	#endif
	DEBUG_PRINTLN(F("Started update server"));
	return 1;

#else

	if (start_ether()) {
		if(m_server)	{ delete m_server; m_server = NULL; }
		m_server = new EthernetServer(httpport);
		m_server->begin();
		useEth = true;
		return 1;
	}	else {
		useEth = false;
		return 0;
	}

#endif
}

byte OpenSprinkler::start_ether() {
#if defined(ESP8266)
	if(hw_rev<2) return 0;  // ethernet capability is only available when hw_rev>=2
	eth.isW5500 = (hw_rev==2)?false:true; // os 3.2 uses enc28j60 and 3.3 uses w5500

	SPI.begin();
	SPI.setBitOrder(MSBFIRST);
	SPI.setDataMode(SPI_MODE0);
	SPI.setFrequency(4000000);

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
	lcd_print_line_clear_pgm(eth.isW5500 ? PSTR("    (w5500)    ") : PSTR("   (enc28j60)   "), 2);
	
	ulong timeout = millis()+30000; // 30 seconds time out
	while (!eth.connected()) {
		DEBUG_PRINT(".");
		delay(1000);
		if(millis()>timeout) return 0;
	}

	DEBUG_PRINTLN();
	DEBUG_PRINT("eth.ip:");
	DEBUG_PRINTLN(eth.localIP());
	DEBUG_PRINT("eth.dns:");
	DEBUG_PRINTLN(WiFi.dnsIP());

	if (iopts[IOPT_USE_DHCP]) {
		memcpy(iopts+IOPT_STATIC_IP1, &(eth.localIP()[0]), 4);
		memcpy(iopts+IOPT_GATEWAY_IP1, &(eth.gatewayIP()[0]),4);
		memcpy(iopts+IOPT_DNS_IP1, &(WiFi.dnsIP()[0]), 4); // todo: lwip need dns ip
		memcpy(iopts+IOPT_SUBNET_MASK1, &(eth.subnetMask()[0]), 4);
		iopts_save();
	}

	return 1;
#elif defined(ESP32)
	// no wired network support now
	return 0;
#else
	Ethernet.init(PIN_ETHER_CS);  // make sure to call this before any Ethernet calls
	if(Ethernet.hardwareStatus()==EthernetNoHardware) return 0;
	load_hardware_mac((uint8_t*)tmp_buffer, true);

	lcd_print_line_clear_pgm(PSTR("Start wired link"), 1);

	if (iopts[IOPT_USE_DHCP]) {
		if(!Ethernet.begin((uint8_t*)tmp_buffer))	return 0;
		memcpy(iopts+IOPT_STATIC_IP1, &(Ethernet.localIP()[0]), 4);
		memcpy(iopts+IOPT_GATEWAY_IP1, &(Ethernet.gatewayIP()[0]),4);
		memcpy(iopts+IOPT_DNS_IP1, &(Ethernet.dnsServerIP()[0]), 4);
		memcpy(iopts+IOPT_SUBNET_MASK1, &(Ethernet.subnetMask()[0]), 4);
		iopts_save();
	} else {
		IPAddress staticip(iopts+IOPT_STATIC_IP1);
		IPAddress gateway(iopts+IOPT_GATEWAY_IP1);
		IPAddress dns(iopts+IOPT_DNS_IP1);
		IPAddress subn(iopts+IOPT_SUBNET_MASK1);
		Ethernet.begin((uint8_t*)tmp_buffer, staticip, dns, gateway, subn);
	}

	return 1;
#endif
}

bool OpenSprinkler::network_connected(void) {
#if defined (ESP8266)
	if(useEth) return true; // todo: lwip currently does not have a way to check link status
	else
		return (get_wifi_mode()==WIFI_MODE_STA && WiFi.status()==WL_CONNECTED && state==OS_STATE_CONNECTED);
#elif defined(ESP32)
	return (get_wifi_mode()==WIFI_MODE_STA && WiFi.status()==WL_CONNECTED && state==OS_STATE_CONNECTED);
#else
	return (Ethernet.linkStatus()==LinkON);
#endif
}

/** Reboot controller */
void OpenSprinkler::reboot_dev(uint8_t cause) {
	lcd_print_line_clear_pgm(PSTR("Rebooting..."), 0);
	if(cause) {
		nvdata.reboot_cause = cause;
		nvdata_save();
	}
#if defined(ESP8266) || defined(ESP32)
	ESP.restart();
#else
	resetFunc();
#endif
}

#else // RPI/BBB/LINUX network init functions

#include "etherport.h"
#include <sys/reboot.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include "utils.h"
#include "opensprinkler_server.h"

/** Initialize network with the given mac address and http port */
byte OpenSprinkler::start_network() {
	unsigned int port = (unsigned int)(iopts[IOPT_HTTPPORT_1]<<8) + (unsigned int)iopts[IOPT_HTTPPORT_0];
#if defined(DEMO)
#if defined(HTTP_PORT)
	port = HTTP_PORT;
#else
	port = 80;
#endif
#endif
	if(m_server) { delete m_server; m_server = 0; }

	m_server = new EthernetServer(port);
	return m_server->begin();
}

bool OpenSprinkler::network_connected(void) {
	return true;
}

// Return mac of first recognised interface and fallback to software mac
// Note: on OSPi, operating system handles interface allocation so 'wired' ignored
bool OpenSprinkler::load_hardware_mac(byte* mac, bool wired) {
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

	if (m_server == NULL) return true;

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == 0) return true;

	// Returns the mac address of the first interface if multiple active
	for (int i = 0; i < sizeof(if_names)/sizeof(const char *); i++) {
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
#endif
}

/** Launch update script */
void OpenSprinkler::update_dev() {
	char cmd[1000];
	sprintf(cmd, "cd %s && ./updater.sh", get_runtime_path());
	system(cmd);
}
#endif // end network init functions

#if defined(ARDUINO)
/** Initialize LCD */
void OpenSprinkler::lcd_start() {

#if defined(ESP8266) || defined(ESP32)
	#if defined(LCD_SH1106)
	DEBUG_PRINTLN("LCD SH1106 init");
	#else
	DEBUG_PRINTLN("LCD SSD1306 init");
	#endif
	// initialize SSD1306
	lcd.init();
	lcd.begin();
	flash_screen();
#else
	// initialize 16x2 character LCD
	// turn on lcd
	lcd.init(1, PIN_LCD_RS, 255, PIN_LCD_EN, PIN_LCD_D4, PIN_LCD_D5, PIN_LCD_D6, PIN_LCD_D7, 0,0,0,0);
	lcd.begin();

	if (lcd.type() == LCD_STD) {
		// this is standard 16x2 LCD
		// set PWM frequency for adjustable LCD backlight and contrast
		TCCR1B = 0x02;	// increase division factor for faster clock
		// turn on LCD backlight and contrast
		lcd_set_brightness();
		lcd_set_contrast();
	} else {
		// for I2C LCD, we don't need to do anything
	}
#endif
}
#endif

//extern void flow_isr();

/** Initialize pins, controller variables, LCD */
void OpenSprinkler::begin() {

#if defined(ARDUINO)
	#if defined(ESP32)
	if(!Wire.begin(SDA_PIN,SCL_PIN)) { DEBUG_PRINTLN(F("Error initiating I2C")); }
	#ifdef ENABLE_DEBUG
	scan_i2c();
	#endif
  	#else 
	Wire.begin(); // init I2C
	#endif
#endif

	hw_type = HW_TYPE_UNKNOWN;
	hw_rev = 0;

#if defined(ESP8266) // ESP8266 specific initializations

	/* check hardware type */
	if(detect_i2c(ACDR_I2CADDR)) hw_type = HW_TYPE_AC;
	else if(detect_i2c(DCDR_I2CADDR)) hw_type = HW_TYPE_DC;
	else if(detect_i2c(LADR_I2CADDR)) hw_type = HW_TYPE_LATCH;

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

		// on revision 0, main IOEXP and driver IOEXP are two separate PCF8574 chips
		if(hw_type==HW_TYPE_DC) {
			drio = new PCF8574(DCDR_I2CADDR);
		} else if(hw_type==HW_TYPE_LATCH) {
			drio = new PCF8574(LADR_I2CADDR);
		} else {
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

		if(hw_type==HW_TYPE_DC) {
			drio = new PCA9555(DCDR_I2CADDR);
		} else if(hw_type==HW_TYPE_LATCH) {
			drio = new PCA9555(LADR_I2CADDR);
		} else {
			drio = new PCA9555(ACDR_I2CADDR);
		}
		mainio = drio;

		pinMode(16, INPUT);
		if(digitalRead(16)==LOW) {
			// revision 1
			hw_rev = 1;
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
		} else {
			// revision 2 and 3
			if(detect_i2c(EEPROM_I2CADDR)) { // revision 3 has a I2C EEPROM
				hw_rev = 3;
			} else {
				hw_rev = 2;
			}
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
		}
	}

	/* detect expanders */
	for(byte i=0;i<(MAX_NUM_BOARDS)/2;i++)
		expanders[i] = NULL;
	detect_expanders();
#elif defined(ESP32)

    hw_type = HW_TYPE_AC;
    
	PIN_RFRX = E0_PIN_RFRX;
	PIN_RFTX = E0_PIN_RFTX;
	PIN_BOOST = E0_PIN_BOOST;
	PIN_BOOST_EN = E0_PIN_BOOST_EN;
	PIN_SENSOR1 = E0_PIN_SENSOR1;
	PIN_SENSOR2 = E0_PIN_SENSOR2;
  
	/*
	#if defined(ETHPORT)
		hw_rev = 2;
	#else
		hw_rev = 0;
	#endif
 	*/
 	hw_rev = 0;
	  // mainio = new IOEXP(ACDR_I2CADDR); // dont need this as all is handled by ESP32 gpio's

  #if ! defined(USE_IOEXP_SR) || ( defined(USE_IOEXP_SR) && USE_IOEXP_SR != 1 )
  	DEBUG_PRINTLN("Using GPIO IOEXP");
	drio = new BUILD_IN_GPIO(); // to handle gpio's on ESP32 board
	drio->set_pins_output_mode();
  #else
  	DEBUG_PRINTLN(F("Using Shift-register IOEXP"));
	drio = new IOEXP_SR();
	DEBUG_PRINTLN(F("Driver class initiated"));
	drio->set_pins_output_mode();
	DEBUG_PRINTLN("PIN SETUP COMPLETE");
  #endif	
	// ROTARY ENCODER not supported now
	#if ! defined(USE_ROTARY_ENCODER)
		PIN_BUTTON_1 = E0_PIN_BUTTON_1;
		PIN_BUTTON_2 = E0_PIN_BUTTON_2;
		PIN_BUTTON_3 = E0_PIN_BUTTON_3;
		
		pinMode(PIN_BUTTON_1, INPUT_PULLUP);
		pinMode(PIN_BUTTON_2, INPUT_PULLUP);
		pinMode(PIN_BUTTON_3, INPUT_PULLUP);
	#else
		DEBUG_PRINTLN(F("Rotary encoder enabled"));
		PIN_BUTTON_1 = ROTARY_ENCODER_A_PIN;
		PIN_BUTTON_2 = ROTARY_ENCODER_BUTTON_PIN;
		PIN_BUTTON_3 = ROTARY_ENCODER_B_PIN;

		pinMode(PIN_BUTTON_1, INPUT_PULLUP);
		pinMode(PIN_BUTTON_2, INPUT_PULLUP);
		pinMode(PIN_BUTTON_3, INPUT_PULLUP);

		attachInterrupt(digitalPinToInterrupt(ROTARY_ENCODER_A_PIN), OpenSprinkler::handleEncoder, CHANGE);

	#endif

	/* TODO - interrupt driven button/rotary encoder */

	/* detect expanders */
	for(byte i=0;i<(MAX_NUM_BOARDS)/2;i++)
		expanders[i] = NULL;
	DEBUG_PRINTLN("Starting to detect expanders");
	detect_expanders();

	DEBUG_PRINT("hw_type = "); DEBUG_PRINT(hw_type); DEBUG_PRINT(" hw_rev = "); DEBUG_PRINTLN(hw_rev);
#else

	// shift register setup
	pinMode(PIN_SR_OE, OUTPUT);
	// pull shift register OE high to disable output
	digitalWrite(PIN_SR_OE, HIGH);
	pinMode(PIN_SR_LATCH, OUTPUT);
	digitalWrite(PIN_SR_LATCH, HIGH);

	pinMode(PIN_SR_CLOCK, OUTPUT);

	#if defined(OSPI)
		pin_sr_data = PIN_SR_DATA;
		// detect RPi revision
		unsigned int rev = detect_rpi_rev();
		if (rev==0x0002 || rev==0x0003)
			pin_sr_data = PIN_SR_DATA_ALT;
		// if this is revision 1, use PIN_SR_DATA_ALT
		pinMode(pin_sr_data, OUTPUT);
	#else
		pinMode(PIN_SR_DATA, OUTPUT);
	#endif

#endif

	// Reset all stations
	clear_all_station_bits();
	apply_all_station_bits();

#if defined(ESP8266) || defined(ESP32)
	// OS 3.0 has two independent sensors
	pinModeExt(PIN_SENSOR1, INPUT_PULLUP);
	pinModeExt(PIN_SENSOR2, INPUT_PULLUP);
	
	DEBUG_PRINTLN("Starting sensors - INPUT_PULLUP");
 	DEBUG_PRINT("Sensor1 PIN: "); DEBUG_PRINTLN(PIN_SENSOR1);
  	DEBUG_PRINT("Sensor2 PIN: "); DEBUG_PRINTLN(PIN_SENSOR2);
#else
	// pull shift register OE low to enable output
	digitalWrite(PIN_SR_OE, LOW);
	// Rain sensor port set up
	pinMode(PIN_SENSOR1, INPUT_PULLUP);
	#if defined(PIN_SENSOR2)
	pinMode(PIN_SENSOR2, INPUT_PULLUP);
	#endif
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

	DEBUG_PRINTLN("Starting RFTX pins ");
  	DEBUG_PRINT("RFTX PIN: "); DEBUG_PRINTLN(PIN_RFTX);

#if defined(ARDUINO)  // AVR SD and LCD functions

	#if defined(ESP8266) || defined(ESP32)  // OS3.0 specific detections
		#if PIN_CURR_SENSE != 255
		status.has_curr_sense = 1;  // OS3.0 has current sensing capacility
		# else
		status.has_curr_sense = 0;  // OS3.0 has current sensing capacility
		#endif
		// measure baseline current
		baseline_current = 80;

	#else // OS 2.3 specific detections

		// detect hardware type
		if (detect_i2c(MAC_CTRL_ID)) {
			Wire.beginTransmission(MAC_CTRL_ID);
			Wire.write(0x00);
			Wire.endTransmission();
			Wire.requestFrom(MAC_CTRL_ID, 1);
			byte ret = Wire.read();
			if (ret == HW_TYPE_AC || ret == HW_TYPE_DC || ret == HW_TYPE_LATCH) {
				hw_type = ret;
			} else {
				hw_type = HW_TYPE_AC; // if type not supported, make it AC
			}
		}

		if (hw_type == HW_TYPE_DC) {
			pinMode(PIN_BOOST, OUTPUT);
			digitalWrite(PIN_BOOST, LOW);

			pinMode(PIN_BOOST_EN, OUTPUT);
			digitalWrite(PIN_BOOST_EN, LOW);
		}

		// detect if current sensing pin is present
		pinMode(PIN_CURR_DIGITAL, INPUT);
		digitalWrite(PIN_CURR_DIGITAL, HIGH); // enable internal pullup
		status.has_curr_sense = digitalRead(PIN_CURR_DIGITAL) ? 0 : 1;
		digitalWrite(PIN_CURR_DIGITAL, LOW);
		baseline_current = 0;

	#endif

	lcd_start();

	#if defined(ESP8266) || defined(ESP32) // todo: we should remove this, until esp32 port has ethernet support
		lcd.createChar(ICON_ETHER_CONNECTED, _iconimage_ether_connected);
		lcd.createChar(ICON_ETHER_DISCONNECTED, _iconimage_ether_disconnected);
	#else
		lcd.createChar(ICON_ETHER_CONNECTED, _iconimage_connected);
		lcd.createChar(ICON_ETHER_DISCONNECTED, _iconimage_disconnected);
	#endif
	lcd.createChar(ICON_REMOTEXT, _iconimage_remotext);
	lcd.createChar(ICON_RAINDELAY, _iconimage_raindelay);
	lcd.createChar(ICON_RAIN, _iconimage_rain);
	lcd.createChar(ICON_SOIL, _iconimage_soil);

	#if defined(ESP8266) || defined(ESP32)

		/* create custom characters */
		lcd.createChar(ICON_WIFI_CONNECTED, _iconimage_wifi_connected);
		lcd.createChar(ICON_WIFI_DISCONNECTED, _iconimage_wifi_disconnected);

		lcd.setCursor(0,0);
		lcd.print(F("Init file system"));
		DEBUG_PRINT("Init file system... ");
		lcd.setCursor(0,1);
		lcd.print(F("Please wait..."));
		#if defined(ESP8266)
		if(!LittleFS.begin()) {
		#elif defined(ESP32)
			// Get the size of the flash memory
			uint32_t flash_size = ESP.getFlashChipSize();
			
			DEBUG_PRINTLN();
			DEBUG_PRINT(F("Flash size: "));
			DEBUG_PRINT(flash_size);
			DEBUG_PRINTLN(F(" bytes"));			

			#if defined(ESP32_LIST_PARTITIONS)
			DEBUG_PRINTLN(F("ESP32 Partition table:"));

			DEBUG_PRINTLN(F("| Type | Sub |  Offset  |   Size   |       Label      |"));
			DEBUG_PRINTLN(F("| ---- | --- | -------- | -------- | ---------------- |"));
			
			esp_partition_iterator_t pi = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
			if (pi != NULL) {
				do {
				const esp_partition_t* p = esp_partition_get(pi);
				printf("|  %02x  | %02x  | 0x%06X | 0x%06X | %-16s |\r\n", 
					p->type, p->subtype, p->address, p->size, p->label);
				} while (pi = (esp_partition_next(pi)));
			}
			#endif
		if(!LittleFS.begin(ESP32_FORMAT_FS_IF_FAILED)) {
		#endif

			// !!! flash init failed, stall as we cannot proceed
			lcd.setCursor(0, 0);
			lcd_print_pgm(PSTR("Error Code: 0x2D"));
			DEBUG_PRINTLN(F("FAILED."));
			delay(10000);
			//reboot_dev(REBOOT_CAUSE_PROGRAM);
		} else {
			ESP32_listDir("/",2);
			
			//ESP32_readFile(IOPTS_FILENAME);
		}
		DEBUG_PRINTLN(F("Done."));
		lcd.clear(0,0);
		lcd.clear(0,1);
		lcd.clear(0,2);

		state = OS_STATE_INITIAL;

	#else

		// set sd cs pin high to release SD
		pinMode(PIN_SD_CS, OUTPUT);
		digitalWrite(PIN_SD_CS, HIGH);

		if(!sd.begin(PIN_SD_CS, SPI_HALF_SPEED)) {
			// !!! sd card not detected, stall as we cannot proceed
			lcd.setCursor(0, 0);
			lcd_print_pgm(PSTR("Error Code: 0x2D"));
			while(1){}
		}

	#endif

	// set button pins
	// enable internal pullup
	pinMode(PIN_BUTTON_1, INPUT_PULLUP);
	pinMode(PIN_BUTTON_2, INPUT_PULLUP);
	pinMode(PIN_BUTTON_3, INPUT_PULLUP);

	// Rotary knob setup should happen here!

	// detect and check RTC type
	DEBUG_PRINT(F("Detecting RTC..."));
	RTC.detect();
	DEBUG_PRINTLN(F("done."));

	#if defined(ESP32)
		// workaround for OTF 
		start_network_ap(wifi_ssid.c_str(),wifi_pass.c_str());
	#endif

#else
	DEBUG_PRINTLN(get_runtime_path());
#endif
}

#if defined(ESP8266) || defined(ESP32)
/** LATCH boost voltage
 *
 */
void OpenSprinkler::latch_boost() {
	digitalWriteExt(PIN_BOOST, HIGH);      // enable boost converter
	delay((int)iopts[IOPT_BOOST_TIME]<<2); // wait for booster to charge
	digitalWriteExt(PIN_BOOST, LOW);       // disable boost converter
}

/** Set all zones (for LATCH controller)
 *  This function sets all zone pins (including COM) to a specified value
 */
void OpenSprinkler::latch_setallzonepins(byte value) {
	digitalWriteExt(PIN_LATCH_COM, value);  // set latch com pin
	// Handle driver board (on main controller)
	if(drio->type==IOEXP_TYPE_9555) { // LATCH contorller only uses PCA9555, no other type
		uint16_t reg = drio->i2c_read(NXP_OUTPUT_REG);  // read current output reg value
		if(value) reg |= 0x00FF;  // first 8 zones are the lowest 8 bits of main driver board
		else reg &= 0xFF00;
		drio->i2c_write(NXP_OUTPUT_REG, reg); // write value to register
	}
	// Handle all expansion boards
	for(byte i=0;i<MAX_EXT_BOARDS/2;i++) {
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

	// todo: handle expander
}

/** Set one zone (for LATCH controller)
 *  This function sets one specified zone pin to a specified value
 */
void OpenSprinkler::latch_setzonepin(byte sid, byte value) {
	if(sid<8) { // on main controller
		if(drio->type==IOEXP_TYPE_9555) { // LATCH contorller only uses PCA9555, no other type
			uint16_t reg = drio->i2c_read(NXP_OUTPUT_REG);  // read current output reg value
			if(value) reg |= (1<<sid);
			else reg &= (~(1<<sid));
			drio->i2c_write(NXP_OUTPUT_REG, reg);  // write value to register
		}
	} else {  // on expander
		byte bid=(sid-8)>>4;
		uint16_t s=(sid-8)&0x0F;
		if(expanders[bid]->type==IOEXP_TYPE_9555) {
			uint16_t reg = expanders[bid]->i2c_read(NXP_OUTPUT_REG);  // read current output reg value
			if(value) reg |= (1<<s);
			else reg &= (~(1<<s));
			expanders[bid]->i2c_write(NXP_OUTPUT_REG, reg);
		}
	}
}

void OpenSprinkler::latch_setzoneoutput_v2(byte sid, byte A, byte K) {
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
		// todo: handle expander
	}
}

/** LATCH open / close a station
 *
 */
void OpenSprinkler::latch_open(byte sid) {
	if(hw_rev>=2) {
		DEBUG_PRINTLN(F("latch_open_v2"));
		latch_disable_alloutputs_v2(); // disable all output pins
		latch_boost(); // generate boost voltage
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

void OpenSprinkler::latch_close(byte sid) {
	if(hw_rev>=2) {
		DEBUG_PRINTLN(F("latch_close_v2"));
		latch_disable_alloutputs_v2(); // disable all output pins
		latch_boost(); // generate boost voltage
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
		for(byte i=0;i<nstations;i++) {
			byte bid=i>>3;
			byte s=i&0x07;
			byte mask=(byte)1<<s;
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
void OpenSprinkler::apply_all_station_bits() {

#if defined(ESP8266) || defined(ESP32)
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

		// DEBUG_PRINT("IOEXP type: ");
		// DEBUG_PRINTLN(drio->type);
		// DEBUG_PRINTLN(station_bits[0]);
		
		// Handle driver board (on main controller)
		if(drio->type==IOEXP_TYPE_8574) {
			/* revision 0 uses PCF8574 with active low logic, so all bits must be flipped */
			drio->i2c_write(NXP_OUTPUT_REG, ~station_bits[0]);
		} else if(drio->type==IOEXP_TYPE_9555) {
			/* revision 1 uses PCA9555 with active high logic */
			uint16_t reg = drio->i2c_read(NXP_OUTPUT_REG);  // read current output reg value
			reg = (reg&0xFF00) | station_bits[0]; // output channels are the low 8-bit
			drio->i2c_write(NXP_OUTPUT_REG, reg); // write value to register
		}
		#if defined(ESP32)
		 else if(drio->type == IOEXP_TYPE_BUILD_IN_GPIO  || drio->type == IOEXP_TYPE_SR){
			// stations_bits[0] holds the master station zones as a byte
			if (STATION_LOGIC) {
				#if defined(SEPARATE_MASTER_VALVE)
				// DEBUG_PRINT("Separate master on GPIO ");
				// DEBUG_PRINTLN(SEPARATE_MASTER_VALVE);
				// DEBUG_PRINTLN(station_bits[0]);
				if ( station_bits[0] > 0 ) {
					// DEBUG_PRINTLN("Turning MASTER VALVE On");
					digitalWrite(SEPARATE_MASTER_VALVE,HIGH);
				} else {
					// DEBUG_PRINTLN("Turning MASTER VALVE Off");
					digitalWrite(SEPARATE_MASTER_VALVE,LOW);
				}
				#endif
				drio->i2c_write(255, station_bits[0]);
			} else {
				#if defined(SEPARATE_MASTER_VALVE)
				if ( station_bits[0] > 0 ) {
					digitalWrite(SEPARATE_MASTER_VALVE,LOW);
				} else {
					digitalWrite(SEPARATE_MASTER_VALVE,HIGH);
				}
				#endif
				drio->i2c_write(255,~station_bits[0]);
			}
		} 
		#endif

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
	byte bid, s, sbits;

	// Shift out all station bit values
	// from the highest bit to the lowest
	for(bid=0;bid<=MAX_EXT_BOARDS;bid++) {
		if (status.enabled)
			sbits = station_bits[MAX_EXT_BOARDS-bid];
		else
			sbits = 0;

		for(s=0;s<8;s++) {
			digitalWrite(PIN_SR_CLOCK, LOW);
	#if defined(OSPI) // if OSPI, use dynamically assigned pin_sr_data
			digitalWrite(pin_sr_data, (sbits & ((byte)1<<(7-s))) ? HIGH : LOW );
	#else
			digitalWrite(PIN_SR_DATA, (sbits & ((byte)1<<(7-s))) ? HIGH : LOW );
	#endif
			digitalWrite(PIN_SR_CLOCK, HIGH);
		}
	}

	#if defined(ARDUINO)
	if((hw_type==HW_TYPE_DC) && engage_booster) {
		// for DC controller: boost voltage
		digitalWrite(PIN_BOOST_EN, LOW);  // disable output path
		digitalWrite(PIN_BOOST, HIGH);    // enable boost converter
		delay((int)iopts[IOPT_BOOST_TIME]<<2);  // wait for booster to charge
		digitalWrite(PIN_BOOST, LOW);  // disable boost converter

		digitalWrite(PIN_BOOST_EN, HIGH);  // enable output path
		digitalWrite(PIN_SR_LATCH, HIGH);
		engage_booster = 0;
	} else {
		digitalWrite(PIN_SR_LATCH, HIGH);
	}
	#else
	digitalWrite(PIN_SR_LATCH, HIGH);
	#endif
#endif

	if(iopts[IOPT_SPE_AUTO_REFRESH]) {
		// handle refresh of RF and remote stations
		// we refresh the station that's next in line
		static byte next_sid_to_refresh = MAX_NUM_STATIONS>>1;
		static byte lastnow = 0;
		ulong curr_time = now_tz();
		byte _now = (curr_time & 0xFF);
		if (lastnow != _now) {  // perform this no more than once per second
			lastnow = _now;
			next_sid_to_refresh = (next_sid_to_refresh+1) % MAX_NUM_STATIONS;
			byte bid=next_sid_to_refresh>>3,s=next_sid_to_refresh&0x07;
			if(os.attrib_spe[bid]&(1<<s)) { // check if this is a special station
				bid=next_sid_to_refresh>>3;
				s=next_sid_to_refresh&0x07;
				bool on = (station_bits[bid]>>s)&0x01;
				uint16_t dur = 0;
				if(on) {
					byte sqi=pd.station_qid[next_sid_to_refresh];
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

/** Read rain sensor status */
void OpenSprinkler::detect_binarysensor_status(ulong curr_time) {
	// sensor_type: 0 if normally closed, 1 if normally open
	if(iopts[IOPT_SENSOR1_TYPE]==SENSOR_TYPE_RAIN || iopts[IOPT_SENSOR1_TYPE]==SENSOR_TYPE_SOIL) {
		if(hw_rev>=2)	pinModeExt(PIN_SENSOR1, INPUT_PULLUP); // this seems necessary for OS 3.2
		byte val = digitalReadExt(PIN_SENSOR1);
		status.sensor1 = (val == iopts[IOPT_SENSOR1_OPTION]) ? 0 : 1;
		if(status.sensor1) {
			if(!sensor1_on_timer) {
				// add minimum of 5 seconds on delay
				ulong delay_time = (ulong)iopts[IOPT_SENSOR1_ON_DELAY]*60;
				sensor1_on_timer = curr_time + (delay_time>5?delay_time:5);
				sensor1_off_timer = 0;
			} else {
				if(curr_time > sensor1_on_timer) {
					status.sensor1_active = 1;
				}
			}
		} else {
			if(!sensor1_off_timer) {
				ulong delay_time = (ulong)iopts[IOPT_SENSOR1_OFF_DELAY]*60;
				sensor1_off_timer = curr_time + (delay_time>5?delay_time:5);
				sensor1_on_timer = 0;
			} else {
				if(curr_time > sensor1_off_timer) {
					status.sensor1_active = 0;
				}
			}
		}
	}

// ESP8266 is guaranteed to have sensor 2
// if your ESP32 board has 2 sensors, then set PIN_SENOR2
#if defined(ESP8266) || defined(PIN_SENSOR2)
	if(iopts[IOPT_SENSOR2_TYPE]==SENSOR_TYPE_RAIN || iopts[IOPT_SENSOR2_TYPE]==SENSOR_TYPE_SOIL) {
		if(hw_rev>=2)	pinModeExt(PIN_SENSOR2, INPUT_PULLUP); // this seems necessary for OS 3.2
		byte val = digitalReadExt(PIN_SENSOR2);
		status.sensor2 = (val == iopts[IOPT_SENSOR2_OPTION]) ? 0 : 1;
		if(status.sensor2) {
			if(!sensor2_on_timer) {
				// add minimum of 5 seconds on delay
				ulong delay_time = (ulong)iopts[IOPT_SENSOR2_ON_DELAY]*60;
				sensor2_on_timer = curr_time + (delay_time>5?delay_time:5);
				sensor2_off_timer = 0;
			} else {
				if(curr_time > sensor2_on_timer) {
					status.sensor2_active = 1;
				}
			}
		} else {
			if(!sensor2_off_timer) {
				ulong delay_time = (ulong)iopts[IOPT_SENSOR2_OFF_DELAY]*60;
				sensor2_off_timer = curr_time + (delay_time>5?delay_time:5);
				sensor2_on_timer = 0;
			} else {
				if(curr_time > sensor2_off_timer) {
					status.sensor2_active = 0;
				}
			}
		}
	}

#endif
}

/** Return program switch status */
byte OpenSprinkler::detect_programswitch_status(ulong curr_time) {
	byte ret = 0;
	if(iopts[IOPT_SENSOR1_TYPE]==SENSOR_TYPE_PSWITCH) {
		static byte sensor1_hist = 0;
		if(hw_rev>=2) pinModeExt(PIN_SENSOR1, INPUT_PULLUP); // this seems necessary for OS 3.2
		status.sensor1 = (digitalReadExt(PIN_SENSOR1) != iopts[IOPT_SENSOR1_OPTION]); // is switch activated?
		sensor1_hist = (sensor1_hist<<1) | status.sensor1;
		// basic noise filtering: only trigger if sensor matches pattern:
		// i.e. two consecutive lows followed by two consecutive highs
		if((sensor1_hist&0b1111) == 0b0011) {
			ret |= 0x01;
		}
	}
#if defined(ESP8266) || defined(PIN_SENSOR2)
	if(iopts[IOPT_SENSOR2_TYPE]==SENSOR_TYPE_PSWITCH) {
		static byte sensor2_hist = 0;
		if(hw_rev>=2) pinModeExt(PIN_SENSOR2, INPUT_PULLUP); // this seems necessary for OS 3.2
		status.sensor2 = (digitalReadExt(PIN_SENSOR2) != iopts[IOPT_SENSOR2_OPTION]); // is sensor activated?
		sensor2_hist = (sensor2_hist<<1) | status.sensor2;
		if((sensor2_hist&0b1111) == 0b0011) {
			ret |= 0x02;
		}
	}
#endif
	return ret;
}

void OpenSprinkler::sensor_resetall() {
	sensor1_on_timer = 0;
	sensor1_off_timer = 0;
	sensor1_active_lasttime = 0;
	sensor2_on_timer = 0;
	sensor2_off_timer = 0;
	sensor2_active_lasttime = 0;
	old_status.sensor1_active = status.sensor1_active = 0;
	old_status.sensor2_active = status.sensor2_active = 0;
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
#if defined(ARDUINO)
uint16_t OpenSprinkler::read_current() {
	float scale = 1.0f;
	if(status.has_curr_sense) {
		if (hw_type == HW_TYPE_DC) {
			#if defined(ESP8266) || defined(ESP32)
			scale = 4.88;
			#else
			scale = 16.11;
			#endif
		} else if (hw_type == HW_TYPE_AC) {
			#if defined(ESP8266) || defined(ESP32)
			scale = 3.45;
			#else
			scale = 11.39;
			#endif
		} else {
			scale = 0.0;  // for other controllers, current is 0
		}
		/* do an average */
		const byte K = 8;
		uint16_t sum = 0;
		for(byte i=0;i<K;i++) {
			sum += analogRead(PIN_CURR_SENSE);
			delay(1);
		}
		return (uint16_t)((sum/K)*scale);
	} else {
		return 0;
	}
}
#endif

/** Read the number of 8-station expansion boards */
// AVR has capability to detect number of expansion boards
int OpenSprinkler::detect_exp() {
#if defined(ARDUINO)
	#if defined(ESP8266) || defined(ESP32)
	// detect the highest expansion board index
	int n;
	for(n=4;n>=0;n--) {
		if(detect_i2c(EXP_I2CADDR_BASE+n)) break;
	}
	return (n+1)*2;
	#else
	// OpenSprinkler uses voltage divider to detect expansion boards
	// Master controller has a 1.6K pull-up;
	// each expansion board (8 stations) has 2x 4.7K pull-down connected in parallel;
	// so the exact ADC value for n expansion boards is:
	//		ADC = 1024 * 9.4 / (10 + 9.4 * n)
	// Reverse this fomular we have:
	//		n = (1024 * 9.4 / ADC - 9.4) / 1.6
	int n = (int)((1024 * 9.4 / analogRead(PIN_EXP_SENSE) - 9.4) / 1.6 + 0.33);
	return n;
	#endif
#else
	return -1;
#endif
}

/** Convert hex code to ulong integer */
static ulong hex2ulong(byte *code, byte len) {
	char c;
	ulong v = 0;
	for(byte i=0;i<len;i++) {
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

/** Parse RF code into on/off/timeing sections */
uint16_t OpenSprinkler::parse_rfstation_code(RFStationData *data, ulong* on, ulong *off) {
	ulong v;
	v = hex2ulong(data->on, sizeof(data->on));
	if (!v) return 0;
	if (on) *on = v;
	v = hex2ulong(data->off, sizeof(data->off));
	if (!v) return 0;
	if (off) *off = v;
	v = hex2ulong(data->timing, sizeof(data->timing));
	if (!v) return 0;
	return v;
}

/** Get station data */
void OpenSprinkler::get_station_data(byte sid, StationData* data) {
	file_read_block(STATIONS_FILENAME, data, (uint32_t)sid*sizeof(StationData), sizeof(StationData));
}

/** Set station data */
/*
void OpenSprinkler::set_station_data(byte sid, StationData* data) {
	file_write_block(STATIONS_FILENAME, data, (uint32_t)sid*sizeof(StationData), sizeof(StationData));
}
*/

/** Get station name */
void OpenSprinkler::get_station_name(byte sid, char tmp[]) {
	tmp[STATION_NAME_SIZE]=0;
	file_read_block(STATIONS_FILENAME, tmp, (uint32_t)sid*sizeof(StationData)+offsetof(StationData, name), STATION_NAME_SIZE);
}

/** Set station name */
void OpenSprinkler::set_station_name(byte sid, char tmp[]) {
	// todo: store the right size
	tmp[STATION_NAME_SIZE]=0;
	char n0[STATION_NAME_SIZE+1];
	get_station_name(sid, n0);
	size_t len = strlen(n0);
	if(len!=strlen(tmp) || memcmp(n0, tmp, len)!=0) { // only write if the name has changed
		file_write_block(STATIONS_FILENAME, tmp, (uint32_t)sid*sizeof(StationData)+offsetof(StationData, name), STATION_NAME_SIZE);
	}
}

/** Get station type */
byte OpenSprinkler::get_station_type(byte sid) {
	return file_read_byte(STATIONS_FILENAME, (uint32_t)sid*sizeof(StationData)+offsetof(StationData, type));
}

byte OpenSprinkler::is_sequential_station(byte sid) {
	return attrib_grp[sid] != PARALLEL_GROUP_ID;
}

byte OpenSprinkler::is_master_station(byte sid) {
	for (byte mas = 0; mas < NUM_MASTER_ZONES; mas++) {
		if (get_master_id(mas) && (get_master_id(mas) - 1 == sid)) {
			return 1;
		}
	}
	return 0;
}

byte OpenSprinkler::is_running(byte sid) {
	return station_bits[(sid >> 3)] >> (sid & 0x07) & 1;
}

byte OpenSprinkler::get_master_id(byte mas) {
	return masters[mas][MASOPT_SID];
}

int16_t OpenSprinkler::get_on_adj(byte mas) {
	return water_time_decode_signed(masters[mas][MASOPT_ON_ADJ]);
}

int16_t OpenSprinkler::get_off_adj(byte mas) {
	return water_time_decode_signed(masters[mas][MASOPT_OFF_ADJ]);
}

byte OpenSprinkler::bound_to_master(byte sid, byte mas) {
	byte bid = sid >> 3;
	byte s = sid & 0x07;
	byte attributes = 0;

	switch (mas) {
		case MASTER_1:
			attributes= attrib_mas[bid];
			break;
		case MASTER_2:
			attributes = attrib_mas2[bid];
			break;
		default:
			break;
	}

	return attributes & (1 << s);
}

byte OpenSprinkler::get_station_gid(byte sid) {
	return attrib_grp[sid];
}

void OpenSprinkler::set_station_gid(byte sid, byte gid) {
	attrib_grp[sid] = gid;
}

/** Save all station attribs to file (backward compatibility) */
void OpenSprinkler::attribs_save() {
	// re-package attribute bits and save
	byte bid, s, sid=0;
	StationAttrib at, at0;
	memset(&at, 0, sizeof(StationAttrib));
	byte ty = STN_TYPE_STANDARD, ty0;
	for(bid=0;bid<MAX_NUM_BOARDS && sid<nstations;bid++) {
		for(s=0;s<8 && sid<nstations;s++,sid++) {
			at.mas = (attrib_mas[bid]>>s) & 1;
			at.igs = (attrib_igs[bid]>>s) & 1;
			at.mas2= (attrib_mas2[bid]>>s)& 1;
			at.igs2= (attrib_igs2[bid]>>s) & 1;
			at.igrd= (attrib_igrd[bid]>>s) & 1;
			at.dis = (attrib_dis[bid]>>s) & 1;
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
	byte bid, s, sid=0;
	StationAttrib at;
	byte ty;
	memset(attrib_mas, 0, nboards);
	memset(attrib_igs, 0, nboards);
	memset(attrib_mas2, 0, nboards);
	memset(attrib_igs2, 0, nboards);
	memset(attrib_igrd, 0, nboards);
	memset(attrib_dis, 0, nboards);
	memset(attrib_spe, 0, nboards);
	memset(attrib_grp, 0, MAX_NUM_STATIONS);

	for(bid=0;bid<MAX_NUM_BOARDS;bid++) {
		for(s=0;s<8;s++,sid++) {
			file_read_block(STATIONS_FILENAME, &at, (uint32_t)sid*sizeof(StationData)+offsetof(StationData, attrib), sizeof(StationAttrib));
			attrib_mas[bid] |= (at.mas<<s);
			attrib_igs[bid] |= (at.igs<<s);
			attrib_mas2[bid]|= (at.mas2<<s);
			attrib_igs2[bid]|= (at.igs2<<s);
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
byte OpenSprinkler::password_verify(const char *pw) {
	return (file_cmp_block(SOPTS_FILENAME, pw, SOPT_PASSWORD*MAX_SOPTS_SIZE)==0) ? 1 : 0;
}

#if defined(BOOT_MENU_V2)
void OpenSprinkler::set_test_mode() {
	wifi_testmode = 1;
	#if defined(TESTMODE_SSID)
	wifi_ssid = TESTMODE_SSID;
	wifi_pass = TESTMODE_PASS;
	#else
	wifi_ssid = "ostest";
	wifi_pass = "opendoor";
	#endif
}
#endif

// ==================
// Schedule Functions
// ==================

/** Index of today's weekday (Monday is 0) */
byte OpenSprinkler::weekday_today() {
	//return ((byte)weekday()+5)%7; // Time::weekday() assumes Sunday is 1
#if defined(ARDUINO)
	ulong wd = now_tz() / 86400L;
	return (wd+3) % 7;	// Jan 1, 1970 is a Thursday
#else
	return 0;
	// todo future: is this function needed for RPI/BBB?
#endif
}

/** Switch special station */
void OpenSprinkler::switch_special_station(byte sid, byte value, uint16_t dur) {
	// check if this is a special station
	byte bid=sid>>3,s=sid&0x07;
	if(!(os.attrib_spe[bid]&(1<<s))) return; // if this is not a special stations
	byte stype = get_station_type(sid);
	if(stype!=STN_TYPE_STANDARD) {
		// read station data
		StationData *pdata=(StationData*) tmp_buffer;
		get_station_data(sid, pdata);
		switch(stype) {

		case STN_TYPE_RF:
			switch_rfstation((RFStationData *)pdata->sped, value);
			break;

		case STN_TYPE_REMOTE:
			switch_remotestation((RemoteStationData *)pdata->sped, value, dur);
			break;

		case STN_TYPE_GPIO:
			switch_gpiostation((GPIOStationData *)pdata->sped, value);
			break;

		case STN_TYPE_HTTP:
			switch_httpstation((HTTPStationData *)pdata->sped, value);
			break;
		}
	}
}

/** Set station bit
 * This function sets/resets the corresponding station bit variable
 * You have to call apply_all_station_bits next to apply the bits
 * (which results in physical actions of opening/closing valves).
 */
byte OpenSprinkler::set_station_bit(byte sid, byte value, uint16_t dur) {
	byte *data = station_bits+(sid>>3);  // pointer to the station byte
	byte mask = (byte)1<<(sid&0x07); // mask
	if (value) {
		if((*data)&mask) return 0;  // if bit is already set, return no change
		else {
			(*data) = (*data) | mask;
			engage_booster = true; // if bit is changing from 0 to 1, set engage_booster
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

/** Clear all station bits */
void OpenSprinkler::clear_all_station_bits() {
	byte sid;
	for(sid=0;sid<=MAX_NUM_STATIONS;sid++) {
		set_station_bit(sid, 0);
	}
}

#if !defined(ARDUINO)
int rf_gpio_fd = -1;
#endif

/** Transmit one RF signal bit */
void transmit_rfbit(ulong lenH, ulong lenL) {
#if defined(ARDUINO)
	#if defined(ESP8266) || defined(ESP32)
		digitalWrite(PIN_RFTX, 1);
		delayMicroseconds(lenH);
		digitalWrite(PIN_RFTX, 0);
		delayMicroseconds(lenL);
	#else
		PORT_RF |= (1<<PINX_RF);
		delayMicroseconds(lenH);
		PORT_RF &=~(1<<PINX_RF);
		delayMicroseconds(lenL);
	#endif
#else
	gpio_write(rf_gpio_fd, 1);
	delayMicrosecondsHard(lenH);
	gpio_write(rf_gpio_fd, 0);
	delayMicrosecondsHard(lenL);
#endif
}

/** Transmit RF signal */
void send_rfsignal(ulong code, ulong len) {
	ulong len3 = len * 3;
	ulong len31 = len * 31;
	for(byte n=0;n<15;n++) {
		int i=23;
		// send code
		while(i>=0) {
			if ((code>>i) & 1) {
				transmit_rfbit(len3, len);
			} else {
				transmit_rfbit(len, len3);
			}
			i--;
		};
		// send sync
		transmit_rfbit(len, len31);
	}
}

/** Switch RF station
 * This function takes a RF code,
 * parses it into signals and timing,
 * and sends it out through RF transmitter.
 */
void OpenSprinkler::switch_rfstation(RFStationData *data, bool turnon) {
	ulong on, off;
	uint16_t length = parse_rfstation_code(data, &on, &off);

	if(PIN_RFTX == 255) return; // ignore RF station if RF pin disabled

#if defined(ARDUINO)
	#if defined(ESP8266) || defined(ESP32)
	rfswitch.enableTransmit(PIN_RFTX);
	rfswitch.setProtocol(1);
	rfswitch.setPulseLength(length);
	rfswitch.send(turnon ? on : off, 24);
	#else
	send_rfsignal(turnon ? on : off, length);
	#endif
#else
	// pre-open gpio file to minimize overhead
	rf_gpio_fd = gpio_fd_open(PIN_RFTX);
	send_rfsignal(turnon ? on : off, length);
	gpio_fd_close(rf_gpio_fd);
	rf_gpio_fd = -1;
#endif

}

/** Switch GPIO station
 * Special data for GPIO Station is three bytes of ascii decimal (not hex)
 * First two bytes are zero padded GPIO pin number.
 * Third byte is either 0 or 1 for active low (GND) or high (+5V) relays
 */
void OpenSprinkler::switch_gpiostation(GPIOStationData *data, bool turnon) {
	byte gpio = (data->pin[0] - '0') * 10 + (data->pin[1] - '0');
	byte activeState = data->active - '0';

	pinMode(gpio, OUTPUT);
	if (turnon)
		digitalWrite(gpio, activeState);
	else
		digitalWrite(gpio, 1-activeState);
}

/** Callback function for switching remote station */
void remote_http_callback(char* buffer) {
/*
	DEBUG_PRINTLN(buffer);
*/
}

int8_t OpenSprinkler::send_http_request(const char* server, uint16_t port, char* p, void(*callback)(char*), uint16_t timeout) {

#if defined(ARDUINO)

	Client *client;
	#if defined(ESP8266) || defined(ESP32)
		WiFiClient wifiClient;
		client = &wifiClient;
	#else
		EthernetClient etherClient;
		client = &etherClient;
	#endif

	#define HTTP_CONNECT_NTRIES 3
	byte tries = 0;
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
		return HTTP_RQT_CONNECT_ERR;
	}
#else

	EthernetClient etherClient;
	EthernetClient *client = &etherClient;
	struct hostent *host;
	DEBUG_PRINT(server);
	DEBUG_PRINT(":");
	DEBUG_PRINTLN(port);
	host = gethostbyname(server);
	if (!host) { return HTTP_RQT_CONNECT_ERR; }
	if(!client->connect((uint8_t*)host->h_addr, port)) {
		DEBUG_PRINT(F("failed."));
		client->stop();
		return HTTP_RQT_CONNECT_ERR;
	}

#endif

	uint16_t len = strlen(p);
	if(len > ETHER_BUFFER_SIZE) len = ETHER_BUFFER_SIZE;
	if(client->connected()) {
		client->write((uint8_t *)p, len);
	} else {
		DEBUG_PRINTLN(F("clint no longer connected"));
	}
	memset(ether_buffer, 0, ETHER_BUFFER_SIZE);
	uint32_t stoptime = millis()+timeout;

	int pos = 0;
#if defined(ARDUINO)
	// with ESP8266 core 3.0.2, client->connected() is not always true even if there is more data
	// so this loop is going to take longer than it should be
	// todo: can consider using HTTPClient for ESP8266
	while(true) {
		int nbytes = client->available();
		if(nbytes>0) {
			if(pos+nbytes>ETHER_BUFFER_SIZE) nbytes=ETHER_BUFFER_SIZE-pos; // cannot read more than buffer size
			client->read((uint8_t*)ether_buffer+pos, nbytes);
			pos+=nbytes;
		}
		if(millis()>stoptime) {
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
	while(client->connected()) {
		int len=client->read((uint8_t *)ether_buffer+pos, ETHER_BUFFER_SIZE);
		if (len<=0) continue;
		pos+=len;
		if(millis()>stoptime) {
			DEBUG_PRINTLN(F("host timeout occured"));
			//return HTTP_RQT_TIMEOUT; // instead of returning with timeout, we'll work with data received so far
			break;
		}
	}
#endif
	ether_buffer[pos]=0; // properly end buffer with 0
	client->stop();
	if(strlen(ether_buffer)==0) return HTTP_RQT_EMPTY_RETURN;
	if(callback) callback(ether_buffer);
	return HTTP_RQT_SUCCESS;
}

int8_t OpenSprinkler::send_http_request(uint32_t ip4, uint16_t port, char* p, void(*callback)(char*), uint16_t timeout) {
	char server[20];
	byte ip[4];
	ip[0] = ip4>>24;
	ip[1] = (ip4>>16)&0xff;
	ip[2] = (ip4>>8)&0xff;
	ip[3] = ip4&0xff;
	sprintf(server, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
	return send_http_request(server, port, p, callback, timeout);
}

int8_t OpenSprinkler::send_http_request(char* server_with_port, char* p, void(*callback)(char*), uint16_t timeout) {
	char * server = strtok(server_with_port, ":");
	char * port = strtok(NULL, ":");
	return send_http_request(server, (port==NULL)?80:atoi(port), p, callback, timeout);
}

/** Switch remote station
 * This function takes a remote station code,
 * parses it into remote IP, port, station index,
 * and makes a HTTP GET request.
 * The remote controller is assumed to have the same
 * password as the main controller
 */
void OpenSprinkler::switch_remotestation(RemoteStationData *data, bool turnon, uint16_t dur) {
	RemoteStationData copy;
	memcpy((char*)&copy, (char*)data, sizeof(RemoteStationData));

	uint32_t ip4 = hex2ulong(copy.ip, sizeof(copy.ip));
	uint16_t port = (uint16_t)hex2ulong(copy.port, sizeof(copy.port));

	byte ip[4];
	ip[0] = ip4>>24;
	ip[1] = (ip4>>16)&0xff;
	ip[2] = (ip4>>8)&0xff;
	ip[3] = ip4&0xff;

	// use tmp_buffer starting at a later location
	// because remote station data is loaded at the beginning
	char *p = tmp_buffer;
	BufferFiller bf = p;
	// if turning on the zone and duration is defined, give duration as the timer value
	// otherwise:
	//   if autorefresh is defined, we give a fixed duration each time, and auto refresh will renew it periodically
	//   if no auto refresh, we will give the maximum allowed duration, and station will be turned off when off command is sent
	uint16_t timer = 0;
	if(turnon) {
		if(dur>0) {
			timer = dur;
		} else {
			timer = iopts[IOPT_SPE_AUTO_REFRESH]?4*MAX_NUM_STATIONS:64800;
		}
	}
	bf.emit_p(PSTR("GET /cm?pw=$O&sid=$D&en=$D&t=$D"),
						SOPT_PASSWORD,
						(int)hex2ulong(copy.sid, sizeof(copy.sid)),
						turnon, timer);
	bf.emit_p(PSTR(" HTTP/1.0\r\nHOST: $D.$D.$D.$D\r\n\r\n"),
						ip[0],ip[1],ip[2],ip[3]);

	char server[20];
	sprintf(server, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
	send_http_request(server, port, p, remote_http_callback);
}

/** Switch http station
 * This function takes an http station code,
 * parses it into a server name and two HTTP GET requests.
 */
void OpenSprinkler::switch_httpstation(HTTPStationData *data, bool turnon) {

	HTTPStationData copy;
	// make a copy of the HTTP station data and work with it
	memcpy((char*)&copy, (char*)data, sizeof(HTTPStationData));
	char * server = strtok((char *)copy.data, ",");
	char * port = strtok(NULL, ",");
	char * on_cmd = strtok(NULL, ",");
	char * off_cmd = strtok(NULL, ",");
	char * cmd = turnon ? on_cmd : off_cmd;

	char *p = tmp_buffer;
	BufferFiller bf = p;

	if(cmd==NULL || server==NULL) return; // proceed only if cmd and server are valid

	bf.emit_p(PSTR("GET /$S HTTP/1.0\r\nHOST: $S\r\n\r\n"), cmd, server);

	send_http_request(server, atoi(port), p, remote_http_callback);
}

/** Prepare factory reset */
void OpenSprinkler::pre_factory_reset() {
	// for ESP8266: wipe out flash
	#if defined(ESP8266) || defined(ESP32)
	lcd_print_line_clear_pgm(PSTR("Wiping flash.."), 0);
	lcd_print_line_clear_pgm(PSTR("Please Wait..."), 1);
		#if defined(ESP8266)
	LittleFS.format();
		#elif defined(ESP32)
	LittleFS.format();
		#endif
	#else
	// remove 'done' file as an indicator for reset
	// todo os2.3 and ospi: delete log files and/or wipe SD card
	remove_file(DONE_FILENAME);
	#endif
}

/** Factory reset */
void OpenSprinkler::factory_reset() {
#if defined(ARDUINO)
	lcd_print_line_clear_pgm(PSTR("Factory reset"), 0);
	lcd_print_line_clear_pgm(PSTR("Please Wait..."), 1);
	DEBUG_PRINTLN(F("Factory reset"));
#else
	DEBUG_PRINT("factory reset...");
#endif

	#if defined(ESP32)
	// 0. create the directory
	//LittleFS.mkdir(IOPTS_FILENAME);
	#endif

	// 1. reset integer options (by saving default values)
	iopts_save();
	// reset string options by first wiping the file clean then write default values
	memset(tmp_buffer, 0, MAX_SOPTS_SIZE);
	for(int i=0; i<NUM_SOPTS; i++) {
		file_write_block(SOPTS_FILENAME, tmp_buffer, (ulong)MAX_SOPTS_SIZE*i, MAX_SOPTS_SIZE);
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

	// 5. write 'done' file
	file_write_byte(DONE_FILENAME, 0, 1);

	#if defined(ESP32)
		// workaround for OTF 
		start_network_ap(wifi_ssid.c_str(),wifi_pass.c_str());
	#endif
}

#define str(s) #s
#define xstr(s) str(s)

/** Parse OTC configuration */
#if defined(ESP8266) || defined(ESP32)
void OpenSprinkler::parse_otc_config() {
	char server[MAX_SOPTS_SIZE+1] = {0};
	char token[MAX_SOPTS_SIZE+1] = {0};
	int port = DEFAULT_OTC_PORT;
	int en = 0;

	char *config = tmp_buffer;
	sopt_load(SOPT_OTC_OPTS, config);
	if (*config != 0) {
		sscanf(config, "\"en\":%d,\"token\":\"%" xstr(MAX_SOPTS_SIZE) "[^\"]\",\"server\":\"%" xstr(MAX_SOPTS_SIZE) "[^\"]\",\"port\":%d",
			&en, token, server, &port);
		token[MAX_SOPTS_SIZE] = 0;
		server[MAX_SOPTS_SIZE] = 0;
	}
	otc.en = en;
	otc.token = String(token);
	otc.server = String(server);
	otc.port = 80;
}
#endif

/** Setup function for options */
void OpenSprinkler::options_setup() {

	#if defined(ESP8266)
	LittleFS.mkdir(IOPTS_FILENAME);
	#endif
	//LittleFS.mkdir(DONE_FILENAME);
	if ( !file_exists(IOPTS_FILENAME) ) {
		DEBUG_PRINTLN("IOPTS missing");
	}
	if ( file_read_byte(IOPTS_FILENAME, IOPT_FW_VERSION)<220 ) {
		DEBUG_PRINTLN("Wrong version");
		DEBUG_PRINTLN(file_read_byte(IOPTS_FILENAME, IOPT_FW_VERSION));
	}
	if ( !file_exists(DONE_FILENAME) ) {
		DEBUG_PRINTLN("DONE missing");
	}
	// Check reset conditions:
	if ( ( !file_exists(IOPTS_FILENAME) || file_read_byte(IOPTS_FILENAME, IOPT_FW_VERSION)<220 ) ||  // fw version is invalid (<219)
			!file_exists(DONE_FILENAME) ) {  // done file doesn't exist

		factory_reset();

	} else	{

		iopts_load();
		nvdata_load();
		last_reboot_cause = nvdata.reboot_cause;
		nvdata.reboot_cause = REBOOT_CAUSE_POWERON;
		nvdata_save();
		#if defined(ESP8266) || defined(ESP32)
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
		parse_otc_config();
		#endif
		attribs_load();
	}

	DEBUG_PRINTLN(F("Checking button state"));

#if defined(ARDUINO)	// handle AVR buttons
	byte button = button_read(BUTTON_WAIT_NONE);

	#ifndef USE_ROTARY_ENCODER
	switch(button & BUTTON_MASK) {
	#ifndef BOOT_MENU_V2
	case BUTTON_1:
		// if BUTTON_1 is pressed during startup, go to 'reset all options'
		ui_set_options(IOPT_RESET);
		if (iopts[IOPT_RESET]) {
			pre_factory_reset();
			reboot_dev(REBOOT_CAUSE_RESET);
		}
		break;
	#endif
	case BUTTON_2:
	#if defined(ESP8266) || defined(ESP32)
		#ifndef BOOT_MENU_V2
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
		#else
			DEBUG_PRINTLN("Showing boot menu v2");
			ui_boot_menu(bootMenu,BOOT_MENU_ITEMS);
			button = 0; // will continue booting
		#endif
	#endif

		break;
	#ifndef BOOT_MENU_V2
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
	#endif
	}
	#else
	// rotary encoder test
	// 4 pages: setup options, device password reset, factory reset, internal test
	// "on hold" will activate a function, normal click is "enter" or "hold for yes"
	byte button = button_read(BUTTON_WAIT_NONE);
	if ( (button & BUTTON_MASK) == BUTTON_2 ) {
		button = 0;
		ui_boot_menu(bootMenu, BOOT_MENU_ITEMS);	
	}
	
	/*
	
	if (iopts[IOPT_RESET]) {
		pre_factory_reset();
		reboot_dev(REBOOT_CAUSE_RESET);
	}
	*/
	#endif
	// turn on LCD backlight and contrast
	lcd_set_brightness();
	lcd_set_contrast();

	if (!button) {
		// flash screen
		lcd.setCursor(1, 0);
		lcd_print_line_clear_pgm(PSTR("OpenSprinkler"),0);
		#if defined(ESP32)
		lcd.setCursor(0, 2);
		lcd_print_line_clear_pgm(PSTR("ESP32 ver - V1pr"),2);
		#endif
		lcd.setCursor((hw_type==HW_TYPE_LATCH)?2:4, 1);
		lcd_print_pgm(PSTR("v"));
		byte hwv = iopts[IOPT_HW_VERSION];
		lcd.print((char)('0'+(hwv/10)));
		lcd.print('.');
		#if defined(ESP8266) || defined(ESP32)
		lcd.print(hw_rev);
		#else
		lcd.print((char)('0'+(hwv%10)));
		#endif
		switch(hw_type) {
		case HW_TYPE_DC:
			lcd_print_pgm(PSTR(" DC"));
			break;
		case HW_TYPE_LATCH:
			lcd_print_pgm(PSTR(" LATCH"));
			break;
		default:
			lcd_print_pgm(PSTR(" AC"));
		}
		delay(1500); // sleep, and print out FW version
		#if defined(ARDUINO)
		lcd.setCursor(2, 1);
		lcd_print_pgm(PSTR("FW "));
		lcd.print((char)('0'+(OS_FW_VERSION/100)));
		lcd.print('.');
		lcd.print((char)('0'+((OS_FW_VERSION/10)%10)));
		lcd.print('.');
		lcd.print((char)('0'+(OS_FW_VERSION%10)));
		lcd.print('(');
		lcd.print(OS_FW_MINOR);
		lcd.print(')');
		delay(1000);
		lcd.clear(0,1);
		lcd.clear(0,2);
		#endif
	}
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

void load_wt_monthly(char* wto);

/** Load integer options from file */
void OpenSprinkler::iopts_load() {
	file_read_block(IOPTS_FILENAME, iopts, 0, NUM_IOPTS);
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
	sopt_load(SOPT_WEATHER_OPTS, tmp_buffer);
	if(iopts[IOPT_USE_WEATHER]==WEATHER_METHOD_MONTHLY) {
		load_wt_monthly(tmp_buffer);
	}
}

void OpenSprinkler::populate_master() {
	masters[MASTER_1][MASOPT_SID] = iopts[IOPT_MASTER_STATION];
	masters[MASTER_1][MASOPT_ON_ADJ] = iopts[IOPT_MASTER_ON_ADJ];
	masters[MASTER_1][MASOPT_OFF_ADJ] = iopts[IOPT_MASTER_OFF_ADJ];

	masters[MASTER_2][MASOPT_SID] = iopts[IOPT_MASTER_STATION_2];
	masters[MASTER_2][MASOPT_ON_ADJ] = iopts[IOPT_MASTER_ON_ADJ_2];
	masters[MASTER_2][MASOPT_OFF_ADJ] = iopts[IOPT_MASTER_OFF_ADJ_2];
}

/** Save integer options to file */
void OpenSprinkler::iopts_save() {
	file_write_block(IOPTS_FILENAME, iopts, 0, NUM_IOPTS);
	nboards = iopts[IOPT_EXT_BOARDS]+1;
	nstations = nboards * 8;
	status.enabled = iopts[IOPT_DEVICE_ENABLE];
}

/** Load a string option from file */
void OpenSprinkler::sopt_load(byte oid, char *buf) {
	file_read_block(SOPTS_FILENAME, buf, MAX_SOPTS_SIZE*oid, MAX_SOPTS_SIZE);
	buf[MAX_SOPTS_SIZE]=0;  // ensure the string ends properly
}

/** Load a string option from file, return String */
String OpenSprinkler::sopt_load(byte oid) {
	sopt_load(oid, tmp_buffer);
	String str = tmp_buffer;
	return str;
}

/** Save a string option to file */
bool OpenSprinkler::sopt_save(byte oid, const char *buf) {
	// smart save: if value hasn't changed, don't write
	if(file_cmp_block(SOPTS_FILENAME, buf, (ulong)MAX_SOPTS_SIZE*oid)==0) return false;
	int len = strlen(buf);
	if(len>=MAX_SOPTS_SIZE) {
		file_write_block(SOPTS_FILENAME, buf, (ulong)MAX_SOPTS_SIZE*oid, MAX_SOPTS_SIZE);
	} else {
		// copy ending 0 too
		file_write_block(SOPTS_FILENAME, buf, (ulong)MAX_SOPTS_SIZE*oid, len+1);
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

/** LCD and button functions */
#if defined(ARDUINO)		// AVR LCD and button functions
/** print a program memory string */
#if defined(ESP8266) || defined(ESP32)
void OpenSprinkler::lcd_print_pgm(PGM_P str) {
#else
void OpenSprinkler::lcd_print_pgm(PGM_P PROGMEM str) {
#endif
	uint8_t c;
	while((c=pgm_read_byte(str++))!= '\0') {
		lcd.print((char)c);
	}
}

/** print a program memory string to a given line with clearing */
#if defined(ESP8266) || defined(ESP32)
void OpenSprinkler::lcd_print_line_clear_pgm(PGM_P str, byte line) {
#else
void OpenSprinkler::lcd_print_line_clear_pgm(PGM_P PROGMEM str, byte line) {
#endif
	lcd.setCursor(0, line);
	uint8_t c;
	int8_t cnt = 0;
	while((c=pgm_read_byte(str++))!= '\0') {
		lcd.print((char)c);
		cnt++;
	}
	for(; (16-cnt) >= 0; cnt ++) lcd_print_pgm(PSTR(" "));
}

void OpenSprinkler::lcd_print_2digit(int v)
{
	lcd.print((int)(v/10));
	lcd.print((int)(v%10));
}

/** print time to a given line */
void OpenSprinkler::lcd_print_time(time_t t)
{
#if defined(ESP8266) || defined(ESP32)
	lcd.setAutoDisplay(false);
#endif
	lcd.setCursor(0, 0);
	lcd_print_2digit(hour(t));
	lcd_print_pgm(PSTR(":"));
	lcd_print_2digit(minute(t));
	lcd_print_pgm(PSTR("  "));
	// each weekday string has 3 characters + ending 0
	lcd_print_pgm(days_str+4*weekday_today());
	lcd_print_pgm(PSTR(" "));
	lcd_print_2digit(month(t));
	lcd_print_pgm(PSTR("-"));
	lcd_print_2digit(day(t));
#if defined(ESP8266) || defined(ESP32)
	lcd.display();
	lcd.setAutoDisplay(true);
#endif
}

/** print ip address */
void OpenSprinkler::lcd_print_ip(const byte *ip, byte endian) {
#if defined(ESP8266) || defined(ESP32)
	lcd.clear(0, 1);
	// lcd.clear(0, 2);
#else
	lcd.clear();
#endif
	lcd.setCursor(0, 0);
	for (byte i=0; i<4; i++) {
		lcd.print(endian ? (int)ip[3-i] : (int)ip[i]);
		if(i<3) lcd_print_pgm(PSTR("."));
	}
}

/** print mac address */
void OpenSprinkler::lcd_print_mac(const byte *mac) {
	lcd.setCursor(0, 0);
	for(byte i=0; i<6; i++) {
		if(i)  lcd_print_pgm(PSTR("-"));
		lcd.print((mac[i]>>4), HEX);
		lcd.print((mac[i]&0x0F), HEX);
		if(i==4) lcd.setCursor(0, 1);
	}
	if(useEth) {
		lcd_print_pgm(PSTR(" (Ether MAC)"));
	} else {
		lcd_print_pgm(PSTR(" (WiFi MAC)"));
	}
}

/** print station bits */
void OpenSprinkler::lcd_print_screen(char c) {
#if defined(ESP8266) || defined(ESP32)
	lcd.setAutoDisplay(false); // reduce screen drawing time by turning off display() when drawing individual characters
#endif
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
		byte bitvalue = station_bits[status.display_board];
		for (byte s=0; s<8; s++) {
			byte sid = (byte)status.display_board<<3;
			sid += (s+1);
			if (sid == iopts[IOPT_MASTER_STATION]) {
				lcd.print((bitvalue&1) ? c : 'M'); // print master station
			} else if (sid == iopts[IOPT_MASTER_STATION_2]) {
				lcd.print((bitvalue&1) ? c : 'N'); // print master2 station
			} else {
				lcd.print((bitvalue&1) ? c : '_');
			}
			bitvalue >>= 1;
		}
	}
	//lcd.print(F("    "));

	lcd.setCursor(LCD_CURSOR_REMOTEXT, 1);
	lcd.write(iopts[IOPT_REMOTE_EXT_MODE]?ICON_REMOTEXT:' ');

	lcd.setCursor(LCD_CURSOR_RAINDELAY, 1);
	lcd.write((status.rain_delayed || status.pause_state)?ICON_RAINDELAY:' ');

	// write sensor 1 icon
	lcd.setCursor(LCD_CURSOR_SENSOR1, 1);
	switch(iopts[IOPT_SENSOR1_TYPE]) {
		case SENSOR_TYPE_RAIN:
			lcd.write(status.sensor1_active?ICON_RAIN:(status.sensor1?'R':'r'));
			break;
		case SENSOR_TYPE_SOIL:
			lcd.write(status.sensor1_active?ICON_SOIL:(status.sensor1?'S':'s'));
			break;
		case SENSOR_TYPE_FLOW:
			lcd.write(flowcount_rt>0?'F':'f');
			break;
		case SENSOR_TYPE_PSWITCH:
			lcd.write(status.sensor1?'P':'p');
			break;
		default:
			lcd.write(' ');
			break;
	}

	// write sensor 2 icon
	lcd.setCursor(LCD_CURSOR_SENSOR2, 1);
	switch(iopts[IOPT_SENSOR2_TYPE]) {
		case SENSOR_TYPE_RAIN:
			lcd.write(status.sensor2_active?ICON_RAIN:(status.sensor2?'R':'r'));
			break;
		case SENSOR_TYPE_SOIL:
			lcd.write(status.sensor2_active?ICON_SOIL:(status.sensor2?'S':'s'));
			break;
			// sensor2 cannot be flow sensor
		/*case SENSOR_TYPE_FLOW:
			lcd.write('F');
			break;*/
		case SENSOR_TYPE_PSWITCH:
			lcd.write(status.sensor2?'Q':'q');
			break;
		default:
			lcd.write(' ');
			break;
	}

	lcd.setCursor(LCD_CURSOR_NETWORK, 1);
#if defined(ESP8266)
	if(useEth) {
		lcd.write(eth.connected()?ICON_ETHER_CONNECTED:ICON_ETHER_DISCONNECTED);	// todo: need to detect ether status
	}
	else
		lcd.write(WiFi.status()==WL_CONNECTED?ICON_WIFI_CONNECTED:ICON_WIFI_DISCONNECTED);
#elif defined(ESP32)
	// only wifi for ESP32
	lcd.write(WiFi.status()==WL_CONNECTED?ICON_WIFI_CONNECTED:ICON_WIFI_DISCONNECTED);
#else
	lcd.write(status.network_fails>2?ICON_ETHER_DISCONNECTED:ICON_ETHER_CONNECTED);  // if network failure detection is more than 2, display disconnect icon
#endif

#if defined(ESP8266) || defined(ESP32)

	if(useEth || (get_wifi_mode()==WIFI_MODE_STA && WiFi.status()==WL_CONNECTED && WiFi.localIP())) {
		lcd.setCursor(0, -1);
		if(status.rain_delayed) {
			lcd.print(F("<Rain Delay On> "));
		} else if(status.pause_state) {
			lcd.print(F("<Program Paused>"));
		} else if(status.program_busy) {
			lcd.print(F("<Running Zones> "));
		} else {
			lcd.print(F(" (System Idle)  "));
		}

		lcd.setCursor(2, 2);
		if(status.program_busy && !status.pause_state) {
			//lcd.print(F("Curr: "));
			#if ! defined(ESP32) || ( defined(ESP32) && PIN_CURR_SENSE != 255 )
			lcd.print(read_current());
			lcd.print(F(" mA      "));
			#endif
		} else {
			lcd.clear(2, 2);
		}
	}
#endif
#if defined(ESP8266) || defined(ESP32)
	lcd.display();
	lcd.setAutoDisplay(true);
#endif
}

/** print a version number */
void OpenSprinkler::lcd_print_version(byte v) {
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
	strncpy_P0(tmp_buffer, iopt_prompts+16*i, 16);
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
	case IOPT_MASTER_OFF_ADJ:
	case IOPT_MASTER_OFF_ADJ_2:
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
		#if defined(ARDUINO)
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
	default:
		// if this is a boolean option
		if (pgm_read_byte(iopt_max+i)==1)
			lcd_print_pgm(iopts[i] ? PSTR("Yes") : PSTR("No"));
		else
			lcd.print((int)iopts[i]);
		break;
	}
	if (i==IOPT_WATER_PERCENTAGE)  lcd_print_pgm(PSTR("%"));
	else if (i==IOPT_MASTER_ON_ADJ || i==IOPT_MASTER_OFF_ADJ || i==IOPT_MASTER_ON_ADJ_2 || i==IOPT_MASTER_OFF_ADJ_2)
		lcd_print_pgm(PSTR(" sec"));

}

/** Button functions */
/** wait for button */
/* this is same for HW buttons and rotary encoder: only center button will'be used here */
byte OpenSprinkler::button_read_busy(byte pin_butt, byte waitmode, byte butt, byte is_holding) {

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

#if defined(USE_ROTARY_ENCODER)

// interrupt driven encoder read
void OpenSprinkler::handleRotaryEncoderRotate() {
	static int lastEncoded = 0;
  	static int encoderValue = 0;
	
	int MSB = digitalRead(ENCODER_PIN_A);
  	int LSB = digitalRead(ENCODER_PIN_B);

  	 int encoded = (MSB << 1) | LSB;
	int sum = (lastEncoded << 2) | encoded;
	int increment = encoderTable[sum & 0x0F];

	encoderValue += increment;
	encoderPos = encoderValue;
	lastEncoded = encoded;
}

/** read button and returns button value 'OR'ed with flag bits */
byte OpenSprinkler::button_read(byte waitmode)
{
	static byte old = BUTTON_NONE;
	byte curr = BUTTON_NONE;
	byte is_holding = (old&BUTTON_FLAG_HOLD);

	delay(BUTTON_DELAY_MS);

	if (digitalReadExt(PIN_BUTTON_2) == 0) {
		curr = button_read_busy(PIN_BUTTON_2, waitmode, BUTTON_2, is_holding);
	}

	// set flags in return value
	byte ret = curr;
	if (!(old&BUTTON_MASK) && (curr&BUTTON_MASK))
		ret |= BUTTON_FLAG_DOWN;
	if ((old&BUTTON_MASK) && !(curr&BUTTON_MASK))
		ret |= BUTTON_FLAG_UP;

	old = curr;

	return ret;
}
#else

/** read button and returns button value 'OR'ed with flag bits */
byte OpenSprinkler::button_read(byte waitmode)
{
	static byte old = BUTTON_NONE;
	byte curr = BUTTON_NONE;
	byte is_holding = (old&BUTTON_FLAG_HOLD);

	delay(BUTTON_DELAY_MS);

	if (digitalReadExt(PIN_BUTTON_1) == 0) {
		curr = button_read_busy(PIN_BUTTON_1, waitmode, BUTTON_1, is_holding);
	} else if (digitalReadExt(PIN_BUTTON_2) == 0) {
		curr = button_read_busy(PIN_BUTTON_2, waitmode, BUTTON_2, is_holding);
	} else if (digitalReadExt(PIN_BUTTON_3) == 0) {
		curr = button_read_busy(PIN_BUTTON_3, waitmode, BUTTON_3, is_holding);
	}

	// set flags in return value
	byte ret = curr;
	if (!(old&BUTTON_MASK) && (curr&BUTTON_MASK))
		ret |= BUTTON_FLAG_DOWN;
	if ((old&BUTTON_MASK) && !(curr&BUTTON_MASK))
		ret |= BUTTON_FLAG_UP;

	old = curr;

	return ret;
}
#endif // if defined(USE_ROTARY_ENCODER)

#ifndef BOOT_MENU_V2
/** user interface for setting options during startup */
void OpenSprinkler::ui_set_options(int oid)
{
	boolean finished = false;
	byte button;
	int i=oid;

	while(!finished) {
		button = button_read(BUTTON_WAIT_HOLD);

		switch (button & BUTTON_MASK) {
		case BUTTON_1:
			if (i==IOPT_FW_VERSION || i==IOPT_HW_VERSION || i==IOPT_FW_MINOR ||
					i==IOPT_HTTPPORT_0 || i==IOPT_HTTPPORT_1 ||
					i==IOPT_PULSE_RATE_0 || i==IOPT_PULSE_RATE_1 ||
					i==IOPT_WIFI_MODE) break; // ignore non-editable options
			if (pgm_read_byte(iopt_max+i) != iopts[i]) iopts[i] ++;
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
				else if (i==IOPT_MASTER_STATION && iopts[i]==0) i+=3; // if not using master station, skip master on/off adjust including two retired options
				else if (i==IOPT_MASTER_STATION_2&& iopts[i]==0) i+=3; // if not using master2, skip master2 on/off adjust
				else	{
					i = (i+1) % NUM_IOPTS;
				}
				if(i==IOPT_SEQUENTIAL_RETIRED) i++;
				if(i==IOPT_URS_RETIRED) i++;
				if(i==IOPT_RSO_RETIRED) i++;
				if (hw_type==HW_TYPE_AC && i==IOPT_BOOST_TIME) i++;	// skip boost time for non-DC controller
				#if defined(ESP8266) || defined(ESP32)
				else if (lcd.type()==LCD_I2C && i==IOPT_LCD_CONTRAST) i+=3;
				#else
				else if (lcd.type()==LCD_I2C && i==IOPT_LCD_CONTRAST) i+=2;
				#endif
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

#else
/** user interface for setting options during startup, using the new, paged menu */
void OpenSprinkler::ui_boot_menu(MenuItem* menuItems, int menuSize)
{
	boolean finished = false;
	
	byte button;
	int encoderDelta = 0;
	int prevMenuItem = -1;
	int currentMenuItem = 0;
	void (*menu_f_p)();

	/*
	if ( currentMainmenu == nullptr ) {
		currentMenumenu = menuItems;
	}
	*/

	// rotary encoder is like 3 buttons: left: B1, button: B2, right: B3
	while(!finished) {
		#if defined(USE_ROTARY_ENODER)
		encoderDelta = encoderPos - lastEncoderPos;
		if ( encoderDelta != 0 ) {
			if ( encoderDelta < 0 ) {
				// CC rotate - BUTTON_1
				button |= BUTTON_1;
				button |= BUTTON_FLAG_UP;
			} else {
				// clockwise rotate - BUTTON_3
				button |= BUTTON_3;
				button |= BUTTON_FLAG_UP;
			}

		} else {
			button = button_read(BUTTON_WAIT_HOLD);
		}
		#else
		//DEBUG_PRINTLN("Button read");
		button = button_read(BUTTON_WAIT_HOLD);
		//DEBUG_PRINTLN(button);
		#endif

		switch (button & BUTTON_MASK) {
		case BUTTON_1:
			DEBUG_PRINTLN("Page-");
			// step left
			if ( currentMenuItem != 0 ) {
				currentMenuItem --;
			}
			break;

		case BUTTON_2:
			DEBUG_PRINTLN("B2");
			// enter
			//if (!(button & BUTTON_FLAG_DOWN)) {
			//	DEBUG_PRINTLN("Flag down");
			//	break;
			//}
			if (button & BUTTON_FLAG_HOLD) {
				DEBUG_PRINTLN("Long hold for item");
				if ( currentMenuItem == menuSize ) {
					// exit with long press
					DEBUG_PRINTLN("Last menuitem, exiting");
					finished = true;	
				} else {
					if ( menuItems[currentMenuItem].fn_name != nullptr ) {
						DEBUG_PRINTLN("Calling defined function");
						menu_f_p = menuItems[currentMenuItem].fn_name;
						menu_f_p();
					} else {
						DEBUG_PRINTLN("Nothing set to do now");
					}
					// long press, save options
					//	iopts_save();
					//finished = true; // will exit only on exit or if reboot needed
				}
			} else if ( button & BUTTON_FLAG_UP ) {
				DEBUG_PRINTLN("Should be short press");
				if ( currentMenuItem == menuSize  ) {
					DEBUG_PRINTLN("Last menuitem, exiting");
					// exit
					finished = true;
				} else {
					// submenu enter, if any
					if ( menuItems[currentMenuItem].subMenu != nullptr ) {
						// enter submenu 
						ui_boot_menu(menuItems[currentMenuItem].subMenu, menuItems[currentMenuItem].subMenuSize);
					}
				}
			} else {
				DEBUG_PRINTLN("Unknown state");
			}
			break;

		case BUTTON_3:
			DEBUG_PRINTLN("Page+");
			if ( currentMenuItem < menuSize ) currentMenuItem++;
			break;
		case BUTTON_NONE:
			//DEBUG_PRINTLN("No button");
			break;
		}

		// prevent flashing the screen
		if ( currentMenuItem != prevMenuItem ) {
			DEBUG_PRINT(F("Printing menu item "));
			DEBUG_PRINTLN(currentMenuItem);
			lcd_print_menu(menuItems[currentMenuItem].name);
		}
		prevMenuItem = currentMenuItem;
		//DEBUG_PRINTLN(button&BUTTON_FLAG_DOWN);
		//DEBUG_PRINTLN(button&BUTTON_FLAG_UP);
		//DEBUG_PRINTLN(button&BUTTON_FLAG_HOLD);
		//delay(50); // set to 350 for testing from 50
	}

	// if in mainmenu, than we should reboot!
	//if ( currentMainmenu == menuItems &&  ) {
	if ( menuItems[currentMenuItem].needReboot ) {
		reboot_dev(REBOOT_CAUSE_RESET);
	}

	lcd.noBlink();
}

void OpenSprinkler::lcd_print_menu(const char* itemName) {

	lcd.clear();
  	lcd.setCursor(0, 0);
  	lcd.println(itemName);
  //lcd.display();

}

#endif

/** Set LCD contrast (using PWM) */
void OpenSprinkler::lcd_set_contrast() {
#ifdef PIN_LCD_CONTRAST
	// set contrast is only valid for standard LCD
	if (lcd.type()==LCD_STD) {
		pinMode(PIN_LCD_CONTRAST, OUTPUT);
		analogWrite(PIN_LCD_CONTRAST, iopts[IOPT_LCD_CONTRAST]);
	}
#endif
}

/** Set LCD brightness (using PWM) */
void OpenSprinkler::lcd_set_brightness(byte value) {
#if defined(PIN_LCD_BACKLIGHT)
	#if defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__)
	if (lcd.type()==LCD_I2C) {
		if (value) lcd.backlight();
		else {
			// turn off LCD backlight
			// only if dimming value is set to 0
			if(iopts[IOPT_LCD_DIMMING]==0)	lcd.noBacklight();
			else lcd.backlight();
		}
	}
	#endif
	if (lcd.type()==LCD_STD) {
		pinMode(PIN_LCD_BACKLIGHT, OUTPUT);
		if (value) {
			analogWrite(PIN_LCD_BACKLIGHT, 255-iopts[IOPT_LCD_BACKLIGHT]);
		} else {
			analogWrite(PIN_LCD_BACKLIGHT, 255-iopts[IOPT_LCD_DIMMING]);
		}
	}

#elif defined(ESP8266) || defined(ESP32)
	if (value) {lcd.displayOn();lcd.setBrightness(255); }
	else {
		if(iopts[IOPT_LCD_DIMMING]==0) lcd.displayOff();
		else { lcd.displayOn();lcd.setBrightness(iopts[IOPT_LCD_DIMMING]); }
	}
#endif
}
#endif  // end of LCD and button functions

#if defined(ESP8266) || defined(ESP32)
#include "images.h"
void OpenSprinkler::flash_screen() {
	lcd.setCursor(0, -1);
	lcd.print(F(" OpenSprinkler"));
	lcd.drawXbm(34, 24, WiFi_Logo_width, WiFi_Logo_height, (const byte*) WiFi_Logo_image);
	lcd.setCursor(0, 2);
	lcd.display();
	delay(1500);
	lcd.clear();
	lcd.display();
}

void OpenSprinkler::toggle_screen_led() {
	static byte status = 0;
	status = 1-status;
	set_screen_led(!status);
}

void OpenSprinkler::set_screen_led(byte status) {
	lcd.setColor(status ? WHITE : BLACK);
	lcd.fillCircle(122, 58, 4);
	lcd.display();
	lcd.setColor(WHITE);
}

void OpenSprinkler::reset_to_ap() {
	iopts[IOPT_WIFI_MODE] = WIFI_MODE_AP;
	iopts_save();
	reboot_dev(REBOOT_CAUSE_RSTAP);
}

void OpenSprinkler::config_ip() {
	if(iopts[IOPT_USE_DHCP] == 0) {
		byte *_ip = iopts+IOPT_STATIC_IP1;
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
	for(byte i=0;i<(MAX_NUM_BOARDS)/2;i++) {
		byte address = EXP_I2CADDR_BASE+i;
		byte type = IOEXP::detectType(address);
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
