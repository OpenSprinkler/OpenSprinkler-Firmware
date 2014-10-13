// Arduino library code for OpenSprinkler Generation 2

/* OpenSprinkler Class Definition
   Creative Commons Attribution-ShareAlike 3.0 license
   Sep 2014 @ Rayshobby.net
*/

#ifndef _OpenSprinkler_h
#define _OpenSprinkler_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include <avr/eeprom.h>
#include "../Wire/Wire.h"

#include "LiquidCrystal.h"
#include "Time.h"
#include "DS1307RTC.h"
#include "EtherCard.h"
#include "defines.h"

// Option Data Structure
struct OptionStruct{
  byte value;     // each option is stored as a byte
  byte max;       // maximum value
  char* str;      // name string
  char* json_str; // json string
};

// Non-Volatile Controller Data
// these will be stored in EEPROM and RAM
struct NVConData {
  uint16_t sunrise_time;      // sunrise time (in minutes)
  uint16_t sunset_time;       // sunset time (in minutes)
  uint32_t rd_stop_time;      // rain delay stop time
};

// Volatile Controller Status Bits
// these will be stored in RAM only
struct ConStatus {
  byte enabled:1;           // operation enable (when set, controller operation is enabled)
  byte rain_delayed:1;      // rain delay bit (when set, rain delay is applied)
  byte rain_sensed:1;       // rain sensor bit (when set, it indicates that rain is detected)
  byte program_busy:1;      // HIGH means a program is being executed currently
  byte has_rtc:1;           // HIGH means the controller has a DS1307 RTC
  byte has_sd:1;            // HIGH means a microSD card is detected
  byte seq:1;               // HIGH means the controller is in sequential mode
  byte wt_received:1;       // HIGH means weather info has been received
  byte display_board:4;     // the board that is being displayed onto the lcd
  byte network_fails:4;     // number of network fails
  byte mas:8;               // master station index
}; 

class OpenSprinkler {
public:
  
  // ====== Data Members ======
  static LiquidCrystal lcd;
  static NVConData nvdata;
  static ConStatus status;
  static ConStatus old_status;
  static byte nboards, nstations;
  
  static OptionStruct options[];  // option values, max, name, and flag
    
  static char* days_str[];		// 3-letter name of each weekday
  static byte station_bits[]; // station activation bits. each byte corresponds to a board (8 stations)
                              // first byte-> master controller, second byte-> ext. board 1, and so on
  /* station attributes */
  static byte masop_bits[];   // station master operation bits. each byte corresponds to a board (8 stations)
  static byte ignrain_bits[]; // ignore rain bits. each byte corresponds to a board (8 stations)
  static byte actrelay_bits[];// activate relay bits. each byte corresponds to a board (8 stations)
  static byte stndis_bits[];  // station disable bits. each byte 
  static unsigned long rainsense_start_time;  // time (in seconds) when rain sensor is detected on
  static unsigned long raindelay_start_time;  // time (in seconds) when rain delay is started
  static unsigned long button_lasttime;
  static unsigned long ntpsync_lasttime;
  static unsigned long checkwt_lasttime;
  static unsigned long network_lasttime;
  static unsigned long dhcpnew_lasttime;
  
  // ====== Member Functions ======
  // -- Setup --
  static void reboot();   // reboot the microcontroller
  static void begin();    // initialization, must call this function before calling other functions
  static byte start_network();  // initialize network with the given mac and port
  //static void self_test(unsigned long ms);  // self-test function
  static void get_station_name(byte sid, char buf[]); // get station name
  static void set_station_name(byte sid, char buf[]); // set station name
  static void station_attrib_bits_save(int addr, byte bits[]); // save station attribute bits to eeprom
  static void station_attrib_bits_load(int addr, byte bits[]); // load station attribute bits from eeprom
  // -- Controller status
  static void nvdata_load();
  static void nvdata_save();
  // -- Options --
  static void options_setup();
  static void options_load();
  static void options_save();

  // -- Operation --
  static void enable();     // enable controller operation
  static void disable();    // disable controller operation, all stations will be closed immediately
  static void raindelay_start();  // start raindelay
  static void raindelay_stop(); // stop rain delay
  static void rainsensor_status(); // update rainsensor status
  static byte detect_exp();        // detect the number of expansion boards
  static byte weekday_today();  // returns index of today's weekday (Monday is 0) 
  // -- Station schedules --
  // Call functions below to set station bits
  // Then call apply_station_bits() to activate/deactivate valves
  static void set_station_bit(byte sid, byte value); // set station bit of one station (sid->station index, value->0/1)
  static void clear_all_station_bits(); // clear all station bits
  static void apply_all_station_bits(); // apply all station bits (activate/deactive values)

  // -- String functions --
  //static void password_set(char *pw);     // save password to eeprom
  static byte password_verify(char *pw);  // verify password
  static void eeprom_string_set(int start_addr, char* buf);
  static void eeprom_string_get(int start_addr, char* buf);
    
  // -- LCD functions --
  static void lcd_print_pgm(PGM_P PROGMEM str);           // print a program memory string
  static void lcd_print_line_clear_pgm(PGM_P PROGMEM str, byte line);
  static void lcd_print_time(byte line);                  // print current time
  static void lcd_print_ip(const byte *ip, int http_port);// print ip and port number
  static void lcd_print_station(byte line, char c);       // print station bits of the board selected by display_board
 
  // -- Button and UI functions --
  static byte button_read(byte waitmode); // Read button value. options for 'waitmodes' are:
                                          // BUTTON_WAIT_NONE, BUTTON_WAIT_RELEASE, BUTTON_WAIT_HOLD
                                          // return values are 'OR'ed with flags
                                          // check defines.h for details

  // -- UI functions --
  static void ui_set_options(int oid);    // ui for setting options (oid-> starting option index)

private:
  static void lcd_print_option(int i);  // print an option to the lcd
  static void lcd_print_2digit(int v);  // print a integer in 2 digits
  static byte button_read_busy(byte pin_butt, byte waitmode, byte butt, byte is_holding);
};

byte strcmp_to_eeprom(const char* src, int addr);
byte water_time_encode(uint16_t i);
uint16_t water_time_decode(byte i);
#endif
