/* OpenSprinkler Unified (AVR/RPI/BBB/LINUX) Firmware
 * Copyright (C) 2015 by Ray Wang (ray@opensprinkler.com)
 *
 * Server functions
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
#include "program.h"

// External variables defined in main ion file
#if defined(ARDUINO)

#include "SdFat.h"
extern SdFat sd;
static uint8_t ntpclientportL = 123; // Default NTP client port

#else

#include <stdarg.h>
#include <stdlib.h>
#include "etherport.h"
#include "server.h"

extern char ether_buffer[];
extern EthernetClient *m_client;

#endif

extern BufferFiller bfill;
extern char tmp_buffer[];
extern OpenSprinkler os;
extern ProgramData pd;

void write_log(byte type, ulong curr_time);
void schedule_all_stations(ulong curr_time);
void turn_off_station(byte sid, ulong curr_time);
void process_dynamic_events(ulong curr_time);
void check_network(time_t curr_time);
void check_weather(time_t curr_time);
void perform_ntp_sync(time_t curr_time);
void log_statistics(time_t curr_time);
void delete_log(char *name);
void analyze_get_url(char *p);
void reset_all_stations_immediate();
void reset_all_stations();
void make_logfile_name(char *name);
int available_ether_buffer();

// Define return error code
#define HTML_OK                0x00
#define HTML_SUCCESS           0x01
#define HTML_UNAUTHORIZED      0x02
#define HTML_MISMATCH          0x03
#define HTML_DATA_MISSING      0x10
#define HTML_DATA_OUTOFBOUND   0x11
#define HTML_DATA_FORMATERROR  0x12
#define HTML_RFCODE_ERROR      0x13
#define HTML_PAGE_NOT_FOUND    0x20
#define HTML_NOT_PERMITTED     0x30
#define HTML_REDIRECT_HOME     0xFF

static prog_uchar html200OK[] PROGMEM =
  "HTTP/1.1 200 OK\r\n"
;

static prog_uchar htmlCacheCtrl[] PROGMEM =
  "Cache-Control: max-age=604800, public\r\n"
;

static prog_uchar htmlNoCache[] PROGMEM =
  "Cache-Control: max-age=0, no-cache, no-store, must-revalidate\r\n"
;

static prog_uchar htmlContentHTML[] PROGMEM =
  "Content-Type: text/html\r\n"
;

static prog_uchar htmlAccessControl[] PROGMEM =
  "Access-Control-Allow-Origin: *\r\n"
;

static prog_uchar htmlContentJSON[] PROGMEM =
  "Content-Type: application/json\r\n"
  "Connection: close\r\n"
;

static prog_uchar htmlMobileHeader[] PROGMEM =
  "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1.0,minimum-scale=1.0,user-scalable=no\">\r\n"
;

static prog_uchar htmlReturnHome[] PROGMEM =
  "<script>window.location=\"/\";</script>\n"
;

extern const char wtopts_name[];

#if defined(ARDUINO)
void print_html_standard_header() {
  bfill.emit_p(PSTR("$F$F$F$F\r\n"), html200OK, htmlContentHTML, htmlNoCache, htmlAccessControl);
}

void print_json_header() {
  bfill.emit_p(PSTR("$F$F$F$F\r\n"), html200OK, htmlContentJSON, htmlAccessControl, htmlNoCache);
}

void print_json_header_with_bracket() {
bfill.emit_p(PSTR("$F$F$F$F\r\n{"), html200OK, htmlContentJSON, htmlAccessControl, htmlNoCache);
}

byte findKeyVal (const char *str,char *strbuf, uint8_t maxlen,const char *key,bool key_in_pgm=false,uint8_t *keyfound=NULL)
{
  uint8_t found=0;
  uint8_t i=0;
  const char *kp;
  kp=key;
  if (key_in_pgm) {
    // key is in program memory space
    while(*str &&  *str!=' ' && *str!='\n' && found==0){
      if (*str == pgm_read_byte(kp)){
        kp++;
        if (pgm_read_byte(kp) == '\0'){
          str++;
          kp=key;
          if (*str == '='){
            found=1;
          }
        }
      } else {
        kp=key;
      }
      str++;
    }
  } else {
    while(*str &&  *str!=' ' && *str!='\n' && found==0){
      if (*str == *kp){
        kp++;
        if (*kp == '\0'){
          str++;
          kp=key;
          if (*str == '='){
            found=1;
          }
        }
      } else {
        kp=key;
      }
      str++;
    }
  }
  if (found==1){
    // copy the value to a buffer and terminate it with '\0'
    while(*str &&  *str!=' ' && *str!='\n' && *str!='&' && i<maxlen-1){
      *strbuf=*str;
      i++;
      str++;
      strbuf++;
    }
    *strbuf='\0';
  }
  // return the length of the value
  if (keyfound) *keyfound = found;
  return(i);
}

void rewind_ether_buffer() {
  bfill = ether.tcpOffset();
}

void send_packet(bool final=false) {
  if(final) {
    ether.httpServerReply_with_flags(bfill.position(), TCP_FLAGS_ACK_V|TCP_FLAGS_FIN_V);
  } else {
    ether.httpServerReply_with_flags(bfill.position(), TCP_FLAGS_ACK_V);
    bfill=ether.tcpOffset();
  }
}

int available_ether_buffer() {
  return ETHER_BUFFER_SIZE - (int)bfill.position();
}

#else
void print_html_standard_header() {
  m_client->write((const uint8_t *)html200OK, strlen(html200OK));
  m_client->write((const uint8_t *)htmlContentHTML, strlen(htmlContentHTML));
  m_client->write((const uint8_t *)htmlNoCache, strlen(htmlNoCache));
  m_client->write((const uint8_t *)htmlAccessControl, strlen(htmlAccessControl));
  m_client->write((const uint8_t *)"\r\n", 2);
}

void print_json_header() {
  m_client->write((const uint8_t *)html200OK, strlen(html200OK));
  m_client->write((const uint8_t *)htmlContentJSON, strlen(htmlContentJSON));
  m_client->write((const uint8_t *)htmlNoCache, strlen(htmlNoCache));
  m_client->write((const uint8_t *)htmlAccessControl, strlen(htmlAccessControl));
  m_client->write((const uint8_t *)"\r\n", 2);
}

void print_json_header_with_bracket() {
  m_client->write((const uint8_t *)html200OK, strlen(html200OK));
  m_client->write((const uint8_t *)htmlContentJSON, strlen(htmlContentJSON));
  m_client->write((const uint8_t *)htmlNoCache, strlen(htmlNoCache));
  m_client->write((const uint8_t *)htmlAccessControl, strlen(htmlAccessControl));
  m_client->write((const uint8_t *)"\r\n{", 3);
}

byte findKeyVal (const char *str,char *strbuf, uint8_t maxlen,const char *key,bool key_in_pgm=false,uint8_t *keyfound=NULL)
{
  uint8_t found=0;
  uint8_t i=0;
  const char *kp;
  kp=key;
  while(*str &&  *str!=' ' && *str!='\n' && found==0){
    if (*str == *kp){
      kp++;
      if (*kp == '\0'){
        str++;
        kp=key;
        if (*str == '='){
            found=1;
        }
      }
    } else {
      kp=key;
    }
    str++;
  }
  if (found==1){
    // copy the value to a buffer and terminate it with '\0'
    while(*str &&  *str!=' ' && *str!='\n' && *str!='&' && i<maxlen-1){
      *strbuf=*str;
      i++;
      str++;
      strbuf++;
    }
    *strbuf='\0';
  }
  // return the length of the value
  if (keyfound) *keyfound = found;
  return(i);
}


void BufferFiller::emit_p(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  for (;;) {
    char c = *fmt++;
    if (c == 0)
      break;
    if (c != '$') {
      *ptr++ = c;
      continue;
    }
    c = *fmt++;
    switch (c) {
    case 'D':
      itoa(va_arg(ap, int), (char*) ptr, 10);  // ray
      break;
    case 'L':
      ultoa(va_arg(ap, long), (char*) ptr, 10); // ray
      break;
    case 'S':
    case 'F':
      strcpy((char*) ptr, va_arg(ap, const char*));
      break;
    case 'E': {
      byte* s = va_arg(ap, byte*);
      char d;
      while ((d = nvm_read_byte(s++)) != 0)
        *ptr++ = d;
      continue;
    }
    default:
      *ptr++ = c;
      continue;
    }
    ptr += strlen((char*) ptr);
  }
  *(ptr)=0;
  va_end(ap);
}

void rewind_ether_buffer() {
  bfill = ether_buffer;
}

void send_packet(bool final=false) {
  m_client->write((const uint8_t *)ether_buffer, strlen(ether_buffer));
  if (final)
    m_client->stop();
  else
    rewind_ether_buffer();
}

int available_ether_buffer() {
  return ETHER_BUFFER_SIZE - (int)bfill.position();
}
#endif

// convert a single hex digit character to its integer value
unsigned char h2int(char c)
{
    if (c >= '0' && c <='9'){
        return((unsigned char)c - '0');
    }
    if (c >= 'a' && c <='f'){
        return((unsigned char)c - 'a' + 10);
    }
    if (c >= 'A' && c <='F'){
        return((unsigned char)c - 'A' + 10);
    }
    return(0);
}

// decode a url string e.g "hello%20joe" or "hello+joe" becomes "hello joe"
void urlDecode (char *urlbuf)
{
    char c;
    char *dst = urlbuf;
    while ((c = *urlbuf) != 0) {
        if (c == '+') c = ' ';
        if (c == '%') {
            c = *++urlbuf;
            c = (h2int(c) << 4) | h2int(*++urlbuf);
        }
        *dst++ = c;
        urlbuf++;
    }
    *dst = '\0';
}

/** Check and verify password */
boolean check_password(char *p)
{
  if (os.options[OPTION_IGNORE_PASSWORD].value)  return true;
  if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("pw"), true)) {
    urlDecode(tmp_buffer);
    if (os.password_verify(tmp_buffer))
      return true;
  }
  return false;
}

