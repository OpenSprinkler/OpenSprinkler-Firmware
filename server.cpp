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
extern ulong flow_count;

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

void schedule_all_stations(ulong curr_time);
void turn_off_station(byte sid, ulong curr_time);
void process_dynamic_events(ulong curr_time);
void check_network(time_t curr_time);
void check_weather(time_t curr_time);
void perform_ntp_sync(time_t curr_time);
void log_statistics(time_t curr_time);
void delete_log(char *name);
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

static const char html200OK[] PROGMEM =
  "HTTP/1.1 200 OK\r\n"
;

static const char htmlCacheCtrl[] PROGMEM =
  "Cache-Control: max-age=604800, public\r\n"
;

static const char htmlNoCache[] PROGMEM =
  "Cache-Control: max-age=0, no-cache, no-store, must-revalidate\r\n"
;

static const char htmlContentHTML[] PROGMEM =
  "Content-Type: text/html\r\n"
;

static const char htmlAccessControl[] PROGMEM =
  "Access-Control-Allow-Origin: *\r\n"
;

static const char htmlContentJSON[] PROGMEM =
  "Content-Type: application/json\r\n"
  "Connection: close\r\n"
;

static const char htmlMobileHeader[] PROGMEM =
  "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1.0,minimum-scale=1.0,user-scalable=no\">\r\n"
;

static const char htmlReturnHome[] PROGMEM =
  "<script>window.location=\"/\";</script>\n"
;

#if defined(ARDUINO)
void print_html_standard_header() {
  bfill.emit_p(PSTR("$F$F$F$F\r\n"), html200OK, htmlContentHTML, htmlNoCache, htmlAccessControl);
}

void print_json_header(bool bracket=true) {
  bfill.emit_p(PSTR("$F$F$F$F\r\n"), html200OK, htmlContentJSON, htmlAccessControl, htmlNoCache);
  if(bracket) bfill.emit_p(PSTR("{"));
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

/* Check available space (number of bytes) in the Ethernet buffer */
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

void print_json_header(bool bracket=true) {
  m_client->write((const uint8_t *)html200OK, strlen(html200OK));
  m_client->write((const uint8_t *)htmlContentJSON, strlen(htmlContentJSON));
  m_client->write((const uint8_t *)htmlNoCache, strlen(htmlNoCache));
  m_client->write((const uint8_t *)htmlAccessControl, strlen(htmlAccessControl));
  if(bracket) m_client->write((const uint8_t *)"\r\n{", 3);
  else m_client->write((const uint8_t *)"\r\n", 2);
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

/** Convert a single hex digit character to its integer value */
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

/** Decode a url string e.g "hello%20joe" or "hello+joe" becomes "hello joe" */
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
  if (os.options[OPTION_IGNORE_PASSWORD])  return true;
  if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("pw"), true)) {
    urlDecode(tmp_buffer);
    if (os.password_verify(tmp_buffer))
      return true;
  }
  return false;
}

void server_json_stations_attrib(const char* name, int addr)
{
  byte *attrib = (byte*)tmp_buffer;
  os.station_attrib_bits_load(addr, attrib);
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
  server_json_stations_attrib(PSTR("masop"), ADDR_NVM_MAS_OP);
  server_json_stations_attrib(PSTR("ignore_rain"), ADDR_NVM_IGNRAIN);
  server_json_stations_attrib(PSTR("masop2"), ADDR_NVM_MAS_OP_2);
  server_json_stations_attrib(PSTR("stn_dis"), ADDR_NVM_STNDISABLE);
  server_json_stations_attrib(PSTR("stn_seq"), ADDR_NVM_STNSEQ);
#if defined(ARDUINO)  // only output stn_spe if it's supported
  if (os.status.has_sd) {
    server_json_stations_attrib(PSTR("stn_spe"), ADDR_NVM_STNSPE);
  }
#else
  server_json_stations_attrib(PSTR("stn_spe"), ADDR_NVM_STNSPE);
#endif
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

/** Output stations data */
byte server_json_stations(char *p) {
  print_json_header();
  server_json_stations_main();
  return HTML_OK;
}

/** Output station special attribute */
byte server_json_station_special(char *p) {
#if defined(ARDUINO)
  // if no sd card, return false
  if (!os.status.has_sd)  return HTML_PAGE_NOT_FOUND;
#endif
  byte sid;
  byte comma=0;
  int stepsize=sizeof(StationSpecialData);
  StationSpecialData *stn = (StationSpecialData *)tmp_buffer;
  print_json_header();
  for(sid=0;sid<os.nstations;sid++) {
    if(os.station_attrib_bits_read(ADDR_NVM_STNSPE+(sid>>3))&(1<<(sid&0x07))) {
      read_from_file(stns_filename, (char*)stn, stepsize, sid*stepsize);
      if (comma) bfill.emit_p(PSTR(","));
      else {comma=1;}
      bfill.emit_p(PSTR("\"$D\":{\"st\":$D,\"sd\":\"$S\"}"), sid, stn->type, stn->data);
    }
  }
  bfill.emit_p(PSTR("}"));
  delay(1);
  return HTML_OK;
}

void server_change_stations_attrib(char *p, char header, int addr)
{
  byte attrib[MAX_EXT_BOARDS+1];
  os.station_attrib_bits_load(addr, attrib);
  char tbuf2[3] = {0, 0, 0};
  byte bid;
  tbuf2[0]=header;
  for(bid=0;bid<os.nboards;bid++) {
    itoa(bid, tbuf2+1, 10);
    if(findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, tbuf2)) {
      attrib[bid] = atoi(tmp_buffer);
    }
  }
  os.station_attrib_bits_save(addr, attrib);
}

