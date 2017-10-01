/* OpenSprinkler Unified (AVR/RPI/BBB/LINUX) Firmware
* Copyright (C) 2015 by Ray Wang (ray@opensprinkler.com)
*
* Notification routines
* Feb 2015 @ OpenSprinkler.com
*
* This file is part of the OpenSprinkler Firmware
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

#if !defined(ARDUINO) || defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__)

#include "OpenSprinkler.h"
#include "program.h"
#include "notifier.h"

#if defined(ARDUINO)
#include "time.h"
#include "SdFat.h"
extern SdFat sd;
extern BufferFiller bfill;
void httpget_callback(byte, uint16_t, uint16_t);
#else // header and defs for RPI/BBB
#include <sys/stat.h>
#include <netdb.h>
#include <stdlib.h>
#include <time.h>
extern char ether_buffer[ETHER_BUFFER_SIZE];
#define sprintf_P sprintf
#define sscanf_P sscanf
#endif

#define NOTIF_TIMEOUT              5    // TImeout in seconds for any network activity

#if PROGRAM_NAME_SIZE > STATION_NAME_SIZE
#define NAME_SIZE PROGRAM_NAME_SIZE
#else
#define NAME_SIZE STATION_NAME_SIZE
#endif
#define IFTTT_KEY_MAXSIZE          128  // Max size for IFTTT key - current keys are around 50 characters
#define IFTTT_EVENT_MAXNUM         8    // Maximum number of unique IFTTT endpoints
#define IFTTT_EVENT_MAXSIZE        20   // Maximum characters per IFTTT event name
#define INFLUX_SERVER_MAXSIZE      30   // Maximum length of the influx server name
#define INFLUX_DATABASE_MAXSIZE    25   // Maximum length of the influx database name
#define WEBHOOK_SERVER_MAXSIZE     30   // Maximum length of the WeebHook server name

typedef enum {
  NOTIF_PROGRAM_SCHEDULE,
  NOTIF_PROGRAM_START,
  NOTIF_PROGRAM_STOP,
  NOTIF_STATION_SCHEDULE,
  NOTIF_STATION_OPEN,
  NOTIF_STATION_CLOSE,
  NOTIF_RAINSENSOR_ON,
  NOTIF_RAINSENSOR_OFF,
  NOTIF_RAINDELAY_START,
  NOTIF_RAINDELAY_STOP,
  NOTIF_FLOW_UPDATE,
  NOTIF_WEATHERCALL_FAIL,
  NOTIF_WEATHERCALL_SUCCESS,
  NOTIF_WATERLEVEL_UPDATE,
  NOTIF_IP_UPDATE,
  NOTIF_REBOOT_COMPLETE
} notification_t;

typedef struct {
  byte id;              // Contains either an SID or a PID for station/program notifications
  char name[NAME_SIZE]; // Contains either a station or program name
  ulong start;          // Start time of a program/station run in unix epoch time in GMT
  ulong end;            // End time of a program/station run in unix epoch time in GMT
  ulong duration;       // Duration of the program/station run in seconds
  byte water_level;     // Water level at the start of a program or station run
  float volume;         // Volume of water used during the program/station run 
  float flow;           // Average flow of water during the station/program run (or 0 if no flow meter)
  ulong ip;             // Newly detected external IP address (4 byte coded)
} push_msg_t;

// IFTTT event mask used to communicate with App
#define IFTTT_PROGRAM_SCHED   0x01
#define IFTTT_RAINSENSOR      0x02
#define IFTTT_FLOWSENSOR      0x04
#define IFTTT_WEATHER_UPDATE  0x08
#define IFTTT_REBOOT          0x10
#define IFTTT_STATION_RUN     0x20
#define IFTTT_SPARE_1         0x40
#define IFTTT_SPARE_2         0x80

// Mapping table from notification types to IFTTT types
byte ifttt_map[] = { 
  0,                    // NOTIF_PROGRAM_SCHEDULE
  IFTTT_PROGRAM_SCHED,  // NOTIF_PROGRAM_START
  0,                    // NOTIF_PROGRAM_STOP
  0,                    // NOTIF_STATION_SCHEDULE
  0,                    // NOTIF_STATION_OPEN
  IFTTT_STATION_RUN,    // NOTIF_STATION_CLOSE
  IFTTT_RAINSENSOR,     // NOTIF_RAINSENSOR_ON
  IFTTT_RAINSENSOR,     // NOTIF_RAINSENSOR_OFF
  0,                    // NOTIF_RAINDELAY_START
  0,                    // NOTIF_RAINDELAY_STOP
  IFTTT_FLOWSENSOR,     // NOTIF_FLOW_UPDATE
  0,                    // NOTIF_WEATHERCALL_FAIL
  0,                    // NOTIF_WEATHERCALL_SUCCESS
  IFTTT_WEATHER_UPDATE, // NOTIF_WATERLEVEL_UPDATE
  0,                    // NOTIF_IP_UPDATE
  IFTTT_REBOOT          // NOTIF_REBOOT_COMPLETE
};

extern ProgramData pd;          // ProgramdData object
extern char tmp_buffer[];       // scratch buffer
static char post_buff[512];     // POST message - largest IFTTT message is approx 350 bytes as contains three versions of the message

static void post_msg(const char * server, int port, const char * path, const char * postval);
static void strip_spaces(char * i);

static void push_message(ulong timestamp, notification_t type, push_msg_t * msg);
static void push_ifttt_message(ulong timestamp, notification_t type, push_msg_t * m);
static void push_influxdb_message(ulong timestamp, notification_t type, push_msg_t * m);
static void push_webhook_message(ulong timestamp, notification_t type, push_msg_t * m);

static int format_text_msg(char * buff, ulong timestamp, notification_t type, push_msg_t * m);
static int format_value_msg(char * buff, ulong timestamp, notification_t type, push_msg_t * m);
static int format_json_msg(char * buff, ulong timestamp, notification_t type, push_msg_t * m);
static int format_influx_msg(char * buff, ulong timestamp, notification_t type, push_msg_t * m);

// ================================
// NOTIFICATION FUNCTIONS
// ================================
void helper_populate_pgm_message(push_msg_t * m, RuntimePgmStruct * pgm) {
  ProgramStruct prog = { 0 };
  byte pid = pgm->pid;

  if (pid == 0 || pid > pd.nprograms) {
    m->id = (pid == 99) ? 99 : 254;
    strcpy_P(m->name, (pid == 99) ? PSTR("Manual") : PSTR("Run-Once"));
    m->water_level = 100;
  }
  else {
    pd.read(pid - 1, &prog);
    m->id = pid - 1;
    strcpy(m->name, (prog.name) ? prog.name : "Unknown");
    m->water_level = prog.use_weather ? os.options[OPTION_WATER_PERCENTAGE] : 100;
  }

  m->start = pgm->st - (int32_t)3600 / 4 * (int32_t)(os.options[OPTION_TIMEZONE] - 48);
  m->end = pgm->et - (int32_t)3600 / 4 * (int32_t)(os.options[OPTION_TIMEZONE] - 48);
  m->duration = m->end - m->start;

  if (pgm->volume > 0 && os.options[OPTION_SENSOR_TYPE] == SENSOR_TYPE_FLOW) {
    m->volume = pgm->volume * ((os.options[OPTION_PULSE_RATE_1] << 8) + os.options[OPTION_PULSE_RATE_0]) / 100.0;
    m->flow = (m->duration == 0) ? 0 : m->volume * 60.0 / m->duration;
  }
}

void helper_populate_stn_message(push_msg_t * m, RuntimeQueueStruct * q)
{
  m->id = q->sid;
  os.get_station_name(q->sid, m->name);
  if (!m->name) strcpy_P(m->name, PSTR("Unknown"));
  m->start = q->st - (int32_t)3600 / 4 * (int32_t)(os.options[OPTION_TIMEZONE] - 48);
  m->end = m->start + q->dur;
  m->duration = q->dur;
  m->water_level = q->wl;

  if (q->volume > 0 && os.options[OPTION_SENSOR_TYPE] == SENSOR_TYPE_FLOW) {
    m->volume = q->volume * ((os.options[OPTION_PULSE_RATE_1] << 8) + os.options[OPTION_PULSE_RATE_0]) / 100.0;
    m->flow = (m->duration == 0) ? 0 : m->volume * 60.0 / m->duration;
  }
}

#define helper_populate_time_fields(m, s, e, d) {m.start = s; m.end = e; m.duration = d;}

void push_program_schedule(RuntimePgmStruct * pgm) {
  push_msg_t m = { 0 };
  helper_populate_pgm_message(&m, pgm);
  push_message(now(), NOTIF_PROGRAM_SCHEDULE, &m);
}

void push_program_start(RuntimePgmStruct * pgm) {
  push_msg_t m = { 0 };
  helper_populate_pgm_message(&m, pgm);
  push_message(now(), NOTIF_PROGRAM_START, &m);
}

void push_program_stop(RuntimePgmStruct * pgm) {
  push_msg_t m = { 0 };
  helper_populate_pgm_message(&m, pgm);
  push_message(now(), NOTIF_PROGRAM_STOP, &m);
}

void push_station_schedule(RuntimeQueueStruct * q) {
  push_msg_t m = { 0 };
  helper_populate_stn_message(&m, q);
  push_message(now(), NOTIF_STATION_SCHEDULE, &m);
}

void push_station_open(RuntimeQueueStruct * q) {
  push_msg_t m = { 0 };
  helper_populate_stn_message(&m, q);
  push_message(now(), NOTIF_STATION_OPEN, &m);
}

void push_station_close(RuntimeQueueStruct * q) {
  push_msg_t m = { 0 };
  helper_populate_stn_message(&m, q);
  push_message(now(), NOTIF_STATION_CLOSE, &m);
}

void push_weather_update(boolean success) {
  push_msg_t m = { 0 };
  m.start = now();
  push_message(now(), (success == true) ? NOTIF_WEATHERCALL_SUCCESS : NOTIF_WEATHERCALL_FAIL, &m);
}

void push_waterlevel_update(byte water_level) {
  push_msg_t m = { 0 };
  m.start = now();
  m.water_level = water_level;
  push_message(now(), NOTIF_WATERLEVEL_UPDATE, &m);
}

void push_flow_update(float volume, ulong duration) {
  push_msg_t m = { 0 };

  helper_populate_time_fields(m, now() - duration, now(), duration);
  m.volume = volume;
  m.flow = (duration == 0) ? 0 : volume * 60.0 / duration;
  push_message(now(), NOTIF_FLOW_UPDATE, &m);
}

void push_ip_update(ulong ip) {
  push_msg_t m = { 0 };
  m.start = now();
  m.ip = ip;
  push_message(now(), NOTIF_IP_UPDATE, &m);
}

void push_raindelay_start(ulong duration) {
  push_msg_t m = { 0 };

  helper_populate_time_fields(m, now(), now() + duration, duration);
  push_message(now(), NOTIF_RAINDELAY_START, &m);
}

void push_raindelay_stop(ulong duration) {
  push_msg_t m = { 0 };
  helper_populate_time_fields(m, now() - duration, now(), duration);
  push_message(now(), NOTIF_RAINDELAY_STOP, &m);
}

void push_rainsensor_on(void) {
  push_msg_t m = { 0 };
  m.start = now();
  push_message(now(), NOTIF_RAINSENSOR_ON, &m);
}

void push_rainsensor_off(ulong duration) {
  push_msg_t m = { 0 };
  helper_populate_time_fields(m, now() - duration, now(), duration);
  push_message(now(), NOTIF_RAINSENSOR_OFF, &m);
}

void push_reboot_complete(void) {
  push_msg_t m = { 0 };
  m.start = now();
  push_message(now(), NOTIF_REBOOT_COMPLETE, &m);
}

// ==========================================
// NOTIFICATION HANDLER
// ==========================================

void push_message(ulong timestamp, notification_t type, push_msg_t * msg) {
  push_ifttt_message(timestamp, type, msg);
  push_influxdb_message(timestamp, type, msg);
  push_webhook_message(timestamp, type, msg);
}

// ==========================================
// NOTIFICATION FUNCTIONS
// ==========================================

void push_ifttt_message(ulong timestamp, notification_t type, push_msg_t * m) {
  static const char path_format[] PROGMEM = "/trigger/%s/with/key/%s";
  char path[sizeof(path_format)/sizeof(char) + IFTTT_EVENT_MAXSIZE + IFTTT_KEY_MAXSIZE + 1] = { 0 };
  char events[IFTTT_EVENT_MAXNUM][IFTTT_EVENT_MAXSIZE + 1] = { 0 };
  char key[IFTTT_KEY_MAXSIZE + 1] = { 0 };

  if ((ifttt_map[type] & os.options[OPTION_IFTTT_ENABLE]) == 0) return;

  read_from_file(ifkey_filename, key, sizeof(key), 0);
  if (strlen(key) == 0) return;

  read_from_file(ifttt_filename, tmp_buffer);
  if (strlen(tmp_buffer) == 0) {
    sprintf_P(path, path_format, "sprinkler", key);
  } else {
    // File contains event endpoints in json format: "events":["events":["event1", ... ,"event8"]
    sscanf_P(tmp_buffer, PSTR("\"events\":[\"%[^\"]\",\"%[^\"]\",\"%[^\"]\",\"%[^\"]\",\"%[^\"]\",\"%[^\"]\",\"%[^\"]\",\"%[^\"]\""),
           events[0], events[1], events[2], events[3], events[4], events[5], events[6], events[7]);

    int pos = ffs(ifttt_map[type]) - 1;
    sprintf_P(path, path_format, events[pos], key);
  }

  // Populate all three parameters with: value1 = human text, value2 = full json, value3 = most applicable single value
  sprintf_P(post_buff, PSTR("{\"value1\":"));
  if (format_text_msg(post_buff + strlen(post_buff), timestamp, type, m) > 0) {
    strcat_P(post_buff, PSTR(",\"value2\":"));
    if (format_json_msg(post_buff + strlen(post_buff), timestamp, type, m) > 0) {
      strcat_P(post_buff, PSTR(",\"value3\":"));
      if (format_value_msg(post_buff + strlen(post_buff), timestamp, type, m) > 0) {
        strcat_P(post_buff, PSTR("}"));
        post_msg(DEFAULT_IFTTT_URL, 80, path, post_buff);
      }
    }
  }
}

void push_influxdb_message(ulong timestamp, notification_t type, push_msg_t * m) {
  static const char path_format[] PROGMEM = "/write?db=%s";
  char server[INFLUX_SERVER_MAXSIZE + 1] = { 0 };
  char db[INFLUX_DATABASE_MAXSIZE + 1] = { 0 };
  char path[sizeof(path_format)/sizeof(char) + INFLUX_DATABASE_MAXSIZE + 1] = { 0 };
  int port = 0, enable= 0;

  // File contains server, port and database settings
  read_from_file(influx_filename, tmp_buffer);
  if (strlen(tmp_buffer) == 0) return;
  sscanf_P(tmp_buffer, PSTR("\"server\":\"%[^\"]\",\"port\":\%d,\"db\":\"%[^\"]\",\"enable\":\%d"), server, &port, db, &enable);
  if (enable == 0) return;

  sprintf_P(path, path_format, db);
  if (format_influx_msg(post_buff, timestamp, type, m) > 0) {
    post_msg(server, port, path, post_buff);
  }
}

void push_webhook_message(ulong timestamp, notification_t type, push_msg_t * m) {
  char server[WEBHOOK_SERVER_MAXSIZE + 1] = { 0 };
  int port = 0, enable = 0;

  // Parameters stored as: "server":"server_name","port":port_number
  read_from_file(webhook_filename, tmp_buffer);
  if (strlen(tmp_buffer) == 0) return;
  sscanf_P(tmp_buffer, PSTR("\"server\":\"%[^\"]\",\"port\":\%d,\"enable\":\%d"), server, &port, &enable);
  if (enable == 0) return;

  if (format_json_msg(post_buff, timestamp, type, m) > 0) {
    post_msg(server, port, "", post_buff);
  }
}

// ==========================================
// NOTIFICATION FORMATTERS
// ==========================================

int format_value_msg(char * buff, ulong timestamp, notification_t type, push_msg_t * m) {
  switch(type) {

    case NOTIF_STATION_SCHEDULE:
    case NOTIF_STATION_OPEN:
    case NOTIF_STATION_CLOSE:
    case NOTIF_PROGRAM_SCHEDULE:
    case NOTIF_PROGRAM_START:
    case NOTIF_PROGRAM_STOP:
      sprintf_P(buff, PSTR("\"%s\""), (m->name) ? m->name : "Unknown");
    break;

    case NOTIF_RAINSENSOR_ON:
    case NOTIF_RAINSENSOR_OFF:
      sprintf_P(buff, PSTR("%d"), (type == NOTIF_RAINSENSOR_ON) ? 1 : 0);
    break;

    case NOTIF_RAINDELAY_START:
      sprintf_P(buff, PSTR("%lu"), m->duration);
    break;

    case NOTIF_RAINDELAY_STOP:
      sprintf_P(buff, PSTR("%lu"), m->duration);
    break;

    case NOTIF_FLOW_UPDATE:
      sprintf_P(buff, PSTR("%d.%02d"), (int)m->flow, ((int)(m->flow * 100) % 100));
    break;

    case NOTIF_WEATHERCALL_FAIL:
    case NOTIF_WEATHERCALL_SUCCESS:
      sprintf_P(buff, PSTR("%d"), (type == NOTIF_WEATHERCALL_FAIL) ? 0 : 1);
    break;

    case NOTIF_WATERLEVEL_UPDATE:
      sprintf_P(buff, PSTR("%d"), m->water_level);
    break;

    case NOTIF_IP_UPDATE:
      sprintf_P(buff, PSTR("\"%d.%d.%d.%d\""),
                (byte)(m->ip >> 24) & 0xFF, (byte)(m->ip >> 16) & 0xFF, (byte)(m->ip >> 8) & 0xFF, (byte)m->ip & 0xFF);
    break;

    case NOTIF_REBOOT_COMPLETE:
      #if defined(ARDUINO)
        sprintf_P(buff, PSTR("\"Reboot\""));
      #else
        sprintf_P(buff, PSTR("\"Restart\""));
      #endif
    break;

    default:
      return 0;
  }

  return strlen(buff);
}

int format_text_msg(char * buff, ulong timestamp, notification_t type, push_msg_t * m) {
  char site[NAME_SIZE + 1] = { 0 };
  char * offset = buff;

  offset += sprintf_P(buff, PSTR("\""));
  read_from_file(site_filename, site, sizeof(site), 0);
  if (strlen(site) != 0)
    offset += sprintf_P(offset, PSTR("%s: "), site);

  switch(type) {

    case NOTIF_STATION_SCHEDULE:
      sprintf_P(offset, PSTR("Station %s scheduled for %lu mins %lu sec with water level of %d%%."),
                m->name, m->duration / 60, m->duration % 60, m->water_level);
    break;

    case NOTIF_STATION_OPEN:
      sprintf_P(offset, PSTR("Station %s running for %lu mins %lu secs with water level of %d%%."),
                m->name, m->duration / 60,  m->duration % 60, m->water_level);
    break;

    case NOTIF_STATION_CLOSE:
      sprintf_P(offset, PSTR("Station %s ran for %lu mins %lu secs with volume of %d.%02d and flow rate at %d.%02d."),
                m->name, m->duration / 60, m->duration % 60, (int)m->volume, ((int)(m->volume * 100) % 100), (int)m->flow, ((int)(m->flow * 100) % 100));
    break;

    case NOTIF_PROGRAM_SCHEDULE:
      sprintf_P(offset, PSTR("Program %s scheduled for %lu mins %lu sec with water level of %d%%."),
                m->name, m->duration / 60, m->duration % 60, m->water_level);
    break;

    case NOTIF_PROGRAM_START:
      sprintf_P(offset, PSTR("Program %s running for %lu mins %lu sec with water level of %d%%."),
                m->name, m->duration / 60, m->duration % 60, m->water_level);
    break;

    case NOTIF_PROGRAM_STOP:
      sprintf_P(offset, PSTR("Program %s ran for %lu mins %lu sec with water volume of %d.%02d and flow rate at %d.%02d."),
                m->name, m->duration / 60, m->duration % 60, (int)m->volume, ((int)(m->volume * 100) % 100), (int)m->flow, ((int)(m->flow * 100) % 100));
    break;

    case NOTIF_RAINSENSOR_ON:
    case NOTIF_RAINSENSOR_OFF:
      sprintf_P(offset, PSTR("Rain sensor %s."), (type == NOTIF_RAINSENSOR_ON) ? "activated" : "de-activated");
    break;

    case NOTIF_RAINDELAY_START:
      sprintf_P(offset, PSTR("Rain delay started for %lu mins %lu secs."), m->duration / 60, m->duration % 60);
    break;

    case NOTIF_RAINDELAY_STOP:
      sprintf_P(offset, PSTR("Rain delay finished."));
    break;

    case NOTIF_FLOW_UPDATE:
      sprintf_P(offset, PSTR("%d.%02d of water delivered at flow rate of %d.%02d during last %lu mins %lu sec."),
                (int)m->volume, ((int)(m->volume * 100) % 100), (int)m->flow, ((int)(m->flow * 100) % 100), m->duration / 60, m->duration % 60);
    break;

    case NOTIF_WEATHERCALL_FAIL:
    case NOTIF_WEATHERCALL_SUCCESS:
        sprintf_P(offset, PSTR("Weather call %s."), (type == NOTIF_WEATHERCALL_FAIL) ? "Failed" : "Succeeded");
    break;

    case NOTIF_WATERLEVEL_UPDATE:
      sprintf_P(offset, PSTR("Water level set to %d%%."), m->water_level);
    break;

    case NOTIF_IP_UPDATE:
      sprintf_P(offset, PSTR("External IP address updated to %d.%d.%d.%d."),
                (byte)(m->ip >> 24) & 0xFF, (byte)(m->ip >> 16) & 0xFF, (byte)(m->ip >> 8) & 0xFF, (byte)m->ip & 0xFF);
    break;

    case NOTIF_REBOOT_COMPLETE:
      #if defined(ARDUINO)
        strcat_P(offset, PSTR("Device rebooted."));
      #else
        strcat_P(offset, PSTR("Process restarted."));
      #endif
    break;

    default:
      return 0;
  }

  strcat_P(buff, PSTR("\""));
  return strlen(buff);
}

int format_json_msg(char * buff, ulong timestamp, notification_t type, push_msg_t * m) {
  char site[NAME_SIZE+1] = { 0 };
  char * offset = NULL;

  read_from_file(site_filename, site, sizeof(site), 0);
  if (site[0] == NULL) strcpy_P(site, PSTR("Site"));
  offset = buff + sprintf_P(buff, PSTR("{\"timestamp\":%lu,"), timestamp);

  switch(type) {

    case NOTIF_PROGRAM_SCHEDULE:
    case NOTIF_PROGRAM_START:
    case NOTIF_STATION_SCHEDULE:
    case NOTIF_STATION_OPEN:
      sprintf_P(offset, PSTR("\"type\":\"%s\",\"data\":{\"site\":\"%s\",\"id\":%d,\"name\":\"%s\",\"start\":%lu,\"end\":%lu,\"duration\":%lu,\"water_level\":%d"),
        (type == NOTIF_PROGRAM_SCHEDULE) ? "program_schedule" : (type == NOTIF_PROGRAM_START) ? "program_start" : (type == NOTIF_STATION_SCHEDULE) ? "station_schedule" : "station_open",
        site, m->id, m->name, m->start, m->end, m->duration, m->water_level);
    break;

    case NOTIF_PROGRAM_STOP:
    case NOTIF_STATION_CLOSE:
      sprintf_P(offset, PSTR("\"type\":\"%s\",\"data\":{\"site\":\"%s\",\"id\":%d,\"name\":\"%s\",\"start\":%lu,\"end\":%lu,\"duration\":%lu,\"water_level\":%d,\"volume\":%d.%02d,\"flow\":%d.%02d"),
        (type == NOTIF_PROGRAM_STOP) ? "program_stop" : "station_close", site, m->id, m->name, m->start, m->end, m->duration, m->water_level,(int)m->volume, ((int)(m->volume * 100) % 100), (int)m->flow, ((int)(m->flow * 100) % 100));
    break;

    case NOTIF_RAINSENSOR_ON:
      sprintf_P(offset, PSTR("\"type\":\"rain_sensor\",\"data\":{\"site\":\"%s\",\"start\":%lu,\"status\":1"), site, m->start);
    break;

    case NOTIF_RAINSENSOR_OFF:
    case NOTIF_RAINDELAY_START:
    case NOTIF_RAINDELAY_STOP:
      sprintf_P(offset, PSTR("\"type\":\"%s\",\"data\":{\"site\":\"%s\",\"start\":%lu,\"end\":%lu,\"duration\":%lu,\"status\":%d"),
        (type == NOTIF_RAINSENSOR_OFF) ? "rain_sensor" : "rain_delay", site, m->start, m->end , m->duration, (type == NOTIF_RAINDELAY_START) ? 1 : 0);
    break;

    case NOTIF_FLOW_UPDATE:
      sprintf_P(offset, PSTR("\"type\":\"flow\",\"data\":{\"site\":\"%s\",\"start\":%lu,\"end\":%lu,\"duration\":%lu,\"volume\":%d.%02d,\"flow\":%d.%02d"),
        site, m->start, m->end , m->duration, (int)m->volume, ((int)(m->volume * 100) % 100), (int)m->flow, ((int)(m->flow * 100) % 100));
    break;

    case NOTIF_WEATHERCALL_FAIL:
    case NOTIF_WEATHERCALL_SUCCESS:
      sprintf_P(offset, PSTR("\"type\":\"weather_call\",\"data\":{\"site\":\"%s\",\"start\":%lu,\"status\":%d"), site, m->start, (type == NOTIF_WEATHERCALL_FAIL) ? 0 : 1);
    break;

    case NOTIF_WATERLEVEL_UPDATE:
      sprintf_P(offset, PSTR("\"type\":\"water_level\",\"data\":{\"site\":\"%s\",\"start\":%lu,\"level\":%d"), site, m->start, m->water_level);
    break;

    case NOTIF_IP_UPDATE:
      sprintf_P(offset, PSTR("\"type\":\"ip_update\",\"data\":{\"site\":\"%s\",\"start\":%lu,\"ip\":\"%d.%d.%d.%d\""),
        site, m->start, (byte)(m->ip >> 24) & 0xFF, (byte)(m->ip >> 16) & 0xFF, (byte)(m->ip >> 8) & 0xFF, (byte)m->ip & 0xFF);
    break;

    case NOTIF_REBOOT_COMPLETE:
      sprintf_P(offset, PSTR("\"type\":\"reboot\",\"data\":{\"site\":\"%s\",\"start\":%lu"), site, m->start);
    break;

    default:
      return 0;
  }

  strcat_P(buff, PSTR("}}"));
  return strlen(buff);
}

int format_influx_msg(char * buff, ulong timestamp, notification_t type, push_msg_t * m) {
  char site[NAME_SIZE+1] = { 0 };    // InfluxDB Tag - Name of the device site
  char name[NAME_SIZE + 1] = { 0 };    // InfluxDB Tag - Name of the program or station

  read_from_file(site_filename, site, sizeof(site), 0);
  if (*site == NULL) strcpy_P(site, PSTR("Site"));
  strcpy(name, m->name);

  // InfluxDB doesn't like spaces in tag names
  strip_spaces(site);
  strip_spaces(name);

  switch (type) {

    case NOTIF_PROGRAM_SCHEDULE:
    case NOTIF_PROGRAM_START:
    case NOTIF_STATION_SCHEDULE:
    case NOTIF_STATION_OPEN:
      sprintf_P(buff, PSTR("%s,site_tag=%s,name_tag=%s,id_tag=%d status=%d,id=%d,name=\"%s\",start=%lu000,end=%lu000,duration=%lu,water_level=%d"),
        (type == NOTIF_PROGRAM_SCHEDULE || type == NOTIF_PROGRAM_START) ? "program" : "station", site, name, m->id,
        (type == NOTIF_PROGRAM_SCHEDULE || type == NOTIF_STATION_SCHEDULE) ? 1 : 2, m->id,
        m->name, m->start, m->end, m->duration, m->water_level);
      break;

    case NOTIF_PROGRAM_STOP:
    case NOTIF_STATION_CLOSE:
      sprintf_P(buff, PSTR("%s,site_tag=%s,name_tag=%s,id_tag=%d status=0,id=%d,name=\"%s\",start=%lu000,end=%lu000,duration=%lu,water_level=%d,volume=%d.%02d,flow=%d.%02d"),
        (type == NOTIF_PROGRAM_STOP) ? "program" : "station", site, name, m->id, m->id, m->name, m->start, m->end, m->duration,
        m->water_level, (int)m->volume, ((int)(m->volume * 100) % 100), (int)m->flow, ((int)(m->flow * 100) % 100));
    break;

    case NOTIF_RAINSENSOR_ON:
      sprintf_P(buff, PSTR("rain_sensor,site_tag=%s start=%lu000,status=1"), site, m->start);
    break;

    case NOTIF_RAINSENSOR_OFF:
    case NOTIF_RAINDELAY_START:
    case NOTIF_RAINDELAY_STOP:
      sprintf_P(buff, PSTR("%s,site_tag=%s start=%lu000,end=%lu000,duration=%lu,status=%d"),
        (type == NOTIF_RAINSENSOR_OFF) ? "rain_sensor" : "rain_delay",
        site, m->start, m->end, m->duration, (type == NOTIF_RAINDELAY_START) ? 1 : 0);
    break;

    case NOTIF_FLOW_UPDATE:
      sprintf_P(buff, PSTR("flow,site_tag=%s start=%lu000,end=%lu000,duration=%lu,volume=%d.%02d,flow=%d.%02d"),
        site, m->start, m->end , m->duration, (int)m->volume, ((int)(m->volume * 100) % 100), (int)m->flow, ((int)(m->flow * 100) % 100));
    break;

    case NOTIF_WEATHERCALL_FAIL:
    case NOTIF_WEATHERCALL_SUCCESS:
      sprintf_P(buff, PSTR("weather_call,site_tag=%s start=%lu000,status=%d"), site, m->start, (type == NOTIF_WEATHERCALL_FAIL) ? 0 : 1);
    break;

    case NOTIF_WATERLEVEL_UPDATE:
      sprintf_P(buff, PSTR("water_level,site_tag=%s start=%lu000,level=%d"), site, m->start, m->water_level);
    break;

    case NOTIF_IP_UPDATE:
      sprintf_P(buff, PSTR("ip_update,site_tag=%s start=%lu000,ip=\"%d.%d.%d.%d\""),
        site, m->start, (byte)(m->ip >> 24) & 0xFF, (byte)(m->ip >> 16) & 0xFF, (byte)(m->ip >> 8) & 0xFF, (byte)m->ip & 0xFF);
    break;

    case NOTIF_REBOOT_COMPLETE:
      sprintf_P(buff, PSTR("reboot,site_tag=%s start=%lu000"), site, m->start);
    break;

  default:
    return 0;
  }

  return strlen(buff);
}

// ==========================================
// HELPER FUNCTIONS
// ==========================================
void check_return_code(const char * buff)
{
#if defined(SERIAL_DEBUG) || defined(ENABLE_DEBUG)
  char ret_str[4] = { 0 };
  strncpy(ret_str, buff + 9, 3);
  int ret_code = atoi(ret_str);

  if (ret_code < 200 || ret_code > 299) {
    const char * resp = strstr(buff, "\r\n\r\n");

    DEBUG_PRINT("Notification error (");
    DEBUG_PRINT(ret_code);
    DEBUG_PRINT("): ");
    if (resp) DEBUG_PRINT(resp+4);
    DEBUG_PRINTLN("");
  }
#endif
}

#if defined(ARDUINO)
static bool post_completed = false;

void post_callback(uint8_t status, uint16_t off, uint16_t len)
{
  post_completed = true;
  check_return_code((const char *)(Ethernet::buffer + off));
}
#endif

void post_msg(const char * server, int port, const char * path, const char * postval)
{
  DEBUG_PRINT("Sending notification: ");
  DEBUG_PRINTLN(postval);

#if defined(ARDUINO)

  if (os.status.network_fails > 0) {
    DEBUG_PRINTLN("Notification Aborted: No network connected");
    return;
  }

  if (!ether.dnsLookup(server, true, NOTIF_TIMEOUT * 1000)) {
    DEBUG_PRINT("Notification Aborted: Could not locate ");
    DEBUG_PRINTLN(server);
    return;
  }

  uint16_t _port = ether.hisport; // make a copy of the original port
  ether.hisport = port;

  ether.httpPostRam(path, server, NULL, postval, post_callback);

  unsigned long start = millis();
  post_completed = false;
  while ((millis() - start < NOTIF_TIMEOUT * 1000) && (post_completed == false))
    ether.packetLoop(ether.packetReceive());

  if (!post_completed) {
    DEBUG_PRINT("Notification Error: Time out sending message to ");
    DEBUG_PRINTLN(server);
  }

  ether.hisport = _port;

#else

  EthernetClient client;
  struct hostent *host;

  host = gethostbyname(server);
  if (!host) {
    DEBUG_PRINT("Notification Aborted: Could not locate ");
    DEBUG_PRINTLN(server);
    return;
  }

  // Reduce the default timeout window from 2 minutes to 3 seconds (same as read timeout)
  // Protects against locking up if the server/internet is not available
  if (!client.connect((uint8_t*)host->h_addr, port, NOTIF_TIMEOUT)) {
    client.stop();
    DEBUG_PRINT("Notification Aborted: Timeout connecting to ");
    DEBUG_PRINTLN(server);
    return;
  }

  char postBuffer[1500];
  sprintf(postBuffer,  "POST %s HTTP/1.0\r\n"
                       "Host: %s\r\n"
                       "Accept: */*\r\n"
                       "Content-Length: %lu\r\n"
                       "Content-Type: application/json\r\n"
                       "\r\n%s",
                       path, host->h_name, strlen(postval), postval);

  client.write((uint8_t *)postBuffer, strlen(postBuffer));
  bzero(ether_buffer, ETHER_BUFFER_SIZE);

  unsigned long start = millis();
  while (millis() - start < NOTIF_TIMEOUT * 1000) {
    int len = client.read((uint8_t *)ether_buffer, ETHER_BUFFER_SIZE);
    if (len <= 0) {
      if (!client.connected())
        break;
      else
        continue;
    } else {
      check_return_code(ether_buffer);
    }
  }

  if (millis() - start >= NOTIF_TIMEOUT * 1000) {
    DEBUG_PRINT("Notification Error: Time out sending message to ");
    DEBUG_PRINTLN(server);
  }

  client.stop();

#endif
}

void strip_spaces(char * i) {
  char * j = i;
  while (*j != 0) { *i = *j++; if (*i != ' ') i++; }
  *i = 0;
}
#endif
