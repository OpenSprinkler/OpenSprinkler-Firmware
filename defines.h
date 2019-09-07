/* OpenSprinkler Unified (AVR/RPI/BBB/LINUX/ESP8266) Firmware
 * Copyright (C) 2015 by Ray Wang (ray@opensprinkler.com)
 *
 * OpenSprinkler macro defines and hardware pin assignments
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

#ifndef _DEFINES_H
#define _DEFINES_H

typedef unsigned char byte;
typedef unsigned long ulong;

#if !defined(ARDUINO) || defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__) || defined(ESP8266)
  #define TMP_BUFFER_SIZE      255   // scratch buffer size
#else
  #define TMP_BUFFER_SIZE      128   // scratch buffer size
#endif

/** Firmware version, hardware version, and maximal values */
#define OS_FW_VERSION  218  // Firmware version: 218 means 2.1.8
                            // if this number is different from the one stored in non-volatile memory
                            // a device reset will be automatically triggered

#define OS_FW_MINOR      4  // Firmware minor version

/** Hardware version base numbers */
#define OS_HW_VERSION_BASE   0x00
#define OSPI_HW_VERSION_BASE 0x40
#define OSBO_HW_VERSION_BASE 0x80
#define SIM_HW_VERSION_BASE  0xC0

/** Hardware type macro defines */
#define HW_TYPE_AC           0xAC   // standard 24VAC for 24VAC solenoids only, with triacs
#define HW_TYPE_DC           0xDC   // DC powered, for both DC and 24VAC solenoids, with boost converter and MOSFETs
#define HW_TYPE_LATCH        0x1A   // DC powered, for DC latching solenoids only, with boost converter and H-bridges
#define HW_TYPE_UNKNOWN      0xFF

/** File names */
#define WEATHER_OPTS_FILENAME "wtopts.txt"    // weather options file
#define STATION_ATTR_FILENAME "stns.dat"      // station attributes data file
#define WIFI_FILENAME         "wifi.dat"      // wifi credentials file
#define IFTTT_KEY_FILENAME    "ifkey.txt"
#define IFTTT_KEY_MAXSIZE     128
#define STATION_SPECIAL_DATA_SIZE  (TMP_BUFFER_SIZE - 8)

#define FLOWCOUNT_RT_WINDOW   30    // flow count window (for computing real-time flow rate), 30 seconds

/** Station type macro defines */
#define STN_TYPE_STANDARD    0x00
#define STN_TYPE_RF          0x01
#define STN_TYPE_REMOTE      0x02
#define STN_TYPE_GPIO        0x03	// Support for raw connection of station to GPIO pin
#define STN_TYPE_HTTP        0x04	// Support for HTTP Get connection
#define STN_TYPE_OTHER       0xFF

#define NOTIFY_PROGRAM_SCHED   0x01
#define NOTIFY_RAINSENSOR      0x02
#define NOTIFY_FLOWSENSOR      0x04
#define NOTIFY_WEATHER_UPDATE  0x08
#define NOTIFY_REBOOT          0x10
#define NOTIFY_STATION_OFF     0x20
#define NOTIFY_STATION_ON      0x40

/** Sensor type macro defines */
#define SENSOR_TYPE_NONE    0x00
#define SENSOR_TYPE_RAIN    0x01  // rain sensor
#define SENSOR_TYPE_FLOW    0x02  // flow sensor
#define SENSOR_TYPE_PSWITCH 0xF0  // program switch
#define SENSOR_TYPE_OTHER   0xFF

/** WiFi related defines */
#ifdef ESP8266

#define WIFI_MODE_AP       0xA9
#define WIFI_MODE_STA      0x2A

#define OS_STATE_INITIAL        0
#define OS_STATE_CONNECTING     1
#define OS_STATE_CONNECTED      2
#define OS_STATE_TRY_CONNECT    3

#define LED_FAST_BLINK 100
#define LED_SLOW_BLINK 500

#endif

/** Non-volatile memory (NVM) defines */
#if defined(ARDUINO) && !defined(ESP8266)