/**
 * Change Station Name and Attributes
 * Command: /cs?pw=xxx&s?=x&m?=x&i?=x&n?=x&d?=x
 *
 * pw: password
 * s?: station name (? is station index, starting from 0)
 * m?: master operation bit field (? is board index, starting from 0)
 * i?: ignore rain bit field
 * n?: master2 operation bit field
 * d?: disable sation bit field
 * q?: station sequeitnal bit field
 * p?: station special flag bit field
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

  server_change_stations_attrib(p, 'm', ADDR_NVM_MAS_OP); // master1
  server_change_stations_attrib(p, 'i', ADDR_NVM_IGNRAIN); // ignore rain
  server_change_stations_attrib(p, 'n', ADDR_NVM_MAS_OP_2); // master2
  server_change_stations_attrib(p, 'd', ADDR_NVM_STNDISABLE); // disable
  server_change_stations_attrib(p, 'q', ADDR_NVM_STNSEQ); // sequential
#if defined(ARDUINO)  // only parse station special bits if it's supported
  if(os.status.has_sd) {
    server_change_stations_attrib(p, 'p', ADDR_NVM_STNSPE); // special
  }
#else
  server_change_stations_attrib(p, 'p', ADDR_NVM_STNSPE); // special
#endif
  /* handle special data */
  if(findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("sid"), true)) {
    sid = atoi(tmp_buffer);
    if(sid<0 || sid>os.nstations) return HTML_DATA_OUTOFBOUND;
    if(findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("st"), true) &&
       findKeyVal(p, tmp_buffer+1, TMP_BUFFER_SIZE-1, PSTR("sd"), true)) {
      int stepsize=sizeof(StationSpecialData);
      tmp_buffer[0]-='0';
      tmp_buffer[stepsize-1] = 0;

#if !defined(ARDUINO) || defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__)
      // only process GPIO and HTTP stations for OS 2.3 and OSPi
	    if(tmp_buffer[0] == STN_TYPE_GPIO) {
        // check that pin does not clash with OSPi pins
		    byte gpio = (tmp_buffer[1] - '0') * 10 + tmp_buffer[2] - '0';
		    byte activeState = tmp_buffer[3] - '0';

		    byte gpioList[] = PIN_FREE_LIST;
		    bool found = false;
		    for (int i = 0; i < sizeof(gpioList) && found == false; i++) {
			    if (gpioList[i] == gpio) found = true;
		    }
		    if (!found || activeState > 1) return HTML_DATA_OUTOFBOUND;
	    } else if (tmp_buffer[0] == STN_TYPE_HTTP) {
		    urlDecode(tmp_buffer + 1); // Unwind any url encoding of special data
		    if (strlen(tmp_buffer+1) > sizeof(HTTPStationData)) {
			    return HTML_DATA_OUTOFBOUND;
		    }
	    }
#endif

      write_to_file(stns_filename, tmp_buffer, strlen(tmp_buffer)+1, stepsize*sid, false);

    } else {

      return HTML_DATA_MISSING;

    }
  }
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