void server_json_stations_attrib(const char* name, const byte* attrib)
{
  bfill.emit_p(PSTR("\"$F\":["), name);
  for(byte i=0;i<os.nboards;i++) {
    bfill.emit_p(PSTR("$D"), attrib[i]);
    if(i!=os.nboards-1)
      bfill.emit_p(PSTR(","));
  }
  bfill.emit_p(PSTR("],"));
}

void server_json_stations_main()
{
  server_json_stations_attrib(PSTR("masop"), os.masop_bits);
  server_json_stations_attrib(PSTR("ignore_rain"), os.ignrain_bits);
  server_json_stations_attrib(PSTR("masop2"), os.masop2_bits);
  server_json_stations_attrib(PSTR("stn_dis"), os.stndis_bits);
  server_json_stations_attrib(PSTR("rfstn"), os.rfstn_bits);
  server_json_stations_attrib(PSTR("stn_seq"), os.stnseq_bits);
  bfill.emit_p(PSTR("\"snames\":["));
  byte sid;
  for(sid=0;sid<os.nstations;sid++) {
    os.get_station_name(sid, tmp_buffer);
    bfill.emit_p(PSTR("\"$S\""), tmp_buffer);
    if(sid!=os.nstations-1)
      bfill.emit_p(PSTR(","));
    if (available_ether_buffer()<80) {
      send_packet();
    }
  }
  bfill.emit_p(PSTR("],\"maxlen\":$D}"), STATION_NAME_SIZE);
  delay(1);
}

/** Output station names and attributes */
byte server_json_stations(char *p)
{
  print_json_header_with_bracket();
  server_json_stations_main();
  return HTML_OK;
}

