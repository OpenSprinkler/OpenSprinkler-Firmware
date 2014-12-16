// Arduino library code for OpenSprinkler Generation 2

/* OpenSprinkler Class Implementation
   Creative Commons Attribution-ShareAlike 3.0 license
   Nov 2014 @ Rayshobby.net
*/

#include "OpenSprinklerGen2.h"

// Declare static data members
LiquidCrystal OpenSprinkler::lcd;
NVConData OpenSprinkler::nvdata;
ConStatus OpenSprinkler::status;
ConStatus OpenSprinkler::old_status;

byte OpenSprinkler::nboards;
byte OpenSprinkler::nstations;
byte OpenSprinkler::station_bits[MAX_EXT_BOARDS+1];
byte OpenSprinkler::masop_bits[MAX_EXT_BOARDS+1];
byte OpenSprinkler::ignrain_bits[MAX_EXT_BOARDS+1];
byte OpenSprinkler::actrelay_bits[MAX_EXT_BOARDS+1];
byte OpenSprinkler::stndis_bits[MAX_EXT_BOARDS+1];
byte OpenSprinkler::rfstn_bits[MAX_EXT_BOARDS+1];
byte OpenSprinkler::stnseq_bits[MAX_EXT_BOARDS+1];

unsigned long OpenSprinkler::rainsense_start_time;
unsigned long OpenSprinkler::raindelay_start_time;
unsigned long OpenSprinkler::button_lasttime;
unsigned long OpenSprinkler::ntpsync_lasttime;
unsigned long OpenSprinkler::checkwt_lasttime;
unsigned long OpenSprinkler::network_lasttime;
unsigned long OpenSprinkler::dhcpnew_lasttime;
byte OpenSprinkler::water_percent_avg;

extern char tmp_buffer[];

// Option json names
prog_char _json_fwv [] PROGMEM = "fwv";
prog_char _json_tz  [] PROGMEM = "tz";
prog_char _json_ntp [] PROGMEM = "ntp";
prog_char _json_dhcp[] PROGMEM = "dhcp";
prog_char _json_ip1 [] PROGMEM = "ip1";
prog_char _json_ip2 [] PROGMEM = "ip2";
prog_char _json_ip3 [] PROGMEM = "ip3";
prog_char _json_ip4 [] PROGMEM = "ip4";
prog_char _json_gw1 [] PROGMEM = "gw1";
prog_char _json_gw2 [] PROGMEM = "gw2";
prog_char _json_gw3 [] PROGMEM = "gw3";
prog_char _json_gw4 [] PROGMEM = "gw4";
prog_char _json_hp0 [] PROGMEM = "hp0";
prog_char _json_hp1 [] PROGMEM = "hp1";
prog_char _json_hwv [] PROGMEM = "hwv";
prog_char _json_ext [] PROGMEM = "ext";
prog_char _json_seq [] PROGMEM = "_";
prog_char _json_sdt [] PROGMEM = "sdt";
prog_char _json_mas [] PROGMEM = "mas";
prog_char _json_mton[] PROGMEM = "mton";
prog_char _json_mtof[] PROGMEM = "mtof";
prog_char _json_urs [] PROGMEM = "urs";
prog_char _json_rso [] PROGMEM = "rso";
prog_char _json_wl  [] PROGMEM = "wl";
prog_char _json_den [] PROGMEM = "den";
prog_char _json_ipas[] PROGMEM = "ipas";
prog_char _json_devid[]PROGMEM = "devid";
prog_char _json_con [] PROGMEM = "con";
prog_char _json_lit [] PROGMEM = "lit";
prog_char _json_dim [] PROGMEM = "dim";
prog_char _json_rlp [] PROGMEM = "rlp";
prog_char _json_uwt [] PROGMEM = "uwt";
prog_char _json_ntp1[] PROGMEM = "ntp1";
prog_char _json_ntp2[] PROGMEM = "ntp2";
prog_char _json_ntp3[] PROGMEM = "ntp3";
prog_char _json_ntp4[] PROGMEM = "ntp4";
prog_char _json_log [] PROGMEM = "lg";
prog_char _json_reset[] PROGMEM = "reset";

