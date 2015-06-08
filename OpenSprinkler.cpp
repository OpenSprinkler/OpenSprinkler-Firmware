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
byte OpenSprinkler::masop_bits[MAX_EXT_BOARDS+1];
byte OpenSprinkler::ignrain_bits[MAX_EXT_BOARDS+1];
byte OpenSprinkler::masop2_bits[MAX_EXT_BOARDS+1];
byte OpenSprinkler::stndis_bits[MAX_EXT_BOARDS+1];
byte OpenSprinkler::rfstn_bits[MAX_EXT_BOARDS+1];
byte OpenSprinkler::stnseq_bits[MAX_EXT_BOARDS+1];

ulong OpenSprinkler::rainsense_start_time;
ulong OpenSprinkler::raindelay_start_time;
byte OpenSprinkler::button_timeout;
ulong OpenSprinkler::ntpsync_lasttime;
ulong OpenSprinkler::checkwt_lasttime;
ulong OpenSprinkler::checkwt_success_lasttime;
ulong OpenSprinkler::network_lasttime;
ulong OpenSprinkler::external_ip;
byte OpenSprinkler::water_percent_avg;

char tmp_buffer[TMP_BUFFER_SIZE+1];       // scratch buffer

#if defined(ARDUINO)
  prog_char wtopts_name[] PROGMEM = WEATHER_OPTS_FILENAME;
#else
  char wtopts_name[] = WEATHER_OPTS_FILENAME;
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

/** Option json names */
static prog_char _json_fwv [] PROGMEM = "fwv";
static prog_char _json_tz  [] PROGMEM = "tz";
static prog_char _json_ntp [] PROGMEM = "ntp";
static prog_char _json_dhcp[] PROGMEM = "dhcp";
static prog_char _json_ip1 [] PROGMEM = "ip1";
static prog_char _json_ip2 [] PROGMEM = "ip2";
static prog_char _json_ip3 [] PROGMEM = "ip3";
static prog_char _json_ip4 [] PROGMEM = "ip4";
static prog_char _json_gw1 [] PROGMEM = "gw1";
static prog_char _json_gw2 [] PROGMEM = "gw2";
static prog_char _json_gw3 [] PROGMEM = "gw3";
static prog_char _json_gw4 [] PROGMEM = "gw4";
static prog_char _json_hp0 [] PROGMEM = "hp0";
static prog_char _json_hp1 [] PROGMEM = "hp1";
static prog_char _json_hwv [] PROGMEM = "hwv";
static prog_char _json_ext [] PROGMEM = "ext";
static prog_char _json_sdt [] PROGMEM = "sdt";
static prog_char _json_mas [] PROGMEM = "mas";
static prog_char _json_mton[] PROGMEM = "mton";
static prog_char _json_mtof[] PROGMEM = "mtof";
static prog_char _json_urs [] PROGMEM = "urs";
static prog_char _json_rso [] PROGMEM = "rso";
static prog_char _json_wl  [] PROGMEM = "wl";
static prog_char _json_den [] PROGMEM = "den";
static prog_char _json_ipas[] PROGMEM = "ipas";
static prog_char _json_devid[]PROGMEM = "devid";
static prog_char _json_con [] PROGMEM = "con";
static prog_char _json_lit [] PROGMEM = "lit";
static prog_char _json_dim [] PROGMEM = "dim";
static prog_char _json_bst [] PROGMEM = "bst";
static prog_char _json_uwt [] PROGMEM = "uwt";
static prog_char _json_ntp1[] PROGMEM = "ntp1";
static prog_char _json_ntp2[] PROGMEM = "ntp2";
static prog_char _json_ntp3[] PROGMEM = "ntp3";
static prog_char _json_ntp4[] PROGMEM = "ntp4";
static prog_char _json_log [] PROGMEM = "lg";
static prog_char _json_mas2[] PROGMEM = "mas2";
static prog_char _json_mton2[]PROGMEM = "mton2";
static prog_char _json_mtof2[]PROGMEM = "mtof2";
static prog_char _json_fwm[]  PROGMEM = "fwm";
static prog_char _json_reset[] PROGMEM = "reset";

