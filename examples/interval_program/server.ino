// Example code for OpenSprinkler Generation 2

/* Server functions
   Creative Commons Attribution-ShareAlike 3.0 license
   Sep 2014 @ Rayshobby.net
*/

#include <OpenSprinklerGen2.h>

// External variables defined in main ion file
extern BufferFiller bfill;
extern char tmp_buffer[];
extern OpenSprinkler os;
extern ProgramData pd;
extern SdFat sd;

static uint8_t ntpclientportL = 123; // Default NTP client port

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
    "HTTP/1.0 200 OK\r\n"
;

static prog_uchar htmlCacheCtrl[] PROGMEM =
    "Cache-Control: max-age=604800, public\r\n"
;

static prog_uchar htmlNoCache[] PROGMEM = 
    "Pragma: no-cache\r\n"
;   
    
static prog_uchar htmlContentHTML[] PROGMEM =
    "Content-Type: text/html\r\n"
;

static prog_uchar htmlAccessControl[] PROGMEM = 
    "Access-Control-Allow-Origin: *\r\n"
;

static prog_uchar htmlContentCgz[] PROGMEM = 
    "Content-Type: text/css;charset=utf-8\r\n"
    "Content-Encoding: gzip\r\n"
;

static prog_uchar htmlContentJgz[] PROGMEM = 
    "Content-Type: text/javascript;charset=utf-8\r\n"
    "Content-Encoding: gzip\r\n"
;
    
static prog_uchar htmlContentJSON[] PROGMEM =
    "Content-Type: application/json\r\n"
    "Connnection: close\r\n"
;

static prog_uchar htmlMobileHeader[] PROGMEM =
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1.0,minimum-scale=1.0,user-scalable=no\">\r\n"
;

static prog_uchar htmlReturnHome[] PROGMEM = 
  "<script>window.location=\"/\";</script>\n"
;

void print_html_standard_header() {
  bfill.emit_p(PSTR("$F$F$F$F\r\n"), html200OK, htmlContentHTML, htmlNoCache, htmlAccessControl);
}

void print_json_header() {
  bfill.emit_p(PSTR("$F$F$F$F\r\n"), html200OK, htmlContentJSON, htmlAccessControl, htmlNoCache);
}

void print_json_header_with_bracket() {
  bfill.emit_p(PSTR("$F$F$F$F\r\n{"), html200OK, htmlContentJSON, htmlAccessControl, htmlNoCache);
}

/**
  Check and verify password
*/
boolean check_password(char *p)
{
  if (os.options[OPTION_IGNORE_PASSWORD].value)  return true;
  if (ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("pw"), true)) {
    ether.urlDecode(tmp_buffer);
    if (os.password_verify(tmp_buffer))
      return true;
  }
  return false;
}

void server_json_stations_attrib(const char* name, const byte* attrib)
{
  bfill.emit_p(PSTR("],\"$F\":["), name);
  for(byte i=0;i<os.nboards;i++) {
    bfill.emit_p(PSTR("$D"), attrib[i]);
    if(i!=os.nboards-1)
      bfill.emit_p(PSTR(","));
  }
}

void server_json_stations_main()
{
  bfill.emit_p(PSTR("\"snames\":["));
  byte sid;
  for(sid=0;sid<os.nstations;sid++) {
    os.get_station_name(sid, tmp_buffer);
    bfill.emit_p(PSTR("\"$S\""), tmp_buffer);
    if(sid!=os.nstations-1)
      bfill.emit_p(PSTR(","));
  }
  server_json_stations_attrib(PSTR("masop"), os.masop_bits);
  server_json_stations_attrib(PSTR("ignore_rain"), os.ignrain_bits);
  server_json_stations_attrib(PSTR("act_relay"), os.actrelay_bits);
  server_json_stations_attrib(PSTR("stn_dis"), os.stndis_bits);
  server_json_stations_attrib(PSTR("rfstn"), os.rfstn_bits);
  server_json_stations_attrib(PSTR("stn_seq"), os.stnseq_bits);
  bfill.emit_p(PSTR("],\"maxlen\":$D}"), STATION_NAME_SIZE);
}

