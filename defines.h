/* OpenSprinkler AVR/RPI/BBB Library
 * Copyright (C) 2014 by Ray Wang (ray@opensprinkler.com)
 *
 * OpenSprinkler macro defines and hardware pin assignments
 * Sep 2014 @ Rayshobby.net
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

#ifndef _DEFINES_H
#define _DEFINES_H

/** Firmware version, hardware version, and maximal values */
#define OS_FW_VERSION  213  // 213 -> 2.1.3
                            // if this number is different from the one stored in non-volatile memory
                            // a device reset will be automatically triggered


#define OS_HW_VERSION_BASE   0x00
#define OSPI_HW_VERSION_BASE 0x40
#define OSBO_HW_VERSION_BASE 0x80

#define MAX_EXT_BOARDS   5  // maximum number of exp. boards (each expands 8 stations)

#define MAX_NUM_STATIONS  ((1+MAX_EXT_BOARDS)*8)  // maximum number of stations

/** Non-volatile memory (NVM) defines */
#if defined(ARDUINO)

/** NVM structure:
  * |     |              |     |---STRING PARAMETERS---|               |----STATION ATTRIBUTES-----  |          |
  * | UID | PROGRAM_DATA | CON | PWD | LOC | URL | KEY | STATION_NAMES | MAS | IGR | ACT | DIS | SEQ | OPTIONS  |
  * | (8) |    (996)     | (8) |(32) |(48) |(64) |(32) | (6*8*16)=768  | (6) | (6) | (6) | (6) | (6) |  (62)    |
  * |     |              |     |     |     |     |     |               |     |     |     |     |     |          |
  * 0     8            1004  1012   1044  1092  1156  1188            1956  1962  1968  1974 1980   1986       2048
  */

  #define NVM_SIZE            2048  // ATmega644 eeprom size
  #define STATION_NAME_SIZE   16    // maximum number of characters in each station name
  
  #define MAX_UIDDATA         8     // unique ID, 8 bytes max
  #define MAX_PROGRAMDATA     996   // program data, 996 bytes max
  #define MAX_NVCONDATA       8     // non-volatile controller data, 8 bytes max
  #define MAX_USER_PASSWORD   32    // user password, 32 bytes max
  #define MAX_LOCATION        48    // location string, 48 bytes max
  #define MAX_SCRIPTURL       64    // javascript url, 64 bytes max
  #define MAX_WEATHER_KEY     32    // weather api key, 32 bytes max
  
#else // NVM defines for RPI/BBB

  #define NVM_FILENAME        "nvm.dat" // for RPI/BBB, nvm data is stored in file
  #define NVM_SIZE            2048 // impose a file size limit: 64KB
  #define STATION_NAME_SIZE   16    // maximum number of characters in each station name
  
  #define MAX_UIDDATA         8     // unique ID, 8 bytes max
  #define MAX_PROGRAMDATA     996  // program data, 3984 bytes max
  #define MAX_NVCONDATA       8     // non-volatile controller data, 8 bytes max
  #define MAX_USER_PASSWORD   32    // user password, 32 bytes max
  #define MAX_LOCATION        48    // location string, 48 bytes max
  #define MAX_SCRIPTURL       64    // javascript url, 64 bytes max
  #define MAX_WEATHER_KEY     32    // weather api key, 32 bytes max
    
#endif  // NVM defines

/** NVM data addresses */
#define ADDR_NVM_UID           0
#define ADDR_NVM_PROGRAMS      (ADDR_NVM_UID+MAX_UIDDATA)   // program starting address
#define ADDR_NVM_NVCONDATA     (ADDR_NVM_PROGRAMS+MAX_PROGRAMDATA)
#define ADDR_NVM_PASSWORD      (ADDR_NVM_NVCONDATA+MAX_NVCONDATA)
#define ADDR_NVM_LOCATION      (ADDR_NVM_PASSWORD+MAX_USER_PASSWORD)
#define ADDR_NVM_SCRIPTURL     (ADDR_NVM_LOCATION+MAX_LOCATION)
#define ADDR_NVM_WEATHER_KEY   (ADDR_NVM_SCRIPTURL+MAX_SCRIPTURL)
#define ADDR_NVM_STN_NAMES     (ADDR_NVM_WEATHER_KEY+MAX_WEATHER_KEY)
#define ADDR_NVM_MAS_OP        (ADDR_NVM_STN_NAMES+MAX_NUM_STATIONS*STATION_NAME_SIZE) // master op bits
#define ADDR_NVM_IGNRAIN       (ADDR_NVM_MAS_OP+(MAX_EXT_BOARDS+1))  // ignore rain bits 
#define ADDR_NVM_ACTRELAY      (ADDR_NVM_IGNRAIN+(MAX_EXT_BOARDS+1)) // activate relay bits
#define ADDR_NVM_STNDISABLE    (ADDR_NVM_ACTRELAY+(MAX_EXT_BOARDS+1))// station disable bits
#define ADDR_NVM_STNSEQ        (ADDR_NVM_STNDISABLE+(MAX_EXT_BOARDS+1))// station sequential bits
#define ADDR_NVM_OPTIONS       (ADDR_NVM_STNSEQ+(MAX_EXT_BOARDS+1))  // options

/** Default password, location string, weather key, script urls */
#define DEFAULT_PASSWORD          "opendoor"
#define DEFAULT_LOCATION          "Boston,MA"
#define DEFAULT_WEATHER_KEY       ""
#define DEFAULT_JAVASCRIPT_URL    "http://ui.opensprinkler.com/js"
#define WEATHER_SCRIPT_HOST       "weather.opensprinkler.com"