/** Option names */
static prog_char _str_fwv [] PROGMEM = "FW:";
static prog_char _str_tz  [] PROGMEM = "TZone:";
static prog_char _str_ntp [] PROGMEM = "NTP?";
static prog_char _str_dhcp[] PROGMEM = "DHCP?";
static prog_char _str_ip1 [] PROGMEM = "OS.ip1:";
static prog_char _str_ip2 [] PROGMEM = ".ip2:";
static prog_char _str_ip3 [] PROGMEM = ".ip3:";
static prog_char _str_ip4 [] PROGMEM = ".ip4:";
static prog_char _str_gw1 [] PROGMEM = "GW.ip1:";
static prog_char _str_gw2 [] PROGMEM = ".ip2:";
static prog_char _str_gw3 [] PROGMEM = ".ip3:";
static prog_char _str_gw4 [] PROGMEM = ".ip4:";
static prog_char _str_hp0 [] PROGMEM = "Port:";
static prog_char _str_hp1 [] PROGMEM = "";
static prog_char _str_hwv [] PROGMEM = "HW: ";
static prog_char _str_ext [] PROGMEM = "Exp. board:";
static prog_char _str_sdt [] PROGMEM = "Stn delay:";
static prog_char _str_mas [] PROGMEM = "Mas1:";
static prog_char _str_mton[] PROGMEM = "Mas1  on adj:";
static prog_char _str_mtof[] PROGMEM = "Mas1 off adj:";
static prog_char _str_urs [] PROGMEM = "Rain sensor:";
static prog_char _str_rso [] PROGMEM = "Normally open?";
static prog_char _str_wl  [] PROGMEM = "% Watering:";
static prog_char _str_den [] PROGMEM = "Dev. enable?";
static prog_char _str_ipas[] PROGMEM = "Ign pwd?";
static prog_char _str_devid[]PROGMEM = "Dev. ID:";
static prog_char _str_con [] PROGMEM = "LCD con.:";
static prog_char _str_lit [] PROGMEM = "LCD lit.:";
static prog_char _str_dim [] PROGMEM = "LCD dim.:";
static prog_char _str_bst [] PROGMEM = "Boost:";
static prog_char _str_uwt [] PROGMEM = "Use weather?";
static prog_char _str_ntp1[] PROGMEM = "NTP.ip1:";
static prog_char _str_ntp2[] PROGMEM = ".ip2:";
static prog_char _str_ntp3[] PROGMEM = ".ip3:";
static prog_char _str_ntp4[] PROGMEM = ".ip4:";
static prog_char _str_log [] PROGMEM = "Log?";
static prog_char _str_mas2[] PROGMEM = "Mas2:";
static prog_char _str_mton2[]PROGMEM = "Mas2  on adj:";
static prog_char _str_mtof2[]PROGMEM = "Mas2 off adj:";
static prog_char _str_fwm[]  PROGMEM = "FWm:";
static prog_char _str_reset[] PROGMEM = "Reset all?";

OptionStruct OpenSprinkler::options[NUM_OPTIONS] = {
  {OS_FW_VERSION, 0, _str_fwv, _json_fwv}, // firmware version
  {28,  108, _str_tz,   _json_tz},    // default time zone: GMT-5
  {1,   1,   _str_ntp,  _json_ntp},   // use NTP sync
  {1,   1,   _str_dhcp, _json_dhcp},  // 0: use static ip, 1: use dhcp
  {0,   255, _str_ip1,  _json_ip1},   // this and next 3 bytes define static ip
  {0,   255, _str_ip2,  _json_ip2},
  {0,   255, _str_ip3,  _json_ip3},
  {0,   255, _str_ip4,  _json_ip4},
  {0,   255, _str_gw1,  _json_gw1},   // this and next 3 bytes define static gateway ip
  {0,   255, _str_gw2,  _json_gw2},
  {0,   255, _str_gw3,  _json_gw3},
  {0,   255, _str_gw4,  _json_gw4},
#if defined(ARDUINO)                  // on AVR, the default HTTP port is 80
  {80,  255, _str_hp0,  _json_hp0},   // this and next byte define http port number
  {0,   255, _str_hp1,  _json_hp1},
#else                                 // on RPI/BBB/LINUX, the default HTTP port is 8080
  {144, 255, _str_hp0,  _json_hp0},   // this and next byte define http port number
  {31,  255, _str_hp1,  _json_hp1},
#endif
  {OS_HW_VERSION, 0, _str_hwv, _json_hwv},
  {0,   MAX_EXT_BOARDS, _str_ext, _json_ext}, // number of extension board. 0: no extension boards
  {1,   1,   0,         0},           // the option 'sequential' is now retired
  {128, 247, _str_sdt,  _json_sdt},   // station delay time (-59 minutes to 59 minutes).
  {0,   MAX_NUM_STATIONS, _str_mas,  _json_mas},   // index of master station. 0: no master station
  {0,   60,  _str_mton, _json_mton},  // master on time [0,60] seconds
  {60,  120, _str_mtof, _json_mtof},  // master off time [-60,60] seconds
  {0,   1,   _str_urs,  _json_urs},   // rain sensor control bit. 1: use rain sensor input; 0: ignore
  {1,   1,   _str_rso,  _json_rso},   // rain sensor type. 0: normally closed; 1: normally open.
  {100, 250, _str_wl,   _json_wl},    // water level (default 100%),
  {1,   1,   _str_den,  _json_den},   // device enable
  {0,   1,   _str_ipas, _json_ipas},  // 1: ignore password; 0: use password
  {0,   255, _str_devid,_json_devid}, // device id
  {110, 255, _str_con,  _json_con},   // lcd contrast
  {100, 255, _str_lit,  _json_lit},   // lcd backlight
  {15,  255, _str_dim,  _json_dim},   // lcd dimming
  {60,  250, _str_bst,  _json_bst},   // boost time (only valid to DC and LATCH type)
  {0,   255, _str_uwt,  _json_uwt},   // weather algorithm (0 means not using weather algorithm)
  {50,  255, _str_ntp1, _json_ntp1},  // this and the next three bytes define the ntp server ip
  {97,  255, _str_ntp2, _json_ntp2},
  {210, 255, _str_ntp3, _json_ntp3},
  {169, 255, _str_ntp4, _json_ntp4},
  {1,   1,   _str_log,  _json_log},   // enable logging: 0: disable; 1: enable.
  {0,   MAX_NUM_STATIONS, _str_mas2, _json_mas2},  // index of master 2. 0: no master2 station
  {0,   60,  _str_mton2,_json_mton2},
  {60,  120, _str_mtof2,_json_mtof2}, 
  {OS_FW_MINOR, 0, _str_fwm, _json_fwm}, // firmware version  
  {0,   1,   _str_reset,_json_reset}
};