// Option names
prog_char _str_fwv [] PROGMEM = "Firmware: ";
prog_char _str_tz  [] PROGMEM = "TZone:";
prog_char _str_ntp [] PROGMEM = "NTP?";
prog_char _str_dhcp[] PROGMEM = "Use DHCP?";
prog_char _str_ip1 [] PROGMEM = "Static.ip1:";
prog_char _str_ip2 [] PROGMEM = "Static.ip2:";
prog_char _str_ip3 [] PROGMEM = "Static.ip3:";
prog_char _str_ip4 [] PROGMEM = "Static.ip4:";
prog_char _str_gw1 [] PROGMEM = "Gateway.ip1:";
prog_char _str_gw2 [] PROGMEM = "Gateway.ip2:";
prog_char _str_gw3 [] PROGMEM = "Gateway.ip3:";
prog_char _str_gw4 [] PROGMEM = "Gateway.ip4:";
prog_char _str_hp0 [] PROGMEM = "HTTP port:";
prog_char _str_hp1 [] PROGMEM = "";
prog_char _str_hwv [] PROGMEM = "Hardware: ";
prog_char _str_ext [] PROGMEM = "Exp. board:";
prog_char _str_seq [] PROGMEM = "";
prog_char _str_sdt [] PROGMEM = "Stn delay:";
prog_char _str_mas [] PROGMEM = "Mas. station:";
prog_char _str_mton[] PROGMEM = "Mas.  on adj.:";
prog_char _str_mtof[] PROGMEM = "Mas. off adj.:";
prog_char _str_urs [] PROGMEM = "Rain sensor:";
prog_char _str_rso [] PROGMEM = "Normally open?";
prog_char _str_wl  [] PROGMEM = "% Water time:";
prog_char _str_den [] PROGMEM = "Device enable?";
prog_char _str_ipas[] PROGMEM = "Ign password?";
prog_char _str_devid[]PROGMEM = "Dev. ID:";
prog_char _str_con [] PROGMEM = "LCD contrast:";
prog_char _str_lit [] PROGMEM = "LCD backlight:";
prog_char _str_dim [] PROGMEM = "LCD dimming:";
prog_char _str_rlp [] PROGMEM = "Relay pulse:";
prog_char _str_uwt [] PROGMEM = "Use weather?";
prog_char _str_ntp1[] PROGMEM = "NTP.ip1:";
prog_char _str_ntp2[] PROGMEM = "NTP.ip2:";
prog_char _str_ntp3[] PROGMEM = "NTP.ip3:";
prog_char _str_ntp4[] PROGMEM = "NTP.ip4:";
prog_char _str_log [] PROGMEM = "Enable logging?";
prog_char _str_reset[] PROGMEM = "Reset all?";

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
  {80,  255, _str_hp0,  _json_hp0},   // this and next byte define http port number
  {0,   255, _str_hp1,  _json_hp1},
  {OS_HW_VERSION, 0, _str_hwv, _json_hwv},
  {0,   MAX_EXT_BOARDS, _str_ext, _json_ext}, // number of extension board. 0: no extension boards
  {1,   1,   _str_seq,  _json_seq},   // reqired
  {128, 247, _str_sdt,  _json_sdt},   // station delay time (-59 minutes to 59 minutes).
  {0,   8,   _str_mas,  _json_mas},   // index of master station. 0: no master station
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
  {15,   255, _str_dim,  _json_dim},   // lcd dimming
  {0,   200, _str_rlp,  _json_rlp},   // relay pulse
  {0,   5,   _str_uwt,  _json_uwt}, 
  {204, 255, _str_ntp1, _json_ntp1},  // this and the next three bytes define the ntp server ip
  {9,   255, _str_ntp2, _json_ntp2}, 
  {54,  255, _str_ntp3, _json_ntp3},
  {119, 255, _str_ntp4, _json_ntp4},
  {1,   1,   _str_log,  _json_log},   // enable logging: 0: disable; 1: enable.   
  {0,   1,   _str_reset,_json_reset}
};

// Weekday display strings
prog_char str_day0[] PROGMEM = "Mon";
prog_char str_day1[] PROGMEM = "Tue";
prog_char str_day2[] PROGMEM = "Wed";
prog_char str_day3[] PROGMEM = "Thu";
prog_char str_day4[] PROGMEM = "Fri";
prog_char str_day5[] PROGMEM = "Sat";
prog_char str_day6[] PROGMEM = "Sun";