/**
  Output station names and attributes
*/
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
    if(ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, tbuf2)) {
      attrib[bid] = atoi(tmp_buffer);
    }
  }
}

/**
  Change Station Name and Attributes
  Command: /cs?pw=xxx&s?=x&m?=x&i?=x&a?=x&d?=x
  
  pw: password
  s?: station name (? is station index, starting from 0)
  m?: master operation bit field (? is board index, starting from 0)
  i?: ignore rain bit field
  a?: activate relay bit field
  d?: disable sation bit field
*/
byte server_change_stations(char *p)
{
  byte sid;
  char tbuf2[4] = {'s', 0, 0, 0};
  // process station names
  for(sid=0;sid<os.nstations;sid++) {
    itoa(sid, tbuf2+1, 10);
    if(ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, tbuf2)) {
      ether.urlDecode(tmp_buffer);
      os.set_station_name(sid, tmp_buffer);
    }
  }

  server_change_stations_attrib(p, 'm', os.masop_bits);
  os.station_attrib_bits_save(ADDR_EEPROM_MAS_OP, os.masop_bits);

  server_change_stations_attrib(p, 'i', os.ignrain_bits);    
  os.station_attrib_bits_save(ADDR_EEPROM_IGNRAIN, os.ignrain_bits);
  
  server_change_stations_attrib(p, 'a', os.actrelay_bits);
  os.station_attrib_bits_save(ADDR_EEPROM_ACTRELAY, os.actrelay_bits);

  server_change_stations_attrib(p, 'd', os.stndis_bits);
  os.station_attrib_bits_save(ADDR_EEPROM_STNDISABLE, os.stndis_bits); 
  
  os.update_rfstation_bits();
  
  server_change_stations_attrib(p, 'q', os.stnseq_bits);
  os.station_attrib_bits_save(ADDR_EEPROM_STNSEQ, os.stnseq_bits); 
  
  return HTML_SUCCESS;
}

/**
  Output script url form
*/
byte server_view_scripturl(char *p) {
  print_html_standard_header();
  bfill.emit_p(PSTR("<hr /><form name=of action=cu method=get><p><b>Script URL:</b><input type=text size=32 maxlength=127 value=\"$E\" name=jsp></p><p>Default is $S<br />If local on uSD card, use ./</p><p><b>Password:</b><input type=password size=32 name=pw><input type=submit></p><hr /></form>"), ADDR_EEPROM_SCRIPTURL, DEFAULT_JAVASCRIPT_URL);  
  return HTML_OK;
}

void server_json_programs_main() {
  bfill.emit_p(PSTR("\"nprogs\":$D,\"nboards\":$D,\"mnp\":$D,\"mnst\":$D,\"pnsize\":$D,\"pd\":["),
               pd.nprograms, os.nboards, MAX_NUMBER_PROGRAMS, MAX_NUM_STARTTIMES, PROGRAM_NAME_SIZE);
  byte pid, bid, i;
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
    if(pid!=pd.nprograms-1)
      bfill.emit_p(PSTR("\"],"));
    else
      bfill.emit_p(PSTR("\"]"));
    if (pid%3==2) { // push out a packet every 4 programs
      ether.httpServerReply_with_flags(bfill.position(), TCP_FLAGS_ACK_V, 3);
      bfill=ether.tcpOffset();
    } 
  }
  bfill.emit_p(PSTR("]}"));   
}

/**
  Output program data
*/
byte server_json_programs(char *p) 
{
  print_json_header_with_bracket();
  server_json_programs_main();
  return HTML_OK;
}