void manual_start_program(byte, byte);
/** Manual start program
 * Command: /mp?pw=xxx&pid=xxx&uwt=xxx
 *
 * pw:  password
 * pid: program index (0 refers to the first program)
 * uwt: use weather (i.e. watering percentage)
 */
byte server_manual_program(char *p) {
  if (!findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("pid"), true))
    return HTML_DATA_MISSING;

  int pid=atoi(tmp_buffer);
  if (pid < 0 || pid >= pd.nprograms) {
    return HTML_DATA_OUTOFBOUND;
  }

  byte uwt = 0;
  if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("uwt"), true)) {
    if(tmp_buffer[0]=='1') uwt = 1;
  }

  // reset all stations and prepare to run one-time program
  reset_all_stations_immediate();

  manual_start_program(pid+1, uwt);

  return HTML_SUCCESS;

}

/**
 * Change run-once program
 * Command: /cr?pw=xxx&t=[x,x,x...]
 *
 * pw: password
 * t:  station water time
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
    if (dur>0 && !(os.station_attrib_bits_read(ADDR_NVM_STNDISABLE+bid)&(1<<s))) {
      RuntimeQueueStruct *q = pd.enqueue();
      if (q) {
        q->st = 0;
        q->dur = water_time_resolve(dur);
        q->pid = 254;
        q->sid = sid;
        match_found = true;
      }
    }
  }
  if(match_found) {
    schedule_all_stations(os.now_tz());
    return HTML_SUCCESS;
  }

  return HTML_DATA_MISSING;
}


/**
 * Delete a program
 * Command: /dp?pw=xxx&pid=xxx
 *
 * pw: password
 * pid:program index (-1 will delete all programs)
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
 * Move up a program
 * Command: /up?pw=xxx&pid=xxx
 *
 * pw:  password
 * pid: program index (must be 1 or larger, because we can't move up program 0)
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
 * Change a program
 * Command: /cp?pw=xxx&pid=x&v=[flag,days0,days1,[start0,start1,start2,start3],[dur0,dur1,dur2..]]&name=x
 *
 * pw:    password
 * pid:   program index
 * flag:  program flag
 * start?:up to 4 start times
 * dur?:  station water time
 * name:  program name
*/
const char _str_program[] PROGMEM = "Program ";
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
    prog.durations[i] = pre;
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
    #if !defined(ARDUINO) // do not send the following parameters for non-Arduino platforms
    if (oid==OPTION_USE_NTP     || oid==OPTION_USE_DHCP    ||
        oid==OPTION_STATIC_IP1  || oid==OPTION_STATIC_IP2  || oid==OPTION_STATIC_IP3  || oid==OPTION_STATIC_IP4  ||
        oid==OPTION_GATEWAY_IP1 || oid==OPTION_GATEWAY_IP2 || oid==OPTION_GATEWAY_IP3 || oid==OPTION_GATEWAY_IP4)
        continue;
    #endif
    int32_t v=os.options[oid];
    if (oid==OPTION_MASTER_OFF_ADJ || oid==OPTION_MASTER_OFF_ADJ_2 ||
        oid==OPTION_MASTER_ON_ADJ  || oid==OPTION_MASTER_ON_ADJ_2 ||
        oid==OPTION_STATION_DELAY_TIME) {
      v=water_time_decode_signed(v);
    }
    #if defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__)
    if (oid==OPTION_BOOST_TIME) {
      if (os.hw_type==HW_TYPE_AC) continue;
      else v<<=2;
    }
    #else
    if (oid==OPTION_BOOST_TIME) continue;
    #endif

    if (oid==OPTION_SEQUENTIAL_RETIRED) continue;
    if (oid==OPTION_DEVICE_ID && os.status.has_hwmac) continue; // do not send DEVICE ID if hardware MAC exists
   
#if defined(ARDUINO) 
    if (os.lcd.type() == LCD_I2C) {
      // for I2C type LCD, we can't adjust contrast or backlight
      if(oid==OPTION_LCD_CONTRAST || oid==OPTION_LCD_BACKLIGHT) continue;
    }
#endif

    // each json name takes 5 characters
    strncpy_P0(tmp_buffer, op_json_names+oid*5, 5);
    bfill.emit_p(PSTR("\"$S\":$D"),
                 tmp_buffer, v);
    if(oid!=NUM_OPTIONS-1)
      bfill.emit_p(PSTR(","));
  }

  bfill.emit_p(PSTR(",\"dexp\":$D,\"mexp\":$D,\"hwt\":$D}"), os.detect_exp(), MAX_EXT_BOARDS, os.hw_type);
  delay(1);
}