void server_change_stations_attrib(char *p, char header, byte *attrib)
{
  char tbuf2[3] = {0, 0, 0};
  byte bid;
  tbuf2[0]=header;
  for(bid=0;bid<os.nboards;bid++) {
    itoa(bid, tbuf2+1, 10);
    if(findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, tbuf2)) {
      attrib[bid] = atoi(tmp_buffer);
    }
  }
}

/**
  Change Station Name and Attributes
  Command: /cs?pw=xxx&s?=x&m?=x&i?=x&n?=x&d?=x

  pw: password
  s?: station name (? is station index, starting from 0)
  m?: master operation bit field (? is board index, starting from 0)
  i?: ignore rain bit field
  n?: master2 operation bit field
  d?: disable sation bit field
*/
byte server_change_stations(char *p)
{
  byte sid;
  char tbuf2[4] = {'s', 0, 0, 0};
  // process station names
  for(sid=0;sid<os.nstations;sid++) {
    itoa(sid, tbuf2+1, 10);
    if(findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, tbuf2)) {
      urlDecode(tmp_buffer);
      os.set_station_name(sid, tmp_buffer);
    }
  }

  server_change_stations_attrib(p, 'm', os.masop_bits);
  os.station_attrib_bits_save(ADDR_NVM_MAS_OP, os.masop_bits);

  server_change_stations_attrib(p, 'i', os.ignrain_bits);
  os.station_attrib_bits_save(ADDR_NVM_IGNRAIN, os.ignrain_bits);

  server_change_stations_attrib(p, 'n', os.masop2_bits);
  os.station_attrib_bits_save(ADDR_NVM_MAS_OP_2, os.masop2_bits);

  server_change_stations_attrib(p, 'd', os.stndis_bits);
  os.station_attrib_bits_save(ADDR_NVM_STNDISABLE, os.stndis_bits);

  os.update_rfstation_bits();

  server_change_stations_attrib(p, 'q', os.stnseq_bits);
  os.station_attrib_bits_save(ADDR_NVM_STNSEQ, os.stnseq_bits);

  return HTML_SUCCESS;
}

/** Parse one number from a comma separate list */
uint16_t parse_listdata(char **p) {
  char* pv;
  int i=0;
  tmp_buffer[i]=0;
  // copy to tmp_buffer until a non-number is encountered
  for(pv=(*p);pv<(*p)+10;pv++) {
    if ((*pv)=='-' || (*pv)=='+' || ((*pv)>='0'&&(*pv)<='9'))
      tmp_buffer[i++] = (*pv);
    else
      break;
  }
  tmp_buffer[i]=0;
  *p = pv+1;
  return (uint16_t)atol(tmp_buffer);
}

/**
  Change run-once program
  Command: /cr?pw=xxx&t=[x,x,x...]

  pw: password
  t:  station water time
*/
byte server_change_runonce(char *p) {
  // decode url first
  urlDecode(p);
  // search for the start of v=[
  char *pv;
  boolean found=false;
  for(pv=p;(*pv)!=0 && pv<p+100;pv++) {
    if(strncmp(pv, "t=[", 3)==0) {
      found=true;
      break;
    }
  }
  if(!found)  return HTML_DATA_MISSING;
  pv+=3;

  // reset all stations and prepare to run one-time program
  reset_all_stations_immediate();

  byte sid, bid, s;
  uint16_t dur;
  boolean match_found = false;
  for(sid=0;sid<os.nstations;sid++) {
    dur=parse_listdata(&pv);
    bid=sid>>3;
    s=sid&0x07;
    // if non-zero duration is given
    // and if the station has not been disabled
    if (dur>0 && !(os.stndis_bits[bid]&(1<<s))) {
      pd.scheduled_stop_time[sid] = water_time_resolve(dur);
      pd.scheduled_program_index[sid] = 254;
      match_found = true;
    }
  }
  if(match_found) {
    schedule_all_stations(os.now_tz());
    return HTML_SUCCESS;
  }

  return HTML_DATA_MISSING;
}


/**
  Delete a program
  Command: /dp?pw=xxx&pid=xxx

  pw: password
  pid:program index (-1 will delete all programs)
*/
byte server_delete_program(char *p) {
  if (!findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("pid"), true))
    return HTML_DATA_MISSING;

  int pid=atoi(tmp_buffer);
  if (pid == -1) {
    pd.eraseall();
  } else if (pid < pd.nprograms) {
    pd.del(pid);
  } else {
    return HTML_DATA_OUTOFBOUND;
  }

  return HTML_SUCCESS;
}

/**
  Move up a program
  Command: /up?pw=xxx&pid=xxx

  pw: password
  pid:program index
*/
byte server_moveup_program(char *p) {
  if (!findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("pid"), true)) {
    return HTML_DATA_MISSING;
  }
  int pid=atoi(tmp_buffer);
  if (!(pid>=1 && pid< pd.nprograms)) return HTML_DATA_OUTOFBOUND;

  pd.moveup(pid);

  return HTML_SUCCESS;
}