/** Macro define of each option 
  * Refer to OpenSprinkler.cpp for details on each option
  */
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
  OPTION_HW_VERSION,
  OPTION_EXT_BOARDS,
  OPTION_SEQUENTIAL_RETIRED,
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
  OPTION_ENABLE_LOGGING,
  OPTION_RESET,
  NUM_OPTIONS	// total number of options
} OS_OPTION_t;

// Log Data Type
#define LOGDATA_STATION    0x00
#define LOGDATA_RAINSENSE  0x01
#define LOGDATA_RAINDELAY  0x02
#define LOGDATA_WATERLEVEL 0x03

#undef OS_HW_VERSION

/** Hardware defines */
#if defined(ARDUINO) 

  #if F_CPU==8000000L
    #define OS_HW_VERSION (OS_HW_VERSION_BASE+20)
  #elif F_CPU==12000000L
    #define OS_HW_VERSION (OS_HW_VERSION_BASE+21)
  #elif F_CPU==16000000L
    #define OS_HW_VERSION (OS_HW_VERSION_BASE+22)
  #endif

  // hardware pins
  #define PIN_BUTTON_1      31    // button 1
  #define PIN_BUTTON_2      30    // button 2
  #define PIN_BUTTON_3      29    // button 3 
  #define PIN_RF_DATA       28    // RF data pin 
  #define PORT_RF        PORTA
  #define PINX_RF        PINA3
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

  // Ethernet buffer size
  #if defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__)
    #define ETHER_BUFFER_SIZE   1500 // ATmega1284 has 16K RAM, so use a bigger buffer
  #else
    #define ETHER_BUFFER_SIZE   850  // ATmega644 has 4K RAM, so use a smaller buffer
  #endif

  #define 	wdt_reset()   __asm__ __volatile__ ("wdr")  // watchdog timer reset

  //#define SERIAL_DEBUG
  #if defined(SERIAL_DEBUG)

    #define DEBUG_BEGIN(x)   Serial.begin(x)
    #define DEBUG_PRINT(x)   Serial.print(x)
    #define DEBUG_PRINTLN(x) Serial.println(x)

  #else

    #define DEBUG_BEGIN(x)   {}
    #define DEBUG_PRINT(x)   {}
    #define DEBUG_PRINTLN(x) {}

  #endif
  typedef unsigned char   uint8_t;
  typedef unsigned int uint16_t;
  typedef int int16_t;

#else // Hardware defines for RPI/BBB

  /** OSPi pin defines */
  #if defined(OSPI)

  #define OS_HW_VERSION    OSPI_HW_VERSION_BASE
  #define PIN_SR_LATCH      22    // shift register latch pin
  #define PIN_SR_DATA       27    // shift register data pin
  #define PIN_SR_CLOCK       4    // shift register clock pin
  #define PIN_SR_OE         17    // shift register output enable pin
  #define PIN_RAINSENSOR    14    // rain sensor
  #define PIN_RELAY         15    // mini relay
  #define PIN_RF_DATA        7    // RF transmitter pin
  #define PIN_BUTTON_1      23    // button 1
  #define PIN_BUTTON_2      24    // button 2
  #define PIN_BUTTON_3      25    // button 3   
  
  /** BBB pin defines */
  #elif defined(OSBO)
  
  #define OS_HW_VERSION    OSBO_HW_VERSION_BASE
  #define PIN_SR_LATCH       3    // shift register latch pin
  #define PIN_SR_DATA       21    // shift register data pin
  #define PIN_SR_CLOCK      22    // shift register clock pin
  #define PIN_SR_OE          1    // shift register output enable pin
  #define PIN_RAINSENSOR    11    // rain sensor is connected to pin D3
  #define PIN_RELAY         14    // mini relay is connected to pin D14

  #else
    #error "cannot recognize hardware name / version"
  #endif
  
  #define ETHER_BUFFER_SIZE   1500
    
  #define DEBUG_BEGIN(x)          {}
  inline  void DEBUG_PRINT(int x) {printf("%d", x);}
  inline  void DEBUG_PRINT(const char*s) {printf("%s", s);}
  #define DEBUG_PRINTLN(x)        {DEBUG_PRINT(x);printf("\n");}

  inline void itoa(int v,char *s,int b)   {sprintf(s,"%d",v);}
  inline void ultoa(unsigned long v,char *s,int b) {sprintf(s,"%lu",v);}
  #define now()       time(0)
  #define delay(x)    {}
  
  /** Re-define avr-specific (e.g. program memory) types to use standard types */
  #define pgm_read_byte(x) *(x)
  #define PSTR(x)      x
  #define strcat_P     strcat
  #define strcpy_P     strcpy
  #define PROGMEM 
  typedef const char prog_char;
  typedef const char prog_uchar;
  typedef const char* PGM_P;
  typedef unsigned char   uint8_t;
  typedef short           int16_t;
  typedef unsigned short  uint16_t;
  typedef bool boolean;
  
#endif  // Hardawre defines

#define TMP_BUFFER_SIZE     120  // scratch buffer size

/** Other defines */
// button values
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
#define BUTTON_HOLD_MS      1000  // long hold expiration time (milliseconds)

// button mode values
#define BUTTON_WAIT_NONE       0  // do not wait, return value immediately
#define BUTTON_WAIT_RELEASE    1  // wait until button is release
#define BUTTON_WAIT_HOLD       2  // wait until button hold time expires

#define DISPLAY_MSG_MS      2000  // message display time (milliseconds)

typedef unsigned char byte;
typedef unsigned long ulong;

#endif  // _DEFINES_H


