/* OpenSprinkler Unified Firmware
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

//#define ENABLE_DEBUG  // enable serial debug

typedef unsigned long ulong;

#define TMP_BUFFER_SIZE      320   // scratch buffer size
#define TMP_BUFFER_SIZE_L      TMP_BUFFER_SIZE+100   // scratch buffer size

/** Firmware version, hardware version, and maximal values */
#define OS_FW_VERSION  233  // Firmware version: 220 means 2.2.0
														// if this number is different from the one stored in non-volatile memory
														// a device reset will be automatically triggered

#define OS_FW_MINOR      176  // Firmware minor version

/** Hardware version base numbers */
#define OS_HW_VERSION_BASE   0x00 // OpenSprinkler
#define OSPI_HW_VERSION_BASE 0x40 // OpenSprinkler Pi
#define SIM_HW_VERSION_BASE  0xC0 // simulation hardware

/** Hardware type macro defines */
#define HW_TYPE_AC           0xAC   // standard 24VAC for 24VAC solenoids only, with triacs
#define HW_TYPE_DC           0xDC   // DC powered, for both DC and 24VAC solenoids, with boost converter and MOSFETs
#define HW_TYPE_LATCH        0x1A   // DC powered, for DC latching solenoids only, with boost converter and H-bridges
#define HW_TYPE_UNKNOWN      0xFF

/** Data file names */
#define IOPTS_FILENAME        "iopts.dat"   // integer options data file
#define SOPTS_FILENAME        "sopts.dat"   // string options data file
#define STATIONS_FILENAME     "stns.dat"    // stations data file
#define STATIONS2_FILENAME    "stns2.dat"   // stations data file 2 - flow alert values
#define STATIONS3_FILENAME    "stns3.dat"   // stations data file 3 - flow avg values
#define NVCON_FILENAME        "nvcon.dat"   // non-volatile controller data file, see OpenSprinkler.h --> struct NVConData
#define PROG_FILENAME         "prog.dat"    // program data file
#define DONE_FILENAME         "done.dat"    // used to indicate the completion of all files

/** Station macro defines */
#define STN_TYPE_STANDARD    0x00 // standard solenoid station
#define STN_TYPE_RF          0x01	// Radio Frequency (RF) station
#define STN_TYPE_REMOTE_IP   0x02	// Remote OpenSprinkler station (by IP)
#define STN_TYPE_GPIO        0x03	// direct GPIO station
#define STN_TYPE_HTTP        0x04	// HTTP station
#define STN_TYPE_HTTPS       0x05	// HTTPS station
#define STN_TYPE_REMOTE_OTC  0x06 // Remote OpenSprinkler station (by OTC)
#define STN_TYPE_OTHER       0xFF

/** Notification macro defines */
#define NOTIFY_PROGRAM_SCHED   0x0001
#define NOTIFY_SENSOR1         0x0002
#define NOTIFY_FLOWSENSOR      0x0004
#define NOTIFY_WEATHER_UPDATE  0x0008
#define NOTIFY_REBOOT          0x0010
#define NOTIFY_STATION_OFF     0x0020
#define NOTIFY_SENSOR2         0x0040
#define NOTIFY_RAINDELAY       0x0080
#define NOTIFY_STATION_ON      0x0100
#define NOTIFY_FLOW_ALERT      0x0200
#define NOTIFY_MONITOR_LOW     0x0400
#define NOTIFY_MONITOR_MID     0x0800
#define NOTIFY_MONITOR_HIGH    0x1000

/** HTTP request macro defines */
#define HTTP_RQT_SUCCESS       0
#define HTTP_RQT_NOT_RECEIVED  1
#define HTTP_RQT_CONNECT_ERR   2
#define HTTP_RQT_TIMEOUT       3
#define HTTP_RQT_EMPTY_RETURN  4

/** Sensor macro defines */
#define SENSOR_TYPE_NONE    0x00
#define SENSOR_TYPE_RAIN    0x01  // rain sensor
#define SENSOR_TYPE_FLOW    0x02  // flow sensor
#define SENSOR_TYPE_SOIL    0x03  // soil moisture sensor
#define SENSOR_TYPE_PSWITCH 0xF0  // program switch sensor
#define SENSOR_TYPE_OTHER   0xFF

