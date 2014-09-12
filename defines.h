// Arduino library code for OpenSprinkler Generation 2

/* Macro definitions and Arduino pin assignments
   Creative Commons Attribution-ShareAlike 3.0 license
   Sep 2014 @ Rayshobby.net
*/

#ifndef _Defines_h
#define _Defines_h

// =================================================
// ====== Firmware Version and Maximal Values ======
// =================================================
#define OS_FW_VERSION  210 // firmware version (210 means 2.1.0 etc)
                            // if this number is different from stored in EEPROM,
                            // an EEPROM reset will be automatically triggered

#define MAX_EXT_BOARDS   5  // maximum number of exp. boards (each expands 8 stations)
                            // total number of stations: (1+MAX_EXT_BOARDS) * 8

#define MAX_NUM_STATIONS  ((1+MAX_EXT_BOARDS)*8)
#define STATION_NAME_SIZE 16 // size of each station name, default is 16 letters max

// =====================================
// ====== Internal EEPROM Defines ======
// =====================================

#define INT_EEPROM_SIZE         2048    // ATmega644 eeprom size

/* EEPROM structure:
 * |              |     |---STRING PARAMETERS---|               |--STATION ATTRIBUTES---|            |
 * | PROGRAM_DATA | CON | PWD | LOC | URL | KEY | STATION_NAMES | MAS | IGR | ACT | DIS | OPTIONS... |
 * |   (1008)     |(16) |(24) |(32) |(72) |(32) | (6*8*16)=768  | (6) | (6) | (6) | (6) |    (46)    |
 * |              |     |     |     |     |     |               |     |     |     |     |            |
 * 0            1008  1024   1048  1080  1152  1216            1984  1990  1996  2002 2008          2048
 */
/* program data is now stored at the beginning of the EEPROM
 * so that they can be preserved across firmware upgrades,
 * unless if program data structure is changed */
#define ADDR_EEPROM_PROGRAMS         0  // program starting address, 1012 bytes max
#define MAX_NUM_PROGRAM_BYTES     1012
#define ADDR_EEPROM_NVCONDATA     1012  // non-volatile controller data, 12 bytes max
#define ADDR_EEPROM_PASSWORD      1024	// user password, 24 bytes max
#define ADDR_EEPROM_LOCATION      1048  // location string, 32 bytes max
#define ADDR_EEPROM_SCRIPTURL     1080	// javascript url, 72 bytes max
#define ADDR_EEPROM_WEATHER_KEY   1152  // weather api key, 32 bytes max
#define ADDR_EEPROM_STN_NAMES     1216  // station names
#define ADDR_EEPROM_MAS_OP        (ADDR_EEPROM_STN_NAMES+MAX_NUM_STATIONS*STATION_NAME_SIZE) // master op bits
#define ADDR_EEPROM_IGNRAIN       (ADDR_EEPROM_MAS_OP+(MAX_EXT_BOARDS+1))  // ignore rain bits 
#define ADDR_EEPROM_ACTRELAY      (ADDR_EEPROM_IGNRAIN+(MAX_EXT_BOARDS+1)) // activate relay bits
#define ADDR_EEPROM_STNDISABLE    (ADDR_EEPROM_ACTRELAY+(MAX_EXT_BOARDS+1))// station disable bits
#define ADDR_EEPROM_OPTIONS       (ADDR_EEPROM_STNDISABLE+(MAX_EXT_BOARDS+1))  // options

/* String Parameters Default Values */
#define DEFAULT_PASSWORD        "opendoor"
#define DEFAULT_LOCATION        "Boston,MA"
#define DEFAULT_WEATHER_KEY     ""
#define DEFAULT_JAVASCRIPT_URL  "http://rayshobby.net/scripts/sprinklers/js"
#define WEATHER_SCRIPT_HOST     "www.rayshobby.net"

// macro define of each option
// See OpenSprinkler.cpp for details on each option
typedef enum {
  OPTION_FW_VERSION = 0,
  OPTION_TIMEZONE,
  OPTION_USE_NTP,
  OPTION_USE_DHCP,
  OPTION_STATIC_IP1,
  OPTION_STATIC_IP2,
  OPTION_STATIC_IP3,
  OPTION_STATIC_IP4,
  OPTION_GATEWAY_IP1,
  OPTION_GATEWAY_IP2,
  OPTION_GATEWAY_IP3,
  OPTION_GATEWAY_IP4,
  OPTION_HTTPPORT_0,
  OPTION_HTTPPORT_1,
  OPTION_NETFAIL_RECONNECT,
  OPTION_EXT_BOARDS,
  OPTION_SEQUENTIAL,
  OPTION_STATION_DELAY_TIME,
  OPTION_MASTER_STATION,
  OPTION_MASTER_ON_ADJ,
  OPTION_MASTER_OFF_ADJ,
  OPTION_USE_RAINSENSOR,
  OPTION_RAINSENSOR_TYPE,
  OPTION_WATER_PERCENTAGE,
  OPTION_DEVICE_ENABLE,
  OPTION_IGNORE_PASSWORD,
  OPTION_DEVICE_ID,
  OPTION_LCD_CONTRAST,
  OPTION_LCD_BACKLIGHT,
  OPTION_LCD_DIMMING,
  OPTION_RELAY_PULSE,
  OPTION_USE_WEATHER,
  OPTION_NTP_IP1,
  OPTION_NTP_IP2,
  OPTION_NTP_IP3,
  OPTION_NTP_IP4,
  OPTION_RESET,
  NUM_OPTIONS	// total number of options
} OS_OPTION_t;