/** 2KB NVM (ATmega644) data structure:
  * |         |     |  ---STRING PARAMETERS---      |           |   ----STATION ATTRIBUTES-----      |          |
  * | PROGRAM | CON | PWD | LOC | JURL | WURL | KEY | STN_NAMES | MAS | IGR | MAS2 | DIS | SEQ | SPE | OPTIONS  |
  * |  (986)  |(12) |(36) |(48) | (40) | (40) |(24) |   (768)   | (6) | (6) |  (6) | (6) | (6) | (6) |  (58)    |
  * |         |     |     |     |      |      |     |           |     |     |      |     |     |     |          |
  * 0        986  998   1034  1082  1122   1162   1186        1954  1960  1966   1972  1978  1984  1990      2048
  */

/** 4KB NVM (ATmega1284) data structure:
  * |         |     |  ---STRING PARAMETERS---      |           |   ----STATION ATTRIBUTES-----      |          |
  * | PROGRAM | CON | PWD | LOC | JURL | WURL | KEY | STN_NAMES | MAS | IGR | MAS2 | DIS | SEQ | SPE | OPTIONS  |
  * |  (2433) |(12) |(36) |(48) | (48) | (48) |(24) |   (1344)  | (7) | (7) |  (7) | (7) | (7) | (7) |   (61)   |
  * |         |     |     |     |      |      |     |           |     |     |      |     |     |     |          |
  * 0       2433  2445   2481  2529  2577   2625   2649        3993  4000  4007   4014  4021  4028  4035      4096
  */

  #if defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__) // for 4KB NVM

    #define MAX_EXT_BOARDS    6  // maximum number of exp. boards (each expands 8 stations)
    #define MAX_NUM_STATIONS  ((1+MAX_EXT_BOARDS)*8)  // maximum number of stations

    #define NVM_SIZE            4096  // For AVR, nvm data is stored in EEPROM, ATmega1284 has 4K EEPROM
    #define STATION_NAME_SIZE   24    // maximum number of characters in each station name

    #define MAX_PROGRAMDATA     2433  // program data
    #define MAX_NVCONDATA       12    // non-volatile controller data
    #define MAX_USER_PASSWORD   36    // user password
    #define MAX_LOCATION        48    // location string
    #define MAX_JAVASCRIPTURL   48    // javascript url
    #define MAX_WEATHERURL      48    // weather script url
    #define MAX_WEATHER_KEY     24    // weather api key

  #else

    #define MAX_EXT_BOARDS    5  // maximum number of exp. boards (each expands 8 stations)
    #define MAX_NUM_STATIONS  ((1+MAX_EXT_BOARDS)*8)  // maximum number of stations

    #define NVM_SIZE            2048  // For AVR, nvm data is stored in EEPROM, ATmega644 has 2K EEPROM
    #define STATION_NAME_SIZE   16    // maximum number of characters in each station name

    #define MAX_PROGRAMDATA     986   // program data
    #define MAX_NVCONDATA       12     // non-volatile controller data
    #define MAX_USER_PASSWORD   36    // user password
    #define MAX_LOCATION        48    // location string
    #define MAX_JAVASCRIPTURL   40    // javascript url
    #define MAX_WEATHERURL      40    // weather script url
    #define MAX_WEATHER_KEY     24    // weather api key,

  #endif

#else // NVM defines for RPI/BBB/LINUX/ESP8266