/**
  Change a program
  Command: /cp?pw=xxx&pid=x&v=[flag,days0,days1,[start0,start1,start2,start3],[dur0,dur1,dur2..]]&name=x

  pw:    password
  pid:   program index
  flag:  program flag
  start?:up to 4 start times
  dur?:  station water time
  name:  program name
*/
prog_char _str_program[] PROGMEM = "Program ";
byte server_change_program(char *p) {
  byte i;

  ProgramStruct prog;

  // parse program index
  if (!findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("pid"), true)) {
    return HTML_DATA_MISSING;
  }
  int pid=atoi(tmp_buffer);
  if (!(pid>=-1 && pid< pd.nprograms)) return HTML_DATA_OUTOFBOUND;

  // parse program name
  if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("name"), true)) {
    urlDecode(tmp_buffer);
    strncpy(prog.name, tmp_buffer, PROGRAM_NAME_SIZE);
  } else {
    strcpy_P(prog.name, _str_program);
    itoa((pid==-1)? (pd.nprograms+1): (pid+1), prog.name+8, 10);
  }

  // do a full string decoding
  urlDecode(p);

  // parse ad-hoc v=[...
  // search for the start of v=[
  char *pv;
  boolean found=false;

  for(pv=p;(*pv)!=0 && pv<p+100;pv++) {
    if(strncmp(pv, "v=[", 3)==0) {
      found=true;
      break;
    }
  }

  if(!found)  return HTML_DATA_MISSING;
  pv+=3;
  // parse headers
  *(char*)(&prog) = parse_listdata(&pv);
  prog.days[0]= parse_listdata(&pv);
  prog.days[1]= parse_listdata(&pv);
  // parse start times
  pv++; // this should be a '['
  for (i=0;i<MAX_NUM_STARTTIMES;i++) {
    prog.starttimes[i] = parse_listdata(&pv);
  }
  pv++; // this should be a ','
  pv++; // this should be a '['
  for (i=0;i<os.nstations;i++) {
    uint16_t pre = parse_listdata(&pv);
    prog.durations[i] = water_time_encode(pre);
  }
  pv++; // this should be a ']'
  pv++; // this should be a ']'
  // parse program name

  // i should be equal to os.nstations at this point
  for(;i<MAX_NUM_STATIONS;i++) {
    prog.durations[i] = 0;     // clear unused field
  }

  // process interval day remainder (relative-> absolute)
  if (prog.type == PROGRAM_TYPE_INTERVAL && prog.days[1] > 1) {
    pd.drem_to_absolute(prog.days);
  }

  if (pid==-1) {
    if(!pd.add(&prog))
      return HTML_DATA_OUTOFBOUND;
  } else {
    if(!pd.modify(pid, &prog))
      return HTML_DATA_OUTOFBOUND;
  }
  return HTML_SUCCESS;
}

void server_json_options_main() {
  byte oid;
  for(oid=0;oid<NUM_OPTIONS;oid++) {
    int32_t v=os.options[oid].value;
    if (oid==OPTION_MASTER_OFF_ADJ || oid==OPTION_MASTER_OFF_ADJ_2) {v-=60;}
    if (oid==OPTION_STATION_DELAY_TIME) {v=water_time_decode_signed(v);}
    #if defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__)
    if (oid==OPTION_BOOST_TIME) {
      if (os.hw_type==HW_TYPE_AC) continue;
      else v<<=2;
    }
    #else
    if (oid==OPTION_BOOST_TIME) continue;
    #endif    
    if (os.options[oid].json_str==0) continue;
    if (oid==OPTION_DEVICE_ID && os.status.has_hwmac) continue; // do not send DEVICE ID if hardware MAC exists
    bfill.emit_p(PSTR("\"$F\":$D"),
                 os.options[oid].json_str, v);
    if(oid!=NUM_OPTIONS-1)
      bfill.emit_p(PSTR(","));
  }

  bfill.emit_p(PSTR(",\"dexp\":$D,\"mexp\":$D,\"hwt\":$D}"), os.detect_exp(), MAX_EXT_BOARDS, os.hw_type);
}


/** Output options */
byte server_json_options(char *p)
{
  print_json_header_with_bracket();
  server_json_options_main();
  return HTML_OK;
}

/** Output program data */
byte server_json_programs(char *p)
{
  print_json_header_with_bracket();
  bfill.emit_p(PSTR("\"nprogs\":$D,\"nboards\":$D,\"mnp\":$D,\"mnst\":$D,\"pnsize\":$D,\"pd\":["),
               pd.nprograms, os.nboards, MAX_NUMBER_PROGRAMS, MAX_NUM_STARTTIMES, PROGRAM_NAME_SIZE);
  byte pid, i;
  ProgramStruct prog;
  for(pid=0;pid<pd.nprograms;pid++) {
    pd.read(pid, &prog);
    if (prog.type == PROGRAM_TYPE_INTERVAL && prog.days[1] > 1) {
      pd.drem_to_relative(prog.days);
    }

    byte bytedata = *(char*)(&prog);
    bfill.emit_p(PSTR("[$D,$D,$D,["), bytedata, prog.days[0], prog.days[1]);
    // start times data
    for (i=0;i<(MAX_NUM_STARTTIMES-1);i++) {
      bfill.emit_p(PSTR("$D,"), prog.starttimes[i]);
    }
    bfill.emit_p(PSTR("$D],["), prog.starttimes[i]);  // this is the last element
    // station water time
    for (i=0; i<os.nstations-1; i++) {
      bfill.emit_p(PSTR("$L,"),(unsigned long)water_time_decode(prog.durations[i]));
    }
    bfill.emit_p(PSTR("$L],\""),(unsigned long)water_time_decode(prog.durations[i])); // this is the last element
    // program name
    strncpy(tmp_buffer, prog.name, PROGRAM_NAME_SIZE);
    tmp_buffer[PROGRAM_NAME_SIZE] = 0;  // make sure the string ends
    bfill.emit_p(PSTR("$S"), tmp_buffer);
    if(pid!=pd.nprograms-1) {
      bfill.emit_p(PSTR("\"],"));
    } else {
      bfill.emit_p(PSTR("\"]"));
    }
    // push out a packet if available
    // buffer size is getting small
    if (available_ether_buffer() < 250) {
      send_packet();
    }
  }
  bfill.emit_p(PSTR("]}"));
  delay(1);
  return HTML_OK;
}

/** Output script url form */
byte server_view_scripturl(char *p) {
  print_html_standard_header();
  bfill.emit_p(PSTR("<hr /><form name=of action=cu method=get><p><b>Script URL:</b><input type=text size=32 maxlength=127 value=\"$E\" name=jsp></p><p>Default is $S<br />If local on uSD card, use ./</p><p><b>Password:</b><input type=password size=32 name=pw><input type=submit></p><hr /></form><script src=https://ui.opensprinkler.com/js/hasher.js></script>"), ADDR_NVM_SCRIPTURL, DEFAULT_JAVASCRIPT_URL);
  return HTML_OK;
}