/** Output Options */
byte server_json_options(char *p) {
  print_json_header();
  server_json_options_main();
  return HTML_OK;
}

void server_json_programs_main() {
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
      bfill.emit_p(PSTR("$L,"),(unsigned long)prog.durations[i]);
    }
    bfill.emit_p(PSTR("$L],\""),(unsigned long)prog.durations[i]); // this is the last element
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
}

/** Output program data */
byte server_json_programs(char *p) {
  print_json_header();
  server_json_programs_main();
  return HTML_OK;
}

/** Output script url form */
byte server_view_scripturl(char *p) {
  print_html_standard_header();
  bfill.emit_p(PSTR("<form name=of action=cu method=get><table cellspacing=\"12\"><tr><td><b>JavaScript</b>:</td><td><input type=text size=40 maxlength=40 value=\"$E\" name=jsp></td></tr><tr><td>Default:</td><td>$S</td></tr><tr><td><b>Weather</b>:</td><td><input type=text size=40 maxlength=40 value=\"$E\" name=wsp></td></tr><tr><td>Default:</td><td>$S</td></tr><tr><td><b>Password</b>:</td><td><input type=password size=32 name=pw> <input type=submit></td></tr></table></form><script src=https://ui.opensprinkler.com/js/hasher.js></script>"), ADDR_NVM_JAVASCRIPTURL, DEFAULT_JAVASCRIPT_URL, ADDR_NVM_WEATHERURL, DEFAULT_WEATHER_URL);
  return HTML_OK;
}

extern ulong flow_count;
void server_json_controller_main() {
  byte bid, sid;
  ulong curr_time = os.now_tz();
  //os.nvm_string_get(ADDR_NVM_LOCATION, tmp_buffer);
  bfill.emit_p(PSTR("\"devt\":$L,\"nbrd\":$D,\"en\":$D,\"rd\":$D,\"rs\":$D,\"rdst\":$L,"
                    "\"loc\":\"$E\",\"wtkey\":\"$E\",\"sunrise\":$D,\"sunset\":$D,\"eip\":$L,\"lwc\":$L,\"lswc\":$L,"
                    "\"lrun\":[$D,$D,$D,$L],"),
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
              os.nvdata.external_ip,
              os.checkwt_lasttime,
              os.checkwt_success_lasttime,
              pd.lastrun.station,
              pd.lastrun.program,
              pd.lastrun.duration,
              pd.lastrun.endtime);

#if defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__)
  if(os.status.has_curr_sense) {
    bfill.emit_p(PSTR("\"curr\":$D,"), os.read_current());
  }
#endif
  if(os.options[OPTION_SENSOR_TYPE]==SENSOR_TYPE_FLOW) {
    bfill.emit_p(PSTR("\"flcrt\":$L,\"flwrt\":$D,"), os.flowcount_rt, FLOWCOUNT_RT_WINDOW);
  }

  bfill.emit_p(PSTR("\"sbits\":["));
  // print sbits
  for(bid=0;bid<os.nboards;bid++)
    bfill.emit_p(PSTR("$D,"), os.station_bits[bid]);
  bfill.emit_p(PSTR("0],\"ps\":["));
  // print ps
  for(sid=0;sid<os.nstations;sid++) {
    unsigned long rem = 0;
    byte qid = pd.station_qid[sid];
    RuntimeQueueStruct *q = pd.queue + qid;
    if (qid<255) {
      rem = (curr_time >= q->st) ? (q->st+q->dur-curr_time) : q->dur;
      if(rem>65535) rem = 0;
    }
    bfill.emit_p(PSTR("[$D,$L,$L]"), (qid<255)?q->pid:0, rem, (qid<255)?q->st:0);
    bfill.emit_p((sid<os.nstations-1)?PSTR(","):PSTR("]"));

    // if available ether buffer is getting small
    // send out a packet
    if(available_ether_buffer() < 80) {
      send_packet();
    }
  }

  if(read_from_file(wtopts_filename, tmp_buffer)) {
    bfill.emit_p(PSTR(",\"wto\":{$S}"), tmp_buffer);
  }
  
#if !defined(ARDUINO) || defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__)
  if(read_from_file(ifkey_filename, tmp_buffer)) {
    bfill.emit_p(PSTR(",\"ifkey\":\"$S\""), tmp_buffer);
  }
#endif

  bfill.emit_p(PSTR("}"));
  delay(1);
}