/** 8KB NVM (RPI/BBB/LINUX/ESP8266) data structure:
  * |         |     |  ---STRING PARAMETERS---      |           |   ----STATION ATTRIBUTES-----      |          |
  * | PROGRAM | CON | PWD | LOC | JURL | WURL | KEY | STN_NAMES | MAS | IGR | MAS2 | DIS | SEQ | SPE | OPTIONS  |
  * |  (6127) |(12) |(36) |(48) | (48) | (48) |(24) |   (1728)  | (9) | (9) |  (9) | (9) | (9) | (9) |   (67)   |
  * |         |     |     |     |      |      |     |           |     |     |      |     |     |     |          |
  * 0       6127  6139   6175  6223  6271   6319   6343        8071  8080  8089   8098  8107  8116  8125      8192
  */

  // These are kept the same as AVR for compatibility reasons
  // But they can be increased if needed
  #define NVM_FILENAME        "nvm.dat" // for RPI/BBB, nvm data is stored in a file

  #define MAX_EXT_BOARDS    8  // maximum number of 8-station exp. boards (a 16-station expander counts as 2)
  #define MAX_NUM_STATIONS  ((1+MAX_EXT_BOARDS)*8)  // maximum number of stations

  #define NVM_SIZE            8192
  #define STATION_NAME_SIZE   24    // maximum number of characters in each station name

  #define MAX_PROGRAMDATA     6127  // program data
  #define MAX_NVCONDATA       12     // non-volatile controller data
  #define MAX_USER_PASSWORD   36    // user password
  #define MAX_LOCATION        48    // location string
  #define MAX_JAVASCRIPTURL   48    // javascript url
  #define MAX_WEATHERURL      48    // weather script url
  #define MAX_WEATHER_KEY     24    // weather api key

#endif  // end of NVM defines

/** NVM data addresses */
#define ADDR_NVM_PROGRAMS      (0)   // program starting address
#define ADDR_NVM_NVCONDATA     (ADDR_NVM_PROGRAMS+MAX_PROGRAMDATA)
#define ADDR_NVM_PASSWORD      (ADDR_NVM_NVCONDATA+MAX_NVCONDATA)
#define ADDR_NVM_LOCATION      (ADDR_NVM_PASSWORD+MAX_USER_PASSWORD)
#define ADDR_NVM_JAVASCRIPTURL (ADDR_NVM_LOCATION+MAX_LOCATION)
#define ADDR_NVM_WEATHERURL    (ADDR_NVM_JAVASCRIPTURL+MAX_JAVASCRIPTURL)
#define ADDR_NVM_WEATHER_KEY   (ADDR_NVM_WEATHERURL+MAX_WEATHERURL)
#define ADDR_NVM_STN_NAMES     (ADDR_NVM_WEATHER_KEY+MAX_WEATHER_KEY)
#define ADDR_NVM_MAS_OP        (ADDR_NVM_STN_NAMES+MAX_NUM_STATIONS*STATION_NAME_SIZE) // master op bits
#define ADDR_NVM_IGNRAIN       (ADDR_NVM_MAS_OP+(MAX_EXT_BOARDS+1))  // ignore rain bits
#define ADDR_NVM_MAS_OP_2      (ADDR_NVM_IGNRAIN+(MAX_EXT_BOARDS+1)) // master2 op bits
#define ADDR_NVM_STNDISABLE    (ADDR_NVM_MAS_OP_2+(MAX_EXT_BOARDS+1))// station disable bits
#define ADDR_NVM_STNSEQ        (ADDR_NVM_STNDISABLE+(MAX_EXT_BOARDS+1))// station sequential bits
#define ADDR_NVM_STNSPE        (ADDR_NVM_STNSEQ+(MAX_EXT_BOARDS+1)) // station special bits (i.e. non-standard stations)
#define ADDR_NVM_OPTIONS       (ADDR_NVM_STNSPE+(MAX_EXT_BOARDS+1))  // options