/** Weekday display strings */
static prog_char str_day0[] PROGMEM = "Mon";
static prog_char str_day1[] PROGMEM = "Tue";
static prog_char str_day2[] PROGMEM = "Wed";
static prog_char str_day3[] PROGMEM = "Thu";
static prog_char str_day4[] PROGMEM = "Fri";
static prog_char str_day5[] PROGMEM = "Sat";
static prog_char str_day6[] PROGMEM = "Sun";

prog_char* days_str[7] = {
  str_day0,
  str_day1,
  str_day2,
  str_day3,
  str_day4,
  str_day5,
  str_day6
};

// return local time (UTC time plus time zone offset)
time_t OpenSprinkler::now_tz() {
  return now()+(int32_t)3600/4*(int32_t)(options[OPTION_TIMEZONE].value-48);
}

#if defined(ARDUINO)  // AVR network init functions

// read hardware MAC
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

  network_lasttime = now();

  // new from 2.2: read hardware MAC
  if(!read_hardware_mac())
  {
    // if no hardware MAC exists, use software MAC
    tmp_buffer[0] = 0x00;
    tmp_buffer[1] = 0x69;
    tmp_buffer[2] = 0x69;
    tmp_buffer[3] = 0x2D;
    tmp_buffer[4] = 0x31;
    tmp_buffer[5] = options[OPTION_DEVICE_ID].value;
  } else {
    // has hardware MAC chip
    status.has_hwmac = 1;
  }

  if(!ether.begin(ETHER_BUFFER_SIZE, (uint8_t*)tmp_buffer, PIN_ETHER_CS))  return 0;
  // calculate http port number
  ether.hisport = (unsigned int)(options[OPTION_HTTPPORT_1].value<<8) + (unsigned int)options[OPTION_HTTPPORT_0].value;

  if (options[OPTION_USE_DHCP].value) {
    // set up DHCP
    // register with domain name "OS-xx" where xx is the last byte of the MAC address
    if (!ether.dhcpSetup()) return 0;
    // once we have valid DHCP IP, we write these into static IP / gateway IP
    byte *ip = ether.myip;
    options[OPTION_STATIC_IP1].value = ip[0];
    options[OPTION_STATIC_IP2].value = ip[1];
    options[OPTION_STATIC_IP3].value = ip[2];
    options[OPTION_STATIC_IP4].value = ip[3];            

    ip = ether.gwip;
    options[OPTION_GATEWAY_IP1].value = ip[0];
    options[OPTION_GATEWAY_IP2].value = ip[1];
    options[OPTION_GATEWAY_IP3].value = ip[2];
    options[OPTION_GATEWAY_IP4].value = ip[3];
    options_save();
    
  } else {
    // set up static IP
    byte staticip[] = {
      options[OPTION_STATIC_IP1].value,
      options[OPTION_STATIC_IP2].value,
      options[OPTION_STATIC_IP3].value,
      options[OPTION_STATIC_IP4].value};

    byte gateway[] = {
      options[OPTION_GATEWAY_IP1].value,
      options[OPTION_GATEWAY_IP2].value,
      options[OPTION_GATEWAY_IP3].value,
      options[OPTION_GATEWAY_IP4].value};
    if (!ether.staticSetup(staticip, gateway, gateway))  return 0;
  }
  return 1;
}