/** Output controller variables in json */
byte server_json_controller(char *p) {
  print_json_header();
  server_json_controller_main();
  return HTML_OK;
}

/** Output homepage */
byte server_home()
{
  print_html_standard_header();
  bfill.emit_p(PSTR("<!DOCTYPE html>\n<html>\n<head>\n$F</head>\n<body>\n<script>"), htmlMobileHeader);

  // send server variables and javascript packets
  bfill.emit_p(PSTR("var ver=$D,ipas=$D;</script>\n"),
               OS_FW_VERSION, os.options[OPTION_IGNORE_PASSWORD]);
  bfill.emit_p(PSTR("<script src=\"$E/home.js\"></script>\n</body>\n</html>"), ADDR_NVM_JAVASCRIPTURL);
  return HTML_OK;
}

/**
 * Change controller variables
 * Command: /cv?pw=xxx&rsn=x&rbt=x&en=x&rd=x&sl=x
 *
 * pw:  password
 * rsn: reset all stations (0 or 1)
 * rbt: reboot controller (0 or 1)
 * en:  enable (0 or 1)
 * rd:  rain delay hours (0 turns off rain delay)
 * re:  remote extension mode
 * update: launch update script (for OSPi/OSBo/Linux only)
 */

byte server_change_values(char *p)
{
  if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("rsn"), true)) {
    reset_all_stations();
  }

#ifndef ARDUINO
    if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("update"), true) && atoi(tmp_buffer) > 0) {
        //bfill.emit_p(PSTR("Updating..."));
        os.update_dev();
    }
#endif

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

  if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("re"), true)) {
    if (tmp_buffer[0]=='1' && !os.options[OPTION_REMOTE_EXT_MODE]) {
      os.options[OPTION_REMOTE_EXT_MODE] = 1;
      os.options_save();
    } else if(tmp_buffer[0]=='0' && os.options[OPTION_REMOTE_EXT_MODE]) {
      os.options[OPTION_REMOTE_EXT_MODE] = 0;
      os.options_save();
    }
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
 * Change script url
 * Command: /cu?pw=xxx&jsp=x
 *
 * pw:  password
 * jsp: Javascript path
 */
byte server_change_scripturl(char *p)
{
#if defined(DEMO)
  return HTML_REDIRECT_HOME;
#endif
  if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("jsp"), true)) {
    urlDecode(tmp_buffer);
    tmp_buffer[MAX_JAVASCRIPTURL]=0;  // make sure we don't exceed the maximum size
    // trim unwanted space characters
    string_remove_space(tmp_buffer);
    nvm_write_block(tmp_buffer, (void *)ADDR_NVM_JAVASCRIPTURL, strlen(tmp_buffer)+1);
  }
  if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("wsp"), true)) {
    urlDecode(tmp_buffer);
    tmp_buffer[MAX_WEATHERURL]=0;
    string_remove_space(tmp_buffer);
    nvm_write_block(tmp_buffer, (void *)ADDR_NVM_WEATHERURL, strlen(tmp_buffer)+1);
  }
  return HTML_REDIRECT_HOME;
}