void server_json_controller_main() {
  byte bid, sid;
  ulong curr_time = os.now_tz();
  //os.nvm_string_get(ADDR_NVM_LOCATION, tmp_buffer);
  bfill.emit_p(PSTR("\"devt\":$L,\"nbrd\":$D,\"en\":$D,\"rd\":$D,\"rs\":$D,\"rdst\":$L,\"loc\":\"$E\",\"wtkey\":\"$E\",\"sunrise\":$D,\"sunset\":$D,\"eip\":$L,\"lwc\":$L,\"lswc\":$L,\"lrun\":[$D,$D,$D,$L],\"sbits\":["),
              curr_time,
              os.nboards,
              os.status.enabled,
              os.status.rain_delayed,
              os.status.rain_sensed,
              os.nvdata.rd_stop_time,
              ADDR_NVM_LOCATION,
              ADDR_NVM_WEATHER_KEY,
              os.nvdata.sunrise_time,
              os.nvdata.sunset_time,
              os.external_ip,
              os.checkwt_lasttime,
              os.checkwt_success_lasttime,
              pd.lastrun.station,
              pd.lastrun.program,
              pd.lastrun.duration,
              pd.lastrun.endtime);
  // print sbits
  for(bid=0;bid<os.nboards;bid++)
    bfill.emit_p(PSTR("$D,"), os.station_bits[bid]);
  bfill.emit_p(PSTR("0],\"ps\":["));
  // print ps
  for(sid=0;sid<os.nstations;sid++) {
    unsigned long rem = 0;
    if (pd.scheduled_program_index[sid] > 0) {
      rem = (curr_time >= pd.scheduled_start_time[sid]) ? (pd.scheduled_stop_time[sid]-curr_time) : (pd.scheduled_stop_time[sid]-pd.scheduled_start_time[sid]);
    }
    bfill.emit_p(PSTR("[$D,$L,$L]"), pd.scheduled_program_index[sid], rem, pd.scheduled_start_time[sid]);
    bfill.emit_p((sid<os.nstations-1)?PSTR(","):PSTR("]"));

    // if available ether buffer is getting small
    // send out a packet
    if(available_ether_buffer() < 80) {
      send_packet();
    }
  }

  if(read_from_file(wtopts_name, tmp_buffer)) {
    bfill.emit_p(PSTR(",\"wto\":{$S}"), tmp_buffer);
  }
  bfill.emit_p(PSTR("}"));
  delay(1);
}


/** Output controller variables in json */
byte server_json_controller(char *p)
{
  print_json_header_with_bracket();
  server_json_controller_main();
  return HTML_OK;
}

/** Output homepage */
byte server_home(char *p)
{
  print_html_standard_header();
  bfill.emit_p(PSTR("<!DOCTYPE html>\n<html>\n<head>\n$F</head>\n<body>\n<script>"), htmlMobileHeader);

  // send server variables and javascript packets
  bfill.emit_p(PSTR("var ver=$D,ipas=$D;</script>\n"),
               OS_FW_VERSION, os.options[OPTION_IGNORE_PASSWORD].value);
  bfill.emit_p(PSTR("<script src=\"$E/home.js\"></script>\n</body>\n</html>"), ADDR_NVM_SCRIPTURL);
  return HTML_OK;
}

/**
  Change controller variables
  Command: /cv?pw=xxx&rsn=x&rbt=x&en=x&rd=x

  pw:  password
  rsn: reset all stations (0 or 1)
  rbt: reboot controller (0 or 1)
  en:  enable (0 or 1)
  rd:  rain delay hours (0 turns off rain delay)
*/

byte server_change_values(char *p)
{
  if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("rsn"), true)) {
    reset_all_stations();
  }

#define TIME_REBOOT_DELAY  20
  if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("rbt"), true) && atoi(tmp_buffer) > 0) {
    print_html_standard_header();
    //bfill.emit_p(PSTR("Rebooting..."));
    send_packet(true);
    os.reboot_dev();
  }

  if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("en"), true)) {
    if (tmp_buffer[0]=='1' && !os.status.enabled)  os.enable();
    else if (tmp_buffer[0]=='0' &&  os.status.enabled)  os.disable();
  }

  if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("rd"), true)) {
    int rd = atoi(tmp_buffer);
    if (rd>0) {
      os.nvdata.rd_stop_time = os.now_tz() + (unsigned long) rd * 3600;
      os.raindelay_start();
    } else if (rd==0){
      os.raindelay_stop();
    } else  return HTML_DATA_OUTOFBOUND;
  }

  return HTML_SUCCESS;
}

// remove spaces from a string
void string_remove_space(char *src) {
	char *dst = src;
	while(1) {
		if (*src != ' ') {
			*dst++ = *src;
		}
		if (*src == 0) break;
		src++;
	}
}

/**
  Change script url
  Command: /cu?pw=xxx&jsp=x

  pw:  password
  jsp: Javascript path
*/
byte server_change_scripturl(char *p)
{
#if defined(DEMO)
  return HTML_REDIRECT_HOME;
#endif
  if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("jsp"), true)) {
    urlDecode(tmp_buffer);
    tmp_buffer[MAX_SCRIPTURL]=0;  // make sure we don't exceed the maximum size
    // trim unwanted space characters
    string_remove_space(tmp_buffer);
    //os.nvm_string_set(ADDR_NVM_SCRIPTURL, tmp_buffer);
    nvm_write_block(tmp_buffer, (void *)ADDR_NVM_SCRIPTURL, strlen(tmp_buffer)+1);
  }
  return HTML_REDIRECT_HOME;
}