/** Reboot controller */
void OpenSprinkler::reboot_dev() {
  resetFunc();
}

#else // RPI/BBB/LINUX network init functions

extern EthernetServer *m_server;

byte OpenSprinkler::start_network() {
  unsigned int port = (unsigned int)(options[OPTION_HTTPPORT_1].value<<8) + (unsigned int)options[OPTION_HTTPPORT_0].value;
#if defined(DEMO)
  port = 8080;
#endif
  if(m_server)  {
    delete m_server;
    m_server = 0;
  }
  
  m_server = new EthernetServer(port);
  return m_server->begin();
}

#include <sys/reboot.h>
void OpenSprinkler::reboot_dev() {
#if defined(DEMO)
  // do nothingb
#else
  sync(); // add sync to prevent file corruption
	reboot(RB_AUTOBOOT);
#endif
}

#endif // end network init functions

#if defined(ARDUINO)
void OpenSprinkler::lcd_start() {
  // turn on lcd
  lcd.init(1, PIN_LCD_RS, 255, PIN_LCD_EN, PIN_LCD_D4, PIN_LCD_D5, PIN_LCD_D6, PIN_LCD_D7, 0,0,0,0);
  lcd.begin();

  if (lcd.type() == LCD_STD) {
    // this is standard 16x2 LCD
    // set PWM frequency for adjustable LCD backlight and contrast
    TCCR1B = 0x01;
    // turn on LCD backlight and contrast
    lcd_set_brightness();
    lcd_set_contrast();
  } else {
    // this is I2C LCD
    // handle brightness
  }
}
#endif

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

#if defined(ARDUINO)
  digitalWrite(PIN_RAINSENSOR, HIGH); // enabled internal pullup on rain sensor
#else
  // RPI and BBB have external pullups
#endif

  // Default controller status variables
  // AVR assigns 0 to static variables by default
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

  /*pinMode(PIN_RELAY, OUTPUT);
  digitalWrite(PIN_RELAY, LOW);*/

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
  _icon[0] = B00000;
  _icon[1] = B00000;
  _icon[2] = B11111;
  _icon[3] = B10001;
  _icon[4] = B11111;
  _icon[5] = B10001;
  _icon[6] = B10011;
  _icon[7] = B11110;
  lcd.createChar(2, _icon);  
  
  // Rain icon
  _icon[0] = B00000;
  _icon[1] = B00000;
  _icon[2] = B00110;
  _icon[3] = B01001;
  _icon[4] = B11111;
  _icon[5] = B00000;
  _icon[6] = B10101;
  _icon[7] = B10101;
  lcd.createChar(3, _icon);
  
  // Connect icon
  _icon[0] = B00000;
  _icon[1] = B00000;
  _icon[2] = B00111;
  _icon[3] = B00011;
  _icon[4] = B00101;
  _icon[5] = B01000;
  _icon[6] = B10000;
  _icon[7] = B00000;
  lcd.createChar(4, _icon);

  // set sd cs pin high to release SD
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);

  if(sd.begin(PIN_SD_CS, SPI_HALF_SPEED)) {
    status.has_sd = 1;
  }

  // set button pins
  // enable internal pullup
  pinMode(PIN_BUTTON_1, INPUT);
  pinMode(PIN_BUTTON_2, INPUT);
  pinMode(PIN_BUTTON_3, INPUT);
  digitalWrite(PIN_BUTTON_1, HIGH);
  digitalWrite(PIN_BUTTON_2, HIGH);
  digitalWrite(PIN_BUTTON_3, HIGH);

  // detect if DS1307 RTC exists
  if (RTC.detect()==0) {
    status.has_rtc = 1;
  }
#endif
}