/**
 * Change options
 * Command: /co?pw=xxx&o?=x&loc=x&wtkey=x&ttt=x
 *
 * pw:  password
 * o?:  option name (? is option index)
 * loc: location
 * wtkey: weather underground api key
 * ttt: manual time (applicable only if ntp=0)
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
  byte max_value;
  for (byte oid=0; oid<NUM_OPTIONS; oid++) {
    //if ((os.options[oid].flag&OPFLAG_WEB_EDIT)==0) continue;

    // skip options that cannot be set through /co command
    if (oid==OPTION_RESET || oid==OPTION_DEVICE_ENABLE ||
        oid==OPTION_FW_VERSION || oid==OPTION_HW_VERSION ||
        oid==OPTION_FW_MINOR || oid==OPTION_SEQUENTIAL_RETIRED ||
        oid==OPTION_REMOTE_EXT_MODE)
      continue;
    prev_value = os.options[oid];
    max_value = pgm_read_byte(op_max+oid);
    if (max_value==1)  os.options[oid] = 0;  // set a bool variable to 0 first
    char tbuf2[5] = {'o', 0, 0, 0, 0};
    itoa(oid, tbuf2+1, 10);
    if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, tbuf2)) {
      if (max_value==1) {
        os.options[oid] = 1;  // if the bool variable is detected, set to 1
      } else {
		    int32_t v = atol(tmp_buffer);
		    if (oid==OPTION_MASTER_OFF_ADJ || oid==OPTION_MASTER_OFF_ADJ_2 ||
		        oid==OPTION_MASTER_ON_ADJ  || oid==OPTION_MASTER_ON_ADJ_2  ||
		        oid==OPTION_STATION_DELAY_TIME) {
		      v=water_time_encode_signed(v);
		    } // encode station delay time
        #if defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__)
        if(os.hw_type==HW_TYPE_DC && oid==OPTION_BOOST_TIME) {
           v>>=2;
        }
        #endif
        if (v>=0 && v<=max_value) {
          os.options[oid] = v;
		    } else {
		      err = 1;
		    }
		  }
    }
    if (os.options[oid] != prev_value) {	// if value has changed
    	if (oid==OPTION_TIMEZONE || oid==OPTION_USE_NTP)    time_change = true;
    	if (oid>=OPTION_NTP_IP1 && oid<=OPTION_NTP_IP4)     time_change = true;
    	if (oid>=OPTION_USE_DHCP && oid<=OPTION_HTTPPORT_1) network_change = true;
    	if (oid==OPTION_DEVICE_ID)  network_change = true;
      if (oid==OPTION_USE_WEATHER) weather_change = true;
    }
  }

  if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("loc"), true)) {
    urlDecode(tmp_buffer);
    tmp_buffer[MAX_LOCATION-1]=0;   // make sure we don't exceed the maximum size
    if (strcmp_to_nvm(tmp_buffer, ADDR_NVM_LOCATION)) { // if location has changed
      nvm_write_block(tmp_buffer, (void*)ADDR_NVM_LOCATION, strlen(tmp_buffer)+1);
      weather_change = true;
    }
  }
  uint8_t keyfound = 0;
  if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("wtkey"), true, &keyfound)) {
    urlDecode(tmp_buffer);
    tmp_buffer[MAX_WEATHER_KEY-1]=0;
    if (strcmp_to_nvm(tmp_buffer, ADDR_NVM_WEATHER_KEY)) {  // if weather key has changed
      nvm_write_block(tmp_buffer, (void*)ADDR_NVM_WEATHER_KEY, strlen(tmp_buffer)+1);
      weather_change = true;
    }
  } else if (keyfound) {
    tmp_buffer[0]=0;
    nvm_write_block(tmp_buffer, (void*)ADDR_NVM_WEATHER_KEY, strlen(tmp_buffer)+1);
  }
  keyfound = 0;
  if (findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("ifkey"), true, &keyfound)) {
    urlDecode(tmp_buffer);
    tmp_buffer[TMP_BUFFER_SIZE-1]=0;
    write_to_file(ifkey_filename, tmp_buffer, strlen(tmp_buffer));
  } else if (keyfound) {
    tmp_buffer[0]=0;
    write_to_file(ifkey_filename, tmp_buffer, strlen(tmp_buffer));
  }  
  // if not using NTP and manually setting time
  if (!os.options[OPTION_USE_NTP] && findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("ttt"), true)) {
    unsigned long t;
    t = atol(tmp_buffer);
    // before chaging time, reset all stations to avoid messing up with timing
    reset_all_stations_immediate();
#if defined(ARDUINO)
    setTime(t);
    RTC.set(t);
#endif
  }
  if(findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("wto"), true)) {
    urlDecode(tmp_buffer);
    tmp_buffer[TMP_BUFFER_SIZE]=0;
    // store weather key
    write_to_file(wtopts_filename, tmp_buffer, strlen(tmp_buffer));
    weather_change = true;
  }
  if (err) {
    return HTML_DATA_OUTOFBOUND;
  }

  os.options_save();

  if(time_change) {
    os.status.req_ntpsync = 1;
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
 * Change password
 * Command: /sp?pw=xxx&npw=x&cpw=x
 *
 * pw:  password
 * npw: new password
 * cpw: confirm new password
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
      tmp_buffer[MAX_USER_PASSWORD-1]=0;  // make sure we don't exceed the maximum size
      nvm_write_block(tmp_buffer, (void*)ADDR_NVM_PASSWORD, strlen(tmp_buffer)+1);
      return HTML_SUCCESS;
    } else {
      return HTML_MISMATCH;
    }
  }
  return HTML_DATA_MISSING;
}

void server_json_status_main() {
  bfill.emit_p(PSTR("\"sn\":["));
  byte sid;

  for (sid=0;sid<os.nstations;sid++) {
    bfill.emit_p(PSTR("$D"), (os.station_bits[(sid>>3)]>>(sid&0x07))&1);
    if(sid!=os.nstations-1) bfill.emit_p(PSTR(","));
  }
  bfill.emit_p(PSTR("],\"nstations\":$D}"), os.nstations);
  delay(1);
}

/** Output station status */
byte server_json_status(char *p)
{
  print_json_header();
  server_json_status_main();
  return HTML_OK;
}

