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
#pragma once

//#define ENABLE_DEBUG  // enable serial debug

#define TMP_BUFFER_SIZE       320   // scratch buffer size
#define TMP_BUFFER_ALLOC_SIZE TMP_BUFFER_SIZE+32 // allocate extra space to allow overflow when needed

/** Firmware version, hardware version, and maximal values */
#define OS_FW_VERSION  221  // Firmware version: 221 means 2.2.1
														// if this number is different from the one stored in non-volatile memory
														// a device reset will be automatically triggered

#define OS_FW_MINOR      6  // Firmware minor version

/** Hardware version base numbers */
#define OS_HW_VERSION_BASE   0x00 // OpenSprinkler
#define OSPI_HW_VERSION_BASE 0x40 // OpenSprinkler Pi
#define SIM_HW_VERSION_BASE  0xC0 // simulation hardware

/** Hardware type macro defines */
#define HW_TYPE_AC           0xAC   // standard 24VAC for 24VAC solenoids only, with triacs
#define HW_TYPE_DC           0xDC   // DC powered, for both DC and 24VAC solenoids, with boost converter and MOSFETs
#define HW_TYPE_LATCH        0x1A   // DC powered, for DC latching solenoids only, with boost converter and H-bridges
#define HW_TYPE_UNKNOWN      0xFF

#if defined(ESP32)
#define HAS_TARGET_PD_VOLTAGE(revision, type) ((type) == HW_TYPE_DC)
#elif defined(ESP8266)
#define HAS_TARGET_PD_VOLTAGE(revision, type) ((revision) == 4 && (type) == HW_TYPE_DC)
#else
#define HAS_TARGET_PD_VOLTAGE(revision, type) false
#endif

/** Data file names */
#if defined(ESP32)
#define IOPTS_FILENAME        "/iopts.dat"
#define SOPTS_FILENAME        "/sopts.dat"
#define STATIONS_FILENAME     "/stns.dat"
#define NVCON_FILENAME        "/nvcon.dat"
#define PROG_FILENAME         "/prog.dat"
#define DONE_FILENAME         "/done.dat"
#else
#define IOPTS_FILENAME        "iopts.dat"   // integer options data file
#define SOPTS_FILENAME        "sopts.dat"   // string options data file
#define STATIONS_FILENAME     "stns.dat"    // stations data file
#define NVCON_FILENAME        "nvcon.dat"   // non-volatile controller data file, see OpenSprinkler.h --> struct NVConData
#define PROG_FILENAME         "prog.dat"    // program data file
#define DONE_FILENAME         "done.dat"    // used to indicate the completion of all files
#endif
// External sensor board (ADS1115-based analog inputs, see sensor.h).
// Unrelated to the onboard SENSOR1/SENSOR2 GPIO inputs.
#if defined(ESP32)
#define SENSORS_FILENAME      "/sens.dat"
#define SENADJ_FILENAME       "/senadj.dat"
#else
#define SENSORS_FILENAME      "sens.dat"    // external sensor definitions
#define SENADJ_FILENAME       "senadj.dat"  // external sensor adjustment data for programs
#endif
#if defined(ARDUINO)
#define LOG_DIR                     "/logs/"   // absolute path on LittleFS; parent dir created implicitly
#else
#define LOG_DIR                     "logs/"    // relative to data dir on Linux; get_filename_fullpath prepends it
#endif
#define SENSORS_LOG_FILENAME        LOG_DIR "sens.log" // external sensor log data (…sens.log000 … sens.logNNN)
#define SENSORS_LOG_HEADER_FILENAME LOG_DIR "sens.hdr" // external sensor log header

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
#define NOTIFY_CURR_ALERT      0x0400
#define NOTIFY_SENSOR3         0x0800
#define NOTIFY_SENSOR4         0x1000

/** Queue Insertion Mode */
enum {
	QUEUE_OPTION_APPEND = 0,
	QUEUE_OPTION_INSERT_FRONT,
	QUEUE_OPTION_REPLACE
};

enum {
	CURR_ALERT_TYPE_UNDER = 0,		// undercurrent when running a station
	CURR_ALERT_TYPE_OVER_STATION,	// overcurrent when turning on a station
	CURR_ALERT_TYPE_OVER_SYSTEM		// overcurrent while system is running
};