#define FLOWCOUNT_RT_WINDOW   30  // flow count window (for computing real-time flow rate), 30 seconds

/** Reboot cause */
#define REBOOT_CAUSE_NONE   0
#define REBOOT_CAUSE_RESET  1
#define REBOOT_CAUSE_BUTTON 2
#define REBOOT_CAUSE_RSTAP  3
#define REBOOT_CAUSE_TIMER  4
#define REBOOT_CAUSE_WEB    5
#define REBOOT_CAUSE_WIFIDONE     6
#define REBOOT_CAUSE_FWUPDATE     7
#define REBOOT_CAUSE_WEATHER_FAIL 8
#define REBOOT_CAUSE_NETWORK_FAIL 9
#define REBOOT_CAUSE_NTP          10
#define REBOOT_CAUSE_PROGRAM      11
#define REBOOT_CAUSE_POWERON      99


/** WiFi defines */
#define WIFI_MODE_AP       0xA9
#define WIFI_MODE_STA      0x2A

#define OS_STATE_INITIAL        0
#define OS_STATE_CONNECTING     1
#define OS_STATE_CONNECTED      2
#define OS_STATE_TRY_CONNECT    3
#define OS_STATE_WAIT_REBOOT    4

#define LED_FAST_BLINK 100
#define LED_SLOW_BLINK 500

/** Storage / zone expander defines */
#if defined(ARDUINO)
	#define MAX_EXT_BOARDS    8  // maximum number of 8-zone expanders (each 16-zone expander counts as 2)
#else
	#define MAX_EXT_BOARDS    24 // allow more zones for linux-based firmwares
#endif

#define MAX_NUM_BOARDS    (1+MAX_EXT_BOARDS)  // maximum number of 8-zone boards including expanders
#define MAX_NUM_STATIONS  (MAX_NUM_BOARDS*8)  // maximum number of stations
#define STATION_NAME_SIZE 32    // maximum number of characters in each station name
#define MAX_SOPTS_SIZE    320   // maximum string option size

#define STATION_SPECIAL_DATA_SIZE  (TMP_BUFFER_SIZE - STATION_NAME_SIZE - 12)

/** Default string option values */
#define DEFAULT_PASSWORD          "a6d82bced638de3def1e9bbb4983225c"  // md5 of 'opendoor'
#define DEFAULT_LOCATION          "49.484018,8.475593"  // Mannheim,Germany
#define DEFAULT_JAVASCRIPT_URL    "https://ui.opensprinklershop.de/js"
#define DEFAULT_WEATHER_URL       "weather.opensprinkler.com"
#define DEFAULT_IFTTT_URL         "maker.ifttt.com"
#define DEFAULT_OTC_SERVER_DEV     "ws.cloud.openthings.io"
#define DEFAULT_OTC_PORT_DEV       80
#define DEFAULT_OTC_SERVER_APP    "cloud.openthings.io"
#define DEFAULT_OTC_PORT_APP       443
#define DEFAULT_OTC_TOKEN_LENGTH   32
#define DEFAULT_DEVICE_NAME       "My OpenSprinkler"
#define DEFAULT_EMPTY_STRING      ""

#if (defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__))
	#define OS_AVR
#else  // all non-AVR platforms support OTF, EMAIL and HTTPS
	#define USE_OTF
	#define SUPPORT_EMAIL
	#define SUPPORT_HTTPS
#endif

/* Weather Adjustment Methods */
enum {
	WEATHER_METHOD_MANUAL = 0,
	WEATHER_METHOD_ZIMMERMAN,
	WEATHER_METHOD_AUTORAINDELY,
	WEATHER_METHOD_ETO,
	WEATHER_METHOD_MONTHLY,
	NUM_WEATHER_METHODS
};

/* Master */
enum {
	MASTER_1 = 0,
	MASTER_2,
	NUM_MASTER_ZONES,
};

enum {
	MASOPT_SID = 0,
	MASOPT_ON_ADJ,
	MASOPT_OFF_ADJ,
	NUM_MASTER_OPTS,
};

// Sequential Groups
#define NUM_SEQ_GROUPS		4
#define PARALLEL_GROUP_ID	255

/** Macro define of each option
  * Refer to OpenSprinkler.cpp for details on each option
  */