/** Default password, location string, weather key, script urls */
#define DEFAULT_PASSWORD          "a6d82bced638de3def1e9bbb4983225c"  // md5 of 'opendoor'
#define DEFAULT_LOCATION          "Boston,MA"
#define DEFAULT_WEATHER_KEY       ""
#define DEFAULT_JAVASCRIPT_URL    "https://ui.opensprinkler.com/js"
#define DEFAULT_WEATHER_URL       "weather.opensprinkler.com"
#define DEFAULT_IFTTT_URL         "maker.ifttt.com"
#define DEFAULT_MQTT_HOST         "localhost"
#define DEFAULT_MQTT_PORT         1883

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
  OPTION_SENSOR1_TYPE,
  OPTION_SENSOR1_OPTION,
  OPTION_WATER_PERCENTAGE,
  OPTION_DEVICE_ENABLE,
  OPTION_IGNORE_PASSWORD,
  OPTION_DEVICE_ID,
  OPTION_LCD_CONTRAST,
  OPTION_LCD_BACKLIGHT,
  OPTION_LCD_DIMMING,
  OPTION_BOOST_TIME,
  OPTION_USE_WEATHER,
  OPTION_NTP_IP1,
  OPTION_NTP_IP2,
  OPTION_NTP_IP3,
  OPTION_NTP_IP4,
  OPTION_ENABLE_LOGGING,
  OPTION_MASTER_STATION_2,
  OPTION_MASTER_ON_ADJ_2,
  OPTION_MASTER_OFF_ADJ_2,
  OPTION_FW_MINOR,
  OPTION_PULSE_RATE_0,
  OPTION_PULSE_RATE_1,
  OPTION_REMOTE_EXT_MODE,
  OPTION_DNS_IP1,
  OPTION_DNS_IP2,
  OPTION_DNS_IP3,
  OPTION_DNS_IP4,
  OPTION_SPE_AUTO_REFRESH,
  OPTION_IFTTT_ENABLE,
  OPTION_MQTT_ENABLE,
  OPTION_SENSOR2_TYPE,
  OPTION_SENSOR2_OPTION,
  OPTION_RESET,
  NUM_OPTIONS	// total number of options
} OS_OPTION_t;

/** Log Data Type */
#define LOGDATA_STATION    0x00
#define LOGDATA_RAINSENSE  0x01
#define LOGDATA_RAINDELAY  0x02
#define LOGDATA_WATERLEVEL 0x03
#define LOGDATA_FLOWSENSE  0x04

#undef OS_HW_VERSION

/** Hardware defines */
#if defined(ARDUINO) && !defined(ESP8266)

  #if F_CPU==8000000L // 8M for OS20
    #define OS_HW_VERSION (OS_HW_VERSION_BASE+20)
  #elif F_CPU==12000000L  // 12M for OS21
    #define OS_HW_VERSION (OS_HW_VERSION_BASE+21)
  #elif F_CPU==16000000L  // 16M for OS22 and OS23
    #if defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__)
      #define OS_HW_VERSION (OS_HW_VERSION_BASE+23)
      #define PIN_FREE_LIST		{2,10,12,13,14,15,18,19}  // Free GPIO pins
    #else
      #define OS_HW_VERSION (OS_HW_VERSION_BASE+22)
    #endif
  #endif

  // hardware pins
  #define PIN_BUTTON_1      31    // button 1
  #define PIN_BUTTON_2      30    // button 2
  #define PIN_BUTTON_3      29    // button 3
  #define PIN_RFTX          28    // RF data pin
  #define PORT_RF        PORTA
  #define PINX_RF        PINA3
  #define PIN_SR_LATCH       3    // shift register latch pin
  #define PIN_SR_DATA       21    // shift register data pin
  #define PIN_SR_CLOCK      22    // shift register clock pin
  #define PIN_SR_OE          1    // shift register output enable pin

  // regular 16x2 LCD pin defines
  #define PIN_LCD_RS        19    // LCD rs pin
  #define PIN_LCD_EN        18    // LCD enable pin
  #define PIN_LCD_D4        20    // LCD d4 pin
  #define PIN_LCD_D5        21    // LCD d5 pin
  #define PIN_LCD_D6        22    // LCD d6 pin
  #define PIN_LCD_D7        23    // LCD d7 pin
  #define PIN_LCD_BACKLIGHT 12    // LCD backlight pin
  #define PIN_LCD_CONTRAST  13    // LCD contrast pin

  // DC controller pin defines
  #define PIN_BOOST         20    // booster pin
  #define PIN_BOOST_EN      23    // boost voltage enable pin

  #define PIN_ETHER_CS       4    // Ethernet controller chip select pin
  #define PIN_SD_CS          0    // SD card chip select pin
  #define PIN_RAINSENSOR    11    // rain sensor is connected to pin D3
  #define PIN_FLOWSENSOR    11    // flow sensor (currently shared with rain sensor, change if using a different pin)
  #define PIN_FLOWSENSOR_INT 1    // flow sensor interrupt pin (INT1)
  #define PIN_EXP_SENSE      4    // expansion board sensing pin (A4)
  #define PIN_CURR_SENSE     7    // current sensing pin (A7)
  #define PIN_CURR_DIGITAL  24    // digital pin index for A7

  // Ethernet buffer size
  #if defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__)
    #define ETHER_BUFFER_SIZE   1400 // ATmega1284 has 16K RAM, so use a bigger buffer
  #else
    #define ETHER_BUFFER_SIZE   950  // ATmega644 has 4K RAM, so use a smaller buffer
  #endif

  #define 	wdt_reset()   __asm__ __volatile__ ("wdr")  // watchdog timer reset

  //#define SERIAL_DEBUG
  #if defined(SERIAL_DEBUG) /** Serial debug functions */

    #define DEBUG_BEGIN(x)   Serial.begin(x)
    #define DEBUG_PRINT(x)   Serial.print(x)
    #define DEBUG_PRINTLN(x) Serial.println(x)
    #define DEBUG_PRINTIP(x) ether.printIp("IP:",x)

  #else

    #define DEBUG_BEGIN(x)   {}
    #define DEBUG_PRINT(x)   {}
    #define DEBUG_PRINTLN(x) {}
    #define DEBUG_PRINTIP(x) {}

  #endif
  typedef unsigned char   uint8_t;
  typedef unsigned int    uint16_t;
  typedef int int16_t;
  #define pinModeExt      pinMode
  #define digitalReadExt  digitalRead
  #define digitalWriteExt digitalWrite  

