/* OpenSprinkler Unified (AVR/RPI/BBB/LINUX) Firmware
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
#if not defined(ARDUINO)
#include <netdb.h>
#endif

#include "OpenSprinkler.h"
#include "gpio.h"

/** Declare static data members */
NVConData OpenSprinkler::nvdata;
ConStatus OpenSprinkler::status;
ConStatus OpenSprinkler::old_status;
byte OpenSprinkler::hw_type;

byte OpenSprinkler::nboards;
byte OpenSprinkler::nstations;
byte OpenSprinkler::station_bits[MAX_EXT_BOARDS+1];
#if defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__)
byte OpenSprinkler::engage_booster;
#endif

ulong OpenSprinkler::sensor_lasttime;
ulong OpenSprinkler::flowcount_log_start;
ulong OpenSprinkler::flowcount_rt;
ulong OpenSprinkler::flowcount_time_ms;
ulong OpenSprinkler::raindelay_start_time;
byte OpenSprinkler::button_timeout;
ulong OpenSprinkler::checkwt_lasttime;
ulong OpenSprinkler::checkwt_success_lasttime;
byte OpenSprinkler::weather_update_flag;

char tmp_buffer[TMP_BUFFER_SIZE+1];       // scratch buffer

#if defined(ARDUINO)
  const char wtopts_filename[] PROGMEM = WEATHER_OPTS_FILENAME;
  const char stns_filename[]   PROGMEM = STATION_ATTR_FILENAME;
  const char ifkey_filename[]  PROGMEM = IFTTT_KEY_FILENAME;
#else
  const char wtopts_filename[] = WEATHER_OPTS_FILENAME;
  const char stns_filename[]   = STATION_ATTR_FILENAME;
  const char ifkey_filename[]  = IFTTT_KEY_FILENAME;  
#endif

#if defined(ARDUINO)
  LiquidCrystal OpenSprinkler::lcd;
  #include <SdFat.h>
  extern SdFat sd;
#elif defined(OSPI)
  // todo: LCD define for OSPi
#endif

#if defined(OSPI)
  byte OpenSprinkler::pin_sr_data = PIN_SR_DATA;
#endif

/** Option json names (stored in progmem) */
// IMPORTANT: each json name is strictly 5 characters
// with 0 fillings if less
#define OP_JSON_NAME_STEPSIZE 5
#if defined(ARDUINO)
const char op_json_names[] PROGMEM =
#else
const char op_json_names[] =
#endif
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
    "reset";

/** Option promopts (stored in progmem, for LCD display) */
// Each string is strictly 16 characters
// with SPACE fillings if less
#if defined(ARDUINO)
const char op_prompts[] PROGMEM =
#else
char op_promopts[] =
#endif
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
    "Sensor type:    "
    "Normally open?  "
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
    "IFTTT Enable: "
    "Factory reset?  ";

/** Option maximum values (stored in progmem) */
#if defined(ARDUINO)
const char op_max[] PROGMEM = {
#else
const char op_max[] = {
#endif
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
  1
};

/** Option values (stored in RAM) */
byte OpenSprinkler::options[] = {
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
  0,  // sensor function (see SENSOR_TYPE macro defines)
  0,  // rain sensor type. 0: normally closed; 1: normally open.
  100,// water level (default 100%),
  1,  // device enable
  0,  // 1: ignore password; 0: use password
  0,  // device id
  150,// lcd contrast
  100,// lcd backlight
  50, // lcd dimming
  80, // boost time (only valid to DC and LATCH type)
  0,  // weather algorithm (0 means not using weather algorithm)
  50, // this and the next three bytes define the ntp server ip
  97,
  210,
  169,
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
  0   // reset
};

/** Weekday strings (stored in progmem, for LCD display) */
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
  return now()+(int32_t)3600/4*(int32_t)(options[OPTION_TIMEZONE]-48);
}

#if defined(ARDUINO)  // AVR network init functions

/** read hardware MAC */
#define MAC_CTRL_ID 0x50
bool OpenSprinkler::read_hardware_mac() {
  uint8_t ret;
  Wire.beginTransmission(MAC_CTRL_ID);
  Wire.write((uint8_t)(0x00));
  ret = Wire.endTransmission();
  if (ret)  return false;

  Wire.beginTransmission(MAC_CTRL_ID);
  Wire.write(0xFA); // The address of the register we want
  Wire.endTransmission(); // Send the data
  Wire.requestFrom(MAC_CTRL_ID, 6); // Request 6 bytes from the EEPROM
  while (!Wire.available()); // Wait for the response
  for (ret=0;ret<6;ret++) {
    tmp_buffer[ret] = Wire.read();
  }
  return true;
}

void(* resetFunc) (void) = 0; // AVR software reset function

/** Initialize network with the given mac address and http port */
byte OpenSprinkler::start_network() {

  lcd_print_line_clear_pgm(PSTR("Connecting..."), 1);

  // new from 2.2: read hardware MAC
  if(!read_hardware_mac())
  {
    // if no hardware MAC exists, use software MAC
    tmp_buffer[0] = 0x00;
    tmp_buffer[1] = 0x69;
    tmp_buffer[2] = 0x69;
    tmp_buffer[3] = 0x2D;
    tmp_buffer[4] = 0x31;
    tmp_buffer[5] = options[OPTION_DEVICE_ID];
  } else {
    // has hardware MAC chip
    status.has_hwmac = 1;
  }

  if(!ether.begin(ETHER_BUFFER_SIZE, (uint8_t*)tmp_buffer, PIN_ETHER_CS))  return 0;
  // calculate http port number
  ether.hisport = (unsigned int)(options[OPTION_HTTPPORT_1]<<8) + (unsigned int)options[OPTION_HTTPPORT_0];

  if (options[OPTION_USE_DHCP]) {
    // set up DHCP
    // register with domain name "OS-xx" where xx is the last byte of the MAC address
    if (!ether.dhcpSetup()) return 0;
    // once we have valid DHCP IP, we write these into static IP / gateway IP
    memcpy(options+OPTION_STATIC_IP1, ether.myip, 4);
    memcpy(options+OPTION_GATEWAY_IP1, ether.gwip,4);
    memcpy(options+OPTION_DNS_IP1, ether.dnsip, 4);
    options_save();
    
  } else {
    // set up static IP
    byte *staticip = options+OPTION_STATIC_IP1;
    byte *gateway  = options+OPTION_GATEWAY_IP1;
    byte *dns      = options+OPTION_DNS_IP1;
    if (!ether.staticSetup(staticip, gateway, dns))  return 0;
  }
  return 1;
}