/** HTTP request macro defines */
#define HTTP_RQT_SUCCESS       0
#define HTTP_RQT_NOT_RECEIVED  -1
#define HTTP_RQT_CONNECT_ERR   -2
#define HTTP_RQT_TIMEOUT       -3
#define HTTP_RQT_EMPTY_RETURN  -4

/** Sensor macro defines */
#define SENSOR_TYPE_NONE    0x00
#define SENSOR_TYPE_RAIN    0x01  // rain sensor
#define SENSOR_TYPE_FLOW    0x02  // flow sensor
#define SENSOR_TYPE_SOIL    0x03  // soil moisture sensor
#define SENSOR_TYPE_PSWITCH 0xF0  // program switch sensor
#define SENSOR_TYPE_OTHER   0xFF

#define FLOWCOUNT_RT_WINDOW   1000  // flow count divisor (for computing real-time flow rate)

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
#define OS_WIFI_MODE_AP       0xA9
#define OS_WIFI_MODE_STA      0x2A

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
#define MAX_PROGRAMMED_DURATION 64800U // maximum stored/manual/remote-command duration (18 hours)
#define MAX_RUNTIME_DURATION (7UL * 24UL * 60UL * 60UL) // maximum effective station run time after adjustments
#define STATION_NAME_SIZE 32    // maximum number of characters in each station name
#define MAX_SOPTS_SIZE    320   // maximum string option size

#if defined(ARDUINO)
#define LOG_SPRINKLER_MAX_KB  1200  // max total size of sprinkler .txt log files in KB (~1.2 MB)
#endif

#define MAX_SENSORS 64
#define SENSOR_LOG_MAGIC            0x55
#define SENSOR_LOG_VERSION          0x01
#define SENSOR_LOG_MAX_FILES        50    // number of data files in the rotation
#if defined(ARDUINO)
	#define SENSOR_LOG_RECORDS_PER_FILE 819    // records per file; 819×10 B = 8 190 B fits in one 8 KB LittleFS block
#else
	// Linux/OSPi/DEMO: 50 × 16 384 = 819 200 records (about 8.2 MB).
	#define SENSOR_LOG_RECORDS_PER_FILE 16384
#endif

#define STATION_SPECIAL_DATA_SIZE  (TMP_BUFFER_SIZE - STATION_NAME_SIZE - 12)

/** Default string option values */
#define DEFAULT_PASSWORD          "a6d82bced638de3def1e9bbb4983225c"  // md5 of 'opendoor'
#define DEFAULT_LOCATION          "42.36,-71.06"  // Boston,MA
#define DEFAULT_JAVASCRIPT_URL    "https://ui.opensprinkler.com/js"
#define DEFAULT_WEATHER_URL       "weather.opensprinkler.com"
#define DEFAULT_IFTTT_URL         "maker.ifttt.com"
#define DEFAULT_OTC_SERVER_DEV     "ws.cloud.openthings.io"
#define DEFAULT_OTC_PORT_DEV       80
#define DEFAULT_OTC_SERVER_APP    "cloud.openthings.io"
#define DEFAULT_OTC_PORT_APP       443
#define DEFAULT_OTC_TOKEN_LENGTH   32
#define DEFAULT_DEVICE_NAME       "My OpenSprinkler"
#define DEFAULT_EMPTY_STRING      ""
#define DEFAULT_UNDERCURRENT_THRESHOLD 100 // in mA
#define DEFAULT_OVERCURRENT_LIMIT 1200 // in mA
#define OVERCURRENT_INRUSH_EXTRA   600 // in mA, extra margin for inrush
#define OVERCURRENT_DC_EXTRA      1200 // in mA, extra margin for DC controller
#define DEFAULT_LATCH_BOOST_VOLTAGE  9 // default latch boost voltage in volt
#define DEFAULT_TARGET_PD_VOLTAGE   75 // default target voltage (unit: 100mV, so 75 means 7500mV ot 7.5V)