/**
 * Test station (previously manual operation)
 * Command: /cm?pw=xxx&sid=x&en=x&t=x
 *
 * pw: password
 * sid:station index (starting from 0)
 * en: enable (0 or 1)
 * t:  timer (required if en=1)
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
      // skip if the station is a master station
      // (because master cannot be scheduled independently)
      byte bid = sid>>3;
      byte s = sid&0x07;
      if ((os.status.mas==sid+1) || (os.status.mas2==sid+1))
        return HTML_NOT_PERMITTED;

      RuntimeQueueStruct *q = NULL;
      byte sqi = pd.station_qid[sid];
      // check if the station already has a schedule
      if (sqi!=0xFF) {  // if we, we will overwrite the schedule
        q = pd.queue+sqi;
      } else {  // otherwise create a new queue element
        q = pd.enqueue();
      }
      // if the queue is not full
      if (q) {
        q->st = 0;
        q->dur = timer;
        q->sid = sid;
        q->pid = 99;  // testing stations are assigned program index 99
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
 * Get log data
 * Command: /jl?start=x&end=x&hist=x&type=x
 *
 * hist:  history (past n days)
 *        when hist is speceified, the start
 *        and end parameters below will be ignored
 * start: start time (epoch time)
 * end:   end time (epoch time)
 * type:  type of log records (optional)
 *        rs, rd, wl
 *        if unspecified, output all records
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

  print_json_header(false);
  bfill.emit_p(PSTR("["));

  bool comma = 0;
  for(int i=start;i<=end;i++) {
    itoa(i, tmp_buffer, 10);
    make_logfile_name(tmp_buffer);

#if defined(ARDUINO)  // prepare to open log file for Arduino
    if (!sd.exists(tmp_buffer)) continue;
    SdFile file;
    file.open(tmp_buffer, O_READ);
#else // prepare to open log file for RPI/BBB
    FILE *file;
    file = fopen(get_filename_fullpath(tmp_buffer), "rb");
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
      // check record type
      // records are all in the form of [x,"xx",...]
      // where x is program index (>0) if this is a station record
      // and "xx" is the type name if this is a special record (e.g. wl, fl, rs)

      // search string until we find the first comma
      char *ptype = tmp_buffer;
      tmp_buffer[TMP_BUFFER_SIZE-1]=0; // make sure the search will end
      while(*ptype && *ptype != ',') ptype++;
      if(*ptype != ',') continue; // didn't find comma, move on
      ptype++;  // move past comma

      if (type_specified && strncmp(type, ptype+1, 2))
        continue;
      // if type is not specified, output everything except "wl" and "fl" records
      if (!type_specified && (!strncmp("wl", ptype+1, 2) || !strncmp("fl", ptype+1, 2)))
        continue;
      // if this is the first record, do not print comma
      if (comma)  bfill.emit_p(PSTR(","));
      else {comma=1;}
      bfill.emit_p(PSTR("$S"), tmp_buffer);
      // if the available ether buffer size is getting small
      // push out a packet
      if (available_ether_buffer() < 80) {
        send_packet();
      }
    }
  }

  bfill.emit_p(PSTR("]"));
  delay(1);
  return HTML_OK;
}
/**
 * Delete log
 * Command: /dl?pw=xxx&day=xxx
 *          /dl?pw=xxx&day=all
 *
 * pw: password
 * day:day (epoch time / 86400)
 * if day=all: delete all log files)
 */
byte server_delete_log(char *p) {

  if (!findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("day"), true))
    return HTML_DATA_MISSING;

  delete_log(tmp_buffer);

  return HTML_SUCCESS;
}

