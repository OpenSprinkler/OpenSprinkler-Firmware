/* OpenSprinkler Unified (AVR/RPI/BBB/LINUX) Firmware
 * Copyright (C) 2015 by Ray Wang (ray@opensprinkler.com)
 *
 * Utility functions
 * Sep 2022 @ OpenSprinklerShop
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

#include "sensors.h"

#include <stdlib.h>

#include "OpenSprinkler.h"
#ifdef ESP8266
#include "Wire.h"
#else
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <modbus/modbus.h>
#include <modbus/modbus-rtu.h>
#include <errno.h>
#endif
#include "defines.h"
#include "opensprinkler_server.h"
#include "program.h"
#include "sensor_mqtt.h"
#include "utils.h"
#include "weather.h"
#include "osinfluxdb.h"
#ifdef ADS1115
#include "sensor_ospi_ads1115.h"
#endif
#ifdef PCF8591
#include "sensor_ospi_pcf8591.h"
#endif

#define MAX_RS485_DEVICES 4

unsigned char findKeyVal(const char *str, char *strbuf, uint16_t maxlen, const char *key,
                bool key_in_pgm = false, uint8_t *keyfound = NULL);

// All sensors:
static Sensor_t *sensors = NULL;
static time_t last_save_time = 0;
static boolean apiInit = false;
static Sensor_t *current_sensor = NULL;

// Boards:
static uint16_t asb_detected_boards = 0;  // bit 1=0x48+0x49 bit 2=0x4A+0x4B usw

// Sensor URLS:
static SensorUrl_t *sensorUrls = NULL;

// Program sensor data
static ProgSensorAdjust_t *progSensorAdjusts = NULL;

// Monitor data
static Monitor_t *monitors = NULL;

// modbus transaction id
static uint16_t modbusTcpId = 0;
#ifdef ESP8266
static uint i2c_rs485_allocated[MAX_RS485_DEVICES];
#else
static modbus_t * ttyDevices[MAX_RS485_DEVICES];
#endif

const char *sensor_unitNames[]{
    "",  "%", "°C", "°F", "V", "%", "in", "mm", "mph", "kmh", "%", "DK", "LM", "LX"
    //0   1     2     3    4    5    6     7      8      9     10,  11,   12,   13
    //   0=Nothing
    //   1=Soil moisture
    //   2=degree celsius temperature
    //   3=degree fahrenheit temperature
    //   4=Volt V
    //   5=Humidity %
    //   6=Rain inch
    //   7=Rain mm
    //   8=Wind mph
    //   9=Wind kmh
    //  10=Level %
    //  11=DK (Permitivität)
    //  12=LM (Lumen)
    //  13=LX (LUX)
};
uint8_t logFileSwitch[3] = {0, 0, 0};  // 0=use smaller File, 1=LOG1, 2=LOG2

// Weather
time_t last_weather_time = 0;
bool current_weather_ok = false;
double current_temp = 0.0;
double current_humidity = 0.0;
double current_precip = 0.0;
double current_wind = 0.0;

uint16_t CRC16(unsigned char buf[], int len) {
  uint16_t crc = 0xFFFF;

  for (int pos = 0; pos < len; pos++) {
    crc ^= (uint16_t)buf[pos];      // XOR byte into least sig. byte of crc
    for (int i = 8; i != 0; i--) {  // Loop over each bit
      if ((crc & 0x0001) != 0) {    // If the LSB is set
        crc >>= 1;                  // Shift right and XOR 0xA001
        crc ^= 0xA001;
      } else        // Else LSB is not set
        crc >>= 1;  // Just shift right
    }
  }
  // Note, this number has low and high bytes swapped, so use it accordingly (or
  // swap bytes)
  return crc;
}  // End: CRC16

/**
 * @brief detect connected boards
 *
 */
void detect_asb_board() {
  // detect analog sensor board, 0x48+0x49=Board1, 0x4A+0x4B=Board2
#if defined(ESP8266)
  if (detect_i2c(ASB_BOARD_ADDR1a) && detect_i2c(ASB_BOARD_ADDR1b))
    asb_detected_boards |= ASB_BOARD1;
  if (detect_i2c(ASB_BOARD_ADDR2a) && detect_i2c(ASB_BOARD_ADDR2b))
    asb_detected_boards |= ASB_BOARD2;

  if (detect_i2c(RS485_TRUEBNER1_ADDR)) asb_detected_boards |= RS485_TRUEBNER1;
  if (detect_i2c(RS485_TRUEBNER2_ADDR)) asb_detected_boards |= RS485_TRUEBNER2;
  if (detect_i2c(RS485_TRUEBNER3_ADDR)) asb_detected_boards |= RS485_TRUEBNER3;
  if (detect_i2c(RS485_TRUEBNER4_ADDR)) asb_detected_boards |= RS485_TRUEBNER4;
#endif

// Old, pre OSPi 1.43 analog inputs:
#if defined(PCF8591)
  asb_detected_boards |= OSPI_PCF8591;
#endif

// New OSPi 1.6 analog inputs:
#if defined(ADS1115)
  asb_detected_boards |= OSPI_ADS1115;
#endif
  DEBUG_PRINT("ASB DETECT=");
  DEBUG_PRINTLN(asb_detected_boards);

  for (int log = 0; log <= 2; log++) {
    checkLogSwitch(log);
#if defined(ENABLE_DEBUG)
    DEBUG_PRINT("log=");
    DEBUG_PRINTLN(log);
    const char *f1 = getlogfile(log);
    DEBUG_PRINT("logfile1=");
    DEBUG_PRINTLN(f1);
    DEBUG_PRINT("size1=");
    DEBUG_PRINTLN(file_size(f1));
    const char *f2 = getlogfile2(log);
    DEBUG_PRINT("logfile2=");
    DEBUG_PRINTLN(f2);
    DEBUG_PRINT("size2=");
    DEBUG_PRINTLN(file_size(f2));
#endif
  }
}

uint16_t get_asb_detected_boards() { return asb_detected_boards; }
/*
 * init sensor api and load data
 */
void sensor_api_init(boolean detect_boards) {
  apiInit = true;
  if (detect_boards)
    detect_asb_board();
  sensor_load();
  prog_adjust_load();
  sensor_mqtt_init();
  monitor_load();
#ifndef ESP8266
  //Read rs485 file. Details see below
  std::ifstream file;
  file.open("rs485", std::ifstream::in);
  if (!file.fail()) {
    std::string tty;
    int idx = 0;
    int n = 0;
    DEBUG_PRINTLN(F("Opening USB RS485 Adapters:"));
    while (std::getline(file, tty)) {
      modbus_t * ctx = modbus_new_rtu(tty.c_str(), 9600, 'E', 8, 1);
      DEBUG_PRINT(idx);
      DEBUG_PRINT(": ");
      DEBUG_PRINTLN(tty.c_str());

      //unavailable on Raspi? modbus_enable_quirks(ctx, MODBUS_QUIRK_MAX_SLAVE);
      modbus_rtu_set_serial_mode(ctx, MODBUS_RTU_RS485);
      modbus_rtu_set_rts(ctx, MODBUS_RTU_RTS_NONE); // we use auto RTS function by the HAT
      modbus_set_response_timeout(ctx, 1, 500000); // 1.5s
      if (modbus_connect(ctx) == -1) {
        modbus_free(ctx);
      } else {
        n++;
        ttyDevices[idx] = ctx;
        asb_detected_boards |= OSPI_USB_RS485;
        #ifdef ENABLE_DEBUG
        modbus_set_debug(ctx, TRUE);
        DEBUG_PRINTLN(F("DEBUG ENABLED"));
        #endif
      }
      idx++;
      if (idx >= MAX_RS485_DEVICES)
        break;
    }
    DEBUG_PRINT(F("Found "));
    DEBUG_PRINT(n);
    DEBUG_PRINTLN(F(" RS485 Adapters"));
  }
#endif
}

void sensor_save_all() {
  sensor_save();
  prog_adjust_save();
  SensorUrl_save();
  monitor_save();
#ifndef ESP8266
  for (int i = 0; i < MAX_RS485_DEVICES; i++) {
    if (ttyDevices[i]) {
      modbus_close(ttyDevices[i]);
      modbus_free(ttyDevices[i]);
    }
    ttyDevices[i] = NULL;
  }
#endif
}

/**
 * @brief Unload sensorapi from memory, free everything. Be sure that you have save all before
 * 
 */
void sensor_api_free() {
  DEBUG_PRINTLN("sensor_api_free1");
  apiInit = false;
  current_sensor = NULL;
  os.mqtt.setCallback(2, NULL);

  while (progSensorAdjusts) {
    ProgSensorAdjust_t* next = progSensorAdjusts->next;
    delete progSensorAdjusts;
    progSensorAdjusts = next;
  }

  DEBUG_PRINTLN("sensor_api_free2");

  while (sensorUrls) {
    SensorUrl_t* next = sensorUrls->next;
    free(sensorUrls->urlstr);
    delete sensorUrls;
    sensorUrls = next;
  }

  DEBUG_PRINTLN("sensor_api_free3");

  while (monitors) {
    Monitor_t* next = monitors->next;
    delete monitors;
    monitors = next;
  }

  DEBUG_PRINTLN("sensor_api_free4");

  while (sensors) {
    Sensor_t* next = sensors->next;
    delete sensors;
    sensors = next;
  }

  modbusTcpId = 0;
  #ifdef ESP8266
  memset(i2c_rs485_allocated, 0, sizeof(i2c_rs485_allocated));
  #endif
  DEBUG_PRINTLN("sensor_api_free5");
}

/*
 * get list of all configured sensors
 */
Sensor_t *getSensors() { return sensors; }
/**
 * @brief delete a sensor
 *
 * @param nr
 */
int sensor_delete(uint nr) {
  Sensor_t *sensor = sensors;
  Sensor_t *last = NULL;
  while (sensor) {
    if (sensor->nr == nr) {
      if (last == NULL)
        sensors = sensor->next;
      else
        last->next = sensor->next;
      delete sensor;
      sensor_save();
      return HTTP_RQT_SUCCESS;
    }
    last = sensor;
    sensor = sensor->next;
  }
  return HTTP_RQT_NOT_RECEIVED;
}

/**
 * @brief define or insert a sensor
 *
 * @param nr  > 0
 * @param type > 0 add/modify, 0=delete
 * @param group group assignment
 * @param ip
 * @param port
 * @param id
 */
int sensor_define(uint nr, const char *name, uint type, uint group, uint32_t ip,
                  uint port, uint id, uint ri, int16_t factor, int16_t divider,
                  const char *userdef_unit, int16_t offset_mv, int16_t offset2,
                  SensorFlags_t flags, int16_t assigned_unitid) {
  if (nr == 0 || type == 0) return HTTP_RQT_NOT_RECEIVED;
  if (ri < 10) ri = 10;

  DEBUG_PRINTLN(F("server_define"));

  Sensor_t *sensor = sensors;
  Sensor_t *last = NULL;
  while (sensor) {
    if (sensor->nr == nr) {  // Modify existing
      sensor->type = type;
      sensor->group = group;
      strncpy(sensor->name, name, sizeof(sensor->name) - 1);
      sensor->ip = ip;
      sensor->port = port;
      sensor->id = id;
      sensor->read_interval = ri;
      sensor->factor = factor;
      sensor->divider = divider;
      sensor->offset_mv = offset_mv;
      sensor->offset2 = offset2;
      strncpy(sensor->userdef_unit, userdef_unit,
              sizeof(sensor->userdef_unit) - 1);
      sensor->flags = flags;
      if (assigned_unitid >= 0) sensor->assigned_unitid = assigned_unitid;
      else sensor->assigned_unitid = 0;
      sensor_save();
      return HTTP_RQT_SUCCESS;
    }

    if (sensor->nr > nr) break;

    last = sensor;
    sensor = sensor->next;
  }

  DEBUG_PRINTLN(F("server_define2"));

  // Insert new
  Sensor_t *new_sensor = new Sensor_t;
  memset(new_sensor, 0, sizeof(Sensor_t));
  new_sensor->nr = nr;
  new_sensor->type = type;
  new_sensor->group = group;
  strncpy(new_sensor->name, name, sizeof(new_sensor->name) - 1);
  new_sensor->ip = ip;
  new_sensor->port = port;
  new_sensor->id = id;
  new_sensor->read_interval = ri;
  new_sensor->factor = factor;
  new_sensor->divider = divider;
  new_sensor->offset_mv = offset_mv;
  new_sensor->offset2 = offset2;
  strncpy(new_sensor->userdef_unit, userdef_unit,
          sizeof(new_sensor->userdef_unit) - 1);
  new_sensor->flags = flags;
  if (assigned_unitid >= 0) new_sensor->assigned_unitid = assigned_unitid;
  else new_sensor->assigned_unitid = 0;

  if (last) {
    new_sensor->next = last->next;
    last->next = new_sensor;
  } else {
    new_sensor->next = sensors;
    sensors = new_sensor;
  }
  sensor_save();
  return HTTP_RQT_SUCCESS;
}

int sensor_define_userdef(uint nr, int16_t factor, int16_t divider,
                          const char *userdef_unit, int16_t offset_mv,
                          int16_t offset2, int16_t assigned_unitid) {
  Sensor_t *sensor = sensor_by_nr(nr);
  if (!sensor) return HTTP_RQT_NOT_RECEIVED;

  sensor->factor = factor;
  sensor->divider = divider;
  sensor->offset_mv = offset_mv;
  sensor->offset2 = offset2;
  sensor->assigned_unitid = assigned_unitid;
  if (userdef_unit)
    strncpy(sensor->userdef_unit, userdef_unit,
            sizeof(sensor->userdef_unit) - 1);
  else
    memset(sensor->userdef_unit, 0, sizeof(sensor->userdef_unit));

  return HTTP_RQT_SUCCESS;
}