PGM_P days_str[7] PROGMEM = {
  str_day0,
  str_day1,
  str_day2,
  str_day3,
  str_day4,
  str_day5,
  str_day6
};

// ====== Ethernet defines ======
//static byte mymac[] = { 0x00,0x69,0x69,0x2D,0x31,0x00 }; // mac address

// ===============
// Setup Functions
// ===============

#define MAC_CTRL_ID 0x50

bool OpenSprinkler::read_hardware_mac() {
  uint8_t ret;
  Wire.beginTransmission(MAC_CTRL_ID); // Begin talking to EEPROM
  Wire.write((uint8_t)(0x00));
  ret = Wire.endTransmission();
  if (ret)  return false;

  Wire.beginTransmission(MAC_CTRL_ID); // Begin talking to EEPROM
  Wire.write(0xFA); // The address of the register we want
  Wire.endTransmission(); // Send the data
  Wire.requestFrom(MAC_CTRL_ID, 6); // Request 6 bytes from the EEPROM
  while (!Wire.available()); // Wait for the response
  for (ret=0;ret<6;ret++) {
    tmp_buffer[ret] = Wire.read();
  }
  return true;
}

// Arduino software reset function
void(* resetFunc) (void) = 0;

// Initialize network with the given mac address and http port
byte OpenSprinkler::start_network() {

  lcd_print_line_clear_pgm(PSTR("Connecting..."), 1);

  network_lasttime = now();
  dhcpnew_lasttime = network_lasttime;

  // new for 2.2: read EEPROM for hardware MAC
  if(!read_hardware_mac())
  {
    // if no hardware MAC exists, set software MAC
    tmp_buffer[0] = 0x00;
    tmp_buffer[1] = 0x69;
    tmp_buffer[2] = 0x69;
    tmp_buffer[3] = 0x2D;
    tmp_buffer[4] = 0x31;
    tmp_buffer[5] = options[OPTION_DEVICE_ID].value;
  }
        
  if(!ether.begin(ETHER_BUFFER_SIZE, (uint8_t*)tmp_buffer, PIN_ETHER_CS))  return 0;
  // calculate http port number
  ether.hisport = (int)(options[OPTION_HTTPPORT_1].value<<8) + (int)options[OPTION_HTTPPORT_0].value;

  if (options[OPTION_USE_DHCP].value) {
    // register with domain name "OpenSprinkler-xx" where xx is the last byte of the MAC address
    if (!ether.dhcpSetup()) return 0;
  } else {
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

// Reboot controller
void OpenSprinkler::reboot() {
  resetFunc();
}

// OpenSprinkler init function
void OpenSprinkler::begin() {

  // shift register setup
  pinMode(PIN_SR_OE, OUTPUT);
  // pull shift register OE high to disable output
  digitalWrite(PIN_SR_OE, HIGH);
  pinMode(PIN_SR_LATCH, OUTPUT);
  digitalWrite(PIN_SR_LATCH, HIGH);

  pinMode(PIN_SR_CLOCK, OUTPUT);
  pinMode(PIN_SR_DATA,  OUTPUT);
  
	// Reset all stations
  clear_all_station_bits();
  apply_all_station_bits();
  
  // pull shift register OE low to enable output
  digitalWrite(PIN_SR_OE, LOW);

  // set sd cs pin high to release SD
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);
  
  // set PWM frequency for adjustable LCD backlight and contrast 
  TCCR1B = 0x01;
  // turn on LCD backlight and contrast
  pinMode(PIN_LCD_BACKLIGHT, OUTPUT);
  pinMode(PIN_LCD_CONTRAST, OUTPUT);
  analogWrite(PIN_LCD_CONTRAST, options[OPTION_LCD_CONTRAST].value);
  analogWrite(PIN_LCD_BACKLIGHT, 255-options[OPTION_LCD_BACKLIGHT].value); 

  // turn on lcd
  lcd.init(1, PIN_LCD_RS, 255, PIN_LCD_EN, PIN_LCD_D4, PIN_LCD_D5, PIN_LCD_D6, PIN_LCD_D7, 0,0,0,0);
  lcd.begin(16, 2);
   
  // Rain sensor port set up
  pinMode(PIN_RAINSENSOR, INPUT);
  digitalWrite(PIN_RAINSENSOR, HIGH); // enabled internal pullup

  // Init I2C
  Wire.begin();
  
  // Default controller status variables
  // AVR assigns 0 to static variables by default
  // so only need to initialize non-zero ones
  status.enabled = 1;
  //status.seq = 1;
  
  old_status = status;
  
  nvdata.sunrise_time = 360;  // 6:00am default
  nvdata.sunset_time = 1080;  // 6:00pm default
  
  nboards = 1;
  nstations = 8;
  
  // define lcd custom icons
  byte lcd_wifi_char[8] = {
    B00000,
    B10100,
    B01000,
    B10101,
    B00001,
    B00101,
    B00101,
    B10101
  };
  byte lcd_sd_char[8] = {
    B00000,
    B00000,
    B11111,
    B10001,
    B11111,
    B10001,
    B10011,
    B11110
  };
  byte lcd_rain_char[8] = {
    B00000,
    B00000,
    B00110,
    B01001,
    B11111,
    B00000,
    B10101,
    B10101
  };  
  byte lcd_connect_char[8] = {
    B00000,
    B00000,
    B00111,
    B00011,
    B00101,
    B01000,
    B10000,
    B00000
  };
  lcd.createChar(1, lcd_wifi_char);  
  lcd_wifi_char[1]=0;
  lcd_wifi_char[2]=0;
  lcd_wifi_char[3]=1;    
  lcd.createChar(0, lcd_wifi_char);  
  lcd.createChar(2, lcd_sd_char);
  lcd.createChar(3, lcd_rain_char);
  lcd.createChar(4, lcd_connect_char);
  
  // set rf data pin
  pinMode(PIN_RF_DATA, OUTPUT);
  digitalWrite(PIN_RF_DATA, LOW);
  
  pinMode(PIN_RELAY, OUTPUT);
  digitalWrite(PIN_RELAY, LOW);
  
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
}

unsigned long eeprom_hex2ulong(unsigned char* addr, byte len) {
  char c;
  unsigned long v = 0;
  for(byte i=0;i<len;i++) {
    c = eeprom_read_byte(addr++);
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

// Get station name from eeprom and parse into RF code
uint16_t OpenSprinkler::get_station_name_rf(byte sid, unsigned long* on, unsigned long *off) {
  unsigned char* start = (unsigned char *)(ADDR_EEPROM_STN_NAMES) + (int)sid * STATION_NAME_SIZE;
  unsigned long v;
  v = eeprom_hex2ulong(start, 6);
  if (!v) return 0;
  if (on) *on = v;
  v = eeprom_hex2ulong(start+6, 6);
  if (!v) return 0;
  if (off) *off = v;
  v = eeprom_hex2ulong(start+12, 4);
  if (!v) return 0;
  return v;
}

// Get station name from eeprom
void OpenSprinkler::get_station_name(byte sid, char tmp[]) {
  int i=0;
  int start = ADDR_EEPROM_STN_NAMES + (int)sid * STATION_NAME_SIZE;
  tmp[STATION_NAME_SIZE]=0;
  while(1) {
    tmp[i] = eeprom_read_byte((unsigned char *)(start+i));
    if (tmp[i]==0 || i==(STATION_NAME_SIZE-1)) break;
    i++;
  }
  return;
}

// Set station name to eeprom
void OpenSprinkler::set_station_name(byte sid, char tmp[]) {
  int i=0;
  int start = ADDR_EEPROM_STN_NAMES + (int)sid * STATION_NAME_SIZE;
  tmp[STATION_NAME_SIZE]=0;
  while(1) {
    eeprom_write_byte((unsigned char *)(start+i), tmp[i]);
    if (tmp[i]==0 || i==(STATION_NAME_SIZE-1)) break;
    i++;
  }
  return;  
}

// Save station attribute bits to EEPROM
void OpenSprinkler::station_attrib_bits_save(int addr, byte bits[]) {
  eeprom_write_block(bits, (unsigned char *)addr, MAX_EXT_BOARDS+1);
}

void OpenSprinkler::station_attrib_bits_load(int addr, byte bits[]) {
  eeprom_read_block(bits, (unsigned char *)addr, MAX_EXT_BOARDS+1);
}


// ==================
// String Functions
// ==================
void OpenSprinkler::eeprom_string_set(int start_addr, char* buf) {
  byte i=0;
  for (; (*buf)!=0; buf++, i++) {
    eeprom_write_byte((unsigned char*)(start_addr+i), *(buf));
  }
  eeprom_write_byte((unsigned char*)(start_addr+i), 0);  
}

void OpenSprinkler::eeprom_string_get(int start_addr, char *buf) {
  byte c;
  byte i = 0;
  do {
    c = eeprom_read_byte((unsigned char*)(start_addr+i));
    //if (c==' ') c='+';
    *(buf++) = c;
    i ++;
  } while (c != 0);
}


// verify if a string matches password
byte OpenSprinkler::password_verify(char *pw) { 
  unsigned char *addr = (unsigned char*)ADDR_EEPROM_PASSWORD;
  byte c1, c2;
  while(1) {
    c1 = eeprom_read_byte(addr++);
    c2 = *pw++;
    if (c1==0 || c2==0)
      break;
    if (c1!=c2) {
      return 0;
    }
  }
  return (c1==c2) ? 1 : 0;
}

// compare a string to eeprom
byte strcmp_to_eeprom(const char* src, int _addr) {
  byte i=0;
  byte c1, c2;
  unsigned char *addr = (unsigned char*)_addr;
  while(1) {
    c1 = eeprom_read_byte(addr++);
    c2 = *src++;
    if (c1==0 || c2==0)
      break;      
    if (c1!=c2)  return 1;
  }
  return (c1==c2) ? 0 : 1;
}

// ==================
// Schedule Functions
// ==================

// Index of today's weekday (Monday is 0)
byte OpenSprinkler::weekday_today() {
  //return ((byte)weekday()+5)%7; // Time::weekday() assumes Sunday is 1
  tmElements_t tm;
  RTC.read(tm);
  return (tm.Wday+5)%7;
}

// Set station bit
void OpenSprinkler::set_station_bit(byte sid, byte value) {
  byte bid = (sid>>3);  // board index
  byte s = sid&0x07;     // station bit index
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

// Apply all station bits
// !!! This will activate/deactivate valves !!!
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
      digitalWrite(PIN_SR_DATA, (sbits & ((byte)1<<(7-s))) ? HIGH : LOW );
      digitalWrite(PIN_SR_CLOCK, HIGH);          
    }
  }
  digitalWrite(PIN_SR_LATCH, HIGH);
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

void transmit_rfbit(unsigned long lenH, unsigned long lenL) {
  PORT_RF |= (1<<PINX_RF);
  delayMicroseconds(lenH);
  PORT_RF &=~(1<<PINX_RF);
  delayMicroseconds(lenL);
}

void send_rfsignal(unsigned long code, unsigned long len) {
  unsigned long len3 = len * 3;
  unsigned long len31 = len * 31; 
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
  unsigned long on, off;
  uint16_t length = get_station_name_rf(sid, &on, &off);
  length = (length>>1)+(length>>2); // screw
  DEBUG_PRINTLN(on);
  DEBUG_PRINTLN(off);
  DEBUG_PRINTLN(length);
  send_rfsignal(turnon ? on : off, length);
}

// =================
// Options Functions
// =================

void OpenSprinkler::options_setup() {

  // add 0.5 second delay to allow EEPROM to stablize
  delay(500);
  
  // check reset condition: either firmware version has changed, or reset flag is up
  byte curr_ver = eeprom_read_byte((unsigned char*)(ADDR_EEPROM_OPTIONS+OPTION_FW_VERSION));
  //if (curr_ver<100) curr_ver = curr_ver*10; // adding a default 0 if version number is the old type
  if (curr_ver != OS_FW_VERSION || eeprom_read_byte((unsigned char*)(ADDR_EEPROM_OPTIONS+OPTION_RESET))==0xAA)  {

    lcd_print_line_clear_pgm(PSTR("Resetting Device"), 0);
    lcd_print_line_clear_pgm(PSTR("Please Wait..."), 1);  
    
    // ======== Reset EEPROM data ========
    int i, sn;

    // 0. wipe out eeprom
    for(i=0;i<INT_EEPROM_SIZE;i++) {
      eeprom_write_byte((unsigned char*)i, 0);
    }
    
    // 1. write program data default parameters
    
    // 2. write non-volatile controller status
    nvdata_save();
    
    // 3. write string parameters 
    eeprom_string_set(ADDR_EEPROM_PASSWORD, DEFAULT_PASSWORD);
    eeprom_string_set(ADDR_EEPROM_LOCATION, DEFAULT_LOCATION);
    eeprom_string_set(ADDR_EEPROM_SCRIPTURL, DEFAULT_JAVASCRIPT_URL);
    eeprom_string_set(ADDR_EEPROM_WEATHER_KEY, DEFAULT_WEATHER_KEY);

    // 4. reset station names, default Sxx
    for(i=ADDR_EEPROM_STN_NAMES, sn=1; i<ADDR_EEPROM_MAS_OP; i+=STATION_NAME_SIZE, sn++) {
      eeprom_write_byte((unsigned char *)i    ,'S');
      eeprom_write_byte((unsigned char *)(i+1),'0'+(sn/10));
      eeprom_write_byte((unsigned char *)(i+2),'0'+(sn%10)); 
    }
    
    // 5. reset station attributes
    // since we wiped out eeprom, only non-zero attributes need to be initialized
    for(i=ADDR_EEPROM_MAS_OP; i<ADDR_EEPROM_MAS_OP+(MAX_EXT_BOARDS+1); i++) {
      // default master operation bits on
      eeprom_write_byte((unsigned char *)i, 0xff);
    }

    for(i=ADDR_EEPROM_STNSEQ; i<ADDR_EEPROM_STNSEQ+(MAX_EXT_BOARDS+1); i++) {
      // default master operation bits on
      eeprom_write_byte((unsigned char *)i, 0xff);
    }

    // 6. write options
    options_save(); // write default option values
    //======== END OF EEPROM RESET CODE ========
    
    // restart after resetting EEPROM.
    delay(500);
    reboot();
  } 
  else {
    // load ram parameters from eeprom
    // 1. load options
    options_load();
    // 2. station attributes
    station_attrib_bits_load(ADDR_EEPROM_MAS_OP, masop_bits);
    station_attrib_bits_load(ADDR_EEPROM_IGNRAIN, ignrain_bits);
    station_attrib_bits_load(ADDR_EEPROM_ACTRELAY, actrelay_bits);
    station_attrib_bits_load(ADDR_EEPROM_STNDISABLE, stndis_bits);
    station_attrib_bits_load(ADDR_EEPROM_STNSEQ, stnseq_bits);    
    // set RF station flags
    update_rfstation_bits();

    // 3. load non-volatile controller data
    nvdata_load();
  }

	byte button = button_read(BUTTON_WAIT_NONE);
	
	switch(button & BUTTON_MASK) {
	
  case BUTTON_1:
  	// if BUTTON_2 is pressed during startup, go to 'reset all options'
		ui_set_options(OPTION_RESET);
		if (options[OPTION_RESET].value) {
			resetFunc();
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
      resetFunc(); 
    }
    break;
  }
  // turn on LCD backlight and contrast
  pinMode(PIN_LCD_BACKLIGHT, OUTPUT);
  pinMode(PIN_LCD_CONTRAST, OUTPUT);
  analogWrite(PIN_LCD_CONTRAST, options[OPTION_LCD_CONTRAST].value);
  analogWrite(PIN_LCD_BACKLIGHT, 255-options[OPTION_LCD_BACKLIGHT].value); 
}

// Load non-volatile controller status data from internal eeprom
void OpenSprinkler::nvdata_load() {
  eeprom_read_block(&nvdata, (unsigned char*)ADDR_EEPROM_NVCONDATA, sizeof(NVConData));
  old_status = status;
}

// Save non-volatile controller status data to internal eeprom
void OpenSprinkler::nvdata_save() {
  eeprom_write_block(&nvdata, (unsigned char*)ADDR_EEPROM_NVCONDATA, sizeof(NVConData));
}

// Load options from internal eeprom
void OpenSprinkler::options_load() {
  for (byte i=0; i<NUM_OPTIONS; i++) {
    options[i].value = eeprom_read_byte((unsigned char *)(ADDR_EEPROM_OPTIONS + i));
  }
  nboards = options[OPTION_EXT_BOARDS].value+1;
  nstations = nboards * 8;
  status.enabled = options[OPTION_DEVICE_ENABLE].value;
}

// Save options to internal eeprom
void OpenSprinkler::options_save() {
  // save options in reverse order so version number is written the last
  for (int i=NUM_OPTIONS-1; i>=0; i--) {
    eeprom_write_byte((unsigned char *) (ADDR_EEPROM_OPTIONS + i), options[i].value);
  }
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

byte OpenSprinkler::detect_exp() {
  unsigned int v = analogRead(PIN_EXP_SENSE);
  // OpenSprinkler uses voltage divider to detect expansion boards
  // Master controller has a 1.5K pull-up;
  // each expansion board (8 stations) has 10K pull-down connected in parallel;
  // so the exact ADC value for n expansion boards is:
  //    ADC = 1024 * 10 / (10 + 1.5 * n)
  // For  0,   1,   2,   3,   4,   5 expansion boards, the ADC values are:
  //   1024, 890, 787, 706, 640, 585
  // Actual threshold is taken as the midpoint between, to account for errors
  byte n = 255;
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
}

void OpenSprinkler::rainsensor_status() {
  // options[OPTION_RS_TYPE]: 0 if normally closed, 1 if normally open
  status.rain_sensed = (digitalRead(PIN_RAINSENSOR) == options[OPTION_RAINSENSOR_TYPE].value ? 0 : 1);
}

// =============
// LCD Functions
// =============

// Print a program memory string
void OpenSprinkler::lcd_print_pgm(PGM_P PROGMEM str) {
  uint8_t c;
  while((c=pgm_read_byte(str++))!= '\0') {
    lcd.print((char)c);
  }
}

// Print a program memory string to a given line with clearing
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

// Print time to a given line
void OpenSprinkler::lcd_print_time(byte line)
{
  time_t t=now();
  lcd.setCursor(0, line);
  lcd_print_2digit(hour(t));
  lcd_print_pgm(PSTR(":"));
  lcd_print_2digit(minute(t));
  lcd_print_pgm(PSTR("  "));
  lcd_print_pgm((PGM_P)pgm_read_word(&days_str[weekday_today()]));
  lcd_print_pgm(PSTR(" "));
  lcd_print_2digit(month(t));
  lcd_print_pgm(PSTR("-"));
  lcd_print_2digit(day(t));
}

// print ip address
void OpenSprinkler::lcd_print_ip(const byte *ip, byte lineno) {
  lcd.setCursor(0, lineno);
  for (byte i=0; i<3; i++) {
    lcd.print((int)ip[i]); 
    lcd_print_pgm(PSTR("."));
  }   
  lcd.print((int)ip[3]);
}

// print mac address
void OpenSprinkler::lcd_print_mac(const byte *mac) {
  lcd.setCursor(0, 0);
  lcd_print_pgm(PSTR("MAC:"));
  for(byte i=0; i<4; i++) {
    lcd.print((mac[i]>>4), HEX);
    lcd.print((mac[i]&0x0F), HEX);    
    lcd_print_pgm(PSTR("-"));
  }
  lcd.setCursor(0, 1);
  for(byte i=4; i<6; i++) {
    if(i!=4) lcd_print_pgm(PSTR("-"));
    lcd.print((mac[i]>>4), HEX);
    lcd.print((mac[i]&0x0F), HEX);    
  }
}

// Print station bits
void OpenSprinkler::lcd_print_station(byte line, char c) {
  //lcd_print_line_clear_pgm(PSTR(""), line);
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
	    if (status.display_board == 0 &&(s+1) == options[OPTION_MASTER_STATION].value) {
	      lcd.print((bitvalue&1) ? (char)c : 'M'); // print master station
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

// Print a version number
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

// Print an option value
void OpenSprinkler::lcd_print_option(int i) {
  lcd_print_line_clear_pgm(options[i].str, 0);  
  lcd_print_line_clear_pgm(PSTR(""), 1);
  lcd.setCursor(0, 1);
  //if(options[i].flag&OPFLAG_SETUP_EDIT) lcd.blink();
  //else lcd.noBlink();
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
    lcd_print_pgm(PSTR("+"));
    lcd.print((int)options[i].value);
    break;
  case OPTION_MASTER_OFF_ADJ:
    if(options[i].value>=60)  lcd_print_pgm(PSTR("+"));
    lcd.print((int)options[i].value-60);
    break;
  case OPTION_HTTPPORT_0:
    lcd.print((int)(options[i+1].value<<8)+options[i].value);
    break;
  case OPTION_RELAY_PULSE:
    lcd.print((int)options[i].value*10);
    break;
  case OPTION_STATION_DELAY_TIME:
    lcd.print(water_time_decode_signed(options[i].value));
    break;
  case OPTION_LCD_CONTRAST:
    analogWrite(PIN_LCD_CONTRAST, options[i].value);
    lcd.print((int)options[i].value);
    break;
  case OPTION_LCD_BACKLIGHT:
    analogWrite(PIN_LCD_BACKLIGHT, 255-options[i].value);
    lcd.print((int)options[i].value);
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
  else if (i==OPTION_MASTER_ON_ADJ || i==OPTION_MASTER_OFF_ADJ)
    lcd_print_pgm(PSTR(" sec"));
  else if (i==OPTION_STATION_DELAY_TIME)
    lcd_print_pgm(PSTR(" sec"));
  else if (i==OPTION_RELAY_PULSE)
    lcd_print_pgm(PSTR(" ms"));
}


// ================
// Button Functions
// ================

// Wait for button
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

// Read button and returns button value 'OR'ed with flag bits
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

  /* set flags in return value */
  byte ret = curr;
  if (!(old&BUTTON_MASK) && (curr&BUTTON_MASK))
    ret |= BUTTON_FLAG_DOWN;
  if ((old&BUTTON_MASK) && !(curr&BUTTON_MASK))
    ret |= BUTTON_FLAG_UP;

  old = curr;
  return ret;
}


// user interface for setting options during startup
void OpenSprinkler::ui_set_options(int oid)
{
  boolean finished = false;
  byte button;
  int i=oid;

  //lcd_print_option(i);
  while(!finished) {
    button = button_read(BUTTON_WAIT_HOLD);

    switch (button & BUTTON_MASK) {
    case BUTTON_1:
      if (i==OPTION_FW_VERSION || i==OPTION_HW_VERSION ||
          i==OPTION_HTTPPORT_0 || i==OPTION_HTTPPORT_1) break; // ignore non-editable options
      if (options[i].max != options[i].value) options[i].value ++;
      break;

    case BUTTON_2:
      if (i==OPTION_FW_VERSION || i==OPTION_HW_VERSION ||
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
        else  {
          i = (i+1) % NUM_OPTIONS;
        }
        if(pgm_read_byte(options[i].json_str)=='_') i++;
      }
      break;
    }

    if (button != BUTTON_NONE) {
      lcd_print_option(i);
    }
  }
  lcd.noBlink();
}


// ================================================
// ====== Data Encoding / Decoding Functions ======
// ================================================
// encode a 16-bit unsigned water time to 8-bit byte
/* encoding scheme:
   byte value : water time
     [0.. 59]  : [0..59]  (seconds)
    [60..238]  : [1..179] (minutes), or 60 to 10740 seconds
   [239..254]  : [3..18]  (hours),   or 10800 to 64800 seconds
*/
byte water_time_encode(uint16_t i) {
  if (i<60) {
    return byte(i);
  } else if (i<10800) {
    return byte(i/60+59);
  } else if (i<64800) {
    return byte(i/3600+236);
  } else {
    return 254;
  }
}

// encode a 16-bit signed water time to 8-bit unsigned byte (leading bit is the sign)
byte water_time_encode_signed(int16_t i) {
  DEBUG_PRINTLN(i);
  byte ret = water_time_encode(i>=0?i : -i);
  DEBUG_PRINTLN(ret);
  return ((i>=0) ? (128+ret) : (128-ret));
}

// decode a 8-bit unsigned byte (leading bit is the sign) into a 16-bit signed water time
int16_t water_time_decode_signed(byte i) {
  int16_t ret = i;
  ret -= 128;
  ret = water_time_decode(ret>=0?ret:-ret);
  return (i>=128 ? ret : -ret);
}

// decode a 8-bit byte to a 16-bit unsigned water time
uint16_t water_time_decode(byte i) {
  uint16_t ii = i;
  if (i<60) {
    return ii;
  } else if (i<239) {
    return (ii-59)*60;
  } else {
    return (ii-236)*3600;
  }  
}