/**
  Change options
  Command: /co?pw=xxx&o?=x&loc=x&wtkey=x&ttt=x

  pw:  password
  o?:  option name (? is option index)
  loc: location
  wtkey: weather underground api key
  ttt: manual time (applicable only if ntp=0)
*/
byte server_change_options(char *p)
{
  // temporarily save some old options values
	bool time_change = false;
	bool weather_change = false;
	bool network_change = false;

  // !!! p and bfill share the same buffer, so don't write
  // to bfill before you are done analyzing the buffer !!!
  // process option values
  byte err = 0;
  byte prev_value;
  for (byte oid=0; oid<NUM_OPTIONS; oid++) {
    //if ((os.options[oid].flag&OPFLAG_WEB_EDIT)==0) continue;

    // skip options that cannot be set through web UI
    if (oid==OPTION_RESET || oid==OPTION_DEVICE_ENABLE ||
        oid==OPTION_FW_VERSION || oid==OPTION_HW_VERSION ||
        oid==OPTION_FW_MINOR || oid==OPTION_SEQUENTIAL_RETIRED)
      continue;
    prev_value = os.options[oid].value;
    if (os.options[oid].max==1)  os.options[oid].value = 0;  // set a bool variable to 0 first
    char tbuf2[5] = {'o', 0, 0, 0, 0};
    itoa(oid, tbuf2+1, 10);
    if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, tbuf2)) {
      if (os.options[oid].max==1) {
        os.options[oid].value = 1;  // if the bool variable is detected, set to 1
      } else {
		    int32_t v = atol(tmp_buffer);
		    if (oid==OPTION_MASTER_OFF_ADJ || oid==OPTION_MASTER_OFF_ADJ_2) {v+=60;} // master off time
		    if (oid==OPTION_STATION_DELAY_TIME) {
		      v=water_time_encode_signed((int16_t)v);
		    } // encode station delay time
		    if (v>=0 && v<=os.options[oid].max) {
		      os.options[oid].value = v;
		    } else {
		      err = 1;
		    }
		  }
    }
    if (os.options[oid].value != prev_value) {	// if value has changed
    	if (oid==OPTION_TIMEZONE || oid==OPTION_USE_NTP)    time_change = true;
    	if (oid>=OPTION_NTP_IP1 && oid<=OPTION_NTP_IP4)     time_change = true;
    	if (oid>=OPTION_USE_DHCP && oid<=OPTION_HTTPPORT_1) network_change = true;
    	if (oid==OPTION_DEVICE_ID)  network_change = true;
    	if (oid==OPTION_USE_WEATHER)       weather_change = true;
    }
  }

  if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("loc"), true)) {
    urlDecode(tmp_buffer);
    tmp_buffer[MAX_LOCATION]=0;   // make sure we don't exceed the maximum size
    if (strcmp_to_nvm(tmp_buffer, ADDR_NVM_LOCATION)) { // if location has changed
      nvm_write_block(tmp_buffer, (void*)ADDR_NVM_LOCATION, strlen(tmp_buffer)+1);
      weather_change = true;
    }
  }
  uint8_t keyfound = 0;
  if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("wtkey"), true, &keyfound)) {
    urlDecode(tmp_buffer);
    tmp_buffer[MAX_WEATHER_KEY]=0;
    if (strcmp_to_nvm(tmp_buffer, ADDR_NVM_WEATHER_KEY)) {  // if weather key has changed
      nvm_write_block(tmp_buffer, (void*)ADDR_NVM_WEATHER_KEY, strlen(tmp_buffer)+1);
      weather_change = true;
    }
  } else if (keyfound) {
    tmp_buffer[0]=0;
    nvm_write_block(tmp_buffer, (void*)ADDR_NVM_WEATHER_KEY, strlen(tmp_buffer)+1);
  }
  // if not using NTP and manually setting time
  if (!os.options[OPTION_USE_NTP].value && findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("ttt"), true)) {
    unsigned long t;
    t = atol(tmp_buffer);
    // before chaging time, reset all stations to avoid messing up with timing
    reset_all_stations_immediate();
#if defined(ARDUINO)
    setTime(t);
    if (os.status.has_rtc) RTC.set(t); // if rtc exists, update rtc
#endif
  }
  if(findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("wto"), true)) {
    urlDecode(tmp_buffer);
    tmp_buffer[TMP_BUFFER_SIZE]=0;
    // store weather key
    write_to_file(wtopts_name, tmp_buffer);
    weather_change = true;
  }
  if (err) {
    return HTML_DATA_OUTOFBOUND;
  }

  os.options_save();

  if(time_change) {
    os.ntpsync_lasttime = 0;
  }

  if(weather_change) {
    os.checkwt_lasttime = 0;  // force weather update
  }

  if(network_change) {
    // network related options have changed
    // this would require a restart to take effect
  }
  return HTML_SUCCESS;
}

/**
  Change password
  Command: /sp?pw=xxx&npw=x&cpw=x

  pw:  password
  npw: new password
  cpw: confirm new password
*/
byte server_change_password(char *p)
{
  if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("npw"), true)) {
    char tbuf2[TMP_BUFFER_SIZE];
    if (findKeyVal(p, tbuf2, TMP_BUFFER_SIZE, PSTR("cpw"), true) && strncmp(tmp_buffer, tbuf2, MAX_USER_PASSWORD) == 0) {
      #if defined(DEMO)
        return HTML_SUCCESS;
      #endif
      urlDecode(tmp_buffer);
      tmp_buffer[MAX_USER_PASSWORD]=0;  // make sure we don't exceed the maximum size
      byte size = strlen(tmp_buffer)+1;
      if(size >= MAX_USER_PASSWORD) size = MAX_USER_PASSWORD;
      nvm_write_block(tmp_buffer, (void*)ADDR_NVM_PASSWORD, size);
      return HTML_SUCCESS;
    } else {
      return HTML_MISMATCH;
    }
  }
  return HTML_DATA_MISSING;
}