enum {
	IOPT_FW_VERSION=0,// read-only (ro)
	IOPT_TIMEZONE,
	IOPT_USE_NTP,
	IOPT_USE_DHCP,
	IOPT_STATIC_IP1,
	IOPT_STATIC_IP2,
	IOPT_STATIC_IP3,
	IOPT_STATIC_IP4,
	IOPT_GATEWAY_IP1,
	IOPT_GATEWAY_IP2,
	IOPT_GATEWAY_IP3,
	IOPT_GATEWAY_IP4,
	IOPT_HTTPPORT_0,
	IOPT_HTTPPORT_1,
	IOPT_HW_VERSION, //ro
	IOPT_EXT_BOARDS,
	IOPT_SEQUENTIAL_RETIRED, //ro
	IOPT_STATION_DELAY_TIME,
	IOPT_MASTER_STATION,
	IOPT_MASTER_ON_ADJ,
	IOPT_MASTER_OFF_ADJ,
	IOPT_URS_RETIRED, // ro
	IOPT_RSO_RETIRED, // ro
	IOPT_WATER_PERCENTAGE,
	IOPT_DEVICE_ENABLE, // editable through jc
	IOPT_IGNORE_PASSWORD,
	IOPT_DEVICE_ID,
	IOPT_LCD_CONTRAST,
	IOPT_LCD_BACKLIGHT,
	IOPT_LCD_DIMMING,
	IOPT_BOOST_TIME,
	IOPT_USE_WEATHER,
	IOPT_NTP_IP1,
	IOPT_NTP_IP2,
	IOPT_NTP_IP3,
	IOPT_NTP_IP4,
	IOPT_ENABLE_LOGGING,
	IOPT_MASTER_STATION_2,
	IOPT_MASTER_ON_ADJ_2,
	IOPT_MASTER_OFF_ADJ_2,
	IOPT_FW_MINOR, //ro
	IOPT_PULSE_RATE_0,
	IOPT_PULSE_RATE_1,
	IOPT_REMOTE_EXT_MODE, // editable through jc
	IOPT_DNS_IP1,
	IOPT_DNS_IP2,
	IOPT_DNS_IP3,
	IOPT_DNS_IP4,
	IOPT_SPE_AUTO_REFRESH,
	IOPT_NOTIF_ENABLE,
	IOPT_SENSOR1_TYPE,
	IOPT_SENSOR1_OPTION,
	IOPT_SENSOR2_TYPE,
	IOPT_SENSOR2_OPTION,
	IOPT_SENSOR1_ON_DELAY,
	IOPT_SENSOR1_OFF_DELAY,
	IOPT_SENSOR2_ON_DELAY,
	IOPT_SENSOR2_OFF_DELAY,
	IOPT_SUBNET_MASK1,
	IOPT_SUBNET_MASK2,
	IOPT_SUBNET_MASK3,
	IOPT_SUBNET_MASK4,
	IOPT_FORCE_WIRED,
	IOPT_LATCH_ON_VOLTAGE,
	IOPT_LATCH_OFF_VOLTAGE,
	IOPT_NOTIF2_ENABLE, // Notification part 2
	IOPT_RESERVE_4,
	IOPT_RESERVE_5,
	IOPT_RESERVE_6,
	IOPT_RESERVE_7,
	IOPT_RESERVE_8,
	IOPT_WIFI_MODE, //ro
	IOPT_RESET,     //ro
	NUM_IOPTS // total number of integer options
};

enum {
	SOPT_PASSWORD=0,
	SOPT_LOCATION,
	SOPT_JAVASCRIPTURL,
	SOPT_WEATHERURL,
	SOPT_WEATHER_OPTS,
	SOPT_IFTTT_KEY, // todo: make this IFTTT config just like MQTT
	SOPT_STA_SSID,
	SOPT_STA_PASS,
	SOPT_MQTT_OPTS,
	SOPT_OTC_OPTS,
	SOPT_DEVICE_NAME,
	SOPT_STA_BSSID_CHL, // wifi extra info: bssid and channel
	SOPT_EMAIL_OPTS,
	NUM_SOPTS // total number of string options
};