/**
 * @brief initial load stored sensor definitions
 *
 */
void sensor_load() {
  // DEBUG_PRINTLN(F("sensor_load"));
  sensors = NULL;
  current_sensor = NULL;
  if (!file_exists(SENSOR_FILENAME)) return;

  ulong pos = 0;
  Sensor_t *last = NULL;
  while (true) {
    Sensor_t *sensor = new Sensor_t;
    memset(sensor, 0, sizeof(Sensor_t));
    file_read_block(SENSOR_FILENAME, sensor, pos, SENSOR_STORE_SIZE);
    if (!sensor->nr || !sensor->type) {
      delete sensor;
      break;
    }
    if (!last)
      sensors = sensor;
    else
      last->next = sensor;
    last = sensor;
    sensor->flags.data_ok = false;
    sensor->next = NULL;
    pos += SENSOR_STORE_SIZE;
  }

  SensorUrl_load();
  last_save_time = os.now_tz();
}

/**
 * @brief Store sensor data
 *
 */
void sensor_save() {
  if (!apiInit) return;
  DEBUG_PRINTLN(F("sensor_save"));
  if (file_exists(SENSOR_FILENAME_BAK)) remove_file(SENSOR_FILENAME_BAK);
  if (file_exists(SENSOR_FILENAME))
    rename_file(SENSOR_FILENAME, SENSOR_FILENAME_BAK);

  ulong pos = 0;
  Sensor_t *sensor = sensors;
  while (sensor) {
    file_write_block(SENSOR_FILENAME, sensor, pos, SENSOR_STORE_SIZE);
    sensor = sensor->next;
    pos += SENSOR_STORE_SIZE;
  }

  last_save_time = os.now_tz();
  DEBUG_PRINTLN(F("sensor_save2"));
  current_sensor = NULL;
}

uint sensor_count() {
  // DEBUG_PRINTLN(F("sensor_count"));
  Sensor_t *sensor = sensors;
  uint count = 0;
  while (sensor) {
    count++;
    sensor = sensor->next;
  }
  return count;
}

Sensor_t *sensor_by_nr(uint nr) {
  // DEBUG_PRINTLN(F("sensor_by_nr"));
  Sensor_t *sensor = sensors;
  while (sensor) {
    if (sensor->nr == nr) return sensor;
    sensor = sensor->next;
  }
  return NULL;
}

Sensor_t *sensor_by_idx(uint idx) {
  // DEBUG_PRINTLN(F("sensor_by_idx"));
  Sensor_t *sensor = sensors;
  uint count = 0;
  while (sensor) {
    if (count == idx) return sensor;
    count++;
    sensor = sensor->next;
  }
  return NULL;
}

// LOGGING METHODS:

/**
 * @brief getlogfile name
 *
 * @param log
 * @return const char*
 */
const char *getlogfile(uint8_t log) {
  uint8_t sw = logFileSwitch[log];
  switch (log) {
    case LOG_STD:
      return sw < 2 ? SENSORLOG_FILENAME1 : SENSORLOG_FILENAME2;
    case LOG_WEEK:
      return sw < 2 ? SENSORLOG_FILENAME_WEEK1 : SENSORLOG_FILENAME_WEEK2;
    case LOG_MONTH:
      return sw < 2 ? SENSORLOG_FILENAME_MONTH1 : SENSORLOG_FILENAME_MONTH2;
  }
  return "";
}

/**
 * @brief getlogfile name2 (opposite file)
 *
 * @param log
 * @return const char*
 */
const char *getlogfile2(uint8_t log) {
  uint8_t sw = logFileSwitch[log];
  switch (log) {
    case 0:
      return sw < 2 ? SENSORLOG_FILENAME2 : SENSORLOG_FILENAME1;
    case 1:
      return sw < 2 ? SENSORLOG_FILENAME_WEEK2 : SENSORLOG_FILENAME_WEEK1;
    case 2:
      return sw < 2 ? SENSORLOG_FILENAME_MONTH2 : SENSORLOG_FILENAME_MONTH1;
  }
  return "";
}

void checkLogSwitch(uint8_t log) {
  if (logFileSwitch[log] == 0) {  // Check file size, use smallest
    ulong logFileSize1 = file_size(getlogfile(log));
    ulong logFileSize2 = file_size(getlogfile2(log));
    if (logFileSize1 < logFileSize2)
      logFileSwitch[log] = 1;
    else
      logFileSwitch[log] = 2;
  }
}

void checkLogSwitchAfterWrite(uint8_t log) {
  ulong size = file_size(getlogfile(log));
  if ((size / SENSORLOG_STORE_SIZE) >= MAX_LOG_SIZE) {  // switch logs if max reached
    if (logFileSwitch[log] == 1)
      logFileSwitch[log] = 2;
    else
      logFileSwitch[log] = 1;
    remove_file(getlogfile(log));
  }
}

bool sensorlog_add(uint8_t log, SensorLog_t *sensorlog) {
#if defined(ESP8266)
  if (checkDiskFree()) {
#endif
    DEBUG_PRINT(F("sensorlog_add "));
    DEBUG_PRINT(log);
    checkLogSwitch(log);
    file_append_block(getlogfile(log), sensorlog, SENSORLOG_STORE_SIZE);
    checkLogSwitchAfterWrite(log);
    DEBUG_PRINT(F("="));
    DEBUG_PRINTLN(sensorlog_filesize(log));
    return true;
#if defined(ESP8266)
  }
  return false;
#endif
}

bool sensorlog_add(uint8_t log, Sensor_t *sensor, ulong time) {

  if (sensor->flags.data_ok && sensor->flags.log && time > 1000) {

    // Write to log file only if necessary
    if (time-sensor->last_logged_time > 86400 || abs(sensor->last_data - sensor->last_logged_data) > 0.00999) {
      SensorLog_t sensorlog;
      memset(&sensorlog, 0, sizeof(SensorLog_t));
      sensorlog.nr = sensor->nr;
      sensorlog.time = time;
      sensorlog.native_data = sensor->last_native_data;
      sensorlog.data = sensor->last_data;
      sensor->last_logged_data = sensor->last_data;
      sensor->last_logged_time = time;

      if (!sensorlog_add(log, &sensorlog)) {
        sensor->flags.log = 0;
        return false;
      }
    }
    return true;
  }
  return false;
}

ulong sensorlog_filesize(uint8_t log) {
  // DEBUG_PRINT(F("sensorlog_filesize "));
  checkLogSwitch(log);
  ulong size = file_size(getlogfile(log)) + file_size(getlogfile2(log));
  // DEBUG_PRINTLN(size);
  return size;
}

ulong sensorlog_size(uint8_t log) {
  // DEBUG_PRINT(F("sensorlog_size "));
  ulong size = sensorlog_filesize(log) / SENSORLOG_STORE_SIZE;
  // DEBUG_PRINTLN(size);
  return size;
}

void sensorlog_clear_all() { sensorlog_clear(true, true, true); }

void sensorlog_clear(bool std, bool week, bool month) {
  DEBUG_PRINTLN(F("sensorlog_clear "));
  DEBUG_PRINT(std);
  DEBUG_PRINT(week);
  DEBUG_PRINT(month);
  if (std) {
    remove_file(SENSORLOG_FILENAME1);
    remove_file(SENSORLOG_FILENAME2);
    logFileSwitch[LOG_STD] = 1;
  }
  if (week) {
    remove_file(SENSORLOG_FILENAME_WEEK1);
    remove_file(SENSORLOG_FILENAME_WEEK2);
    logFileSwitch[LOG_WEEK] = 1;
  }
  if (month) {
    remove_file(SENSORLOG_FILENAME_MONTH1);
    remove_file(SENSORLOG_FILENAME_MONTH2);
    logFileSwitch[LOG_MONTH] = 1;
  }
}

ulong sensorlog_clear_sensor(uint sensorNr, uint8_t log, bool use_under,
                             double under, bool use_over, double over, time_t before, time_t after) {
  SensorLog_t sensorlog;
  checkLogSwitch(log);
  const char *flast = getlogfile2(log);
  const char *fcur = getlogfile(log);
  ulong size = file_size(flast) / SENSORLOG_STORE_SIZE;
  ulong size2 = size + file_size(fcur) / SENSORLOG_STORE_SIZE;
  const char *f;
  ulong idxr = 0;
  ulong n = 0;
  DEBUG_PRINTLN(F("clearlog1"));
  DEBUG_PRINTF("nr: %d log: %d under:%lf over: %lf before: %lld after: %lld size: %ld size2: %ld\n", sensorNr, log, under, over, before, after, size, size2);
  while (idxr < size2) {
    ulong idx = idxr;
    if (idx >= size) {
      idx -= size;
      f = fcur;
    } else {
      f = flast;
    }

    ulong result = file_read_block(f, &sensorlog, idx * SENSORLOG_STORE_SIZE,
                                   SENSORLOG_STORE_SIZE);
    if (result == 0) break;
    if (sensorlog.nr > 0 && (sensorlog.nr == sensorNr || sensorNr == 0)) {
      DEBUG_PRINTF("clearlog2 idx=%ld idx2=%ld\n", idx, idxr);
      boolean found = false;
      if (use_under && sensorlog.data < under) found = true;
      if (use_over && sensorlog.data > over) found = true;
      if (before && sensorlog.time < before) found = true;
      if (after && sensorlog.time > after) found = true;
      if (sensorNr > 0 && sensorlog.nr != sensorNr) found = false;
      if (sensorNr > 0 && sensorlog.nr == sensorNr && !use_under && !use_over && !before && !after) found = true;
      if (found) {
        sensorlog.nr = 0;
        file_write_block(f, &sensorlog, idx * SENSORLOG_STORE_SIZE,
                         SENSORLOG_STORE_SIZE);
        DEBUG_PRINTF("clearlog3 idx=%ld idxr=%ld\n", idx, idxr);
        n++;
      }
    }
    idxr++;
  }
  DEBUG_PRINTF("clearlog4 n=%ld\n", n);
  return n;
}

SensorLog_t *sensorlog_load(uint8_t log, ulong idx) {
  SensorLog_t *sensorlog = new SensorLog_t;
  return sensorlog_load(log, idx, sensorlog);
}

SensorLog_t *sensorlog_load(uint8_t log, ulong idx, SensorLog_t *sensorlog) {
  // DEBUG_PRINTLN(F("sensorlog_load"));

  // Map lower idx to the other log file
  checkLogSwitch(log);
  const char *flast = getlogfile2(log);
  const char *fcur = getlogfile(log);
  ulong size = file_size(flast) / SENSORLOG_STORE_SIZE;
  const char *f;
  if (idx >= size) {
    idx -= size;
    f = fcur;
  } else {
    f = flast;
  }

  file_read_block(f, sensorlog, idx * SENSORLOG_STORE_SIZE,
                  SENSORLOG_STORE_SIZE);
  return sensorlog;
}

int sensorlog_load2(uint8_t log, ulong idx, int count, SensorLog_t *sensorlog) {
  // DEBUG_PRINTLN(F("sensorlog_load"));

  // Map lower idx to the other log file
  checkLogSwitch(log);
  const char *flast = getlogfile2(log);
  const char *fcur = getlogfile(log);
  ulong size = file_size(flast) / SENSORLOG_STORE_SIZE;
  const char *f;
  if (idx >= size) {
    idx -= size;
    f = fcur;
    size = file_size(f) / SENSORLOG_STORE_SIZE;
  } else {
    f = flast;
  }

  if (idx + count > size) count = size - idx;
  if (count <= 0) return 0;
  file_read_block(f, sensorlog, idx * SENSORLOG_STORE_SIZE,
                  count * SENSORLOG_STORE_SIZE);
  return count;
}

ulong findLogPosition(uint8_t log, ulong after) {
  ulong log_size = sensorlog_size(log);
  ulong a = 0;
  ulong b = log_size - 1;
  ulong lastIdx = 0;
  SensorLog_t sensorlog;
  while (true) {
    ulong idx = (b - a) / 2 + a;
    sensorlog_load(log, idx, &sensorlog);
    if (sensorlog.time < after) {
      a = idx;
    } else if (sensorlog.time > after) {
      b = idx;
    }
    if (a >= b || idx == lastIdx) return idx;
    lastIdx = idx;
  }
  return 0;
}

#if !defined(ARDUINO)
/**
 * compatibility functions for OSPi:
 **/
#define timeSet 0
int timeStatus() { return timeSet; }

void dtostrf(float value, int min_width, int precision, char *txt) {
  printf(txt, "%*.*f", min_width, precision, value);
}

void dtostrf(double value, int min_width, int precision, char *txt) {
  printf(txt, "%*.*d", min_width, precision, value);
}
#endif

// 1/4 of a day: 6*60*60
#define BLOCKSIZE 64
#define CALCRANGE_WEEK 21600
#define CALCRANGE_MONTH 172800
static ulong next_week_calc = 0;
static ulong next_month_calc = 0;