/** Reboot controller */
void OpenSprinkler::reboot_dev() {
  resetFunc();
}

#else // RPI/BBB/LINUX network init functions

#include "etherport.h"
#include <sys/reboot.h>
#include <stdlib.h>
#include "utils.h"
#include "server.h"

extern EthernetServer *m_server;
extern char ether_buffer[];

/** Initialize network with the given mac address and http port */
byte OpenSprinkler::start_network() {
  unsigned int port = (unsigned int)(options[OPTION_HTTPPORT_1]<<8) + (unsigned int)options[OPTION_HTTPPORT_0];
#if defined(DEMO)
  port = 80;
#endif
  if(m_server)  {
    delete m_server;
    m_server = 0;
  }

  m_server = new EthernetServer(port);
  return m_server->begin();
}

/** Reboot controller */
void OpenSprinkler::reboot_dev() {
#if defined(DEMO)
  // do nothing
#else
  sync(); // add sync to prevent file corruption
	reboot(RB_AUTOBOOT);
#endif
}

/** Launch update script */
void OpenSprinkler::update_dev() {
  char cmd[1024];
  sprintf(cmd, "cd %s & ./updater.sh", get_runtime_path());
  system(cmd);
}
#endif // end network init functions

#if defined(ARDUINO)
/** Initialize LCD */
void OpenSprinkler::lcd_start() {
  // turn on lcd
  lcd.init(1, PIN_LCD_RS, 255, PIN_LCD_EN, PIN_LCD_D4, PIN_LCD_D5, PIN_LCD_D6, PIN_LCD_D7, 0,0,0,0);
  lcd.begin();

  if (lcd.type() == LCD_STD) {
    // this is standard 16x2 LCD
    // set PWM frequency for adjustable LCD backlight and contrast
#if OS_HW_VERSION==(OS_HW_VERSION_BASE+20) || OS_HW_VERSION==(OS_HW_VERSION_BASE+21)  // 8MHz and 12MHz
    TCCR1B = 0x01;
#else // 16MHz
    TCCR1B = 0x02;  // increase division factor for faster clock
#endif
    // turn on LCD backlight and contrast
    lcd_set_brightness();
    lcd_set_contrast();
  } else {
    // for I2C LCD, we don't need to do anything
  }
}
#endif