void server_json_status_main()
{
  bfill.emit_p(PSTR("\"sn\":["));
  byte sid;

  for (sid=0;sid<os.nstations;sid++) {
    bfill.emit_p(PSTR("$D"), (os.station_bits[(sid>>3)]>>(sid&0x07))&1);
    if(sid!=os.nstations-1) bfill.emit_p(PSTR(","));
  }
  bfill.emit_p(PSTR("],\"nstations\":$D}"), os.nstations);
}

/**
  Output station status
*/
byte server_json_status(char *p)
{
  print_json_header_with_bracket();
  server_json_status_main();
  return HTML_OK;
}

/**
  Test station (previously manual operation)
  Command: /cm?pw=xxx&sid=x&en=x&t=x

  pw: password
  sid:station name (starting from 0)
  en: enable (0 or 1)
  t:  timer (required if en=1)
*/
byte server_change_manual(char *p) {
  int sid=-1;
  if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("sid"), true)) {
    sid=atoi(tmp_buffer);
    if (sid<0 || sid>=os.nstations) return HTML_DATA_OUTOFBOUND;
  } else {
    return HTML_DATA_MISSING;
  }

  byte en=0;
  if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("en"), true)) {
    en=atoi(tmp_buffer);
  } else {
    return HTML_DATA_MISSING;
  }

  uint16_t timer=0;
  unsigned long curr_time = os.now_tz();
  if (en) { // if turning on a station, must provide timer
    if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("t"), true)) {
      timer=(uint16_t)atol(tmp_buffer);
      if (timer==0 || timer>64800) {
        return HTML_DATA_OUTOFBOUND;
      }
      // schedule manual station
      // skip if the station is:
      // - master station (because master cannot be scheduled independently
      // - currently running (cannot handle overlapping schedules of the same station)
      byte bid = sid>>3;
      byte s = sid&0x07;
      if ((os.status.mas==sid+1) || (os.status.mas2==sid+1) || (os.station_bits[bid]&(1<<s)))
        return HTML_NOT_PERMITTED;

      // if the station doesn't already have a scheduled stop time
      if (!pd.scheduled_stop_time[sid]) {
        // initialize schedule data by storing water time temporarily in stop_time
        // water time is scaled by watering percentage
        pd.scheduled_stop_time[sid] = timer;
        pd.scheduled_program_index[sid] = 99;   // testing stations are assigned program index 99
        schedule_all_stations(curr_time);
      } else {
        return HTML_NOT_PERMITTED;
      }
    } else {
      return HTML_DATA_MISSING;
    }
  } else {  // turn off station
    turn_off_station(sid, curr_time);
  }
  return HTML_SUCCESS;
}

/**
  Get log data
  Command: /jl?start=x&end=x&hist=x&type=x

  hist:  history (past n days)
         when hist is speceified, the start
         and end parameters below will be ignored
  start: start time (epoch time)
  end:   end time (epoch time)
  type:  type of log records (optional)
         rs, rd, wl
         if unspecified, output all records
*/
byte server_json_log(char *p) {

#if defined(ARDUINO)
  // if no sd card, return false
  if (!os.status.has_sd)  return HTML_PAGE_NOT_FOUND;
#endif

  unsigned int start, end;

  // past n day history
  if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("hist"), true)) {
    int hist = atoi(tmp_buffer);
    if (hist< 0 || hist > 365) return HTML_DATA_OUTOFBOUND;
    end = os.now_tz() / 86400L;
    start = end - hist;
  }
  else
  {
    if (!findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("start"), true))
      return HTML_DATA_MISSING;
    start = atol(tmp_buffer) / 86400L;

    if (!findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("end"), true))
      return HTML_DATA_MISSING;
    end = atol(tmp_buffer) / 86400L;

    // start must be prior to end, and can't retrieve more than 365 days of data
    if ((start>end) || (end-start)>365)  return HTML_DATA_OUTOFBOUND;
  }

  // extract the type parameter
  char type[4] = {0};
  bool type_specified = false;
  if (findKeyVal(p, type, 4, PSTR("type"), true))
    type_specified = true;

  print_json_header();
  bfill.emit_p(PSTR("["));

  bool first = true;
  for(int i=start;i<=end;i++) {
    itoa(i, tmp_buffer, 10);
    make_logfile_name(tmp_buffer);

#if defined(ARDUINO)  // prepare to open log file for Arduino
    if (!sd.exists(tmp_buffer)) continue;
    SdFile file;
    file.open(tmp_buffer, O_READ);
#else // prepare to open log file for RPI/BBB
    FILE *file;
    file = fopen(tmp_buffer, "rb");
    if(!file) continue;
#endif // prepare to open log file

    int res;
    while(true) {
    #if defined(ARDUINO)
      res = file.fgets(tmp_buffer, TMP_BUFFER_SIZE);
      if (res <= 0) {
        file.close();
        break;
      }
    #else
      if(fgets(tmp_buffer, TMP_BUFFER_SIZE, file)) {
        res = strlen(tmp_buffer);
      } else {
        res = 0;
      }
      if (res <= 0) {
        fclose(file);
        break;
      }
    #endif

      // remove the \n character
      if (tmp_buffer[res-1] == '\n')  tmp_buffer[res-1] = 0;

      // check record type
      // special records are all in the form of [0,"xx",...]
      // where xx is the type name
      if (type_specified && strncmp(type, tmp_buffer+4, 2))
        continue;
      // if type is not specified, output everything except "wl" records
      if (!type_specified && !strncmp("wl", tmp_buffer+4, 2))
        continue;
      // if this is the first record, do not print comma
      if (first)  { bfill.emit_p(PSTR("$S"), tmp_buffer); first=false;}
      else {  bfill.emit_p(PSTR(",$S"), tmp_buffer); }
      //count ++;
      // if the available ether buffer size is getting small
      // push out a packet
      if (available_ether_buffer() < 80) {
        send_packet();
        //count = 0;
      }
    }
  }

  bfill.emit_p(PSTR("]"));
  delay(1);
  return HTML_OK;
}
/**
  Delete log
  Command: /dl?pw=xxx&day=xxx
           /dl?pw=xxx&day=all

  pw: password
  day:day (epoch time / 86400)
  if day=all: delete all log files)
*/
byte server_delete_log(char *p) {

  if (!findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("day"), true))
    return HTML_DATA_MISSING;

  delete_log(tmp_buffer);

  return HTML_SUCCESS;
}