#else // Hardware defines for RPI/BBB/ESP8266

  #if defined(ESP8266)

    #define OS_HW_VERSION    (OS_HW_VERSION_BASE+30)
    #define IOEXP_PIN        0x80 // base for pins on main IO expander
    #define MAIN_I2CADDR     0x20 // main IO expander I2C address
    #define ACDR_I2CADDR     0x21 // ac driver I2C address
    #define DCDR_I2CADDR     0x22 // dc driver I2C address
    #define LADR_I2CADDR     0x23 // latch driver I2C address
    #define EXP_I2CADDR_BASE 0x24 // base of expander I2C address
    #define LCD_I2CADDR      0x3C // 128x64 OLED display I2C address

    #define PIN_CURR_SENSE    A0
    #define PIN_FREE_LIST     {}  // no free GPIO pin at the moment
    #define ETHER_BUFFER_SIZE   4096

    #define PIN_ETHER_CS       16 // ENC28J60 CS (chip select pin) is 16 on OS 3.2.

    /* To accommodate different OS30 versions, we use software defines pins */ 
    extern byte PIN_BUTTON_1;
    extern byte PIN_BUTTON_2;
    extern byte PIN_BUTTON_3;
    extern byte PIN_RFRX;
    extern byte PIN_RFTX;
    extern byte PIN_BOOST;
    extern byte PIN_BOOST_EN;
    extern byte PIN_LATCH_COM;
    extern byte PIN_SENSOR1;
    extern byte PIN_SENSOR2;
    extern byte PIN_RAINSENSOR;
    extern byte PIN_FLOWSENSOR;
    extern byte PIN_IOEXP_INT;

    /* Original OS30 pin defines */
    //#define V0_MAIN_INPUTMASK 0b00001010 // main input pin mask
    // pins on main PCF8574 IO expander have pin numbers IOEXP_PIN+i
    #define V0_PIN_BUTTON_1      IOEXP_PIN+1 // button 1
    #define V0_PIN_BUTTON_2      0           // button 2
    #define V0_PIN_BUTTON_3      IOEXP_PIN+3 // button 3
    #define V0_PIN_RFRX          14
    #define V0_PIN_PWR_RX        IOEXP_PIN+0
    #define V0_PIN_RFTX          16
    #define V0_PIN_PWR_TX        IOEXP_PIN+2
    #define V0_PIN_BOOST         IOEXP_PIN+6
    #define V0_PIN_BOOST_EN      IOEXP_PIN+7
    #define V0_PIN_SENSOR1       12 // sensor 1
    #define V0_PIN_SENSOR2       13 // sensor 2
    #define V0_PIN_RAINSENSOR    V0_PIN_SENSOR1 // for this firmware, rain and flow sensors are both assumed on sensor 1
    #define V0_PIN_FLOWSENSOR    V0_PIN_SENSOR1

    /* OS30 revision 1 pin defines */
    // pins on PCA9555A IO expander have pin numbers IOEXP_PIN+i
    #define V1_IO_CONFIG         0x1F00 // config bits
    #define V1_IO_OUTPUT         0x1F00 // output bits
    #define V1_PIN_BUTTON_1      IOEXP_PIN+10 // button 1
    #define V1_PIN_BUTTON_2      IOEXP_PIN+11 // button 2
    #define V1_PIN_BUTTON_3      IOEXP_PIN+12 // button 3
    #define V1_PIN_RFRX          14
    #define V1_PIN_RFTX          16
    #define V1_PIN_IOEXP_INT     12
    #define V1_PIN_BOOST         IOEXP_PIN+13
    #define V1_PIN_BOOST_EN      IOEXP_PIN+14
    #define V1_PIN_LATCH_COM     IOEXP_PIN+15
    #define V1_PIN_SENSOR1       IOEXP_PIN+8 // sensor 1
    #define V1_PIN_SENSOR2       IOEXP_PIN+9 // sensor 2
    #define V1_PIN_RAINSENSOR    V1_PIN_SENSOR1 // for this firmware, rain and flow sensors are both assumed on sensor 1
    #define V1_PIN_FLOWSENSOR    V1_PIN_SENSOR1

    /* OS30 revision 2 pin defines */
    // pins on PCA9555A IO expander have pin numbers IOEXP_PIN+i
    #define V2_IO_CONFIG         0x9F00 // config bits
    #define V2_IO_OUTPUT         0x9F00 // output bits
    #define V2_PIN_BUTTON_1      2 // button 1
    #define V2_PIN_BUTTON_2      0 // button 2
    #define V2_PIN_BUTTON_3      IOEXP_PIN+12 // button 3
    #define V2_PIN_RFTX          15
    #define V2_PIN_BOOST         IOEXP_PIN+13
    #define V2_PIN_BOOST_EN      IOEXP_PIN+14
    #define V2_PIN_SENSOR1       3  // sensor 1
    #define V2_PIN_SENSOR2       10 // sensor 2
    #define V2_PIN_RAINSENSOR    V2_PIN_SENSOR1
    #define V2_PIN_FLOWSENSOR    V2_PIN_SENSOR1
    
  /** OSPi pin defines */
  #elif defined(OSPI)

    #define OS_HW_VERSION    OSPI_HW_VERSION_BASE
    #define PIN_SR_LATCH      22    // shift register latch pin
    #define PIN_SR_DATA       27    // shift register data pin
    #define PIN_SR_DATA_ALT   21    // shift register data pin (alternative, for RPi 1 rev. 1 boards)
    #define PIN_SR_CLOCK       4    // shift register clock pin
    #define PIN_SR_OE         17    // shift register output enable pin
    #define PIN_RAINSENSOR    14    // rain sensor
    #define PIN_FLOWSENSOR    14    // flow sensor (currently shared with rain sensor, change if using a different pin)
    #define PIN_RFTX          15    // RF transmitter pin
    #define PIN_BUTTON_1      23    // button 1
    #define PIN_BUTTON_2      24    // button 2
    #define PIN_BUTTON_3      25    // button 3

    #define PIN_FREE_LIST		{5,6,7,8,9,10,11,12,13,16,18,19,20,21,23,24,25,26}  // free GPIO pins
    #define ETHER_BUFFER_SIZE   16384
  /** BBB pin defines */
  #elif defined(OSBO)

    #define OS_HW_VERSION    OSBO_HW_VERSION_BASE
    // these are gpio pin numbers, refer to
    // https://github.com/mkaczanowski/BeagleBoneBlack-GPIO/blob/master/GPIO/GPIOConst.cpp
    #define PIN_SR_LATCH      60    // P9_12, shift register latch pin
    #define PIN_SR_DATA       30    // P9_11, shift register data pin
    #define PIN_SR_CLOCK      31    // P9_13, shift register clock pin
    #define PIN_SR_OE         50    // P9_14, shift register output enable pin
    #define PIN_RAINSENSOR    48    // P9_15, rain sensor is connected to pin D3
    #define PIN_FLOWSENSOR    48    // flow sensor (currently shared with rain sensor, change if using a different pin)
    #define PIN_RFTX          51    // RF transmitter pin
    
    #define PIN_FREE_LIST     {38,39,34,35,45,44,26,47,27,65,63,62,37,36,33,32,61,86,88,87,89,76,77,74,72,73,70,71}
    #define ETHER_BUFFER_SIZE   16384
  #else
    // For Linux or other software simulators
    // use fake hardware pins
    #if defined(DEMO)
      #define OS_HW_VERSION 255   // assign hardware number 255 to DEMO firmware
    #else
      #define OS_HW_VERSION SIM_HW_VERSION_BASE
    #endif
    #define PIN_SR_LATCH    0
    #define PIN_SR_DATA     0
    #define PIN_SR_CLOCK    0
    #define PIN_SR_OE       0
    #define PIN_RAINSENSOR  0
    #define PIN_FLOWSENSOR  0
    #define PIN_RFTX     0
  	#define PIN_FREE_LIST	{}
    #define ETHER_BUFFER_SIZE   16384
  #endif

  //#define ENABLE_DEBUG 
  #if defined(ENABLE_DEBUG)
    #if defined(ESP8266)
      #define DEBUG_BEGIN(x)   Serial.begin(x)
      #define DEBUG_PRINT(x)   Serial.print(x)
      #define DEBUG_PRINTLN(x) Serial.println(x)
    #else
    	#include <stdio.h>
      #define DEBUG_BEGIN(x)          {}  /** Serial debug functions */
      inline  void DEBUG_PRINT(int x) {printf("%d", x);}
      inline  void DEBUG_PRINT(const char*s) {printf("%s", s);}
      #define DEBUG_PRINTLN(x)        {DEBUG_PRINT(x);printf("\n");}
    #endif
  #else
    #define DEBUG_BEGIN(x) {}
    #define DEBUG_PRINT(x) {}
    #define DEBUG_PRINTLN(x) {}
  #endif

  /** Re-define avr-specific (e.g. PGM) types to use standard types */
  #if !defined(ESP8266)
  	#include <stdio.h>
  	#include <string.h>
    inline void itoa(int v,char *s,int b)   {sprintf(s,"%d",v);}
    inline void ultoa(unsigned long v,char *s,int b) {sprintf(s,"%lu",v);}
    #define now()       time(0)
    #define pgm_read_byte(x) *(x)
    #define PSTR(x)      x
    #define strcat_P     strcat
    #define strcpy_P     strcpy
    #define sprintf_P    sprintf
    #define PROGMEM
    typedef const char* PGM_P;
    typedef unsigned char   uint8_t;
    typedef short           int16_t;
    typedef unsigned short  uint16_t;
    typedef bool boolean;
    #define pinModeExt      pinMode
    #define digitalReadExt  digitalRead
    #define digitalWriteExt digitalWrite
  #endif

#endif  // end of Hardawre defines

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

#endif  // _DEFINES_H