/**
  Change run-once program
  Command: /cr?pw=xxx&t=[x,x,x...]
  
  pw: password
  t:  station water time
*/
byte server_change_runonce(char *p) {
  // decode url first
  ether.urlDecode(p);
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
      pd.scheduled_stop_time[sid] = dur;
      pd.scheduled_program_index[sid] = 254;      
      match_found = true;
    }
  }
  if(match_found) {
    schedule_all_stations(now());
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
  if (!ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("pid"), true))
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
  Parse one number from a comma separate list
*/
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
  Move up a program
  Command: /up?pw=xxx&pid=xxx
  
  pw: password
  pid:program index
*/
byte server_moveup_program(char *p) {
  if (!ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("pid"), true)) {
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
  // decode url first
  ether.urlDecode(p);
  // parse program index
  if (!ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("pid"), true)) {
    return HTML_DATA_MISSING;
  }
  int pid=atoi(tmp_buffer);
  if (!(pid>=-1 && pid< pd.nprograms)) return HTML_DATA_OUTOFBOUND;
  
  // parse program data
  ProgramStruct prog;
    
  // search for the start of v=[
  char *pv;
  boolean found=false;

  for(pv=p;(*pv)!=0 && pv<p+100;pv++) {
    if(strncmp(pv, "v=[", 3)==0) {
      found=true;
      break;
    }
  }
  byte i;
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

  if (ether.findKeyVal(pv, tmp_buffer, TMP_BUFFER_SIZE, PSTR("name"), true)) {
    ether.urlDecode(tmp_buffer);
    strncpy(prog.name, tmp_buffer, PROGRAM_NAME_SIZE);
  } else {
    strcpy_P(prog.name, _str_program);
    //strcpy(prog.name, "Program ");
    itoa((pid==-1)? (pd.nprograms+1): (pid+1), prog.name+8, 10); 
  }

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

byte server_json_controller(char *p)
{
  print_json_header_with_bracket();
  server_json_controller_main();
  return HTML_OK;
}

void server_json_controller_main()
{
  byte bid, sid;
  unsigned long curr_time = now();  
  //os.eeprom_string_get(ADDR_EEPROM_LOCATION, tmp_buffer);
  bfill.emit_p(PSTR("\"devt\":$L,\"nbrd\":$D,\"en\":$D,\"rd\":$D,\"rs\":$D,\"rdst\":$L,\"loc\":\"$E\",\"wtkey\":\"$E\",\"sunrise\":$D,\"sunset\":$D,\"sbits\":["),
              curr_time,
              os.nboards,
              os.status.enabled,
              os.status.rain_delayed,
              os.status.rain_sensed,
              os.nvdata.rd_stop_time,
              ADDR_EEPROM_LOCATION,
              ADDR_EEPROM_WEATHER_KEY,
              os.nvdata.sunrise_time,
              os.nvdata.sunset_time);
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
    bfill.emit_p(PSTR("[$D,$L,$L],"), pd.scheduled_program_index[sid], rem, pd.scheduled_start_time[sid]);
  }
  
  bfill.emit_p(PSTR("[0,0]],\"lrun\":[$D,$D,$D,$L]}"), pd.lastrun.station, pd.lastrun.program, pd.lastrun.duration, pd.lastrun.endtime);
  
}

/**
  Output homepage
*/
byte server_home(char *p)
{
  byte bid, sid;
  unsigned long curr_time = now();

  print_html_standard_header();
  bfill.emit_p(PSTR("<!DOCTYPE html>\n<html>\n<head>\n$F</head>\n<body>\n<script>"), htmlMobileHeader);

  // send server variables and javascript packets
  bfill.emit_p(PSTR("var ver=$D,ipas=$D;</script>\n"),
               OS_FW_VERSION, os.options[OPTION_IGNORE_PASSWORD].value);
  bfill.emit_p(PSTR("<script src=\"$E/home.js\"></script>\n</body>\n</html>"), ADDR_EEPROM_SCRIPTURL);
  return HTML_OK;
}