/* Weather Adjustment Methods */
enum {
	WEATHER_METHOD_MANUAL = 0,
	WEATHER_METHOD_ZIMMERMAN,
	WEATHER_METHOD_AUTORAINDELAY,
	WEATHER_METHOD_ETO,
	WEATHER_METHOD_MONTHLY,
	NUM_WEATHER_METHODS
};

/* Master */
enum {
	MASTER_1 = 0,
	MASTER_2,
	MASTER_3,
	MASTER_4,
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
	IOPT_NOTIF2_ENABLE,
	IOPT_I_MIN_THRESHOLD,
	IOPT_I_MAX_LIMIT,
	IOPT_TARGET_PD_VOLTAGE,
	IOPT_RESERVE_7,
	IOPT_RESERVE_8,
	IOPT_WIFI_MODE, //ro
	IOPT_RESET,     //ro
	IOPT_MASTER_STATION_3,
	IOPT_MASTER_ON_ADJ_3,
	IOPT_MASTER_OFF_ADJ_3,
	IOPT_MASTER_STATION_4,
	IOPT_MASTER_ON_ADJ_4,
	IOPT_MASTER_OFF_ADJ_4,
	IOPT_SENSOR3_TYPE,
	IOPT_SENSOR3_OPTION,
	IOPT_SENSOR3_ON_DELAY,
	IOPT_SENSOR3_OFF_DELAY,
	IOPT_SENSOR4_TYPE,
	IOPT_SENSOR4_OPTION,
	IOPT_SENSOR4_ON_DELAY,
	IOPT_SENSOR4_OFF_DELAY,
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
#define LOGDATA_SENSOR3    0x06
#define LOGDATA_SENSOR4    0x07
#define LOGDATA_CURRENT    0x80

#undef OS_HW_VERSION

/** Hardware defines */
#include "boards/board_profile.h"

// Compatibility names for callers while pin access migrates to board profiles.
#define PIN_BUTTON_1    (osboard::active().pins.buttons[0])
#define PIN_BUTTON_2    (osboard::active().pins.buttons[1])
#define PIN_BUTTON_3    (osboard::active().pins.buttons[2])
#define PIN_SENSOR1     (osboard::active().pins.sensors[0])
#define PIN_SENSOR2     (osboard::active().pins.sensors[1])
#define PIN_SENSOR3     (osboard::active().pins.sensors[2])
#define PIN_SENSOR4     (osboard::active().pins.sensors[3])
#define PIN_RFRX        (osboard::active().pins.rf_rx)
#define PIN_RFTX        (osboard::active().pins.rf_tx)
#define PIN_BOOST       (osboard::active().pins.boost)
#define PIN_BOOST_EN    (osboard::active().pins.boost_enable)
#define PIN_LATCH_COM   (osboard::active().pins.latch_common)
#define PIN_LATCH_COMA  (osboard::active().pins.latch_common_anode)
#define PIN_LATCH_COMK  (osboard::active().pins.latch_common_cathode)
#define PIN_IOEXP_INT   (osboard::active().pins.io_expander_interrupt)

#if defined(ESP8266) // for ESP8266

	#define OS_HW_VERSION    (OS_HW_VERSION_BASE+30)
	#define IOEXP_PIN        osboard::IO_EXPANDER_PIN_BASE
	#define MAIN_I2CADDR     osboard::MAIN_IO_EXPANDER_ADDRESS
	#define ACDR_I2CADDR     osboard::AC_DRIVER_ADDRESS
	#define DCDR_I2CADDR     osboard::DC_DRIVER_ADDRESS
	#define LADR_I2CADDR     osboard::LATCH_DRIVER_ADDRESS
	#define EXP_I2CADDR_BASE osboard::EXPANDER_ADDRESS_BASE
	#define LCD_I2CADDR      osboard::LCD_ADDRESS
	#define EEPROM_I2CADDR   osboard::EEPROM_ADDRESS
	#define CH224_I2CADDR    osboard::CH224_ADDRESS

	#define PIN_CURR_SENSE    A0    // current sensing pin
	#define PIN_LATCH_VOLT_SENSE A0 // latch voltage sensing pin
	#define PIN_FREE_LIST     {} // no free GPIO pin at the moment
	#define ETHER_BUFFER_SIZE   2048
	#define ETHER_BUFFER_ALLOC_SIZE   ETHER_BUFFER_SIZE