struct URLStruct{
  PGM_P PROGMEM url;
  byte (*handler)(char*);
};


// Server function urls
// !!!Important!!!: to save space, each url must be two characters long
prog_char _url_cv [] PROGMEM = "cv";
prog_char _url_jc [] PROGMEM = "jc";

prog_char _url_dp [] PROGMEM = "dp";
prog_char _url_cp [] PROGMEM = "cp";
prog_char _url_cr [] PROGMEM = "cr";
prog_char _url_up [] PROGMEM = "up";
prog_char _url_jp [] PROGMEM = "jp";

prog_char _url_co [] PROGMEM = "co";
prog_char _url_jo [] PROGMEM = "jo";
prog_char _url_sp [] PROGMEM = "sp";

prog_char _url_js [] PROGMEM = "js";
prog_char _url_cm [] PROGMEM = "cm";

prog_char _url_cs [] PROGMEM = "cs";
prog_char _url_jn [] PROGMEM = "jn";

prog_char _url_jl [] PROGMEM = "jl";
prog_char _url_dl [] PROGMEM = "dl";

prog_char _url_su [] PROGMEM = "su";
prog_char _url_cu [] PROGMEM = "cu";


// Server function handlers
URLStruct urls[] = {
  {_url_cv,server_change_values},
  {_url_jc,server_json_controller},

  {_url_dp,server_delete_program},
  {_url_cp,server_change_program},
  {_url_cr,server_change_runonce},
  {_url_up,server_moveup_program},
  {_url_jp,server_json_programs},


  {_url_co,server_change_options},
  {_url_jo,server_json_options},
  {_url_sp,server_change_password},

  {_url_js,server_json_status},
  {_url_cm,server_change_manual},

  {_url_cs,server_change_stations},
  {_url_jn,server_json_stations},

  {_url_jl,server_json_log},
  {_url_dl,server_delete_log},

  {_url_su,server_view_scripturl},
  {_url_cu,server_change_scripturl},

};

// analyze the current url
void analyze_get_url(char *p)
{
#if defined(ARDUINO)
  ether.httpServerReplyAck();
#endif
  rewind_ether_buffer();

  // the tcp packet usually starts with 'GET /' -> 5 chars
  char *str = p+5;

  if(str[0]==' ') {
    server_home(str);  // home page handler
    send_packet(true);
  } else {
    // server funtion handlers
    byte i;
    // scan to see if there is a / in the name
    char *ss = str;
    while(*ss &&  *ss!=' ' && *ss!='\n' && *ss != '?' && *ss != '/') {
      ss++;
    }
    // if a '/' is encountered this is requesting a file
    i = 0;
    if (*ss == '/') {
      i = sizeof(urls)/sizeof(URLStruct);
    }
    for(;i<sizeof(urls)/sizeof(URLStruct);i++) {
      if(pgm_read_byte(urls[i].url)==str[0]
       &&pgm_read_byte(urls[i].url+1)==str[1]) {

        // check password
        byte ret = HTML_UNAUTHORIZED;

        if (str[0]=='s' && str[1]=='u') { // for /su do not require password
          str+=3;
          ret = (urls[i].handler)(str);
        } else if (str[0]=='j' && str[1]=='o')  { // for /jo page we output fwv if password fails
          str+=3;
          if(check_password(str)==false) {
            print_json_header_with_bracket();
            bfill.emit_p(PSTR("\"$F\":$D}"),
                   os.options[0].json_str, os.options[0].value);
            ret = HTML_OK;
          } else {
            ret = (urls[i].handler)(str);
          }
        } else {
          // first check password
          str+=3;
          if(check_password(str)==false) {
            ret = HTML_UNAUTHORIZED;
          } else {
            ret = (urls[i].handler)(str);
          }
        }
        switch(ret) {
        case HTML_OK:
          break;
        case HTML_REDIRECT_HOME:
          print_html_standard_header();
          bfill.emit_p(PSTR("$F"), htmlReturnHome);
          break;
        default:
          print_json_header();
          bfill.emit_p(PSTR("{\"result\":$D}"), ret);
        }
        break;
      }
    }

    if(i==sizeof(urls)/sizeof(URLStruct)) {
      // no server funtion found
      print_json_header();
      bfill.emit_p(PSTR("{\"result\":$D}"), HTML_PAGE_NOT_FOUND);
    }
    send_packet(true);
  }
  //delay(50); // add a bit of delay here
}


#if defined(ARDUINO)
// NTP sync
unsigned long getNtpTime()
{
  byte ntpip[4] = {
    os.options[OPTION_NTP_IP1].value,
    os.options[OPTION_NTP_IP2].value,
    os.options[OPTION_NTP_IP3].value,
    os.options[OPTION_NTP_IP4].value};
  uint32_t time;
  byte tick=0;
  unsigned long expire;
  do {
    ether.ntpRequest(ntpip, ++ntpclientportL);
    expire = millis() + 1000; // wait for at most 1 second
    do {
      word len = ether.packetReceive();
      ether.packetLoop(len);
      if(len > 0 && ether.ntpProcessAnswer(&time, ntpclientportL)) {
        if ((time & 0x80000000UL) ==0){
          time+=2085978496;
        }else{
          time-=2208988800UL;
        }
        return time;
      }
    } while(millis() < expire);
    tick ++;
  } while(tick<20);
  return 0;
}
#endif