/** Log Data Type */
#define LOGDATA_STATION    0x00
#define LOGDATA_SENSOR1    0x01
#define LOGDATA_RAINDELAY  0x02
#define LOGDATA_WATERLEVEL 0x03
#define LOGDATA_FLOWSENSE  0x04
#define LOGDATA_SENSOR2    0x05
#define LOGDATA_CURRENT    0x80

#undef OS_HW_VERSION

/** Hardware defines */
#if defined(OS_AVR) // for OS 2.3

	#define OS_HW_VERSION   (OS_HW_VERSION_BASE+23)
	#define PIN_FREE_LIST   {2,10,12,13,14,15,18,19}  // Free GPIO pins

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
	#define PIN_SENSOR1       11    //
	#define PIN_SD_CS          0    // SD card chip select pin
	#define PIN_FLOWSENSOR_INT 1    // flow sensor interrupt pin (INT1)
	#define PIN_EXP_SENSE      4    // expansion board sensing pin (A4)
	#define PIN_CURR_SENSE     7    // current sensing pin (A7)
	#define PIN_CURR_DIGITAL  24    // digital pin index for A7

	#define ETHER_BUFFER_SIZE   2048
	#define ETHER_BUFFER_SIZE_L   ETHER_BUFFER_SIZE+100

	#define 	wdt_reset()   __asm__ __volatile__ ("wdr")  // watchdog timer reset

	#define pinModeExt        pinMode
	#define digitalReadExt    digitalRead
	#define digitalWriteExt   digitalWrite

	#define USE_DISPLAY
	#define USE_LCD
#elif defined(ESP8266) // for ESP8266

	#define OS_HW_VERSION    (OS_HW_VERSION_BASE+30)
	#define IOEXP_PIN        0x80 // base for pins on main IO expander
	#define MAIN_I2CADDR     0x20 // main IO expander I2C address
	#define ACDR_I2CADDR     0x21 // ac driver I2C address
	#define DCDR_I2CADDR     0x22 // dc driver I2C address
	#define LADR_I2CADDR     0x23 // latch driver I2C address
	#define EXP_I2CADDR_BASE 0x24 // base of expander I2C address
	#define LCD_I2CADDR      0x3C // 128x64 OLED display I2C address
	#define EEPROM_I2CADDR   0x50 // 24C02 EEPROM I2C address
	#define EEPROM_I2CADDR   0x50 // 24C02 EEPROM I2C address

	#define PIN_CURR_SENSE    A0    // current sensing pin
	#define PIN_LATCH_VOLT_SENSE A0 // latch voltage sensing pin
	#define PIN_FREE_LIST     {} // no free GPIO pin at the moment
	#define ETHER_BUFFER_SIZE   2048
	#define ETHER_BUFFER_SIZE_L   ETHER_BUFFER_SIZE+100

	#define PIN_ETHER_CS       16 // Ethernet CS (chip select pin) is 16 on OS 3.2 and above

	/* To accommodate different OS30 versions, we use software defines pins */
	extern unsigned char PIN_BUTTON_1;
	extern unsigned char PIN_BUTTON_2;
	extern unsigned char PIN_BUTTON_3;
	extern unsigned char PIN_RFRX;
	extern unsigned char PIN_RFTX;
	extern unsigned char PIN_BOOST;
	extern unsigned char PIN_BOOST_EN;
	extern unsigned char PIN_LATCH_COM;
	extern unsigned char PIN_LATCH_COMA;
	extern unsigned char PIN_LATCH_COMK;
	extern unsigned char PIN_SENSOR1;
	extern unsigned char PIN_SENSOR2;
	extern unsigned char PIN_IOEXP_INT;

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

	/* OS31 pin defines */
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

	/* OS32 pin defines */
	// pins on PCA9555A IO expander have pin numbers IOEXP_PIN+i
	#define V2_IO_CONFIG         0x1000 // config bits
	#define V2_IO_OUTPUT         0x1E00 // output bits
	#define V2_PIN_BUTTON_1      2 // button 1
	#define V2_PIN_BUTTON_2      0 // button 2
	#define V2_PIN_BUTTON_3      IOEXP_PIN+12 // button 3
	#define V2_PIN_RFTX          15
	#define V2_PIN_BOOST         IOEXP_PIN+13
	#define V2_PIN_BOOST_EN      IOEXP_PIN+14
	#define V2_PIN_LATCH_COMA    IOEXP_PIN+8  // latch COM+ (anode)
	#define V2_PIN_SRLAT         IOEXP_PIN+9  // shift register latch
	#define V2_PIN_SRCLK         IOEXP_PIN+10 // shift register clock
	#define V2_PIN_SRDAT         IOEXP_PIN+11 // shift register data
	#define V2_PIN_LATCH_COMK    IOEXP_PIN+15 // latch COM- (cathode)
	#define V2_PIN_SENSOR1       3  // sensor 1
	#define V2_PIN_SENSOR2       10 // sensor 2

	#define USE_DISPLAY
	#define USE_SSD1306