/**
Calculate week+month Data
We store only the average value of 6 hours utc
**/
void calc_sensorlogs() {
  if (!sensors || timeStatus() != timeSet) return;

  ulong log_size = sensorlog_size(LOG_STD);
  if (log_size == 0) return;

  SensorLog_t *sensorlog = NULL;

  time_t time = os.now_tz();
  time_t last_day = time;

  if (time >= next_week_calc) {
    DEBUG_PRINTLN(F("calc_sensorlogs WEEK start"));
    sensorlog = (SensorLog_t *)malloc(sizeof(SensorLog_t) * BLOCKSIZE);
    ulong size = sensorlog_size(LOG_WEEK);
    if (size == 0) {
      sensorlog_load(LOG_STD, 0, sensorlog);
      last_day = sensorlog->time;
    } else {
      sensorlog_load(LOG_WEEK, size - 1, sensorlog);  // last record
      last_day = sensorlog->time + CALCRANGE_WEEK;    // Skip last Range
    }
    time_t fromdate = (last_day / CALCRANGE_WEEK) * CALCRANGE_WEEK;
    time_t todate = fromdate + CALCRANGE_WEEK;

    // 4 blocks per day

    while (todate < time) {
      ulong startidx = findLogPosition(LOG_STD, fromdate);
      Sensor_t *sensor = sensors;
      while (sensor) {
        if (sensor->flags.enable && sensor->flags.log) {
          ulong idx = startidx;
          double data = 0;
          ulong n = 0;
          bool done = false;
          while (!done) {
            int sn = sensorlog_load2(LOG_STD, idx, BLOCKSIZE, sensorlog);
            if (sn <= 0) break;
            for (int i = 0; i < sn; i++) {
              idx++;
              if (sensorlog[i].time >= todate) {
                done = true;
                break;
              }
              if (sensorlog[i].nr == sensor->nr) {
                data += sensorlog[i].data;
                n++;
              }
            }
          }
          if (n > 0) {
            sensorlog->nr = sensor->nr;
            sensorlog->time = fromdate;
            sensorlog->data = data / (double)n;
            sensorlog->native_data = 0;
            sensorlog_add(LOG_WEEK, sensorlog);
          }
        }
        sensor = sensor->next;
      }
      fromdate += CALCRANGE_WEEK;
      todate += CALCRANGE_WEEK;
    }
    next_week_calc = todate;
    DEBUG_PRINTLN(F("calc_sensorlogs WEEK end"));
  }

  if (time >= next_month_calc) {
    log_size = sensorlog_size(LOG_WEEK);
    if (log_size <= 0) {
      if (sensorlog) free(sensorlog);
      return;
    }
    if (!sensorlog)
      sensorlog = (SensorLog_t *)malloc(sizeof(SensorLog_t) * BLOCKSIZE);

    DEBUG_PRINTLN(F("calc_sensorlogs MONTH start"));
    ulong size = sensorlog_size(LOG_MONTH);
    if (size == 0) {
      sensorlog_load(LOG_WEEK, 0, sensorlog);
      last_day = sensorlog->time;
    } else {
      sensorlog_load(LOG_MONTH, size - 1, sensorlog);  // last record
      last_day = sensorlog->time + CALCRANGE_MONTH;    // Skip last Range
    }
    time_t fromdate = (last_day / CALCRANGE_MONTH) * CALCRANGE_MONTH;
    time_t todate = fromdate + CALCRANGE_MONTH;
    // 4 blocks per day

    while (todate < time) {
      ulong startidx = findLogPosition(LOG_WEEK, fromdate);
      Sensor_t *sensor = sensors;
      while (sensor) {
        if (sensor->flags.enable && sensor->flags.log) {
          ulong idx = startidx;
          double data = 0;
          ulong n = 0;
          bool done = false;
          while (!done) {
            int sn = sensorlog_load2(LOG_WEEK, idx, BLOCKSIZE, sensorlog);
            if (sn <= 0) break;
            for (int i = 0; i < sn; i++) {
              idx++;
              if (sensorlog[i].time >= todate) {
                done = true;
                break;
              }
              if (sensorlog[i].nr == sensor->nr) {
                data += sensorlog[i].data;
                n++;
              }
            }
          }
          if (n > 0) {
            sensorlog->nr = sensor->nr;
            sensorlog->time = fromdate;
            sensorlog->data = data / (double)n;
            sensorlog->native_data = 0;
            sensorlog_add(LOG_MONTH, sensorlog);
          }
        }
        sensor = sensor->next;
      }
      fromdate += CALCRANGE_MONTH;
      todate += CALCRANGE_MONTH;
    }
    next_month_calc = todate;
    DEBUG_PRINTLN(F("calc_sensorlogs MONTH end"));
  }
  if (sensorlog) free(sensorlog);
}

void sensor_remote_http_callback(char *) {
  // unused
}

void push_message(Sensor_t *sensor) {
  if (!sensor || !sensor->last_read) return;

  static char topic[TMP_BUFFER_SIZE];
  static char payload[TMP_BUFFER_SIZE];
  char *postval = tmp_buffer;

  if (os.mqtt.enabled()) {
    DEBUG_PRINTLN("push mqtt1");
    strncpy_P(topic, PSTR("analogsensor/"), sizeof(topic) - 1);
    strncat(topic, sensor->name, sizeof(topic) - 1);
    snprintf_P(payload, TMP_BUFFER_SIZE,
              PSTR("{\"nr\":%u,\"type\":%u,\"data_ok\":%u,\"time\":%u,"
                   "\"value\":%d.%02d,\"unit\":\"%s\"}"),
              sensor->nr, sensor->type, sensor->flags.data_ok,
              sensor->last_read, (int)sensor->last_data,
              abs((int)(sensor->last_data * 100) % 100), getSensorUnit(sensor));

    if (!os.mqtt.connected()) os.mqtt.reconnect();
    os.mqtt.publish(topic, payload);
    DEBUG_PRINTLN("push mqtt2");
  }
  
  //ifttt is enabled, when the ifttt key is present!
  os.sopt_load(SOPT_IFTTT_KEY, tmp_buffer);
	bool ifttt_enabled = strlen(tmp_buffer)!=0;
  if (ifttt_enabled) {
    DEBUG_PRINTLN("push ifttt");
    strcpy_P(postval, PSTR("{\"value1\":\"On site ["));
    os.sopt_load(SOPT_DEVICE_NAME, postval + strlen(postval));
    strcat_P(postval, PSTR("], "));

    strcat_P(postval, PSTR("analogsensor "));
    snprintf_P(postval + strlen(postval), TMP_BUFFER_SIZE - strlen(postval),
              PSTR("nr: %u, type: %u, data_ok: %u, time: %u, value: %d.%02d, "
                   "unit: %s"),
              sensor->nr, sensor->type, sensor->flags.data_ok,
              sensor->last_read, (int)sensor->last_data,
              abs((int)(sensor->last_data * 100) % 100), getSensorUnit(sensor));
    strcat_P(postval, PSTR("\"}"));

    // char postBuffer[1500];
    BufferFiller bf = BufferFiller(ether_buffer, ETHER_BUFFER_SIZE_L);
    bf.emit_p(PSTR("POST /trigger/sprinkler/with/key/$O HTTP/1.0\r\n"
                   "Host: $S\r\n"
                   "Accept: */*\r\n"
                   "Content-Length: $D\r\n"
                   "Content-Type: application/json\r\n\r\n$S"),
              SOPT_IFTTT_KEY, DEFAULT_IFTTT_URL, strlen(postval), postval);

    os.send_http_request(DEFAULT_IFTTT_URL, 80, ether_buffer, sensor_remote_http_callback);
    DEBUG_PRINTLN("push ifttt2");
  }

  add_influx_data(sensor);
}

void read_all_sensors(boolean online) {
  if (!sensors) return;
  //DEBUG_PRINTLN(F("read_all_sensors"));

  ulong time = os.now_tz();

#ifdef ENABLE_DEBUG
  if (time < os.powerup_lasttime + 3)
#else
  if (time < os.powerup_lasttime + 30)
#endif
    return;  // wait 30s before first sensor read

  // When we run out of time, skip some sensors and continue on next loop
  if (current_sensor == NULL) current_sensor = sensors;

  while (current_sensor) {
    if (time >= current_sensor->last_read + current_sensor->read_interval ||
        current_sensor->repeat_read) {
      if (online || (current_sensor->ip == 0 && current_sensor->type != SENSOR_MQTT)) {
        int result = read_sensor(current_sensor, time);
        if (result == HTTP_RQT_SUCCESS) {
          check_monitors();
          sensorlog_add(LOG_STD, current_sensor, time);
          push_message(current_sensor);
        } else if (result == HTTP_RQT_TIMEOUT) {
          // delay next read on timeout:
          current_sensor->last_read = time + max((uint)60, current_sensor->read_interval);
          current_sensor->repeat_read = 0;
          DEBUG_PRINTF("Delayed1: %s", current_sensor->name);
        } else if (result == HTTP_RQT_CONNECT_ERR) {
          // delay next read on error:
          current_sensor->last_read = time + max((uint)60, current_sensor->read_interval);
          current_sensor->repeat_read = 0;
          DEBUG_PRINTF("Delayed2: %s", current_sensor->name);
        }
        ulong passed = os.now_tz() - time;
        if (passed > MAX_SENSOR_READ_TIME) {
          current_sensor = current_sensor->next;
          break;
        }
      }
    }
    current_sensor = current_sensor->next;
  }
  sensor_update_groups();
  calc_sensorlogs();
  if (time - last_save_time > 3600)  // 1h
    sensor_save();
}

#if defined(ARDUINO)
#if defined(ESP8266)
/**
 * Read ESP8296 ADS1115 sensors
 */
int read_sensor_adc(Sensor_t *sensor, ulong time) {
  //DEBUG_PRINTLN(F("read_sensor_adc"));
  if (!sensor || !sensor->flags.enable) return HTTP_RQT_NOT_RECEIVED;
  if (sensor->id >= 16) return HTTP_RQT_NOT_RECEIVED;
  // Init + Detect:

  if (sensor->id < 8 && ((asb_detected_boards & ASB_BOARD1) == 0))
    return HTTP_RQT_NOT_RECEIVED;
  if (sensor->id >= 8 && sensor->id < 16 &&
      ((asb_detected_boards & ASB_BOARD2) == 0))
    return HTTP_RQT_NOT_RECEIVED;

  int port = ASB_BOARD_ADDR1a + sensor->id / 4;
  int id = sensor->id % 4;

  // unsigned long startTime = millis();
  ADS1115 adc(port);
  if (!adc.begin()) {
    DEBUG_PRINTLN(F("no asb board?!?"));
    return HTTP_RQT_NOT_RECEIVED;
  }
  // unsigned long endTime = millis();
  // DEBUG_PRINTF("t=%lu ms\n", endTime-startTime);

  // startTime = millis();
  sensor->repeat_native += adc.readADC(id);
  // endTime = millis();
  // DEBUG_PRINTF("t=%lu ms\n", endTime-startTime);

  if (++sensor->repeat_read < MAX_SENSOR_REPEAT_READ &&
      time < sensor->last_read + sensor->read_interval)
    return HTTP_RQT_NOT_RECEIVED;

  uint64_t avgValue = sensor->repeat_native / sensor->repeat_read;

  sensor->repeat_native = avgValue;
  sensor->repeat_data = 0;
  sensor->repeat_read = 1;  // read continously

  sensor->last_native_data = avgValue;
  sensor->last_data = adc.toVoltage(sensor->last_native_data);
  double v = sensor->last_data;

  switch (sensor->type) {
    case SENSOR_SMT50_MOIS:  // SMT50 VWC [%] = (U * 50) : 3
      sensor->last_data = (v * 50.0) / 3.0;
      break;
    case SENSOR_SMT50_TEMP:  // SMT50 T [°C] = (U – 0,5) * 100
      sensor->last_data = (v - 0.5) * 100.0;
      break;
    case SENSOR_ANALOG_EXTENSION_BOARD_P:  // 0..3,3V -> 0..100%
      sensor->last_data = v * 100.0 / 3.3;
      if (sensor->last_data < 0)
        sensor->last_data = 0;
      else if (sensor->last_data > 100)
        sensor->last_data = 100;
      break;
    case SENSOR_SMT100_ANALOG_MOIS:  // 0..3V -> 0..100%
      sensor->last_data = v * 100.0 / 3;
      break;
    case SENSOR_SMT100_ANALOG_TEMP:  // 0..3V -> -40°C..60°C
      sensor->last_data = v * 100.0 / 3 - 40;
      break;

    case SENSOR_VH400:  // http://vegetronix.com/Products/VH400/VH400-Piecewise-Curve
      if (v <= 1.1)  // 0 to 1.1V         VWC= 10*V-1
        sensor->last_data = 10 * v - 1;
      else if (v < 1.3)  // 1.1V to 1.3V      VWC= 25*V- 17.5
        sensor->last_data = 25 * v - 17.5;
      else if (v < 1.82)  // 1.3V to 1.82V     VWC= 48.08*V- 47.5
        sensor->last_data = 48.08 * v - 47.5;
      else if (v < 2.2)  // 1.82V to 2.2V     VWC= 26.32*V- 7.89
        sensor->last_data = 26.32 * v - 7.89;
      else  // 2.2V - 3.0V       VWC= 62.5*V - 87.5
        sensor->last_data = 62.5 * v - 87.5;
      break;
    case SENSOR_THERM200:  // http://vegetronix.com/Products/THERM200/
      sensor->last_data = v * 41.67 - 40;
      break;
    case SENSOR_AQUAPLUMB:  // http://vegetronix.com/Products/AquaPlumb/
      sensor->last_data = v * 100.0 / 3.0;  // 0..3V -> 0..100%
      if (sensor->last_data < 0)
        sensor->last_data = 0;
      else if (sensor->last_data > 100)
        sensor->last_data = 100;
      break;
    case SENSOR_USERDEF:  // User defined sensor
      v -= (double)sensor->offset_mv /
           1000;  // adjust zero-point offset in millivolt
      if (sensor->factor && sensor->divider)
        v *= (double)sensor->factor / (double)sensor->divider;
      else if (sensor->divider)
        v /= sensor->divider;
      else if (sensor->factor)
        v *= sensor->factor;
      sensor->last_data = v + sensor->offset2 / 100;
      break;
  }

  sensor->flags.data_ok = true;
  sensor->last_read = time;

  DEBUG_PRINT(F("adc sensor values: "));
  DEBUG_PRINT(sensor->last_native_data);
  DEBUG_PRINT(",");
  DEBUG_PRINTLN(sensor->last_data);

  return HTTP_RQT_SUCCESS;
}
#endif
#endif