/*void server_json_header()
{
  bfill.emit_p(PSTR("$F{"), htmlJSONHeader);
}*/

void server_json_options_main() {
  byte oid;
  for(oid=0;oid<NUM_OPTIONS;oid++) {
    int32_t v=os.options[oid].value;
    if (oid==OPTION_MASTER_OFF_ADJ) {v-=60;}
    if (oid==OPTION_RELAY_PULSE) {v*=10;}    
    if (oid==OPTION_STATION_DELAY_TIME) {v=water_time_decode_signed(v);}
    if (pgm_read_byte(os.options[oid].json_str)=='_') continue;
    bfill.emit_p(PSTR("\"$F\":$D"),
                 os.options[oid].json_str, v);
    if(oid!=NUM_OPTIONS-1)
      bfill.emit_p(PSTR(","));
  }

  bfill.emit_p(PSTR(",\"dexp\":$D,\"mexp\":$D}"), (int)os.detect_exp(), MAX_EXT_BOARDS);
}


/**
  Output options
*/
byte server_json_options(char *p)
{
  print_json_header_with_bracket();
  server_json_options_main();
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
  if (ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("rsn"), true)) {
    reset_all_stations();
  }
  
#define TIME_REBOOT_DELAY  20
  if (ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("rbt"), true) && atoi(tmp_buffer) > 0) {
    print_html_standard_header();
    //bfill.emit_p(PSTR("Rebooting..."));
    ether.httpServerReply_with_flags(bfill.position(), TCP_FLAGS_ACK_V|TCP_FLAGS_FIN_V);
    os.reboot();
  } 
  
  if (ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("en"), true)) {
    if (tmp_buffer[0]=='1' && !os.status.enabled)  os.enable();
    else if (tmp_buffer[0]=='0' &&  os.status.enabled)  os.disable();
  }   
  
  if (ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("rd"), true)) {
    int rd = atoi(tmp_buffer);
    if (rd>0) {
      os.nvdata.rd_stop_time = now() + (unsigned long) rd * 3600;    
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
  if (ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("jsp"), true)) {
    ether.urlDecode(tmp_buffer);
    tmp_buffer[MAX_SCRIPTURL]=0;  // make sure we don't exceed the maximum size
    // trim unwanted space characters
    string_remove_space(tmp_buffer);
    os.eeprom_string_set(ADDR_EEPROM_SCRIPTURL, tmp_buffer);
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
prog_char _key_loc[] PROGMEM = "loc";
prog_char _key_wtkey[] PROGMEM = "wtkey";
prog_char _key_ttt[] PROGMEM = "ttt";
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
        oid==OPTION_SEQUENTIAL_RETIRED)
      continue;
    prev_value = os.options[oid].value;
    if (os.options[oid].max==1)  os.options[oid].value = 0;  // set a bool variable to 0 first
    char tbuf2[5] = {'o', 0, 0, 0, 0};
    itoa(oid, tbuf2+1, 10);
    if (ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, tbuf2)) {
      if (os.options[oid].max==1) {
        os.options[oid].value = 1;  // if the bool variable is detected, set to 1
      } else {
		    int32_t v = atol(tmp_buffer);
		    if (oid==OPTION_MASTER_OFF_ADJ) {v+=60;} // master off time
		    if (oid==OPTION_RELAY_PULSE) {v/=10;} // relay pulse time
		    if (oid==OPTION_STATION_DELAY_TIME) {
		      DEBUG_PRINTLN(v);
		      v=water_time_encode_signed((int16_t)v);
		      DEBUG_PRINTLN(v);
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
  
  if (ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("loc"), true)) {
    ether.urlDecode(tmp_buffer);
    tmp_buffer[MAX_LOCATION]=0;   // make sure we don't exceed the maximum size
    if (strcmp_to_eeprom(tmp_buffer, ADDR_EEPROM_LOCATION)) { // if location has changed
      os.eeprom_string_set(ADDR_EEPROM_LOCATION, tmp_buffer);
      weather_change = true;
    }
  }
  uint8_t keyfound = 0;
  if (ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("wtkey"), true, &keyfound)) {
    ether.urlDecode(tmp_buffer);
    tmp_buffer[MAX_WEATHER_KEY]=0;
    if (strcmp_to_eeprom(tmp_buffer, ADDR_EEPROM_WEATHER_KEY)) {  // if weather key has changed
      os.eeprom_string_set(ADDR_EEPROM_WEATHER_KEY, tmp_buffer);
      //os.checkwt_lasttime = 0;  // immediately update weather
      weather_change = true;
    }
  } else if (keyfound) {
    tmp_buffer[0]=0;
    os.eeprom_string_set(ADDR_EEPROM_WEATHER_KEY, tmp_buffer);
  }
  // if not using NTP and manually setting time
  if (!os.options[OPTION_USE_NTP].value && ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("ttt"), true)) {
    unsigned long t;
    t = atol(tmp_buffer);
    // before chaging time, reset all stations to avoid messing up with timing
    reset_all_stations_immediate();
    setTime(t);  
    if (os.status.has_rtc) RTC.set(t); // if rtc exists, update rtc
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
  	os.network_lasttime = 0;	// force restart network
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
  if (ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("npw"), true)) {
    char tbuf2[TMP_BUFFER_SIZE];
    if (ether.findKeyVal(p, tbuf2, TMP_BUFFER_SIZE, PSTR("cpw"), true) && strncmp(tmp_buffer, tbuf2, 16) == 0) {
      //os.password_set(tmp_buffer);
      ether.urlDecode(tmp_buffer);
      tmp_buffer[MAX_USER_PASSWORD]=0;  // make sure we don't exceed the maximum size
      os.eeprom_string_set(ADDR_EEPROM_PASSWORD, tmp_buffer);
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
  if (ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("sid"), true)) {
    sid=atoi(tmp_buffer);
    if (sid<0 || sid>=os.nstations) return HTML_DATA_OUTOFBOUND;  
  } else {
    return HTML_DATA_MISSING;  
  }
  
  byte en=0;
  if (ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("en"), true)) {
    en=atoi(tmp_buffer);
  } else {
    return HTML_DATA_MISSING;
  }
  
  uint16_t timer=0;
  unsigned long curr_time = now();
  if (en) { // if turning on a station, must provide timer
    if (ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("t"), true)) {
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
      if ((os.status.mas==sid+1) || (os.station_bits[bid]&(1<<s)))
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
    turn_off_station(sid, os.status.mas, curr_time);
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

  // if no sd card, return false
  if (!os.status.has_sd)  return HTML_PAGE_NOT_FOUND;
  
  unsigned int start, end;

  // past n day history
  if (ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("hist"), true)) {
    int hist = atoi(tmp_buffer);
    if (hist< 0 || hist > 365) return HTML_DATA_OUTOFBOUND;
    end = now() / 86400L;
    start = end - hist;
  }
  else
  {
    if (!ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("start"), true))
      return HTML_DATA_MISSING;
    start = atol(tmp_buffer) / 86400L;
    
    if (!ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("end"), true))
      return HTML_DATA_MISSING;
    end = atol(tmp_buffer) / 86400L;
    
    // start must be prior to end, and can't retrieve more than 365 days of data
    if ((start>end) || (end-start)>365)  return HTML_DATA_OUTOFBOUND;
  }

  // extract the type parameter
  char type[4] = {0};
  bool type_specified = false;
  if (ether.findKeyVal(p, type, 4, PSTR("type"), true))
    type_specified = true;
  
  print_json_header();
  bfill.emit_p(PSTR("["));
 
  char *s;
  int res, count = 0;
  bool first = true;
  for(int i=start;i<=end;i++) {
    itoa(i, tmp_buffer, 10);
    make_logfile_name(tmp_buffer);

    if (!sd.exists(tmp_buffer)) continue;
    
    SdFile file;
    int res;
    file.open(tmp_buffer, O_READ);
    while(true) {
      res = file.fgets(tmp_buffer, TMP_BUFFER_SIZE-1);
      if (res <= 0)
      {
        file.close();
        break;
      }
      
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
      count ++;
      if (count % 25 == 0) {  // push out a packet every 25 records
        ether.httpServerReply_with_flags(bfill.position(), TCP_FLAGS_ACK_V, 3);
        bfill=ether.tcpOffset();
        count = 0;
      }
    }
  }

  bfill.emit_p(PSTR("]"));   
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

  if (!ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, PSTR("day"), true))
    return HTML_DATA_MISSING;
  
  delete_log(tmp_buffer);
  
  return HTML_SUCCESS;
}