#elif defined(OSPI) // for OSPi

	#define OS_HW_VERSION    OSPI_HW_VERSION_BASE
	#define PIN_SR_LATCH      22    // shift register latch pin
	#define PIN_SR_DATA       27    // shift register data pin
	#define PIN_SR_DATA_ALT   21    // shift register data pin (alternative, for RPi 1 rev. 1 boards)
	#define PIN_SR_CLOCK       4    // shift register clock pin
	#define PIN_SR_OE         17    // shift register output enable pin
	#define PIN_SENSOR1       14
	#define PIN_SENSOR2       23
	#define PIN_RFTX          15    // RF transmitter pin
	#define PIN_BUTTON_1      24    // button 1
	#define PIN_BUTTON_2      18    // button 2
	#define PIN_BUTTON_3      10    // button 3

	#define PIN_FREE_LIST       {5,6,7,8,9,11,12,13,16,19,20,21,23,25,26}  // free GPIO pins
	#define ETHER_BUFFER_SIZE   16384
	#define ETHER_BUFFER_SIZE_L   ETHER_BUFFER_SIZE+100

	#define SDA 0
	#define SCL 0

	#define USE_DISPLAY
	#define USE_SSD1306

#else // for demo / simulation
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
	#define PIN_SENSOR1     0
	#define PIN_SENSOR2     0
	#define PIN_RFTX        0
	#define PIN_FREE_LIST  {}
	#define ETHER_BUFFER_SIZE   16384
	#define ETHER_BUFFER_SIZE_L   ETHER_BUFFER_SIZE+100

#endif

#if defined(ENABLE_DEBUG) /** Serial debug functions */

	#if defined(ARDUINO)
		#define DEBUG_BEGIN(x)   {Serial.begin(x);}
		#define DEBUG_PRINT(x)   {Serial.print(x);}
		#define DEBUG_PRINTLN(x) {Serial.println(x);}
		#define DEBUG_PRINTF(msg, ...)    {Serial.printf(msg, ##__VA_ARGS__);}
	#else
		#include <stdio.h>
		#define DEBUG_BEGIN(x)          {}  /** Serial debug functions */
		inline  void DEBUG_PRINT(int x) {fprintf(stdout, "%d", x);}
		inline  void DEBUG_PRINT(const char*s) {fprintf(stdout, "%s", s);}
		#define DEBUG_PRINTLN(x)        {DEBUG_PRINT(x);fprintf(stdout, "\n");}
		#define DEBUG_PRINTF(msg, ...)    {fprintf(stdout, msg, ##__VA_ARGS__);}
	#endif

#else

	#define DEBUG_BEGIN(x)   {}
	#define DEBUG_PRINT(x)   {}
	#define DEBUG_PRINTLN(x) {}
	#define DEBUG_PRINTF(x, ...)  {}

#endif

/** Re-define avr-specific (e.g. PGM) types to use standard types */
#if !defined(ARDUINO)
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
	#include <stddef.h>
	inline void itoa(int v,char *s,int b)   {sprintf(s,"%d",v);}
	inline void ultoa(unsigned long v,char *s,int b) {sprintf(s,"%lu",v);}
	#define pgm_read_byte(x) *(x)
	#define PSTR(x)      x
	#define F(x)         x
	#define strcat_P     strcat
	#define strncat_P    strncat
	#define strcpy_P     strcpy
	#define memcpy_P     memcpy
	#define snprintf_P    snprintf
	#include<string>
	#define String       string
	using namespace std;
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