	#define PIN_ETHER_CS       osboard::ETHERNET_CS_PIN
	#define ETHER_SPI_CLOCK    osboard::ETHERNET_SPI_CLOCK_HZ

	#define USE_DISPLAY

#elif defined(ESP32) // OpenSprinkler v4.0 ESP32-C6

	#define OS_HW_VERSION    (OS_HW_VERSION_BASE + osboard::OS40_HARDWARE_VERSION)
	#define IOEXP_PIN        osboard::IO_EXPANDER_PIN_BASE
	#define MAIN_I2CADDR     osboard::MAIN_IO_EXPANDER_ADDRESS
	#define EXP_I2CADDR_BASE osboard::EXPANDER_ADDRESS_BASE
	#define LCD_I2CADDR      osboard::LCD_ADDRESS
	#define RTC_I2CADDR      osboard::RTC_ADDRESS
	#define CH224_I2CADDR    osboard::CH224_ADDRESS

	#define PIN_CURR_SENSE   osboard::OS40_CURRENT_SENSE_PIN
	#define PIN_FLASH_CS     osboard::OS40_EXTERNAL_FLASH_CS_PIN
	#define PIN_ETHER_IRQ    osboard::OS40_ETHERNET_IRQ_PIN
	#define PIN_ETHER_RESET  osboard::OS40_ETHERNET_RESET_PIN
	#define PIN_ETHER_CS     osboard::OS40_ETHERNET_CS_PIN
	#define PIN_SPI_MOSI     osboard::OS40_SPI_MOSI_PIN
	#define PIN_SPI_MISO     osboard::OS40_SPI_MISO_PIN
	#define PIN_SPI_SCK      osboard::OS40_SPI_CLOCK_PIN
	#define PIN_I2C_SCL      osboard::OS40_I2C_CLOCK_PIN
	#define PIN_I2C_SDA      osboard::OS40_I2C_DATA_PIN
	#define ETHER_SPI_CLOCK  osboard::ETHERNET_SPI_CLOCK_HZ
	#define ETHER_BUFFER_SIZE 4096
	#define ETHER_BUFFER_ALLOC_SIZE ETHER_BUFFER_SIZE
	#define PIN_FREE_LIST    {}

	#define USE_DISPLAY

#elif defined(OSPI) // for OSPi

	#define OS_HW_VERSION    OSPI_HW_VERSION_BASE
	#define PIN_SR_LATCH      osboard::OSPI_SHIFT_LATCH_PIN
	#define PIN_SR_DATA       osboard::OSPI_SHIFT_DATA_PIN
	#define PIN_SR_DATA_ALT   osboard::OSPI_SHIFT_DATA_ALT_PIN
	#define PIN_SR_CLOCK      osboard::OSPI_SHIFT_CLOCK_PIN
	#define PIN_SR_OE         osboard::OSPI_SHIFT_OUTPUT_ENABLE_PIN

	#define PIN_FREE_LIST       {5,6,7,8,9,11,12,13,16,19,20,21,23,25,26}  // free GPIO pins
	#define ETHER_BUFFER_SIZE   8192
	#define ETHER_BUFFER_ALLOC_SIZE   ETHER_BUFFER_SIZE

	#define SDA 0
	#define SCL 0

	#define USE_DISPLAY

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
	#define PIN_FREE_LIST  {}
	#define ETHER_BUFFER_SIZE   8192  // HTTP client send/receive (weather, notifier, remote station)
	#define ETHER_BUFFER_ALLOC_SIZE   ETHER_BUFFER_SIZE

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

	#if defined(ARDUINO)
	// work-around for PIN_SENSOR1 on OS3.2 and above
	#define DEBUG_BEGIN(x)   {Serial.begin(115200); Serial.end();}
	#else
	#define DEBUG_BEGIN(x)   {}
	#endif
	#define DEBUG_PRINT(x)   {}
	#define DEBUG_PRINTLN(x) {}
	#define DEBUG_PRINTF(x, ...)  {}

#endif

/** Re-define arduino-specific (e.g. PGM) types to use standard types */
#if !defined(ARDUINO)
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
	#include <stddef.h>
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