bool extract(char *s, char *buf, int maxlen) {
  s = strstr(s, ":");
  if (!s) return false;
  s++;
  while (*s == ' ') s++;  // skip spaces
  char *e = strstr(s, ",");
  char *f = strstr(s, "}");
  if (!e && !f) return false;
  if (f && f < e) e = f;
  int l = e - s;
  if (l < 1 || l > maxlen) return false;
  strncpy(buf, s, l);
  buf[l] = 0;
  // DEBUG_PRINTLN(buf);
  return true;
}

int read_sensor_http(Sensor_t *sensor, ulong time) {
#if defined(ESP8266)
  IPAddress _ip(sensor->ip);
  unsigned char ip[4] = {_ip[0], _ip[1], _ip[2], _ip[3]};
#else
  unsigned char ip[4];
  ip[3] = (unsigned char)((sensor->ip >> 24) & 0xFF);
  ip[2] = (unsigned char)((sensor->ip >> 16) & 0xFF);
  ip[1] = (unsigned char)((sensor->ip >> 8) & 0xFF);
  ip[0] = (unsigned char)((sensor->ip & 0xFF));
#endif

  DEBUG_PRINTLN(F("read_sensor_http"));

  char *p = tmp_buffer;
  BufferFiller bf = BufferFiller(tmp_buffer, TMP_BUFFER_SIZE);

  bf.emit_p(PSTR("GET /sg?pw=$O&nr=$D"), SOPT_PASSWORD, sensor->id);
  bf.emit_p(PSTR(" HTTP/1.0\r\nHOST: $D.$D.$D.$D\r\n\r\n"), ip[0], ip[1], ip[2],
            ip[3]);

  DEBUG_PRINTLN(p);

  char server[20];
  sprintf(server, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

  int res = os.send_http_request(server, sensor->port, p, NULL, false, 500);
  if (res == HTTP_RQT_SUCCESS) {
    DEBUG_PRINTLN("Send Ok");
    p = ether_buffer;
    DEBUG_PRINTLN(p);

    sensor->last_read = time;

    char buf[20];
    char *s = strstr(p, "\"nativedata\":");
    if (s && extract(s, buf, sizeof(buf))) {
      sensor->last_native_data = strtoul(buf, NULL, 0);
    }
    s = strstr(p, "\"data\":");
    if (s && extract(s, buf, sizeof(buf))) {
      double value = -1;
      int ok = sscanf(buf, "%lf", &value);
      if (ok && (value != sensor->last_data || !sensor->flags.data_ok ||
                 time - sensor->last_read > 6000)) {
        sensor->last_data = value;
        sensor->flags.data_ok = true;
      } else {
        return HTTP_RQT_NOT_RECEIVED;
      }
    }
    s = strstr(p, "\"unitid\":");
    if (s && extract(s, buf, sizeof(buf))) {
      sensor->unitid = atoi(buf);
      sensor->assigned_unitid = sensor->unitid;
    }
    s = strstr(p, "\"unit\":");
    if (s && extract(s, buf, sizeof(buf))) {
      urlDecodeAndUnescape(buf);
      strncpy(sensor->userdef_unit, buf, sizeof(sensor->userdef_unit) - 1);
    }
    s = strstr(p, "\"last\":");
    if (s && extract(s, buf, sizeof(buf))) {
      ulong last = strtoul(buf, NULL, 0);
      if (last == 0 || last == sensor->last_read) {
        return HTTP_RQT_NOT_RECEIVED;
      } else {
        sensor->last_read = last;
      }
    }

    return HTTP_RQT_SUCCESS;
  }
  return res;
}

#if defined(ESP8266)
/**
 * @brief Truebner RS485 Interface
 *
 * @param sensor
 * @return int
 */
int read_sensor_rs485(Sensor_t *sensor) {
  DEBUG_PRINTLN(F("read_sensor_rs485"));
  int device = sensor->port;
  if (device >= MAX_RS485_DEVICES || (asb_detected_boards & (RS485_TRUEBNER1 << device)) == 0) 
    return HTTP_RQT_NOT_RECEIVED;

  if (i2c_rs485_allocated[device] > 0 && i2c_rs485_allocated[device] != sensor->nr) {
    sensor->repeat_read = 1000;
    DEBUG_PRINT(F("cant' read, allocated by sensor "));
    DEBUG_PRINTLN(i2c_rs485_allocated[device]);
    Sensor_t *t = sensor_by_nr(i2c_rs485_allocated[device]);
    if (!t || !t->flags.enable)
      i2c_rs485_allocated[device] = 0; //breakout
    return HTTP_RQT_NOT_RECEIVED;
  }

  DEBUG_PRINTLN(F("read_sensor_rs485: check-ok"));

  bool isTemp = sensor->type == SENSOR_SMT100_TEMP || sensor->type == SENSOR_TH100_TEMP;
  bool isMois = sensor->type == SENSOR_SMT100_MOIS || sensor->type == SENSOR_TH100_MOIS;
  uint8_t type = isTemp ? 0x00 : isMois ? 0x01 : 0x02;
  
  if (sensor->repeat_read == 0 || sensor->repeat_read == 1000) {
    Wire.beginTransmission(RS485_TRUEBNER1_ADDR + device);
    Wire.write((uint8_t)sensor->id);
    Wire.write(type);
    if (Wire.endTransmission() == 0) {
      DEBUG_PRINTF("read_sensor_rs485: request send: %d - %d\n", sensor->id,
                   type);
      sensor->repeat_read = 1;
      i2c_rs485_allocated[device] = sensor->nr;
    }
    return HTTP_RQT_NOT_RECEIVED;
    // delay(500);
  }

  if (Wire.requestFrom((uint8_t)(RS485_TRUEBNER1_ADDR + device), (size_t)4, true)) {
    // read the incoming bytes:
    uint8_t addr = Wire.read();
    uint8_t reg = Wire.read();
    uint8_t low_byte = Wire.read();
    uint8_t high_byte = Wire.read();
    if (addr == sensor->id && reg == type) {
      uint16_t data = (high_byte << 8) | low_byte;
      DEBUG_PRINTF("read_sensor_rs485: result: %d - %d (%d %d)\n", sensor->id,
                   data, low_byte, high_byte);
      double value = isTemp ? (data / 100.0) - 100.0 : (isMois ? data / 100.0 : data);
      sensor->last_native_data = data;
      sensor->last_data = value;
      DEBUG_PRINTLN(sensor->last_data);

      sensor->flags.data_ok = true;

      sensor->repeat_read = 0;
      i2c_rs485_allocated[device] = 0;
      return HTTP_RQT_SUCCESS;
    }
  }

  sensor->repeat_read++;
  if (sensor->repeat_read > 4) {  // timeout
    sensor->repeat_read = 0;
    sensor->flags.data_ok = false;
    i2c_rs485_allocated[device] = 0;
    DEBUG_PRINTLN(F("read_sensor_rs485: timeout"));
  }
  DEBUG_PRINTLN(F("read_sensor_rs485: exit"));
  return HTTP_RQT_NOT_RECEIVED;
}
#else
/**
 * USB RS485 Adapter
 * Howto use: Create a file rs485 inside the OpenSprinkler-Firmware directory, enter the USB-TTY connections here.
 * For example:
 *  /dev/ttyUSB0
 *  /dev/ttyUSB1
 * You can use multiple rs485 adapters, every line increments the "port" index, starting with 0 for the first line
 */

/**
 * @brief Raspberry PI RS485 Interface
 *
 * @param sensor
 * @return int
 */
int read_sensor_rs485(Sensor_t *sensor) {
  DEBUG_PRINTLN(F("read_sensor_rs485"));
  int device = sensor->port;
  if (device >= MAX_RS485_DEVICES || !ttyDevices[device])
    return HTTP_RQT_NOT_RECEIVED;

  DEBUG_PRINTLN(F("read_sensor_rs485: check-ok"));

  uint8_t buffer[10];
  bool isTemp = sensor->type == SENSOR_SMT100_TEMP || sensor->type == SENSOR_TH100_TEMP;
  bool isMois = sensor->type == SENSOR_SMT100_MOIS || sensor->type == SENSOR_TH100_MOIS;
  uint8_t type = isTemp ? 0x00 : isMois ? 0x01 : 0x02;

  uint16_t tab_reg[3] = {0};
  modbus_set_slave(ttyDevices[device], sensor->id);
  if (modbus_read_registers(ttyDevices[device], type, 2, tab_reg) > 0) {
    uint16_t data = tab_reg[0];
    DEBUG_PRINTF("read_sensor_rs485: result: %d - %d\n", sensor->id, data);
    double value = isTemp ? (data / 100.0) - 100.0 : (isMois ? data / 100.0 : data);
    sensor->last_native_data = data;
    sensor->last_data = value;
    DEBUG_PRINTLN(sensor->last_data);
    sensor->flags.data_ok = true;
    return HTTP_RQT_SUCCESS;
  }
  DEBUG_PRINTLN(F("read_sensor_rs485: exit"));
  return HTTP_RQT_NOT_RECEIVED;
}

#endif

/**
 * Read ip connected sensors
 */
int read_sensor_ip(Sensor_t *sensor) {
#if defined(ARDUINO)

  Client *client;
#if defined(ESP8266)
  WiFiClient wifiClient;
  client = &wifiClient;
#else
  EthernetClient etherClient;
  client = &etherClient;
#endif

#else
  EthernetClient etherClient;
  EthernetClient *client = &etherClient;
#endif

  sensor->flags.data_ok = false;
  if (!sensor->ip || !sensor->port) {
    sensor->flags.enable = false;
    return HTTP_RQT_CONNECT_ERR;
  }

#if defined(ARDUINO)
  IPAddress _ip(sensor->ip);
  unsigned char ip[4] = {_ip[0], _ip[1], _ip[2], _ip[3]};
#else
  unsigned char ip[4];
  ip[3] = (unsigned char)((sensor->ip >> 24) & 0xFF);
  ip[2] = (unsigned char)((sensor->ip >> 16) & 0xFF);
  ip[1] = (unsigned char)((sensor->ip >> 8) & 0xFF);
  ip[0] = (unsigned char)((sensor->ip & 0xFF));
#endif
  char server[20];
  sprintf(server, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  client->setTimeout(200);
  if (!client->connect(server, sensor->port)) {
    DEBUG_PRINT(server);
    DEBUG_PRINT(":");
    DEBUG_PRINT(sensor->port);
    DEBUG_PRINT(" ");
    DEBUG_PRINTLN(F("failed."));
    client->stop();
    return HTTP_RQT_TIMEOUT;
  }

  uint8_t buffer[20];

  // https://ipc2u.com/articles/knowledge-base/detailed-description-of-the-modbus-tcp-protocol-with-command-examples/

  if (modbusTcpId >= 0xFFFE)
    modbusTcpId = 1;
  else
    modbusTcpId++;

  bool isTemp = sensor->type == SENSOR_SMT100_TEMP || sensor->type == SENSOR_TH100_TEMP;
  bool isMois = sensor->type == SENSOR_SMT100_MOIS || sensor->type == SENSOR_TH100_MOIS;
  uint8_t type = isTemp ? 0x00 : isMois ? 0x01 : 0x02;

  buffer[0] = (0xFF00 & modbusTcpId) >> 8;
  buffer[1] = (0x00FF & modbusTcpId);
  buffer[2] = 0;
  buffer[3] = 0;
  buffer[4] = 0;
  buffer[5] = 6;  // len
  buffer[6] = sensor->id;  // Modbus ID
  buffer[7] = 0x03;        // Read Holding Registers
  buffer[8] = 0x00;
  buffer[9] = type;
  buffer[10] = 0x00;
  buffer[11] = 0x01;  

  client->write(buffer, 12);
#if defined(ESP8266)
  client->flush();
#endif

  // Read result:
  switch (sensor->type) {
    case SENSOR_SMT100_MOIS:
    case SENSOR_SMT100_TEMP:
    case SENSOR_SMT100_PMTY:
    case SENSOR_TH100_MOIS:
	  case SENSOR_TH100_TEMP:
      uint32_t stoptime = millis() + SENSOR_READ_TIMEOUT;
#if defined(ESP8266)
      while (true) {
        if (client->available()) break;
        if (millis() >= stoptime) {
          client->stop();
          DEBUG_PRINT(F("Sensor "));
          DEBUG_PRINT(sensor->nr);
          DEBUG_PRINTLN(F(" timeout read!"));
          return HTTP_RQT_TIMEOUT;
        }
        delay(5);
      }

      int n = client->read(buffer, 11);
      if (n < 11) n += client->read(buffer + n, 11 - n);
#else
      int n = 0;
      while (true) {
        n = client->read(buffer, 11);
        if (n < 11) n += client->read(buffer + n, 11 - n);
        if (n > 0) break;
        if (millis() >= stoptime) {
          client->stop();
          DEBUG_PRINT(F("Sensor "));
          DEBUG_PRINT(sensor->nr);
          DEBUG_PRINT(F(" timeout read!"));
          return HTTP_RQT_TIMEOUT;
        }
        delay(5);
      }
#endif
      client->stop();
      DEBUG_PRINT(F("Sensor "));
      DEBUG_PRINT(sensor->nr);
      if (n != 11) {
        DEBUG_PRINT(F(" returned "));
        DEBUG_PRINT(n);
        DEBUG_PRINTLN(F(" bytes??"));
        return n == 0 ? HTTP_RQT_EMPTY_RETURN : HTTP_RQT_TIMEOUT;
      }
      if (buffer[0] != (0xFF00 & modbusTcpId) >> 8 ||
          buffer[1] != (0x00FF & modbusTcpId)) {
        DEBUG_PRINT(F(" returned transaction id "));
        DEBUG_PRINTLN((uint16_t)((buffer[0] << 8) + buffer[1]));
        return HTTP_RQT_NOT_RECEIVED;
      }
      if ((buffer[6] != sensor->id && sensor->id != 253)) {  // 253 is broadcast
        DEBUG_PRINT(F(" returned sensor id "));
        DEBUG_PRINTLN((int)buffer[0]);
        return HTTP_RQT_NOT_RECEIVED;
      }

      // Valid result:
      sensor->last_native_data = (buffer[9] << 8) | buffer[10];
      DEBUG_PRINT(F(" native: "));
      DEBUG_PRINT(sensor->last_native_data);

      // Convert to readable value:
      switch (sensor->type) {
        case SENSOR_TH100_MOIS:
        case SENSOR_SMT100_MOIS:  // Equation: soil moisture [vol.%]=
                                  // (16Bit_soil_moisture_value / 100)
          sensor->last_data = ((double)sensor->last_native_data / 100.0);
          sensor->flags.data_ok = sensor->last_native_data < 10000;
          DEBUG_PRINT(F(" soil moisture %: "));
          break;
        case SENSOR_TH100_TEMP:
        case SENSOR_SMT100_TEMP:  // Equation: temperature [°C]=
                                  // (16Bit_temperature_value / 100)-100
          sensor->last_data =
              ((double)sensor->last_native_data / 100.0) - 100.0;
          sensor->flags.data_ok = sensor->last_native_data > 7000;
          DEBUG_PRINT(F(" temperature °C: "));
          break;
        case SENSOR_SMT100_PMTY:  // permittivity
          sensor->last_data = ((double)sensor->last_native_data / 100.0);
          sensor->flags.data_ok = true;
          DEBUG_PRINT(F(" permittivity DK: "));
          break;
      }
      DEBUG_PRINTLN(sensor->last_data);
      return sensor->flags.data_ok ? HTTP_RQT_SUCCESS : HTTP_RQT_NOT_RECEIVED;
  }

  return HTTP_RQT_NOT_RECEIVED;
}

#if defined(OSPI)
int read_internal_raspi(Sensor_t *sensor, ulong time) {
  if (!sensor || !sensor->flags.enable) return HTTP_RQT_NOT_RECEIVED;

  char buf[10];
  size_t res = 0;
  FILE *fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
	if(fp) {
		res = fread(buf, 1, 10, fp);
		fclose(fp);
	}
  if (!res)
    return HTTP_RQT_NOT_RECEIVED;

  sensor->last_read = time;
  sensor->last_native_data = strtol(buf, NULL, 0);
  sensor->last_data = (double)sensor->last_native_data / 1000;
  sensor->flags.data_ok = true;

	return HTTP_RQT_SUCCESS;
}
#endif
/**
 * read a sensor
 */
int read_sensor(Sensor_t *sensor, ulong time) {
  if (!sensor || !sensor->flags.enable) return HTTP_RQT_NOT_RECEIVED;

  switch (sensor->type) {
    case SENSOR_SMT100_MOIS:
    case SENSOR_SMT100_TEMP:
    case SENSOR_SMT100_PMTY:
    case SENSOR_TH100_MOIS:
	  case SENSOR_TH100_TEMP:
      //DEBUG_PRINT(F("Reading sensor "));
      //DEBUG_PRINTLN(sensor->name);
      sensor->last_read = time;
      if (sensor->ip) return read_sensor_ip(sensor);
      return read_sensor_rs485(sensor);
      break;

#if defined(ARDUINO)
#if defined(ESP8266)
    case SENSOR_ANALOG_EXTENSION_BOARD:
    case SENSOR_ANALOG_EXTENSION_BOARD_P:
    case SENSOR_SMT50_MOIS:          // SMT50 VWC [%] = (U * 50) : 3
    case SENSOR_SMT50_TEMP:          // SMT50 T [°C] = (U – 0,5) * 100
    case SENSOR_SMT100_ANALOG_MOIS:  // SMT100 Analog Moisture
    case SENSOR_SMT100_ANALOG_TEMP:  // SMT100 Analog Temperature
    case SENSOR_VH400:
    case SENSOR_THERM200:
    case SENSOR_AQUAPLUMB:
    case SENSOR_USERDEF:
      //DEBUG_PRINT(F("Reading sensor "));
      //DEBUG_PRINTLN(sensor->name);
      return read_sensor_adc(sensor, time);
    case SENSOR_FREE_MEMORY:
    {
      uint32_t fm = freeMemory();
      if (sensor->last_native_data == fm)
        return HTTP_RQT_NOT_RECEIVED;
      sensor->last_native_data = fm;
      sensor->last_data = fm;
      sensor->last_read = time;
      sensor->flags.data_ok = true;
      return HTTP_RQT_SUCCESS;
    }
    case SENSOR_FREE_STORE: {
    	struct FSInfo fsinfo;
	    boolean ok = LittleFS.info(fsinfo);
      if (ok) {
        uint32_t fd = fsinfo.totalBytes-fsinfo.usedBytes;
        if (sensor->last_native_data == fd)
          return HTTP_RQT_NOT_RECEIVED;
        sensor->last_native_data = fd;
        sensor->last_data = fd;
      } 
      sensor->flags.data_ok = ok;
      sensor->last_read = time;
      return HTTP_RQT_SUCCESS;
    }
#endif
#else
#if defined ADS1115 | PCF8591
    case SENSOR_OSPI_ANALOG:
    case SENSOR_OSPI_ANALOG_P:
    case SENSOR_OSPI_ANALOG_SMT50_MOIS:
    case SENSOR_OSPI_ANALOG_SMT50_TEMP:
      return read_sensor_ospi(sensor, time);
#endif
#if defined(OSPI)
    case SENSOR_OSPI_INTERNAL_TEMP:
      return read_internal_raspi(sensor, time);
#endif
#endif
    case SENSOR_REMOTE:
      return read_sensor_http(sensor, time);
    case SENSOR_MQTT:
      sensor->last_read = time;
      return read_sensor_mqtt(sensor);

    case SENSOR_WEATHER_TEMP_F:
    case SENSOR_WEATHER_TEMP_C:
    case SENSOR_WEATHER_HUM:
    case SENSOR_WEATHER_PRECIP_IN:
    case SENSOR_WEATHER_PRECIP_MM:
    case SENSOR_WEATHER_WIND_MPH:
    case SENSOR_WEATHER_WIND_KMH: {
      GetSensorWeather();
      if (current_weather_ok) {
        DEBUG_PRINT(F("Reading sensor "));
        DEBUG_PRINTLN(sensor->name);

        sensor->last_read = time;
        sensor->last_native_data = 0;
        sensor->flags.data_ok = true;

        switch (sensor->type) {
          case SENSOR_WEATHER_TEMP_F: {
            sensor->last_data = current_temp;
            break;
          }
          case SENSOR_WEATHER_TEMP_C: {
            sensor->last_data = (current_temp - 32.0) / 1.8;
            break;
          }
          case SENSOR_WEATHER_HUM: {
            sensor->last_data = current_humidity;
            break;
          }
          case SENSOR_WEATHER_PRECIP_IN: {
            sensor->last_data = current_precip;
            break;
          }
          case SENSOR_WEATHER_PRECIP_MM: {
            sensor->last_data = current_precip * 25.4;
            break;
          }
          case SENSOR_WEATHER_WIND_MPH: {
            sensor->last_data = current_wind;
            break;
          }
          case SENSOR_WEATHER_WIND_KMH: {
            sensor->last_data = current_wind * 1.609344;
            break;
          }
        }
        return HTTP_RQT_SUCCESS;
      }
    }
  }
  return HTTP_RQT_NOT_RECEIVED;
}

/**
 * @brief Update group values
 *
 */
void sensor_update_groups() {
  Sensor_t *sensor = sensors;

  ulong time = os.now_tz();

  while (sensor) {
    if (time >= sensor->last_read + sensor->read_interval) {
      switch (sensor->type) {
        case SENSOR_GROUP_MIN:
        case SENSOR_GROUP_MAX:
        case SENSOR_GROUP_AVG:
        case SENSOR_GROUP_SUM: {
          uint nr = sensor->nr;
          Sensor_t *group = sensors;
          double value = 0;
          int n = 0;
          while (group) {
            if (group->nr != nr && group->group == nr &&
                group->flags.enable) {  // && group->flags.data_ok) {
              switch (sensor->type) {
                case SENSOR_GROUP_MIN:
                  if (n++ == 0)
                    value = group->last_data;
                  else if (group->last_data < value)
                    value = group->last_data;
                  break;
                case SENSOR_GROUP_MAX:
                  if (n++ == 0)
                    value = group->last_data;
                  else if (group->last_data > value)
                    value = group->last_data;
                  break;
                case SENSOR_GROUP_AVG:
                case SENSOR_GROUP_SUM:
                  n++;
                  value += group->last_data;
                  break;
              }
            }
            group = group->next;
          }
          if (sensor->type == SENSOR_GROUP_AVG && n > 0) {
            value = value / (double)n;
          }
          sensor->last_data = value;
          sensor->last_native_data = 0;
          sensor->last_read = time;
          sensor->flags.data_ok = n > 0;
          sensorlog_add(LOG_STD, sensor, time);
          break;
        }
      }
    }
    sensor = sensor->next;
  }
}

int set_sensor_address_ip(Sensor_t *sensor, uint8_t new_address) {
#if defined(ARDUINO)
  Client *client;
#if defined(ESP8266)
  WiFiClient wifiClient;
  client = &wifiClient;
#else
  EthernetClient etherClient;
  client = &etherClient;
#endif
#else
  EthernetClient etherClient;
  EthernetClient *client = &etherClient;
#endif
#if defined(ESP8266)
  IPAddress _ip(sensor->ip);
  unsigned char ip[4] = {_ip[0], _ip[1], _ip[2], _ip[3]};
#else
  unsigned char ip[4];
  ip[3] = (unsigned char)((sensor->ip >> 24) & 0xFF);
  ip[2] = (unsigned char)((sensor->ip >> 16) & 0xFF);
  ip[1] = (unsigned char)((sensor->ip >> 8) & 0xFF);
  ip[0] = (unsigned char)((sensor->ip & 0xFF));
#endif
  char server[20];
  sprintf(server, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  client->setTimeout(200);
  if (!client->connect(server, sensor->port)) {
    DEBUG_PRINT(F("Cannot connect to "));
    DEBUG_PRINT(server);
    DEBUG_PRINT(":");
    DEBUG_PRINTLN(sensor->port);
    client->stop();
    return HTTP_RQT_CONNECT_ERR;
  }

  uint8_t buffer[20];

  // https://ipc2u.com/articles/knowledge-base/detailed-description-of-the-modbus-tcp-protocol-with-command-examples/

  if (modbusTcpId >= 0xFFFE)
    modbusTcpId = 1;
  else
    modbusTcpId++;

  buffer[0] = (0xFF00 & modbusTcpId) >> 8;
  buffer[1] = (0x00FF & modbusTcpId);
  buffer[2] = 0;
  buffer[3] = 0;
  buffer[4] = 0;
  buffer[5] = 6;  // len
  buffer[6] = sensor->id;
  buffer[7] = 0x06;
  buffer[8] = 0x00;
  buffer[9] = 0x04;
  buffer[10] = 0x00;
  buffer[11] = new_address;

  client->write(buffer, 12);
#if defined(ESP8266)
  client->flush();
#endif

  // Read result:
  int n = client->read(buffer, 12);
  client->stop();
  DEBUG_PRINT(F("Sensor "));
  DEBUG_PRINT(sensor->nr);
  if (n != 12) {
    DEBUG_PRINT(F(" returned "));
    DEBUG_PRINT(n);
    DEBUG_PRINT(F(" bytes??"));
    return n == 0 ? HTTP_RQT_EMPTY_RETURN : HTTP_RQT_TIMEOUT;
  }
  if (buffer[0] != (0xFF00 & modbusTcpId) >> 8 ||
      buffer[1] != (0x00FF & modbusTcpId)) {
    DEBUG_PRINT(F(" returned transaction id "));
    DEBUG_PRINTLN((uint16_t)((buffer[0] << 8) + buffer[1]));
    return HTTP_RQT_NOT_RECEIVED;
  }
  if ((buffer[6] != sensor->id && sensor->id != 253)) {  // 253 is broadcast
    DEBUG_PRINT(F(" returned sensor id "));
    DEBUG_PRINT((int)buffer[0]);
    return HTTP_RQT_NOT_RECEIVED;
  }
  // Read OK:
  sensor->id = new_address;
  sensor_save();
  return HTTP_RQT_SUCCESS;
}

#ifdef ESP8266
/**
 * @brief Set the sensor address i2c
 *
 * @param sensor
 * @param new_address
 * @return int
 */
int set_sensor_address_rs485(Sensor_t *sensor, uint8_t new_address) {
  DEBUG_PRINTLN(F("set_sensor_address_rs485"));
  int device = sensor->port;
  if (device >= MAX_RS485_DEVICES || (asb_detected_boards & (RS485_TRUEBNER1 << device)) == 0) 
    return HTTP_RQT_NOT_RECEIVED;

  if (i2c_rs485_allocated[device] > 0) {
    DEBUG_PRINT(F("sensor currently allocated by "));
    DEBUG_PRINTLN(i2c_rs485_allocated[device]);
    Sensor_t *t = sensor_by_nr(i2c_rs485_allocated[device]);
    if (!t || !t->flags.enable)
      i2c_rs485_allocated[device] = 0; //breakout
    return HTTP_RQT_NOT_RECEIVED;
  }

  Wire.beginTransmission(RS485_TRUEBNER1_ADDR + device);
  Wire.write(254);
  Wire.write(new_address);
  Wire.endTransmission();
  delay(3000);
  Wire.requestFrom((uint8_t)(RS485_TRUEBNER1_ADDR + device), (size_t)1, true);
  if (Wire.available()) {
    delay(10);
    uint8_t modbus_address = Wire.read();
    if (modbus_address == new_address) return HTTP_RQT_SUCCESS;
  }
  return HTTP_RQT_NOT_RECEIVED;
}
#else
/**
 * @brief Raspberry PI RS485 Interface - Set SMT100 Sensor address
 *
 * @param sensor
 * @return int
 */
int set_sensor_address_rs485(Sensor_t *sensor, uint8_t new_address) {
  DEBUG_PRINTLN(F("set_sensor_address_rs485"));
  int device = sensor->port;
  if (device >= MAX_RS485_DEVICES || !ttyDevices[device])
    return HTTP_RQT_NOT_RECEIVED;


  uint8_t request[10];
  request[0] = 0xFD; //253=Truebner Broadcast
  request[1] = 0x06; //Write
  request[2] = 0x00;
  request[3] = 0x04; //Register address
  request[4] = 0x00;
  request[5] = new_address;
  
  if (modbus_send_raw_request(ttyDevices[device], request, 6) > 0) {
    modbus_flush(ttyDevices[device]);
    return HTTP_RQT_SUCCESS;
  }
  return HTTP_RQT_NOT_RECEIVED;
}

#endif

int set_sensor_address(Sensor_t *sensor, uint8_t new_address) {
  if (!sensor) return HTTP_RQT_NOT_RECEIVED;

  switch (sensor->type) {
    case SENSOR_SMT100_MOIS:
    case SENSOR_SMT100_TEMP:
    case SENSOR_SMT100_PMTY:
    case SENSOR_TH100_MOIS:
	  case SENSOR_TH100_TEMP:
      sensor->flags.data_ok = false;
      if (sensor->ip && sensor->port)
        return set_sensor_address_ip(sensor, new_address);
      return set_sensor_address_rs485(sensor, new_address);
  }
  return HTTP_RQT_CONNECT_ERR;
}

double calc_linear(ProgSensorAdjust_t *p, double sensorData) {
  //   min max  factor1 factor2
  //   10..90 -> 5..1 factor1 > factor2
  //    a   b    c  d
  //   (b-sensorData) / (b-a) * (c-d) + d
  //
  //   10..90 -> 1..5 factor1 < factor2
  //    a   b    c  d
  //   (sensorData-a) / (b-a) * (d-c) + c

  // Limit to min/max:
  if (sensorData < p->min) sensorData = p->min;
  if (sensorData > p->max) sensorData = p->max;

  // Calculate:
  double div = (p->max - p->min);
  if (abs(div) < 0.00001) return 0;

  if (p->factor1 > p->factor2) {  // invers scaling factor:
    return (p->max - sensorData) / div * (p->factor1 - p->factor2) + p->factor2;
  } else {  // upscaling factor:
    return (sensorData - p->min) / div * (p->factor2 - p->factor1) + p->factor1;
  }
}

double calc_digital_min(ProgSensorAdjust_t *p, double sensorData) {
  return sensorData <= p->min ? p->factor1 : p->factor2;
}

double calc_digital_max(ProgSensorAdjust_t *p, double sensorData) {
  return sensorData >= p->max ? p->factor2 : p->factor1;
}

double calc_digital_minmax(ProgSensorAdjust_t *p, double sensorData) {
  if (sensorData <= p->min) return p->factor1;
  if (sensorData >= p->max) return p->factor1;
  return p->factor2;
}
/**
 * @brief calculate adjustment
 *
 * @param prog
 * @return double
 */
double calc_sensor_watering(uint prog) {
  double result = 1;
  ProgSensorAdjust_t *p = progSensorAdjusts;

  while (p) {
    if (p->prog - 1 == prog) {
      Sensor_t *sensor = sensor_by_nr(p->sensor);
      if (sensor && sensor->flags.enable && sensor->flags.data_ok) {
        double res = calc_sensor_watering_int(p, sensor->last_data);
        result = result * res;
      }
    }

    p = p->next;
  }
  if (result < 0.0) result = 0.0;
  if (result > 20.0)  // Factor 20 is a huge value!
    result = 20.0;
  return result;
}

double calc_sensor_watering_int(ProgSensorAdjust_t *p, double sensorData) {
  double res = 0;
  if (!p) return res;
  switch (p->type) {
    case PROG_NONE:
      res = 1;
      break;
    case PROG_LINEAR:
      res = calc_linear(p, sensorData);
      break;
    case PROG_DIGITAL_MIN:
      res = calc_digital_min(p, sensorData);
      break;
    case PROG_DIGITAL_MAX:
      res = calc_digital_max(p, sensorData);
      break;
    case PROG_DIGITAL_MINMAX:
      res = calc_digital_minmax(p, sensorData);
      break;
    default:
      res = 0;
  }
  return res;
}

/**
 * @brief calculate adjustment
 *
 * @param nr
 * @return double
 */
double calc_sensor_watering_by_nr(uint nr) {
  double result = 1;
  ProgSensorAdjust_t *p = progSensorAdjusts;

  while (p) {
    if (p->nr == nr) {
      Sensor_t *sensor = sensor_by_nr(p->sensor);
      if (sensor && sensor->flags.enable && sensor->flags.data_ok) {
        double res = 0;
        switch (p->type) {
          case PROG_NONE:
            res = 1;
            break;
          case PROG_LINEAR:
            res = calc_linear(p, sensor->last_data);
            break;
          case PROG_DIGITAL_MIN:
            res = calc_digital_min(p, sensor->last_data);
            break;
          case PROG_DIGITAL_MAX:
            res = calc_digital_max(p, sensor->last_data);
            break;
          default:
            res = 0;
        }

        result = result * res;
      }
      break;
    }

    p = p->next;
  }

  return result;
}

int prog_adjust_define(uint nr, uint type, uint sensor, uint prog,
                       double factor1, double factor2, double min, double max, char * name) {
  ProgSensorAdjust_t *p = progSensorAdjusts;

  ProgSensorAdjust_t *last = NULL;

  while (p) {
    if (p->nr == nr) {
      p->type = type;
      p->sensor = sensor;
      p->prog = prog;
      p->factor1 = factor1;
      p->factor2 = factor2;
      p->min = min;
      p->max = max;
      strncpy(p->name, name, sizeof(p->name));
      prog_adjust_save();
      return HTTP_RQT_SUCCESS;
    }

    if (p->nr > nr) break;

    last = p;
    p = p->next;
  }

  p = new ProgSensorAdjust_t;
  p->nr = nr;
  p->type = type;
  p->sensor = sensor;
  p->prog = prog;
  p->factor1 = factor1;
  p->factor2 = factor2;
  p->min = min;
  p->max = max;
  strncpy(p->name, name, sizeof(p->name));
  if (last) {
    p->next = last->next;
    last->next = p;
  } else {
    p->next = progSensorAdjusts;
    progSensorAdjusts = p;
  }

  prog_adjust_save();
  return HTTP_RQT_SUCCESS;
}

int prog_adjust_delete(uint nr) {
  ProgSensorAdjust_t *p = progSensorAdjusts;

  ProgSensorAdjust_t *last = NULL;

  while (p) {
    if (p->nr == nr) {
      if (last)
        last->next = p->next;
      else
        progSensorAdjusts = p->next;
      delete p;
      prog_adjust_save();
      return HTTP_RQT_SUCCESS;
    }
    last = p;
    p = p->next;
  }
  return HTTP_RQT_NOT_RECEIVED;
}

void prog_adjust_save() {
  if (!apiInit) return;
  if (file_exists(PROG_SENSOR_FILENAME)) remove_file(PROG_SENSOR_FILENAME);

  ulong pos = 0;
  ProgSensorAdjust_t *pa = progSensorAdjusts;
  while (pa) {
    file_write_block(PROG_SENSOR_FILENAME, pa, pos, PROGSENSOR_STORE_SIZE);
    pa = pa->next;
    pos += PROGSENSOR_STORE_SIZE;
  }
}

void prog_adjust_load() {
  DEBUG_PRINTLN(F("prog_adjust_load"));
  progSensorAdjusts = NULL;
  if (!file_exists(PROG_SENSOR_FILENAME)) return;

  ulong pos = 0;
  ProgSensorAdjust_t *last = NULL;
  while (true) {
    ProgSensorAdjust_t *pa = new ProgSensorAdjust_t;
    memset(pa, 0, sizeof(ProgSensorAdjust_t));
    file_read_block(PROG_SENSOR_FILENAME, pa, pos, PROGSENSOR_STORE_SIZE);
    if (!pa->nr || !pa->type) {
      delete pa;
      break;
    }
    if (!last)
      progSensorAdjusts = pa;
    else
      last->next = pa;
    last = pa;
    pa->next = NULL;
    pos += PROGSENSOR_STORE_SIZE;
  }
}

uint prog_adjust_count() {
  uint count = 0;
  ProgSensorAdjust_t *pa = progSensorAdjusts;
  while (pa) {
    count++;
    pa = pa->next;
  }
  return count;
}

ProgSensorAdjust_t *prog_adjust_by_nr(uint nr) {
  ProgSensorAdjust_t *pa = progSensorAdjusts;
  while (pa) {
    if (pa->nr == nr) return pa;
    pa = pa->next;
  }
  return NULL;
}

ProgSensorAdjust_t *prog_adjust_by_idx(uint idx) {
  ProgSensorAdjust_t *pa = progSensorAdjusts;
  uint idxCounter = 0;
  while (pa) {
    if (idxCounter++ == idx) return pa;
    pa = pa->next;
  }
  return NULL;
}

#if defined(ESP8266)
ulong diskFree() {
  struct FSInfo fsinfo;
  LittleFS.info(fsinfo);
  return fsinfo.totalBytes - fsinfo.usedBytes;
}

bool checkDiskFree() {
  if (diskFree() < MIN_DISK_FREE) {
    DEBUG_PRINT(F("fs has low space!"));
    return false;
  }
  return true;
}
#endif

const char *getSensorUnit(int unitid) {
  if (unitid == UNIT_USERDEF) return "?";
  if (unitid < 0 || (uint16_t)unitid >= sizeof(sensor_unitNames))
    return sensor_unitNames[0];
  return sensor_unitNames[unitid];
}

const char *getSensorUnit(Sensor_t *sensor) {
  if (!sensor) return sensor_unitNames[0];

  int unitid = getSensorUnitId(sensor);
  if (unitid == UNIT_USERDEF) return sensor->userdef_unit;
  if (unitid < 0 || (uint16_t)unitid >= sizeof(sensor_unitNames))
    return sensor_unitNames[0];
  return sensor_unitNames[unitid];
}

boolean sensor_isgroup(Sensor_t *sensor) {
  if (!sensor) return false;

  switch (sensor->type) {
    case SENSOR_GROUP_MIN:
    case SENSOR_GROUP_MAX:
    case SENSOR_GROUP_AVG:
    case SENSOR_GROUP_SUM:
      return true;

    default:
      return false;
  }
}

unsigned char getSensorUnitId(int type) {
  switch (type) {
    case SENSOR_SMT100_MOIS:
      return UNIT_PERCENT;
    case SENSOR_SMT100_TEMP:
      return UNIT_DEGREE;
    case SENSOR_SMT100_PMTY:
      return UNIT_DK;
    case SENSOR_TH100_MOIS:
      return UNIT_HUM_PERCENT;
	  case SENSOR_TH100_TEMP:
      return UNIT_DEGREE;
#if defined(ARDUINO)
#if defined(ESP8266)
    case SENSOR_ANALOG_EXTENSION_BOARD:
      return UNIT_VOLT;
    case SENSOR_ANALOG_EXTENSION_BOARD_P:
      return UNIT_PERCENT;
    case SENSOR_SMT50_MOIS:
      return UNIT_PERCENT;
    case SENSOR_SMT50_TEMP:
      return UNIT_DEGREE;
    case SENSOR_SMT100_ANALOG_MOIS:
      return UNIT_PERCENT;
    case SENSOR_SMT100_ANALOG_TEMP:
      return UNIT_DEGREE;

    case SENSOR_VH400:
      return UNIT_PERCENT;
    case SENSOR_THERM200:
      return UNIT_DEGREE;
    case SENSOR_AQUAPLUMB:
      return UNIT_PERCENT;
    case SENSOR_USERDEF:
    case SENSOR_FREE_MEMORY:
    case SENSOR_FREE_STORE:
      return UNIT_USERDEF;
#endif
#else
    case SENSOR_OSPI_ANALOG:
      return UNIT_VOLT;
    case SENSOR_OSPI_ANALOG_P:
      return UNIT_PERCENT;
    case SENSOR_OSPI_ANALOG_SMT50_MOIS:
      return UNIT_PERCENT;
    case SENSOR_OSPI_ANALOG_SMT50_TEMP:
    case SENSOR_OSPI_INTERNAL_TEMP:
      return UNIT_DEGREE;
#endif
    case SENSOR_MQTT:
      return UNIT_USERDEF;
    case SENSOR_WEATHER_TEMP_F:
      return UNIT_FAHRENHEIT;
    case SENSOR_WEATHER_TEMP_C:
      return UNIT_DEGREE;
    case SENSOR_WEATHER_HUM:
      return UNIT_HUM_PERCENT;
    case SENSOR_WEATHER_PRECIP_IN:
      return UNIT_INCH;
    case SENSOR_WEATHER_PRECIP_MM:
      return UNIT_MM;
    case SENSOR_WEATHER_WIND_MPH:
      return UNIT_MPH;
    case SENSOR_WEATHER_WIND_KMH:
      return UNIT_KMH;

    default:
      return UNIT_NONE;
  }
}

unsigned char getSensorUnitId(Sensor_t *sensor) {
  if (!sensor) return 0;

  switch (sensor->type) {
    case SENSOR_SMT100_MOIS:
      return UNIT_PERCENT;
    case SENSOR_SMT100_TEMP:
      return UNIT_DEGREE;
    case SENSOR_SMT100_PMTY:
      return UNIT_DK;
    case SENSOR_TH100_MOIS:
      return UNIT_HUM_PERCENT;
	  case SENSOR_TH100_TEMP:
      return UNIT_DEGREE;
#if defined(ARDUINO)
#if defined(ESP8266)
    case SENSOR_ANALOG_EXTENSION_BOARD:
      return UNIT_VOLT;
    case SENSOR_ANALOG_EXTENSION_BOARD_P:
      return UNIT_LEVEL;
    case SENSOR_SMT50_MOIS:
      return UNIT_PERCENT;
    case SENSOR_SMT50_TEMP:
      return UNIT_DEGREE;
    case SENSOR_SMT100_ANALOG_MOIS:
      return UNIT_PERCENT;
    case SENSOR_SMT100_ANALOG_TEMP:
      return UNIT_DEGREE;

    case SENSOR_VH400:
      return UNIT_PERCENT;
    case SENSOR_THERM200:
      return UNIT_DEGREE;
    case SENSOR_AQUAPLUMB:
      return UNIT_PERCENT;
    case SENSOR_FREE_MEMORY:
    case SENSOR_FREE_STORE:
      return UNIT_USERDEF;
#endif
#else
    case SENSOR_OSPI_ANALOG:
      return UNIT_VOLT;
    case SENSOR_OSPI_ANALOG_P:
      return UNIT_PERCENT;
    case SENSOR_OSPI_ANALOG_SMT50_MOIS:
      return UNIT_PERCENT;
    case SENSOR_OSPI_ANALOG_SMT50_TEMP:
    case SENSOR_OSPI_INTERNAL_TEMP:
      return UNIT_DEGREE;
#endif
    case SENSOR_USERDEF:
    case SENSOR_MQTT:
    case SENSOR_REMOTE:
      return sensor->assigned_unitid > 0 ? sensor->assigned_unitid
                                         : UNIT_USERDEF;

    case SENSOR_WEATHER_TEMP_F:
      return UNIT_FAHRENHEIT;
    case SENSOR_WEATHER_TEMP_C:
      return UNIT_DEGREE;
    case SENSOR_WEATHER_HUM:
      return UNIT_HUM_PERCENT;
    case SENSOR_WEATHER_PRECIP_IN:
      return UNIT_INCH;
    case SENSOR_WEATHER_PRECIP_MM:
      return UNIT_MM;
    case SENSOR_WEATHER_WIND_MPH:
      return UNIT_MPH;
    case SENSOR_WEATHER_WIND_KMH:
      return UNIT_KMH;

    case SENSOR_GROUP_MIN:
    case SENSOR_GROUP_MAX:
    case SENSOR_GROUP_AVG:
    case SENSOR_GROUP_SUM:

      for (int i = 0; i < 100; i++) {
        Sensor_t *sen = sensors;
        while (sen) {
          if (sen != sensor && sen->group > 0 && sen->group == sensor->nr) {
            if (!sensor_isgroup(sen)) return getSensorUnitId(sen);
            sensor = sen;
            break;
          }
          sen = sen->next;
        }
      }
      
    default:
      return UNIT_NONE;
  }
}

void GetSensorWeather() {
#if defined(ESP8266)
  if (!useEth)
    if (os.state != OS_STATE_CONNECTED || WiFi.status() != WL_CONNECTED) return;
#endif
  time_t time = os.now_tz();
  if (last_weather_time == 0) last_weather_time = time - 59 * 60;

  if (time < last_weather_time + 60 * 60) return;

  // use temp buffer to construct get command
  BufferFiller bf = BufferFiller(tmp_buffer, TMP_BUFFER_SIZE);
  bf.emit_p(PSTR("weatherData?loc=$O&wto=$O"), SOPT_LOCATION,
            SOPT_WEATHER_OPTS);

  char *src = tmp_buffer + strlen(tmp_buffer);
  char *dst = tmp_buffer + TMP_BUFFER_SIZE - 12;

  char c;
  // url encode. convert SPACE to %20
  // copy reversely from the end because we are potentially expanding
  // the string size
  while (src != tmp_buffer) {
    c = *src--;
    if (c == ' ') {
      *dst-- = '0';
      *dst-- = '2';
      *dst-- = '%';
    } else {
      *dst-- = c;
    }
  };
  *dst = *src;

  strcpy(ether_buffer, "GET /");
  strcat(ether_buffer, dst);
  // because dst is part of tmp_buffer,
  // must load weather url AFTER dst is copied to ether_buffer

  // load weather url to tmp_buffer
  char *host = tmp_buffer;
  os.sopt_load(SOPT_WEATHERURL, host);

  strcat(ether_buffer, " HTTP/1.0\r\nHOST: ");
  strcat(ether_buffer, host);
  strcat(ether_buffer, "\r\n\r\n");

  DEBUG_PRINTLN(F("GetSensorWeather"));
  DEBUG_PRINTLN(ether_buffer);

  last_weather_time = time;
  int ret = os.send_http_request(host, ether_buffer, NULL, false, 500);
  if (ret == HTTP_RQT_SUCCESS) {
    DEBUG_PRINTLN(ether_buffer);

    char buf[20];
    char *s = strstr(ether_buffer, "\"temp\":");
    if (s && extract(s, buf, sizeof(buf))) {
      current_temp = atof(buf);
    }
    s = strstr(ether_buffer, "\"humidity\":");
    if (s && extract(s, buf, sizeof(buf))) {
      current_humidity = atof(buf);
    }
    s = strstr(ether_buffer, "\"precip\":");
    if (s && extract(s, buf, sizeof(buf))) {
      current_precip = atof(buf);
    }
    s = strstr(ether_buffer, "\"wind\":");
    if (s && extract(s, buf, sizeof(buf))) {
      current_wind = atof(buf);
    }
#ifdef ENABLE_DEBUG
    char tmp[10];
    DEBUG_PRINT("temp: ");
    dtostrf(current_temp, 2, 2, tmp);
    DEBUG_PRINTLN(tmp)
    DEBUG_PRINT("humidity: ");
    dtostrf(current_humidity, 2, 2, tmp);
    DEBUG_PRINTLN(tmp)
    DEBUG_PRINT("precip: ");
    dtostrf(current_precip, 2, 2, tmp);
    DEBUG_PRINTLN(tmp)
    DEBUG_PRINT("wind: ");
    dtostrf(current_wind, 2, 2, tmp);
    DEBUG_PRINTLN(tmp)
#endif
    current_weather_ok = true;
  } else {
    current_weather_ok = false;
  }
}

void SensorUrl_load() {
  sensorUrls = NULL;
  DEBUG_PRINTLN("SensorUrl_load1");
  if (!file_exists(SENSORURL_FILENAME)) return;

  DEBUG_PRINTLN("SensorUrl_load2");
  ulong pos = 0;
  SensorUrl_t *last = NULL;
  while (true) {
    SensorUrl_t *sensorUrl = new SensorUrl_t;
    memset(sensorUrl, 0, sizeof(SensorUrl_t));
    if (file_read_block(SENSORURL_FILENAME, sensorUrl, pos,
                        SENSORURL_STORE_SIZE) < SENSORURL_STORE_SIZE) {
      delete sensorUrl;
      break;
    }
    sensorUrl->urlstr = (char *)malloc(sensorUrl->length + 1);
    pos += SENSORURL_STORE_SIZE;
    ulong result = file_read_block(SENSORURL_FILENAME, sensorUrl->urlstr, pos,
                    sensorUrl->length);
    if (result != sensorUrl->length) {
      free(sensorUrl->urlstr);
      delete sensorUrl;
      break;
    }
                
    sensorUrl->urlstr[sensorUrl->length] = 0;
    pos += sensorUrl->length;

    DEBUG_PRINT(sensorUrl->nr);
    DEBUG_PRINT("/");
    DEBUG_PRINT(sensorUrl->type);
    DEBUG_PRINT(": ");
    DEBUG_PRINTLN(sensorUrl->urlstr);

    if (!last)
      sensorUrls = sensorUrl;
    else
      last->next = sensorUrl;
    last = sensorUrl;
    sensorUrl->next = NULL;
  }
  DEBUG_PRINTLN("SensorUrl_load3");
}

void SensorUrl_save() {
  if (!apiInit) return;
  if (file_exists(SENSORURL_FILENAME)) remove_file(SENSORURL_FILENAME);

  ulong pos = 0;
  SensorUrl_t *sensorUrl = sensorUrls;
  while (sensorUrl) {
    file_write_block(SENSORURL_FILENAME, sensorUrl, pos, SENSORURL_STORE_SIZE);
    pos += SENSORURL_STORE_SIZE;
    file_write_block(SENSORURL_FILENAME, sensorUrl->urlstr, pos,
                     sensorUrl->length);
    pos += sensorUrl->length;

    sensorUrl = sensorUrl->next;
  }
}

bool SensorUrl_delete(uint nr, uint type) {
  SensorUrl_t *sensorUrl = sensorUrls;
  SensorUrl_t *last = NULL;
  while (sensorUrl) {
    if (sensorUrl->nr == nr && sensorUrl->type == type) {
      if (last)
        last->next = sensorUrl->next;
      else
        sensorUrls = sensorUrl->next;

      sensor_mqtt_unsubscribe(nr, type, sensorUrl->urlstr);

      free(sensorUrl->urlstr);
      delete sensorUrl;
      SensorUrl_save();
      return true;
    }
    last = sensorUrl;
    sensorUrl = sensorUrl->next;
  }
  return false;
}

bool SensorUrl_add(uint nr, uint type, const char *urlstr) {
  if (!urlstr || !strlen(urlstr)) {  // empty string? delete!
    return SensorUrl_delete(nr, type);
  }
  SensorUrl_t *sensorUrl = sensorUrls;
  while (sensorUrl) {
    if (sensorUrl->nr == nr && sensorUrl->type == type) {  // replace existing
      sensor_mqtt_unsubscribe(nr, type, sensorUrl->urlstr);
      free(sensorUrl->urlstr);
      sensorUrl->length = strlen(urlstr);
      sensorUrl->urlstr = strdup(urlstr);
      SensorUrl_save();
      sensor_mqtt_subscribe(nr, type, urlstr);
      return true;
    }
    sensorUrl = sensorUrl->next;
  }

  // Add new:
  sensorUrl = new SensorUrl_t;
  memset(sensorUrl, 0, sizeof(SensorUrl_t));
  sensorUrl->nr = nr;
  sensorUrl->type = type;
  sensorUrl->length = strlen(urlstr);
  sensorUrl->urlstr = strdup(urlstr);
  sensorUrl->next = sensorUrls;
  sensorUrls = sensorUrl;
  SensorUrl_save();

  sensor_mqtt_subscribe(nr, type, urlstr);

  return true;
}

char *SensorUrl_get(uint nr, uint type) {
  SensorUrl_t *sensorUrl = sensorUrls;
  while (sensorUrl) {
    if (sensorUrl->nr == nr && sensorUrl->type == type)  // replace existing
      return sensorUrl->urlstr;
    sensorUrl = sensorUrl->next;
  }
  return NULL;
}

/**
 * @brief Write data to influx db
 * 
 * @param sensor 
 * @param log 
 */
void add_influx_data(Sensor_t *sensor) {
  if (!os.influxdb.isEnabled())
    return;

  #if defined(ESP8266)
  Point sensor_data("analogsensor");
  os.sopt_load(SOPT_DEVICE_NAME, tmp_buffer);
  sensor_data.addTag("devicename", tmp_buffer);
  snprintf(tmp_buffer, 10, "%d", sensor->nr);
  sensor_data.addTag("nr", tmp_buffer);
  sensor_data.addTag("name", sensor->name);
  sensor_data.addTag("unit", getSensorUnit(sensor));

  sensor_data.addField("native_data", sensor->last_native_data);
  sensor_data.addField("data", sensor->last_data);

  os.influxdb.write_influx_data(sensor_data);

  #else

/*
influxdb_cpp::server_info si("127.0.0.1", 8086, "db", "usr", "pwd");
influxdb_cpp::builder()
    .meas("foo")
    .tag("k", "v")
    .tag("x", "y")
    .field("x", 10)
    .field("y", 10.3, 2)
    .field("z", 10.3456)
    .field("b", !!10)
    .timestamp(1512722735522840439)
    .post_http(si);
*/
  influxdb_cpp::server_info * client = os.influxdb.get_client();
  if (!client)
    return;

  os.sopt_load(SOPT_DEVICE_NAME, tmp_buffer);
  char nr_buf[10];
  snprintf(nr_buf, 10, "%d", sensor->nr);
  influxdb_cpp::builder()
    .meas("analogsensor")
    .tag("devicename", tmp_buffer)
    .tag("nr", nr_buf)
    .tag("name", sensor->name)
    .tag("unit", getSensorUnit(sensor))
    .field("native_data", (long)sensor->last_native_data)
    .field("data", sensor->last_data, 2)
    .timestamp(millis())
    .post_http(*client);

  #endif
}


//Value Monitoring
void monitor_load() {
  DEBUG_PRINTLN(F("monitor_load"));
  monitors = NULL;
  if (!file_exists(MONITOR_FILENAME)) return;
  if (file_size(MONITOR_FILENAME) % MONITOR_STORE_SIZE != 0) return;
  
  ulong pos = 0;
  Monitor_t *last = NULL;
  while (true) {
    Monitor_t *mon = new Monitor_t;
    memset(mon, 0, sizeof(Monitor_t));
    file_read_block(MONITOR_FILENAME, mon, pos, MONITOR_STORE_SIZE);
    if (!mon->nr || !mon->type) {
      delete mon;
      break;
    }
    if (!last)
      monitors = mon;
    else
      last->next = mon;
    last = mon;
    mon->next = NULL;
    pos += MONITOR_STORE_SIZE;
  }
}

void monitor_save() {
  if (!apiInit) return;
  if (file_exists(MONITOR_FILENAME)) remove_file(MONITOR_FILENAME);

  ulong pos = 0;
  Monitor_t *mon = monitors;
  while (mon) {
    file_write_block(MONITOR_FILENAME, mon, pos, MONITOR_STORE_SIZE);
    mon = mon->next;
    pos += MONITOR_STORE_SIZE;
  }
}

int monitor_count() {
  int count = 0;
  Monitor_t *mon = monitors;
  while (mon) {
    mon = mon->next;
    count++;
  }
  return count;
}

int monitor_delete(uint nr) {
  Monitor_t *p = monitors;

  Monitor_t *last = NULL;

  while (p) {
    if (p->nr == nr) {
      if (last)
        last->next = p->next;
      else
        monitors = p->next;
      delete p;
      monitor_save();
      return HTTP_RQT_SUCCESS;
    }
    last = p;
    p = p->next;
  }
  return HTTP_RQT_NOT_RECEIVED;
}

bool monitor_define(uint nr, uint type, uint sensor, uint prog, uint zone, const Monitor_Union_t m, char * name, ulong maxRuntime, uint8_t prio) {
  Monitor_t *p = monitors;

  Monitor_t *last = NULL;

  while (p) {
    if (p->nr == nr) {
      p->type = type;
      p->sensor = sensor;
      p->prog = prog;
      p->zone = zone;
      p->m = m;
      //p->active = false;
      p->maxRuntime = maxRuntime;
      p->prio = prio;
      strncpy(p->name, name, sizeof(p->name)-1);
      monitor_save();
      check_monitors();
      return HTTP_RQT_SUCCESS;
    }

    if (p->nr > nr) break;

    last = p;
    p = p->next;
  }

  p = new Monitor_t;
  p->nr = nr;
  p->type = type;
  p->sensor = sensor;
  p->prog = prog;
  p->zone = zone;
  p->m = m;
  p->active = false;
  p->maxRuntime = maxRuntime;
  p->prio = prio;
  strncpy(p->name, name, sizeof(p->name)-1);
  if (last) {
    p->next = last->next;
    last->next = p;
  } else {
    p->next = monitors;
    monitors = p;
  }

  monitor_save();
  check_monitors();
  return HTTP_RQT_SUCCESS;
}

Monitor_t *monitor_by_nr(uint nr) {
  Monitor_t *mon = monitors;
  while (mon) {
    if (mon->nr == nr) return mon;
    mon = mon->next;
  }
  return NULL;
}

Monitor_t *monitor_by_idx(uint idx) {
  Monitor_t *mon = monitors;
  uint idxCounter = 0;
  while (mon) {
    if (idxCounter++ == idx) return mon;
    mon = mon->next;
  }
  return NULL;
}

void manual_start_program(unsigned char, unsigned char);
void schedule_all_stations(time_os_t curr_time);
void turn_off_station(unsigned char sid, time_os_t curr_time, unsigned char shift=0);
void push_message(uint16_t type, uint32_t lval=0, float fval=0.f, const char* sval=NULL);

void start_monitor_action(Monitor_t * mon) {
  mon->time = os.now_tz();
  if (mon->prog > 0)
    manual_start_program(mon->prog, 255);

  if (mon->zone > 0) {
    uint sid = mon->zone-1;

		// schedule manual station
		// skip if the station is a master station
		// (because master cannot be scheduled independently)
		if ((os.status.mas==sid+1) || (os.status.mas2==sid+1))
			return;

    uint16_t timer=mon->maxRuntime;
    RuntimeQueueStruct *q = NULL;
		unsigned char sqi = pd.station_qid[sid];
		// check if the station already has a schedule
		if (sqi!=0xFF) {  // if so, we will overwrite the schedule
			q = pd.queue+sqi;
		} else {  // otherwise create a new queue element
			q = pd.enqueue();
		}
		// if the queue is not full
		if (q) {
			q->st = 0;
			q->dur = timer;
			q->sid = sid;
			q->pid = 253;
			schedule_all_stations(mon->time);
		} 
  }
}

void stop_monitor_action(Monitor_t * mon) {
  mon->time = os.now_tz();
  if (mon->zone > 0) {
    int sid = mon->zone-1;
    RuntimeQueueStruct *q = pd.queue + pd.station_qid[sid];
    if (q) {
		  q->deque_time = mon->time;
		  turn_off_station(sid, mon->time);
    }
  }
}

void push_message(Monitor_t * mon, float value) {
  uint16_t type; 
  switch(mon->prio) {
    case 0: type = NOTIFY_MONITOR_LOW; break;
    case 1: type = NOTIFY_MONITOR_MID; break;
    case 2: type = NOTIFY_MONITOR_HIGH; break;
    default: return;
  }
  char name[30];
  strncpy(name, mon->name, sizeof(name)-1);
  DEBUG_PRINT("monitoring: activated ");
  DEBUG_PRINT(name);
  DEBUG_PRINT(" - ");
  DEBUG_PRINTLN(type);
  push_message(type, (uint32_t)mon->prio, value, name);
}

bool get_monitor(uint nr, bool inv, bool defaultBool) {
  Monitor_t *mon = monitor_by_nr(nr);
  if (!mon) return defaultBool;
  return inv ? !mon->active : mon->active;
}

bool get_remote_monitor(Monitor_t *mon, bool defaultBool) {
#if defined(ESP8266)
  IPAddress _ip(mon->m.remote.ip);
  unsigned char ip[4] = {_ip[0], _ip[1], _ip[2], _ip[3]};
#else
  unsigned char ip[4];
  ip[3] = (unsigned char)((mon->m.remote.ip >> 24) & 0xFF);
  ip[2] = (unsigned char)((mon->m.remote.ip >> 16) & 0xFF);
  ip[1] = (unsigned char)((mon->m.remote.ip >> 8) & 0xFF);
  ip[0] = (unsigned char)((mon->m.remote.ip & 0xFF));
#endif

  DEBUG_PRINTLN(F("read_monitor_http"));

  char *p = tmp_buffer;
  BufferFiller bf = BufferFiller(tmp_buffer, TMP_BUFFER_SIZE);

  bf.emit_p(PSTR("GET /ml?pw=$O&nr=$D"), SOPT_PASSWORD, mon->m.remote.rmonitor);
  bf.emit_p(PSTR(" HTTP/1.0\r\nHOST: $D.$D.$D.$D\r\n\r\n"), ip[0], ip[1], ip[2], ip[3]);

  DEBUG_PRINTLN(p);

  char server[20];
  sprintf(server, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

  int res = os.send_http_request(server, mon->m.remote.port, p, NULL, false, 500);
  if (res == HTTP_RQT_SUCCESS) {
    DEBUG_PRINTLN("Send Ok");
    p = ether_buffer;
    DEBUG_PRINTLN(p);

    char buf[20];
    char *s = strstr(p, "\"time\":");
    if (s && extract(s, buf, sizeof(buf))) {
      ulong time = strtoul(buf, NULL, 0);
      if (time == 0 || time == mon->time) {
        return defaultBool;
      } else {
        mon->time = time;
      }
    }

    s = strstr(p, "\"active\":");
    if (s && extract(s, buf, sizeof(buf))) {
      return strtoul(buf, NULL, 0);
    }

    return HTTP_RQT_SUCCESS;
  }
  return defaultBool;
}

void check_monitors() {
  Monitor_t *mon = monitors;
  while (mon) {
    uint nr = mon->nr;

    bool wasActive = mon->active;
    double value = 0;

    switch(mon->type) {
      case MONITOR_MIN: 
      case MONITOR_MAX: {
       Sensor_t * sensor = sensor_by_nr(mon->sensor);
        if (sensor && sensor->flags.data_ok) {
          value = sensor->last_data;

          if (!mon->active) {
            if ((mon->type == MONITOR_MIN && value <= mon->m.minmax.value1) || 
              (mon->type == MONITOR_MAX && value >= mon->m.minmax.value1)) {
              mon->active = true;
            }
          } else {
            if ((mon->type == MONITOR_MIN && value >= mon->m.minmax.value2) || 
              (mon->type == MONITOR_MAX && value <= mon->m.minmax.value2)) {
              mon->active = false;
            }
          }
        }
        break; }

      case MONITOR_SENSOR12:
        if (mon->m.sensor12.sensor12 == 1)        
      	  if (os.iopts[IOPT_SENSOR1_TYPE] == SENSOR_TYPE_RAIN || os.iopts[IOPT_SENSOR1_TYPE] == SENSOR_TYPE_SOIL)
            mon->active = mon->m.sensor12.invers? !os.status.sensor1_active : os.status.sensor1_active;
        if (mon->m.sensor12.sensor12 == 2)
          if (os.iopts[IOPT_SENSOR2_TYPE] == SENSOR_TYPE_RAIN || os.iopts[IOPT_SENSOR2_TYPE] == SENSOR_TYPE_SOIL)
            mon->active = mon->m.sensor12.invers? !os.status.sensor2_active : os.status.sensor2_active;
        break;

      case MONITOR_SET_SENSOR12:
        mon->active = get_monitor(mon->m.set_sensor12.monitor, false, false);
        if (mon->m.set_sensor12.sensor12 == 1) {
          os.status.forced_sensor1 = mon->active;
        }
        if (mon->m.set_sensor12.sensor12 == 2) {
          os.status.forced_sensor2 = mon->active;
        }
        break;
      case MONITOR_AND:
        mon->active = get_monitor(mon->m.andorxor.monitor1, mon->m.andorxor.invers1, true) &&
          get_monitor(mon->m.andorxor.monitor2, mon->m.andorxor.invers2, true) &&
          get_monitor(mon->m.andorxor.monitor3, mon->m.andorxor.invers3, true) &&
          get_monitor(mon->m.andorxor.monitor4, mon->m.andorxor.invers4, true);          
        break;
      case MONITOR_OR:
        mon->active = get_monitor(mon->m.andorxor.monitor1, mon->m.andorxor.invers1, false) ||
          get_monitor(mon->m.andorxor.monitor2, mon->m.andorxor.invers2, false) ||
          get_monitor(mon->m.andorxor.monitor3, mon->m.andorxor.invers3, false) ||
          get_monitor(mon->m.andorxor.monitor4, mon->m.andorxor.invers4, false);
        break;
      case MONITOR_XOR:
        mon->active = get_monitor(mon->m.andorxor.monitor1, mon->m.andorxor.invers1, false) ^
          get_monitor(mon->m.andorxor.monitor2, mon->m.andorxor.invers2, false) ^
          get_monitor(mon->m.andorxor.monitor3, mon->m.andorxor.invers3, false) ^
          get_monitor(mon->m.andorxor.monitor4, mon->m.andorxor.invers4, false);
        break;
      case MONITOR_NOT:
        mon->active = get_monitor(mon->m.mnot.monitor, true, false);
        break;
      case MONITOR_TIME: {
        time_os_t timeNow = os.now_tz();
        uint16_t time = hour(timeNow) * 100 + minute(timeNow); //HHMM
#if defined(ARDUINO)       
        uint8_t wday = (weekday(timeNow)+5)%7; //Monday = 0
#else
        time_os_t ct = timeNow;
	struct tm *ti = gmtime(&ct);
	uint8_t wday = (ti->tm_wday+1)%7; 
#endif
        mon->active  = (mon->m.mtime.weekdays >> wday) & 0x01;
        if (mon->m.mtime.time_from > mon->m.mtime.time_to) // FROM > TO ? Over night value
          mon->active &= time >= mon->m.mtime.time_from || time <= mon->m.mtime.time_to;
        else
          mon->active &= time >= mon->m.mtime.time_from && time <= mon->m.mtime.time_to;
        break;
      }
      case MONITOR_REMOTE:
        mon->active = get_remote_monitor(mon, wasActive);
        break;
    }

    if (mon->active != wasActive) {
      if (mon->active) {
        start_monitor_action(mon);
        push_message(mon, value);
        mon = monitor_by_nr(nr); //restart because if send by mail we unloaded+reloaded the monitors
      } else {
        stop_monitor_action(mon);
      }
    }

    mon = mon->next;
  }
}