struct URLStruct{
  PGM_P PROGMEM url;
  boolean (*handler)(char*);
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
  ether.httpServerReplyAck();
  bfill = ether.tcpOffset();

  // the tcp packet usually starts with 'GET /' -> 5 chars    
  char *str = p+5;

  if(str[0]==' ') {
    server_home(str);  // home page handler
    ether.httpServerReply_with_flags(bfill.position(), TCP_FLAGS_ACK_V|TCP_FLAGS_FIN_V);
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
    
    if((i==sizeof(urls)/sizeof(URLStruct)) && os.status.has_sd) {
      // no server funtion found, file handler
      byte k=0;  
      while (str[k]!=' ' && k<32) {tmp_buffer[k]=str[k];k++;}//search the end, indicated by space
      tmp_buffer[k]=0;
      // change dir to root
      sd.chdir("/");
      if (streamfile ((char *)tmp_buffer)==0) {
        // file not found
        print_json_header();
        bfill.emit_p(PSTR("{\"result\":$D}"), HTML_PAGE_NOT_FOUND);
        ether.httpServerReply_with_flags(bfill.position(), TCP_FLAGS_ACK_V|TCP_FLAGS_FIN_V);
      }
    } else {
      ether.httpServerReply_with_flags(bfill.position(), TCP_FLAGS_ACK_V|TCP_FLAGS_FIN_V);
    }
  }
  //delay(50); // add a bit of delay here
}