// Apply all station bits
// !!! This will activate/deactivate valves !!!
void OpenSprinkler::apply_all_station_bits() {
  digitalWrite(PIN_SR_LATCH, LOW);
  byte bid, s, sbits;

  bool engage_booster = false;
  
  #if defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__)
  // old station bits
  static byte old_station_bits[MAX_EXT_BOARDS+1];
  #endif
  
  // Shift out all station bit values
  // from the highest bit to the lowest
  for(bid=0;bid<=MAX_EXT_BOARDS;bid++) {
    if (status.enabled)
      sbits = station_bits[MAX_EXT_BOARDS-bid];
    else
      sbits = 0;
    
    #if defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__)
    // check if any station is changing from 0 to 1
    // take the bit inverse of the old status
    // and with the new status
    if ((~old_station_bits[MAX_EXT_BOARDS-bid]) & sbits) {
      engage_booster = true;
    }
    old_station_bits[MAX_EXT_BOARDS-bid] = sbits;
    #endif
          
    for(s=0;s<8;s++) {
      digitalWrite(PIN_SR_CLOCK, LOW);
#if defined(OSPI) // if OSPi, use dynamically assigned pin_sr_data
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
    // boost voltage
    digitalWrite(PIN_BOOST, HIGH);
    delay(250);
    digitalWrite(PIN_BOOST, LOW);
   
    // enable boosted voltage for a short period of time
    digitalWrite(PIN_BOOST_EN, HIGH);
    digitalWrite(PIN_SR_LATCH, HIGH);
    delay(500);
    digitalWrite(PIN_BOOST_EN, LOW);
  } else {
    digitalWrite(PIN_SR_LATCH, HIGH);
  }
  #else
  digitalWrite(PIN_SR_LATCH, HIGH);
  #endif
}

void OpenSprinkler::rainsensor_status() {
  // options[OPTION_RS_TYPE]: 0 if normally closed, 1 if normally open
  status.rain_sensed = (digitalRead(PIN_RAINSENSOR) == options[OPTION_RAINSENSOR_TYPE].value ? 0 : 1);
}

int OpenSprinkler::detect_exp() { // AVR has capability to detect number of expansion boards
#if defined(ARDUINO)
  unsigned int v = analogRead(PIN_EXP_SENSE);
  // OpenSprinkler uses voltage divider to detect expansion boards
  // Master controller has a 1.5K pull-up;
  // each expansion board (8 stations) has 10K pull-down connected in parallel;
  // so the exact ADC value for n expansion boards is:
  //    ADC = 1024 * 10 / (10 + 1.5 * n)
  // For  0,   1,   2,   3,   4,   5 expansion boards, the ADC values are:
  //   1024, 890, 787, 706, 640, 585
  // Actual threshold is taken as the midpoint between, to account for errors
  int n = -1;
  if (v > 957) { // 0
    n = 0;
  } else if (v > 838) { // 1
    n = 1;
  } else if (v > 746) { // 2
    n = 2;
  } else if (v > 673) { // 3
    n = 3;
  } else if (v > 612) { // 4
    n = 4;
  } else if (v > 562) { // 5
    n = 5;
  } else {  // cannot determine
  }
  return n;
#else
  return -1;
#endif
}

static ulong nvm_hex2ulong(byte *addr, byte len) {
  char c;
  ulong v = 0;
  nvm_read_block(tmp_buffer, (void*)addr, len);
  for(byte i=0;i<len;i++) {
    c = tmp_buffer[i];
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

// Get station name from nvm and parse into RF code
uint16_t OpenSprinkler::get_station_name_rf(byte sid, ulong* on, ulong *off) {
  byte *start = (byte *)(ADDR_NVM_STN_NAMES) + (int)sid * STATION_NAME_SIZE;
  ulong v;
  v = nvm_hex2ulong(start, 6);
  if (!v) return 0;
  if (on) *on = v;
  v = nvm_hex2ulong(start+6, 6);
  if (!v) return 0;
  if (off) *off = v;
  v = nvm_hex2ulong(start+12, 4);
  if (!v) return 0;
  return v;
}

// Get station name from nvm
void OpenSprinkler::get_station_name(byte sid, char tmp[]) {
  tmp[STATION_NAME_SIZE]=0;
  nvm_read_block(tmp, (void*)(ADDR_NVM_STN_NAMES+(int)sid*STATION_NAME_SIZE), STATION_NAME_SIZE);
  return;
}

// Set station name to nvm
void OpenSprinkler::set_station_name(byte sid, char tmp[]) {
  tmp[STATION_NAME_SIZE]=0;
  nvm_write_block(tmp, (void*)(ADDR_NVM_STN_NAMES+(int)sid*STATION_NAME_SIZE), STATION_NAME_SIZE);
  return;
}

// Save station attribute bits to NVM
void OpenSprinkler::station_attrib_bits_save(int addr, byte bits[]) {
  nvm_write_block(bits, (void*)addr, MAX_EXT_BOARDS+1);
}

void OpenSprinkler::station_attrib_bits_load(int addr, byte bits[]) {
  nvm_read_block(bits, (void*)addr, MAX_EXT_BOARDS+1);
}

// verify if a string matches password
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

// Index of today's weekday (Monday is 0)
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

// Set station bit
void OpenSprinkler::set_station_bit(byte sid, byte value) {
  byte bid = (sid>>3);  // board index
  byte s = sid&0x07;    // station bit index
  if (value) {
    station_bits[bid] = station_bits[bid] | ((byte)1<<s);
  }
  else {
    station_bits[bid] = station_bits[bid] &~((byte)1<<s);
  }
}

// Clear all station bits
void OpenSprinkler::clear_all_station_bits() {
  byte bid;
  for(bid=0;bid<=MAX_EXT_BOARDS;bid++) {
    station_bits[bid] = 0;
  }
}

void transmit_rfbit(ulong lenH, ulong lenL) {
#if defined(ARDUINO)
  PORT_RF |= (1<<PINX_RF);
  delayMicroseconds(lenH);
  PORT_RF &=~(1<<PINX_RF);
  delayMicroseconds(lenL);
#else
  digitalWrite(PIN_RF_DATA, 1);
  usleep(lenH);
  digitalWrite(PIN_RF_DATA, 0);
  usleep(lenL);
#endif
}

void OpenSprinkler::update_rfstation_bits() {
  byte bid, s, sid;
  for(bid=0;bid<(1+MAX_EXT_BOARDS);bid++) {
    rfstn_bits[bid] = 0;
    for(s=0;s<8;s++) {
      sid = (bid<<3) | s;
      if(get_station_name_rf(sid, NULL, NULL)) {
        rfstn_bits[bid] |= (1<<s);
      }
    }
  }
}

void send_rfsignal(ulong code, ulong len) {
  ulong len3 = len * 3;
  ulong len31 = len * 31;
  for(byte n=0;n<24;n++) {
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

void OpenSprinkler::send_rfstation_signal(byte sid, bool turnon) {
  ulong on, off;
  uint16_t length = get_station_name_rf(sid, &on, &off);
#if defined(ARDUINO)
  length = (length>>1)+(length>>2);   // due to internal call delay, scale time down to 75%
#else
  length = (length>>2)+(length>>3);   // on RPi and BBB, there is even more overhead, scale to 37.5%
#endif
  send_rfsignal(turnon ? on : off, length);
}


/** Options Functions */
void OpenSprinkler::options_setup() {

  // add 0.5 second delay to allow nvm to stablize
  delay(500);

  // check reset condition: either firmware version has changed, or reset flag is up
  byte curr_ver = nvm_read_byte((byte*)(ADDR_NVM_OPTIONS+OPTION_FW_VERSION));

  //if (curr_ver<100) curr_ver = curr_ver*10; // adding a default 0 if version number is the old type
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

    // 1. write program data default parameters

    // 2. write non-volatile controller status
    nvdata_save();

    // 3. write string parameters
    nvm_write_block(DEFAULT_PASSWORD, (void*)ADDR_NVM_PASSWORD, strlen(DEFAULT_PASSWORD)+1);
    nvm_write_block(DEFAULT_LOCATION, (void*)ADDR_NVM_LOCATION, strlen(DEFAULT_LOCATION)+1);
    nvm_write_block(DEFAULT_JAVASCRIPT_URL, (void*)ADDR_NVM_SCRIPTURL, strlen(DEFAULT_JAVASCRIPT_URL)+1);
    nvm_write_block(DEFAULT_WEATHER_KEY, (void*)ADDR_NVM_WEATHER_KEY, strlen(DEFAULT_WEATHER_KEY)+1);

    // 4. reset station names, default Sxx
    tmp_buffer[0]='S';
    tmp_buffer[3]=0;
    for(i=ADDR_NVM_STN_NAMES, sn=1; i<ADDR_NVM_MAS_OP; i+=STATION_NAME_SIZE, sn++) {
      tmp_buffer[1]='0'+(sn/10);
      tmp_buffer[2]='0'+(sn%10);
      nvm_write_block(tmp_buffer, (void*)i, strlen(tmp_buffer)+1);
    }

    // 5. reset station attributes
    // since we wiped out nvm, only non-zero attributes need to be initialized
    for(i=0;i<MAX_EXT_BOARDS+1;i++) {
      tmp_buffer[i]=0xff;
    }
    nvm_write_block(tmp_buffer, (void*)ADDR_NVM_MAS_OP, MAX_EXT_BOARDS+1);
    nvm_write_block(tmp_buffer, (void*)ADDR_NVM_STNSEQ, MAX_EXT_BOARDS+1);

    // 6. write options
    options_save(); // write default option values
    
    // 7. delete sd files
    remove_file(wtopts_name);
    //======== END OF NVM RESET CODE ========

    // restart after resetting NVM.
    delay(500);
#if defined(ARDUINO)
    reboot_dev();
#endif
  }

  {
    // load ram parameters from nvm
    // 1. load options
    options_load();
    // 2. station attributes
    station_attrib_bits_load(ADDR_NVM_MAS_OP, masop_bits);
    station_attrib_bits_load(ADDR_NVM_IGNRAIN, ignrain_bits);
    station_attrib_bits_load(ADDR_NVM_MAS_OP_2, masop2_bits);
    station_attrib_bits_load(ADDR_NVM_STNDISABLE, stndis_bits);
    station_attrib_bits_load(ADDR_NVM_STNSEQ, stnseq_bits);
    // set RF station flags
    update_rfstation_bits();

    // 3. load non-volatile controller data
    nvdata_load();
  }

#if defined(ARDUINO)  // handle AVR buttons
	byte button = button_read(BUTTON_WAIT_NONE);

	switch(button & BUTTON_MASK) {

  case BUTTON_1:
  	// if BUTTON_2 is pressed during startup, go to 'reset all options'
		ui_set_options(OPTION_RESET);
		if (options[OPTION_RESET].value) {
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
    if (options[OPTION_RESET].value) {
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
    byte hwv = options[OPTION_HW_VERSION].value;
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

// Load non-volatile controller status data from internal nvm
void OpenSprinkler::nvdata_load() {
  nvm_read_block(&nvdata, (void*)ADDR_NVM_NVCONDATA, sizeof(NVConData));
  old_status = status;
}

// Save non-volatile controller status data to internal nvm
void OpenSprinkler::nvdata_save() {
  nvm_write_block(&nvdata, (void*)ADDR_NVM_NVCONDATA, sizeof(NVConData));
}

// Load options from internal nvm
void OpenSprinkler::options_load() {
  nvm_read_block(tmp_buffer, (void*)ADDR_NVM_OPTIONS, NUM_OPTIONS);
  for (byte i=0; i<NUM_OPTIONS; i++) {
    options[i].value = tmp_buffer[i];
  }
  nboards = options[OPTION_EXT_BOARDS].value+1;
  nstations = nboards * 8;
  status.enabled = options[OPTION_DEVICE_ENABLE].value;
  options[OPTION_FW_MINOR].value = OS_FW_MINOR;
}

// Save options to internal nvm
void OpenSprinkler::options_save() {
  // save options in reverse order so version number is written the last
  for (int i=NUM_OPTIONS-1; i>=0; i--) {
    tmp_buffer[i] = options[i].value;
  }
  nvm_write_block(tmp_buffer, (void*)ADDR_NVM_OPTIONS, NUM_OPTIONS);
  nboards = options[OPTION_EXT_BOARDS].value+1;
  nstations = nboards * 8;
  status.enabled = options[OPTION_DEVICE_ENABLE].value;
}

// ==============================
// Controller Operation Functions
// ==============================

// Enable controller operation
void OpenSprinkler::enable() {
  status.enabled = 1;
  options[OPTION_DEVICE_ENABLE].value = 1;
  options_save();
}

// Disable controller operation
void OpenSprinkler::disable() {
  status.enabled = 0;
  options[OPTION_DEVICE_ENABLE].value = 0;
  options_save();
}

void OpenSprinkler::raindelay_start() {
  status.rain_delayed = 1;
  nvdata_save();
}

void OpenSprinkler::raindelay_stop() {
  status.rain_delayed = 0;
  nvdata.rd_stop_time = 0;
  nvdata_save();
}

#if defined(ARDUINO)    // AVR LCD and button functions
/** LCD and button functions */
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
  lcd_print_pgm(days_str[weekday_today()]);
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
	    if (sid == options[OPTION_MASTER_STATION].value) {
	      lcd.print((bitvalue&1) ? (char)c : 'M'); // print master station
	    } else if (sid == options[OPTION_MASTER_STATION_2].value) {
	      lcd.print((bitvalue&1) ? (char)c : 'N'); // print master2 station
	    } else {
	      lcd.print((bitvalue&1) ? (char)c : '_');
	    }
  	  bitvalue >>= 1;
	  }
	}
	lcd_print_pgm(PSTR("    "));
	lcd.setCursor(13, 1);
  if(status.rain_delayed || (status.rain_sensed && options[OPTION_USE_RAINSENSOR].value))
  {
    lcd.write(3);
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
  lcd_print_line_clear_pgm(options[i].str, 0);
  lcd_print_line_clear_pgm(PSTR(""), 1);
  lcd.setCursor(0, 1);
  int tz;
  switch(i) {
  case OPTION_HW_VERSION:
    lcd.print("v");
  case OPTION_FW_VERSION:
    lcd_print_version(options[i].value);
    break;
  case OPTION_TIMEZONE: // if this is the time zone option, do some conversion
    tz = (int)options[i].value-48;
    if (tz>=0) lcd_print_pgm(PSTR("+"));
    else {lcd_print_pgm(PSTR("-")); tz=-tz;}
    lcd.print(tz/4); // print integer portion
    lcd_print_pgm(PSTR(":"));
    tz = (tz%4)*15;
    if (tz==0)  lcd_print_pgm(PSTR("00"));
    else {
      lcd.print(tz);  // print fractional portion
    }
    lcd_print_pgm(PSTR(" GMT"));
    break;
  case OPTION_MASTER_ON_ADJ:
  case OPTION_MASTER_ON_ADJ_2:
    lcd_print_pgm(PSTR("+"));
    lcd.print((int)options[i].value);
    break;
  case OPTION_MASTER_OFF_ADJ:
  case OPTION_MASTER_OFF_ADJ_2:
    if(options[i].value>=60)  lcd_print_pgm(PSTR("+"));
    lcd.print((int)options[i].value-60);
    break;
  case OPTION_HTTPPORT_0:
    lcd.print((unsigned int)(options[i+1].value<<8)+options[i].value);
    break;
  case OPTION_STATION_DELAY_TIME:
    lcd.print(water_time_decode_signed(options[i].value));
    break;
  case OPTION_LCD_CONTRAST:
    lcd_set_contrast();
    lcd.print((int)options[i].value);
    break;
  case OPTION_LCD_BACKLIGHT:
    lcd_set_brightness();
    lcd.print((int)options[i].value);
    break;
  case OPTION_BOOST_TIME:
    #if defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__)
    if(hw_type==HW_TYPE_AC) {
      lcd.print('-');
    } else {
      lcd.print((int)options[i].value*4);
      lcd_print_pgm(PSTR(" ms"));
    }
    #else
    lcd.print('-');
    #endif
    break;
  default:
    // if this is a boolean option
    if (options[i].max==1)
      lcd_print_pgm(options[i].value ? PSTR("Yes") : PSTR("No"));
    else
      lcd.print((int)options[i].value);
    break;
  }
  if (i==OPTION_WATER_PERCENTAGE)  lcd_print_pgm(PSTR("%"));
  else if (i==OPTION_MASTER_ON_ADJ || i==OPTION_MASTER_OFF_ADJ || i==OPTION_MASTER_ON_ADJ_2 || i==OPTION_MASTER_OFF_ADJ_2 || i==OPTION_STATION_DELAY_TIME)
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
          i==OPTION_HTTPPORT_0 || i==OPTION_HTTPPORT_1) break; // ignore non-editable options
      if (options[i].max != options[i].value) options[i].value ++;
      break;

    case BUTTON_2:
      if (i==OPTION_FW_VERSION || i==OPTION_HW_VERSION || i==OPTION_FW_MINOR ||
          i==OPTION_HTTPPORT_0 || i==OPTION_HTTPPORT_1) break; // ignore non-editable options
      if (options[i].value != 0) options[i].value --;
      break;

    case BUTTON_3:
      if (!(button & BUTTON_FLAG_DOWN)) break;
      if (button & BUTTON_FLAG_HOLD) {
        // if OPTION_RESET is set to nonzero, change it to reset condition value
        if (options[OPTION_RESET].value) {
          options[OPTION_RESET].value = 0xAA;
        }
        // long press, save options
        options_save();
        finished = true;
      }
      else {
        // click, move to the next option
        if (i==OPTION_USE_DHCP && options[i].value) i += 9; // if use DHCP, skip static ip set
        else if(i==OPTION_HTTPPORT_0) i+=2; // skip OPTION_HTTPPORT_1
        else if(i==OPTION_USE_RAINSENSOR && options[i].value==0) i+=2; // if not using rain sensor, skip rain sensor type
        else if(i==OPTION_MASTER_STATION && options[i].value==0) i+=3; // if not using master station, skip master on/off adjust
        else if(i==OPTION_MASTER_STATION_2&& options[i].value==0) i+=3; // if not using master2, skip master2 on/off adjust
        else  {
          i = (i+1) % NUM_OPTIONS;
        }
        if(options[i].json_str==0) i++;
      }
      break;
    }

    if (button != BUTTON_NONE) {
      lcd_print_option(i);
    }
  }
  lcd.noBlink();
}

void OpenSprinkler::lcd_set_contrast() {
  // set contrast is only valid for standard LCD
  if (lcd.type()==LCD_STD) {
    pinMode(PIN_LCD_CONTRAST, OUTPUT);
    analogWrite(PIN_LCD_CONTRAST, options[OPTION_LCD_CONTRAST].value);
  }
}

void OpenSprinkler::lcd_set_brightness(byte value) {
  #if defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__)
  if (lcd.type()==LCD_I2C) {
    if (value) lcd.backlight();
    else lcd.noBacklight();
  }
  #endif
  if (lcd.type()==LCD_STD) {
    pinMode(PIN_LCD_BACKLIGHT, OUTPUT);
    if (value) {
      analogWrite(PIN_LCD_BACKLIGHT, 255-options[OPTION_LCD_BACKLIGHT].value);
    } else {
      analogWrite(PIN_LCD_BACKLIGHT, 255-options[OPTION_LCD_DIMMING].value);    
    }
  }
}
#endif  // end of LCD and button functions