/** Output all JSON data, including jc, jp, jo, js, jn */
byte server_json_all(char *p) {
  print_json_header();
  bfill.emit_p(PSTR("\"settings\":{"));
  server_json_controller_main();
  send_packet();
  bfill.emit_p(PSTR(",\"programs\":{"));
  server_json_programs_main();
  send_packet();
  bfill.emit_p(PSTR(",\"options\":{"));
  server_json_options_main();
  send_packet();
  bfill.emit_p(PSTR(",\"status\":{"));
  server_json_status_main();
  send_packet();
  bfill.emit_p(PSTR(",\"stations\":{"));
  server_json_stations_main();
  bfill.emit_p(PSTR("}"));
  delay(1);
  return HTML_OK;
}

typedef byte (*URLHandler)(char*);
/*struct URLStruct{
  PGM_P PROGMEM url;

};*/


/* Server function urls
 * To save RAM space, each GET command keyword is exactly
 * 2 characters long, with no ending 0
 * The order must exactly match the order of the
 * handler functions below
 */
const char _url_keys[] PROGMEM =
  "cv"
  "jc"
  "dp"
  "cp"
  "cr"
  "mp"
  "up"
  "jp"
  "co"
  "jo"
  "sp"
  "js"
  "cm"
  "cs"
  "jn"
  "je"
  "jl"
  "dl"
  "su"
  "cu"
  "ja";

// Server function handlers
URLHandler urls[] = {
  server_change_values,   // cv
  server_json_controller, // jc
  server_delete_program,  // dp
  server_change_program,  // cp
  server_change_runonce,  // cr
  server_manual_program,  // mp  
  server_moveup_program,  // up
  server_json_programs,   // jp
  server_change_options,  // co
  server_json_options,    // jo
  server_change_password, // sp
  server_json_status,     // js
  server_change_manual,   // cm
  server_change_stations, // cs
  server_json_stations,   // jn
  server_json_station_special,// je
  server_json_log,        // jl
  server_delete_log,      // dl
  server_view_scripturl,  // su
  server_change_scripturl,// cu
  server_json_all         // ja
};

// handle Ethernet request
void handle_web_request(char *p)
{
#if defined(ARDUINO)
  ether.httpServerReplyAck();
#endif
  rewind_ether_buffer();

  // assume this is a GET request
  // GET /xx?xxxx
  char *com = p+5;
  char *dat = com+3;

  if(com[0]==' ') {
    server_home();  // home page handler
    send_packet(true);
  } else {
    // server funtion handlers
    byte i;
    for(i=0;i<sizeof(urls)/sizeof(URLHandler);i++) {
      if(pgm_read_byte(_url_keys+2*i)==com[0]
       &&pgm_read_byte(_url_keys+2*i+1)==com[1]) {

        // check password
        byte ret = HTML_UNAUTHORIZED;

        if (com[0]=='s' && com[1]=='u') { // for /su do not require password
          ret = (urls[i])(dat);
        } else if ((com[0]=='j' && com[1]=='o') ||
                   (com[0]=='j' && com[1]=='a'))  { // for /jo and /ja we output fwv if password fails
          if(check_password(dat)==false) {
            print_json_header();
            bfill.emit_p(PSTR("\"$F\":$D}"),
                   op_json_names+0, os.options[0]);
            ret = HTML_OK;
          } else {
            ret = (urls[i])(dat);
          }
        } else {
          // first check password
          if(check_password(dat)==false) {
            ret = HTML_UNAUTHORIZED;
          } else {
            ret = (urls[i])(dat);
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
          bfill.emit_p(PSTR("\"result\":$D}"), ret);
        }
        break;
      }
    }

    if(i==sizeof(urls)/sizeof(URLHandler)) {
      // no server funtion found
      print_json_header();
      bfill.emit_p(PSTR("\"result\":$D}"), HTML_PAGE_NOT_FOUND);
    }
    send_packet(true);
  }
  //delay(50); // add a bit of delay here
}


#if defined(ARDUINO)
/** NTP sync request */
unsigned long getNtpTime()
{
  byte ntpip[4] = {
    os.options[OPTION_NTP_IP1],
    os.options[OPTION_NTP_IP2],
    os.options[OPTION_NTP_IP3],
    os.options[OPTION_NTP_IP4]};
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