extern void flow_isr();
/** Initialize pins, controller variables, LCD */
void OpenSprinkler::begin() {

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
  pinMode(PIN_SR_DATA,  OUTPUT);
#endif

	// Reset all stations
  clear_all_station_bits();
  apply_all_station_bits();

  // pull shift register OE low to enable output
  digitalWrite(PIN_SR_OE, LOW);

  // Rain sensor port set up
  pinMode(PIN_RAINSENSOR, INPUT);

  // Set up sensors
#if defined(ARDUINO)
  digitalWrite(PIN_RAINSENSOR, HIGH); // enabled internal pullup on rain sensor
  attachInterrupt(PIN_FLOWSENSOR_INT, flow_isr, FALLING);
#else
  // OSPI and OSBO use external pullups
  attachInterrupt(PIN_FLOWSENSOR, "falling", flow_isr);
#endif



  // Default controller status variables
  // Static variables are assigned 0 by default
  // so only need to initialize non-zero ones
  status.enabled = 1;
  status.safe_reboot = 0;

  old_status = status;

  nvdata.sunrise_time = 360;  // 6:00am default sunrise
  nvdata.sunset_time = 1080;  // 6:00pm default sunset

  nboards = 1;
  nstations = 8;

  // set rf data pin
  pinMode(PIN_RF_DATA, OUTPUT);
  digitalWrite(PIN_RF_DATA, LOW);

  hw_type = HW_TYPE_AC;
#if defined(ARDUINO)  // AVR SD and LCD functions
  // Init I2C
  Wire.begin();

  #if defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__) // OS 2.3 specific detections
    uint8_t ret;

    // detect hardware type
    Wire.beginTransmission(MAC_CTRL_ID);
    Wire.write(0x00);
  	ret = Wire.endTransmission();
  	if (!ret) {
    	Wire.requestFrom(MAC_CTRL_ID, 1);
    	while(!Wire.available());
    	ret = Wire.read();
      if (ret == HW_TYPE_AC || ret == HW_TYPE_DC || ret == HW_TYPE_LATCH) {
        hw_type = ret;
      } else {
        // hardware type is not assigned
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
  #endif

  lcd_start();

  // define lcd custom icons
  byte _icon[8];
  // WiFi icon
  _icon[0] = B00000;
  _icon[1] = B10100;
  _icon[2] = B01000;
  _icon[3] = B10101;
  _icon[4] = B00001;
  _icon[5] = B00101;
  _icon[6] = B00101;
  _icon[7] = B10101;
  lcd.createChar(1, _icon);

  _icon[1]=0;
  _icon[2]=0;
  _icon[3]=1;
  lcd.createChar(0, _icon);

  // uSD card icon
  _icon[1] = B00000;
  _icon[2] = B11111;
  _icon[3] = B10001;
  _icon[4] = B11111;
  _icon[5] = B10001;
  _icon[6] = B10011;
  _icon[7] = B11110;
  lcd.createChar(2, _icon);

  // Rain icon
  _icon[2] = B00110;
  _icon[3] = B01001;
  _icon[4] = B11111;
  _icon[5] = B00000;
  _icon[6] = B10101;
  _icon[7] = B10101;
  lcd.createChar(3, _icon);

  // Connect icon
  _icon[2] = B00111;
  _icon[3] = B00011;
  _icon[4] = B00101;
  _icon[5] = B01000;
  _icon[6] = B10000;
  _icon[7] = B00000;
  lcd.createChar(4, _icon);

  // Remote extension icon
  _icon[2] = B00000;
  _icon[3] = B10001;
  _icon[4] = B01011;
  _icon[5] = B00101;
  _icon[6] = B01001;
  _icon[7] = B11110;
  lcd.createChar(5, _icon);

  // Flow sensor icon
  _icon[2] = B00000;
  _icon[3] = B11010;
  _icon[4] = B10010;
  _icon[5] = B11010;
  _icon[6] = B10011;
  _icon[7] = B00000;
  lcd.createChar(6, _icon);

  // Program switch icon
  _icon[1] = B11100;
  _icon[2] = B10100;
  _icon[3] = B11100;
  _icon[4] = B10010;
  _icon[5] = B10110;
  _icon[6] = B00010;
  _icon[7] = B00111;
  lcd.createChar(7, _icon);  

  // set sd cs pin high to release SD
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);

  if(sd.begin(PIN_SD_CS, SPI_HALF_SPEED)) {
    status.has_sd = 1;
  }
  
  #if defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__)
  if(!status.has_sd) {
    lcd.setCursor(0, 0);
    lcd_print_pgm(PSTR("Error Code: 0x2D"));
    while(1){}
  }
  #endif

  // set button pins
  // enable internal pullup
  pinMode(PIN_BUTTON_1, INPUT);
  pinMode(PIN_BUTTON_2, INPUT);
  pinMode(PIN_BUTTON_3, INPUT);
  digitalWrite(PIN_BUTTON_1, HIGH);
  digitalWrite(PIN_BUTTON_2, HIGH);
  digitalWrite(PIN_BUTTON_3, HIGH);

  // detect and check RTC type
  RTC.detect();
#else
  DEBUG_PRINTLN(get_runtime_path());
#endif
}

/** Apply all station bits
 * !!! This will activate/deactivate valves !!!
 */
void OpenSprinkler::apply_all_station_bits() {
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

  #if defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__)
  if((hw_type==HW_TYPE_DC) && engage_booster) {
    DEBUG_PRINTLN(F("engage booster"));
    // for DC controller: boost voltage
    digitalWrite(PIN_BOOST_EN, LOW);  // disable output path
    digitalWrite(PIN_BOOST, HIGH);    // enable boost converter
    delay((int)options[OPTION_BOOST_TIME]<<2);  // wait for booster to charge
    digitalWrite(PIN_BOOST, LOW);     // disable boost converter

    digitalWrite(PIN_BOOST_EN, HIGH); // enable output path
    digitalWrite(PIN_SR_LATCH, HIGH);
    engage_booster = 0;
  } else {
    digitalWrite(PIN_SR_LATCH, HIGH);
  }
  #else
  digitalWrite(PIN_SR_LATCH, HIGH);
  #endif

  if(options[OPTION_SPE_AUTO_REFRESH]) {
    // handle refresh of RF and remote stations
    // each time apply_all_station_bits is called
    // we refresh the station whose index is the current time modulo MAX_NUM_STATIONS
    static byte last_sid = 0;
    byte sid = now() % MAX_NUM_STATIONS;
    if (sid != last_sid) {  // avoid refreshing the same station twice in a roll
      last_sid = sid;
      bid=sid>>3;
      s=sid&0x07;
      switch_special_station(sid, (station_bits[bid]>>s)&0x01);
    }
  }
}

/** Read rain sensor status */
void OpenSprinkler::rainsensor_status() {
  // options[OPTION_RS_TYPE]: 0 if normally closed, 1 if normally open
  if(options[OPTION_SENSOR_TYPE]!=SENSOR_TYPE_RAIN) return;
  status.rain_sensed = (digitalRead(PIN_RAINSENSOR) == options[OPTION_RAINSENSOR_TYPE] ? 0 : 1);
}

/** Return program switch status */
bool OpenSprinkler::programswitch_status(ulong curr_time) {
  if(options[OPTION_SENSOR_TYPE]!=SENSOR_TYPE_PSWITCH) return false;
  static ulong keydown_time = 0;
  byte val = digitalRead(PIN_RAINSENSOR);
  if(!val && !keydown_time) keydown_time = curr_time;
  else if(val && keydown_time && (curr_time > keydown_time)) {
    keydown_time = 0;
    return true;
  }
  return false;
}
/** Read current sensing value
 * OpenSprinkler has a 0.2 ohm current sensing resistor.
 * Therefore the conversion from analog reading to milli-amp is:
 * (r/1024)*3.3*1000/0.2
 * Newer AC controller has a 0.2 ohm curent sensing resistor
 * with op-amp to sense the peak current. Therefore the actual
 * current is discounted by 0.707
 */
#if defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__)
uint16_t OpenSprinkler::read_current() {
  if(status.has_curr_sense) {
    if (hw_type == HW_TYPE_DC) {
      return (uint16_t)(analogRead(PIN_CURR_SENSE) * 16.11);
    } else {
      return (uint16_t)(analogRead(PIN_CURR_SENSE) * 11.39);
    }
  } else {
    return 0;
  }
}
#endif

/** Read the number of 8-station expansion boards */
// AVR has capability to detect number of expansion boards
int OpenSprinkler::detect_exp() {
#if defined(ARDUINO)
  unsigned int v = analogRead(PIN_EXP_SENSE);
  // OpenSprinkler uses voltage divider to detect expansion boards
  // Master controller has a 1.6K pull-up;
  // each expansion board (8 stations) has 10K pull-down connected in parallel;
  // so the exact ADC value for n expansion boards is:
  //    ADC = 1024 * 10 / (10 + 1.6 * n)
  // For  0,   1,   2,   3,   4,   5,  6 expansion boards, the ADC values are:
  //   1024, 882, 775, 691, 624, 568, 522
  // Actual threshold is taken as the midpoint between, to account for errors
  int n = -1;
  if (v > 953) { // 0
    n = 0;
  } else if (v > 828) { // 1
    n = 1;
  } else if (v > 733) { // 2
    n = 2;
  } else if (v > 657) { // 3
    n = 3;
  } else if (v > 596) { // 4
    n = 4;
  } else if (v > 545) { // 5
    n = 5;
  } else if (v > 502) { // 6
    n = 6;
  } else {  // cannot determine
  }
  return n;
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

/** Get station name from NVM */
void OpenSprinkler::get_station_name(byte sid, char tmp[]) {
  tmp[STATION_NAME_SIZE]=0;
  nvm_read_block(tmp, (void*)(ADDR_NVM_STN_NAMES+(int)sid*STATION_NAME_SIZE), STATION_NAME_SIZE);
}

/** Set station name to NVM */
void OpenSprinkler::set_station_name(byte sid, char tmp[]) {
  tmp[STATION_NAME_SIZE]=0;
  nvm_write_block(tmp, (void*)(ADDR_NVM_STN_NAMES+(int)sid*STATION_NAME_SIZE), STATION_NAME_SIZE);
}

/** Save station attribute bits to NVM */
void OpenSprinkler::station_attrib_bits_save(int addr, byte bits[]) {
  nvm_write_block(bits, (void*)addr, MAX_EXT_BOARDS+1);
}

/** Load all station attribute bits from NVM */
void OpenSprinkler::station_attrib_bits_load(int addr, byte bits[]) {
  nvm_read_block(bits, (void*)addr, MAX_EXT_BOARDS+1);
}

/** Read one station attribute byte from NVM */
byte OpenSprinkler::station_attrib_bits_read(int addr) {
  return nvm_read_byte((byte*)addr);
}

/** verify if a string matches password */
byte OpenSprinkler::password_verify(char *pw) {
  byte *addr = (byte*)ADDR_NVM_PASSWORD;
  byte c1, c2;
  while(1) {
    if(addr == (byte*)ADDR_NVM_PASSWORD+MAX_USER_PASSWORD)
      c1 = 0;
    else
      c1 = nvm_read_byte(addr++);
    c2 = *pw++;
    if (c1==0 || c2==0)
      break;
    if (c1!=c2) {
      return 0;
    }
  }
  return (c1==c2) ? 1 : 0;
}

// ==================
// Schedule Functions
// ==================

/** Index of today's weekday (Monday is 0) */
byte OpenSprinkler::weekday_today() {
  //return ((byte)weekday()+5)%7; // Time::weekday() assumes Sunday is 1
#if defined(ARDUINO)
  ulong wd = now_tz() / 86400L;
  return (wd+3) % 7;  // Jan 1, 1970 is a Thursday
#else
  return 0;
  // todo: is this function needed for RPI/BBB?
#endif
}

/** Switch special station */
void OpenSprinkler::switch_special_station(byte sid, byte value) {
  // check station special bit
  if(station_attrib_bits_read(ADDR_NVM_STNSPE+(sid>>3))&(1<<(sid&0x07))) {
    // read station special data from sd card
    int stepsize=sizeof(StationSpecialData);
    read_from_file(stns_filename, tmp_buffer, stepsize, sid*stepsize);
    StationSpecialData *stn = (StationSpecialData *)tmp_buffer;
    // check station type
    if(stn->type==STN_TYPE_RF) {
      // transmit RF signal
      switch_rfstation((RFStationData *)stn->data, value);
    } else if(stn->type==STN_TYPE_REMOTE) {
      // request remote station
      switch_remotestation((RemoteStationData *)stn->data, value);
    }
#if !defined(ARDUINO) || defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__)
    // GPIO and HTTP stations are only available for OS23 or OSPi
    else if(stn->type==STN_TYPE_GPIO) {
      // set GPIO pin
      switch_gpiostation((GPIOStationData *)stn->data, value);
    } else if(stn->type==STN_TYPE_HTTP) {
      // send GET command
      switch_httpstation((HTTPStationData *)stn->data, value);
    }
#endif    
  }
}

/** Set station bit
 * This function sets/resets the corresponding station bit variable
 * You have to call apply_all_station_bits next to apply the bits
 * (which results in physical actions of opening/closing valves).
 */
byte OpenSprinkler::set_station_bit(byte sid, byte value) {
  byte *data = station_bits+(sid>>3);  // pointer to the station byte
  byte mask = (byte)1<<(sid&0x07); // mask
  if (value) {
    if((*data)&mask) return 0;  // if bit is already set, return no change
    else {
      (*data) = (*data) | mask;
#if defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__)
      engage_booster = true; // if bit is changing from 0 to 1, set engage_booster
#endif
      switch_special_station(sid, 1); // handle special stations
      return 1;
    }
  } else {
    if(!((*data)&mask)) return 0; // if bit is already reset, return no change
    else {
      (*data) = (*data) & (~mask);
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
  PORT_RF |= (1<<PINX_RF);
  delayMicroseconds(lenH);
  PORT_RF &=~(1<<PINX_RF);
  delayMicroseconds(lenL);
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
#if defined(ARDUINO)
  send_rfsignal(turnon ? on : off, length);
#else
  // pre-open gpio file to minimize overhead
  rf_gpio_fd = gpio_fd_open(PIN_RF_DATA);
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

/** Callback function for browseUrl calls */
void httpget_callback(byte status, uint16_t off, uint16_t len) {
#if defined(SERIAL_DEBUG)
  Ethernet::buffer[off+ETHER_BUFFER_SIZE-1] = 0;
  DEBUG_PRINTLN((const char*) Ethernet::buffer + off);
#endif
}

/** Switch remote station
 * This function takes a remote station code,
 * parses it into remote IP, port, station index,
 * and makes a HTTP GET request.
 * The remote controller is assumed to have the same
 * password as the main controller
 */
void OpenSprinkler::switch_remotestation(RemoteStationData *data, bool turnon) {
#if defined(ARDUINO)
  // construct string
  ulong ip = hex2ulong(data->ip, sizeof(data->ip));
  ether.hisip[0] = ip>>24;
  ether.hisip[1] = (ip>>16)&0xff;
  ether.hisip[2] = (ip>>8)&0xff;
  ether.hisip[3] = ip&0xff;

  uint16_t _port = ether.hisport; // save current port number
  ether.hisport = hex2ulong(data->port, sizeof(data->port));

  char *p = tmp_buffer + sizeof(RemoteStationData) + 1;
  BufferFiller bf = (byte*)p;
  // MAX_NUM_STATIONS is the refresh cycle
  uint16_t timer = options[OPTION_SPE_AUTO_REFRESH]?2*MAX_NUM_STATIONS:65535;
  bf.emit_p(PSTR("?pw=$E&sid=$D&en=$D&t=$D"),
            ADDR_NVM_PASSWORD,
            (int)hex2ulong(data->sid,sizeof(data->sid)),
            turnon, timer);
  ether.browseUrl(PSTR("/cm"), p, PSTR("*"), httpget_callback);
  for(int l=0;l<100;l++)  ether.packetLoop(ether.packetReceive());
  ether.hisport = _port;
#else
  EthernetClient client;

  uint8_t hisip[4];
  uint16_t hisport;
  ulong ip = hex2ulong(data->ip, sizeof(data->ip));
  hisip[0] = ip>>24;
  hisip[1] = (ip>>16)&0xff;
  hisip[2] = (ip>>8)&0xff;
  hisip[3] = ip&0xff;
  hisport = hex2ulong(data->port, sizeof(data->port));

  if (!client.connect(hisip, hisport)) {
    client.stop();
    return;
  }

  char *p = tmp_buffer + sizeof(RemoteStationData) + 1;
  BufferFiller bf = p;
  // MAX_NUM_STATIONS is the refresh cycle
  uint16_t timer = options[OPTION_SPE_AUTO_REFRESH]?2*MAX_NUM_STATIONS:65535;  
  bf.emit_p(PSTR("GET /cm?pw=$E&sid=$D&en=$D&t=$D"),
            ADDR_NVM_PASSWORD,
            (int)hex2ulong(data->sid, sizeof(data->sid)),
            turnon, timer);
  bf.emit_p(PSTR(" HTTP/1.0\r\nHOST: *\r\n\r\n"));

  client.write((uint8_t *)p, strlen(p));

  bzero(ether_buffer, ETHER_BUFFER_SIZE);

  time_t timeout = now() + 5; // 5 seconds timeout
  while(now() < timeout) {
    int len=client.read((uint8_t *)ether_buffer, ETHER_BUFFER_SIZE);
    if (len<=0) {
      if(!client.connected())
        break;
      else
        continue;
    }
    httpget_callback(0, 0, ETHER_BUFFER_SIZE);
  }
  client.stop();
#endif
}

/** Switch http station
 * This function takes an http station code,
 * parses it into a server name and two HTTP GET requests.
 */
void OpenSprinkler::switch_httpstation(HTTPStationData *data, bool turnon) {

  static HTTPStationData copy;
  // make a copy of the HTTP station data and work with it
  memcpy((char*)&copy, (char*)data, sizeof(HTTPStationData));
  char * server = strtok((char *)copy.data, ",");
  char * port = strtok(NULL, ",");
  char * on_cmd = strtok(NULL, ",");
  char * off_cmd = strtok(NULL, ",");
  char * cmd = turnon ? on_cmd : off_cmd;

#if defined(ARDUINO)

  if(!ether.dnsLookup(server, true)) {
    char *ip0 = strtok(server, ".");
    char *ip1 = strtok(NULL, ".");
    char *ip2 = strtok(NULL, ".");
    char *ip3 = strtok(NULL, ".");
  
    ether.hisip[0] = ip0 ? atoi(ip0) : 0;
    ether.hisip[1] = ip1 ? atoi(ip1) : 0;
    ether.hisip[2] = ip2 ? atoi(ip2) : 0;
    ether.hisip[3] = ip3 ? atoi(ip3) : 0;
  }

  uint16_t _port = ether.hisport;
  ether.hisport = atoi(port);
  ether.browseUrlRamHost(PSTR("/"), cmd, server, httpget_callback);
  for(int l=0;l<100;l++)  ether.packetLoop(ether.packetReceive());
  ether.hisport = _port;

#else

  EthernetClient client;
  struct hostent *host;

  host = gethostbyname(server);
  if (!host) {
    DEBUG_PRINT("can't resolve http station - ");
    DEBUG_PRINTLN(server);
    return;
  }

  if (!client.connect((uint8_t*)host->h_addr, atoi(port))) {
    client.stop();
    return;
  }

  char getBuffer[255];
  sprintf(getBuffer, "GET /%s HTTP/1.0\r\nHOST: %s\r\n\r\n", cmd, host->h_name);
  client.write((uint8_t *)getBuffer, strlen(getBuffer));

  bzero(ether_buffer, ETHER_BUFFER_SIZE);

  time_t timeout = now() + 5; // 5 seconds timeout
  while(now() < timeout) {
    int len=client.read((uint8_t *)ether_buffer, ETHER_BUFFER_SIZE);
    if (len<=0) {
      if(!client.connected())
        break;
      else
        continue;
    }
    httpget_callback(0, 0, ETHER_BUFFER_SIZE);
  }

  client.stop();
#endif
}

/** Setup function for options */
void OpenSprinkler::options_setup() {

  // add 0.25 second delay to allow nvm to stablize
  delay(250);

  byte curr_ver = nvm_read_byte((byte*)(ADDR_NVM_OPTIONS+OPTION_FW_VERSION));

  // check reset condition: either firmware version has changed, or reset flag is up
  // if so, trigger a factory reset
  if (curr_ver != OS_FW_VERSION || nvm_read_byte((byte*)(ADDR_NVM_OPTIONS+OPTION_RESET))==0xAA)  {
#if defined(ARDUINO)
    lcd_print_line_clear_pgm(PSTR("Resetting..."), 0);
    lcd_print_line_clear_pgm(PSTR("Please Wait..."), 1);
#else
    DEBUG_PRINT("Resetting Options...");
#endif
    // ======== Reset NVM data ========
    int i, sn;

    // 0. wipe out nvm
    for(i=0;i<TMP_BUFFER_SIZE;i++) tmp_buffer[i]=0;
    for(i=0;i<NVM_SIZE;i+=TMP_BUFFER_SIZE) {
      int nbytes = ((NVM_SIZE-i)>TMP_BUFFER_SIZE)?TMP_BUFFER_SIZE:(NVM_SIZE-i);
      nvm_write_block(tmp_buffer, (void*)i, nbytes);
    }

    // 1. write non-volatile controller status
    nvdata_save();

    // 2. write string parameters
    nvm_write_block(DEFAULT_PASSWORD, (void*)ADDR_NVM_PASSWORD, strlen(DEFAULT_PASSWORD)+1);
    nvm_write_block(DEFAULT_LOCATION, (void*)ADDR_NVM_LOCATION, strlen(DEFAULT_LOCATION)+1);
    nvm_write_block(DEFAULT_JAVASCRIPT_URL, (void*)ADDR_NVM_JAVASCRIPTURL, strlen(DEFAULT_JAVASCRIPT_URL)+1);
    nvm_write_block(DEFAULT_WEATHER_URL, (void*)ADDR_NVM_WEATHERURL, strlen(DEFAULT_WEATHER_URL)+1);
    nvm_write_block(DEFAULT_WEATHER_KEY, (void*)ADDR_NVM_WEATHER_KEY, strlen(DEFAULT_WEATHER_KEY)+1);

    // 3. reset station names and special attributes, default Sxx
    tmp_buffer[0]='S';
    tmp_buffer[3]=0;
    for(i=ADDR_NVM_STN_NAMES, sn=1; i<ADDR_NVM_MAS_OP; i+=STATION_NAME_SIZE, sn++) {
      tmp_buffer[1]='0'+(sn/10);
      tmp_buffer[2]='0'+(sn%10);
      nvm_write_block(tmp_buffer, (void*)i, strlen(tmp_buffer)+1);
    }

    remove_file(stns_filename);
    tmp_buffer[0]=STN_TYPE_STANDARD;
    tmp_buffer[1]='0';
    tmp_buffer[2]=0;
    int stepsize=sizeof(StationSpecialData);
    for(i=0;i<MAX_NUM_STATIONS;i++) {
        write_to_file(stns_filename, tmp_buffer, stepsize, i*stepsize, false);
    }
    // 4. reset station attribute bits
    // since we wiped out nvm, only non-zero attributes need to be initialized
    for(i=0;i<MAX_EXT_BOARDS+1;i++) {
      tmp_buffer[i]=0xff;
    }
    nvm_write_block(tmp_buffer, (void*)ADDR_NVM_MAS_OP, MAX_EXT_BOARDS+1);
    nvm_write_block(tmp_buffer, (void*)ADDR_NVM_STNSEQ, MAX_EXT_BOARDS+1);

    // 5. delete sd file
    remove_file(wtopts_filename);

    // 6. write options
    options_save(); // write default option values

    //======== END OF NVM RESET CODE ========

    // restart after resetting NVM.
    delay(500);
#if defined(ARDUINO)
    reboot_dev();
#endif
  }

  {
    // load ram parameters from nvm
    // load options
    options_load();

    // load non-volatile controller data
    nvdata_load();
  }

#if defined(ARDUINO)  // handle AVR buttons
	byte button = button_read(BUTTON_WAIT_NONE);

	switch(button & BUTTON_MASK) {

  case BUTTON_1:
  	// if BUTTON_2 is pressed during startup, go to 'reset all options'
		ui_set_options(OPTION_RESET);
    if (options[OPTION_RESET]) {
			reboot_dev();
		}
		break;

  case BUTTON_2:  // button 2 is used to enter bootloader
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
    if (options[OPTION_RESET]) {
      reboot_dev();
    }
    break;
  }

  // turn on LCD backlight and contrast
  lcd_set_brightness();
  lcd_set_contrast();

  if (!button) {
    // flash screen
    lcd_print_line_clear_pgm(PSTR(" OpenSprinkler"),0);
    lcd.setCursor(2, 1);
    lcd_print_pgm(PSTR("HW v"));
    byte hwv = options[OPTION_HW_VERSION];
    lcd.print((char)('0'+(hwv/10)));
    lcd.print('.');
    lcd.print((char)('0'+(hwv%10)));
    switch(hw_type) {
    case HW_TYPE_DC:
      lcd_print_pgm(PSTR(" DC"));
      break;
    case HW_TYPE_LATCH:
      lcd_print_pgm(PSTR(" LA"));
      break;
    default:
      lcd_print_pgm(PSTR(" AC"));
    }
    delay(1000);
  }
#endif
}

/** Load non-volatile controller status data from internal NVM */
void OpenSprinkler::nvdata_load() {
  nvm_read_block(&nvdata, (void*)ADDR_NVM_NVCONDATA, sizeof(NVConData));
  old_status = status;
}

/** Save non-volatile controller status data to internal NVM */
void OpenSprinkler::nvdata_save() {
  nvm_write_block(&nvdata, (void*)ADDR_NVM_NVCONDATA, sizeof(NVConData));
}

/** Load options from internal NVM */
void OpenSprinkler::options_load() {
  nvm_read_block(tmp_buffer, (void*)ADDR_NVM_OPTIONS, NUM_OPTIONS);
  for (byte i=0; i<NUM_OPTIONS; i++) {
    options[i] = tmp_buffer[i];
  }
  nboards = options[OPTION_EXT_BOARDS]+1;
  nstations = nboards * 8;
  status.enabled = options[OPTION_DEVICE_ENABLE];
  options[OPTION_FW_MINOR] = OS_FW_MINOR;
}

/** Save options to internal NVM */
void OpenSprinkler::options_save() {
  // save options in reverse order so version number is written the last
  for (int i=NUM_OPTIONS-1; i>=0; i--) {
    tmp_buffer[i] = options[i];
  }
  nvm_write_block(tmp_buffer, (void*)ADDR_NVM_OPTIONS, NUM_OPTIONS);
  nboards = options[OPTION_EXT_BOARDS]+1;
  nstations = nboards * 8;
  status.enabled = options[OPTION_DEVICE_ENABLE];
}

// ==============================
// Controller Operation Functions
// ==============================

/** Enable controller operation */
void OpenSprinkler::enable() {
  status.enabled = 1;
  options[OPTION_DEVICE_ENABLE] = 1;
  options_save();
}

/** Disable controller operation */
void OpenSprinkler::disable() {
  status.enabled = 0;
  options[OPTION_DEVICE_ENABLE] = 0;
  options_save();
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
#if defined(ARDUINO)    // AVR LCD and button functions
/** print a program memory string */
void OpenSprinkler::lcd_print_pgm(PGM_P PROGMEM str) {
  uint8_t c;
  while((c=pgm_read_byte(str++))!= '\0') {
    lcd.print((char)c);
  }
}

/** print a program memory string to a given line with clearing */
void OpenSprinkler::lcd_print_line_clear_pgm(PGM_P PROGMEM str, byte line) {
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
}

/** print ip address */
void OpenSprinkler::lcd_print_ip(const byte *ip, byte endian) {
  lcd.clear();
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
  lcd_print_pgm(PSTR(" (MAC)"));
}

/** print station bits */
void OpenSprinkler::lcd_print_station(byte line, char c) {
  lcd.setCursor(0, line);
  if (status.display_board == 0) {
    lcd_print_pgm(PSTR("MC:"));  // Master controller is display as 'MC'
  }
  else {
    lcd_print_pgm(PSTR("E"));
    lcd.print((int)status.display_board);
    lcd_print_pgm(PSTR(":"));   // extension boards are displayed as E1, E2...
  }

  if (!status.enabled) {
  	lcd_print_line_clear_pgm(PSTR("-Disabled!-"), 1);
  } else {
	  byte bitvalue = station_bits[status.display_board];
	  for (byte s=0; s<8; s++) {
	    byte sid = (byte)status.display_board<<3;
	    sid += (s+1);
      if (sid == options[OPTION_MASTER_STATION]) {
	      lcd.print((bitvalue&1) ? (char)c : 'M'); // print master station
      } else if (sid == options[OPTION_MASTER_STATION_2]) {
	      lcd.print((bitvalue&1) ? (char)c : 'N'); // print master2 station
	    } else {
	      lcd.print((bitvalue&1) ? (char)c : '_');
	    }
  	  bitvalue >>= 1;
	  }
	}
	lcd_print_pgm(PSTR("    "));
  lcd.setCursor(12, 1);
  if(options[OPTION_REMOTE_EXT_MODE]) {
    lcd.write(5);
  }
	lcd.setCursor(13, 1);
  if(status.rain_delayed || (status.rain_sensed && options[OPTION_SENSOR_TYPE]==SENSOR_TYPE_RAIN))  {
    lcd.write(3);
  }
  if(options[OPTION_SENSOR_TYPE]==SENSOR_TYPE_FLOW) {
    lcd.write(6);
  }
  if(options[OPTION_SENSOR_TYPE]==SENSOR_TYPE_PSWITCH) {
    lcd.write(7);
  }
  lcd.setCursor(14, 1);
  if (status.has_sd)  lcd.write(2);

	lcd.setCursor(15, 1);
  lcd.write(status.network_fails>2?1:0);  // if network failure detection is more than 2, display disconnect icon

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
  strncpy_P0(tmp_buffer, op_prompts+16*i, 16);
  lcd.setCursor(0, 0);
  lcd.print(tmp_buffer);
  lcd_print_line_clear_pgm(PSTR(""), 1);
  lcd.setCursor(0, 1);
  int tz;
  switch(i) {
  case OPTION_HW_VERSION:
    lcd.print("v");
  case OPTION_FW_VERSION:
    lcd_print_version(options[i]);
    break;
  case OPTION_TIMEZONE: // if this is the time zone option, do some conversion
    tz = (int)options[i]-48;
    if (tz>=0) lcd_print_pgm(PSTR("+"));
    else {lcd_print_pgm(PSTR("-")); tz=-tz;}
    lcd.print(tz/4); // print integer portion
    lcd_print_pgm(PSTR(":"));
    tz = (tz%4)*15;
    if (tz==0)  lcd_print_pgm(PSTR("00"));
    else {
      lcd.print(tz);  // print fractional portion
    }
    break;
  case OPTION_MASTER_ON_ADJ:
  case OPTION_MASTER_ON_ADJ_2:
  case OPTION_MASTER_OFF_ADJ:
  case OPTION_MASTER_OFF_ADJ_2:
  case OPTION_STATION_DELAY_TIME:
    {
    int16_t t=water_time_decode_signed(options[i]);
    if(t>=0)  lcd_print_pgm(PSTR("+"));
    lcd.print(t);    
    }
    break;
  case OPTION_HTTPPORT_0:
    lcd.print((unsigned int)(options[i+1]<<8)+options[i]);
    break;
  case OPTION_PULSE_RATE_0:
    {
    uint16_t fpr = (unsigned int)(options[i+1]<<8)+options[i];
    lcd.print(fpr/100);
    lcd_print_pgm(PSTR("."));
    lcd.print((fpr/10)%10);
    lcd.print(fpr%10);
    }
    break;
  case OPTION_LCD_CONTRAST:
    lcd_set_contrast();
    lcd.print((int)options[i]);
    break;
  case OPTION_LCD_BACKLIGHT:
    lcd_set_brightness();
    lcd.print((int)options[i]);
    break;
  case OPTION_BOOST_TIME:
    #if defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__)
    if(hw_type==HW_TYPE_AC) {
      lcd.print('-');
    } else {
      lcd.print((int)options[i]*4);
      lcd_print_pgm(PSTR(" ms"));
    }
    #else
    lcd.print('-');
    #endif
    break;
  default:
    // if this is a boolean option
    if (pgm_read_byte(op_max+i)==1)
      lcd_print_pgm(options[i] ? PSTR("Yes") : PSTR("No"));
    else
      lcd.print((int)options[i]);
    break;
  }
  if (i==OPTION_WATER_PERCENTAGE)  lcd_print_pgm(PSTR("%"));
  else if (i==OPTION_MASTER_ON_ADJ || i==OPTION_MASTER_OFF_ADJ || i==OPTION_MASTER_ON_ADJ_2 || i==OPTION_MASTER_OFF_ADJ_2)
    lcd_print_pgm(PSTR(" sec"));
}


/** Button functions */
/** wait for button */
byte OpenSprinkler::button_read_busy(byte pin_butt, byte waitmode, byte butt, byte is_holding) {

  int hold_time = 0;

  if (waitmode==BUTTON_WAIT_NONE || (waitmode == BUTTON_WAIT_HOLD && is_holding)) {
    if (digitalRead(pin_butt) != 0) return BUTTON_NONE;
    return butt | (is_holding ? BUTTON_FLAG_HOLD : 0);
  }

  while (digitalRead(pin_butt) == 0 &&
         (waitmode == BUTTON_WAIT_RELEASE || (waitmode == BUTTON_WAIT_HOLD && hold_time<BUTTON_HOLD_MS))) {
    delay(BUTTON_DELAY_MS);
    hold_time += BUTTON_DELAY_MS;
  };
  if (is_holding || hold_time >= BUTTON_HOLD_MS)
    butt |= BUTTON_FLAG_HOLD;
  return butt;

}

/** read button and returns button value 'OR'ed with flag bits */
byte OpenSprinkler::button_read(byte waitmode)
{
  static byte old = BUTTON_NONE;
  byte curr = BUTTON_NONE;
  byte is_holding = (old&BUTTON_FLAG_HOLD);

  delay(BUTTON_DELAY_MS);

  if (digitalRead(PIN_BUTTON_1) == 0) {
    curr = button_read_busy(PIN_BUTTON_1, waitmode, BUTTON_1, is_holding);
  } else if (digitalRead(PIN_BUTTON_2) == 0) {
    curr = button_read_busy(PIN_BUTTON_2, waitmode, BUTTON_2, is_holding);
  } else if (digitalRead(PIN_BUTTON_3) == 0) {
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
      if (i==OPTION_FW_VERSION || i==OPTION_HW_VERSION || i==OPTION_FW_MINOR ||
          i==OPTION_HTTPPORT_0 || i==OPTION_HTTPPORT_1 ||
          i==OPTION_PULSE_RATE_0 || i==OPTION_PULSE_RATE_1) break; // ignore non-editable options
      if (pgm_read_byte(op_max+i) != options[i]) options[i] ++;
      break;

    case BUTTON_2:
      if (i==OPTION_FW_VERSION || i==OPTION_HW_VERSION || i==OPTION_FW_MINOR ||
          i==OPTION_HTTPPORT_0 || i==OPTION_HTTPPORT_1 ||
          i==OPTION_PULSE_RATE_0 || i==OPTION_PULSE_RATE_1) break; // ignore non-editable options
      if (options[i] != 0) options[i] --;
      break;

    case BUTTON_3:
      if (!(button & BUTTON_FLAG_DOWN)) break;
      if (button & BUTTON_FLAG_HOLD) {
        // if OPTION_RESET is set to nonzero, change it to reset condition value
        if (options[OPTION_RESET]) {
          options[OPTION_RESET] = 0xAA;
        }
        // long press, save options
        options_save();
        finished = true;
      }
      else {
        // click, move to the next option
        if (i==OPTION_USE_DHCP && options[i]) i += 9; // if use DHCP, skip static ip set
        else if (i==OPTION_HTTPPORT_0) i+=2; // skip OPTION_HTTPPORT_1
        else if (i==OPTION_PULSE_RATE_0) i+=2; // skip OPTION_PULSE_RATE_1
        else if (i==OPTION_SENSOR_TYPE && options[i]!=SENSOR_TYPE_RAIN) i+=2; // if not using rain sensor, skip rain sensor type
        else if (i==OPTION_MASTER_STATION && options[i]==0) i+=3; // if not using master station, skip master on/off adjust
        else if (i==OPTION_MASTER_STATION_2&& options[i]==0) i+=3; // if not using master2, skip master2 on/off adjust
        else  {
          i = (i+1) % NUM_OPTIONS;
        }
        if(i==OPTION_SEQUENTIAL_RETIRED) i++;
        #if defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__)
        else if (hw_type==HW_TYPE_AC && i==OPTION_BOOST_TIME) i++;  // skip boost time for non-DC controller
        else if (lcd.type()==LCD_I2C && i==OPTION_LCD_CONTRAST) i+=2;
        #endif      
      }
      break;
    }

    if (button != BUTTON_NONE) {
      lcd_print_option(i);
    }
  }
  lcd.noBlink();
}

/** Set LCD contrast (using PWM) */
void OpenSprinkler::lcd_set_contrast() {
  // set contrast is only valid for standard LCD
  if (lcd.type()==LCD_STD) {
    pinMode(PIN_LCD_CONTRAST, OUTPUT);
    analogWrite(PIN_LCD_CONTRAST, options[OPTION_LCD_CONTRAST]);
  }
}

/** Set LCD brightness (using PWM) */
void OpenSprinkler::lcd_set_brightness(byte value) {
  #if defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__)
  if (lcd.type()==LCD_I2C) {
    if (value) lcd.backlight();
    else {
      // turn off LCD backlight
      // only if dimming value is set to 0
      if(!options[OPTION_LCD_DIMMING])  lcd.noBacklight();
      else lcd.backlight();
    }
  }
  #endif
  if (lcd.type()==LCD_STD) {
    pinMode(PIN_LCD_BACKLIGHT, OUTPUT);
    if (value) {
      analogWrite(PIN_LCD_BACKLIGHT, 255-options[OPTION_LCD_BACKLIGHT]);
    } else {
      analogWrite(PIN_LCD_BACKLIGHT, 255-options[OPTION_LCD_DIMMING]);
    }
  }
}
#endif  // end of LCD and button functions