// Log Data Type
#define LOGDATA_STATION    0x00
#define LOGDATA_RAINSENSE  0x01
#define LOGDATA_RAINDELAY  0x02

// =====================================
// ====== Arduino Pin Assignments ======
// =====================================

// ====== Define hardware version here ======
#define OS_HW_VERSION 20

#ifndef OS_HW_VERSION
#error "==This error is intentional==: you must define OS_HW_VERSION in arduino-xxxx/libraries/OpenSprinklerGen2/defines.h"
#endif

#if OS_HW_VERSION == 20 || OS_HW_VERSION == 21

  #define PIN_BUTTON_1      31    // button 1
  #define PIN_BUTTON_2      30    // button 2
  #define PIN_BUTTON_3      29    // button 3 
  #define PIN_RF_DATA       28    // RF data pin 
  #define PIN_SR_LATCH       3    // shift register latch pin
  #define PIN_SR_DATA       21    // shift register data pin
  #define PIN_SR_CLOCK      22    // shift register clock pin
  #define PIN_SR_OE          1    // shift register output enable pin
  #define PIN_LCD_RS        19    // LCD rs pin
  #define PIN_LCD_EN        18    // LCD enable pin
  #define PIN_LCD_D4        20    // LCD d4 pin
  #define PIN_LCD_D5        21    // LCD d5 pin
  #define PIN_LCD_D6        22    // LCD d6 pin
  #define PIN_LCD_D7        23    // LCD d7 pin
  #define PIN_LCD_BACKLIGHT 12    // LCD backlight pin
  #define PIN_LCD_CONTRAST  13    // LCD contrast pin
  #define PIN_ETHER_CS       4    // Ethernet controller chip select pin
  #define PIN_SD_CS          0    // SD card chip select pin
  #define PIN_RAINSENSOR    11    // rain sensor is connected to pin D3
  #define PIN_RELAY         14    // mini relay is connected to pin D14
  #define PIN_EXP_SENSE      4    // expansion board sensing pin (A4)
#endif 

// ====== Button Defines ======
#define BUTTON_1            0x01
#define BUTTON_2            0x02
#define BUTTON_3            0x04

// button status values
#define BUTTON_NONE         0x00  // no button pressed
#define BUTTON_MASK         0x0F  // button status mask
#define BUTTON_FLAG_HOLD    0x80  // long hold flag
#define BUTTON_FLAG_DOWN    0x40  // down flag
#define BUTTON_FLAG_UP      0x20  // up flag

// button timing values
#define BUTTON_DELAY_MS        1  // short delay (milliseconds)
#define BUTTON_HOLD_MS       800  // long hold expiration time (milliseconds)
#define BUTTON_IDLE_TIMEOUT    8  // timeout if no button is pressed within certain number of seconds

// button mode values
#define BUTTON_WAIT_NONE       0  // do not wait, return value immediately
#define BUTTON_WAIT_RELEASE    1  // wait until button is release
#define BUTTON_WAIT_HOLD       2  // wait until button hold time expires

// ====== Timing Defines ======
#define DISPLAY_MSG_MS      2000  // message display time (milliseconds)

// ====== Ethernet Defines ======
#define ETHER_BUFFER_SIZE   900  // if buffer size is increased, you must check the total RAM consumption
                                 // otherwise it may cause the program to crash
#define TMP_BUFFER_SIZE     100  // scratch buffer size

#define 	wdt_reset()   __asm__ __volatile__ ("wdr")

//#define SERIAL_DEBUG

#ifdef SERIAL_DEBUG

#define DEBUG_BEGIN(x)   Serial.begin(x)
#define DEBUG_PRINT(x)   Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)

#else

#define DEBUG_BEGIN(x)   {}
#define DEBUG_PRINT(x)   {}
#define DEBUG_PRINTLN(x) {}

#endif

#endif