byte streamfile (char* name) { //send a file to the buffer 
  unsigned long cur=0;
  if(!sd.exists(name))  {return 0;}
  SdFile myfile(name, O_READ);

  // print file header dependeing on file type
  bfill.emit_p(PSTR("$F$F"), html200OK, htmlCacheCtrl);  
  char *pext = name+strlen(name)-4;
  if(strncmp(pext, ".cgz", 4)==0) {
    bfill.emit_p(PSTR("$F"), htmlContentCgz);
  } else if(strncmp(pext, ".jgz", 4)==0) {
    bfill.emit_p(PSTR("$F"), htmlContentJgz);
  }
  bfill.emit_p(PSTR("\r\n"));
  ether.httpServerReply_with_flags(bfill.position(), TCP_FLAGS_ACK_V);
  bfill = ether.tcpOffset();
  while(myfile.available()) {
    int nbytes = myfile.read(Ethernet::buffer+54, ETHER_BUFFER_SIZE - 54);
    cur = nbytes;
    if (cur>=ETHER_BUFFER_SIZE - 54) {
      ether.httpServerReply_with_flags(cur,TCP_FLAGS_ACK_V, 3);
      cur=0;
    } else {
      break;      
    }
  }
  ether.httpServerReply_with_flags(cur, TCP_FLAGS_ACK_V+TCP_FLAGS_FIN_V, 3);

  myfile.close();
  return 1;
}

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
        return time + (int32_t)3600/4*(int32_t)(os.options[OPTION_TIMEZONE].value-48);      
      }
    } while(millis() < expire);
    tick ++;
  } while(tick<20);  
  return 0;
}
