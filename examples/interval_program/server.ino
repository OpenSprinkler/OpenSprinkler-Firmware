// Example code for OpenSprinkler Generation 2

/* Server functions
   Creative Commons Attribution-ShareAlike 3.0 license
   Sep 2014 @ Rayshobby.net
*/

#include <OpenSprinklerGen2.h>

// External variables defined in main pde file
extern BufferFiller bfill;
extern char tmp_buffer[];
extern OpenSprinkler os;
extern ProgramData pd;
extern SdFat sd;
extern unsigned long last_sync_time;

static uint8_t ntpclientportL = 123; // Default NTP client port

#define HTML_OK                0x00
#define HTML_SUCCESS           0x01
#define HTML_UNAUTHORIZED      0x02
#define HTML_MISMATCH          0x03
#define HTML_DATA_MISSING      0x10
#define HTML_DATA_OUTOFBOUND   0x11
#define HTML_DATA_FORMATERROR  0x12
#define HTML_PAGE_NOT_FOUND    0x20
#define HTML_NOT_PERMITTED     0x30

static prog_uchar htmlOkHeader[] PROGMEM = 
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Pragma: no-cache\r\n"
    "Access-Control-Allow-Origin: *\r\n"
    "\r\n"
;

static prog_uchar htmlFileHeader[] PROGMEM =
    "HTTP/1.0 200 OK\r\n"
    "Cache-Control: max-age=604800, public\r\n"
    "\r\n"
;

static prog_uchar htmlJSONHeader[] PROGMEM =
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: application/json\r\n"
    "Connnection: close\r\n"
    "Access-Control-Allow-Origin: *\r\n"
    "Cache-Control: no-cache\r\n"
    "\r\n"
;

static prog_uchar htmlMobileHeader[] PROGMEM =
    "<meta name=\"viewport\" content=\"width=640\">\r\n"
;

/*static prog_uchar htmlUnauthorized[] PROGMEM = 
    "HTTP/1.0 401 Unauthorized\r\n"
    "Content-Type: text/html\r\n"
    "\r\n"
    "401 Unauthorized"
;*/

/*static prog_uchar htmlReturnHome[] PROGMEM = 
  "window.location=\"/\";</script>\n"
;*/


// check and verify password
boolean check_password(char *p)
{
  if (os.options[OPTION_IGNORE_PASSWORD].value)  return true;
  if (ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, "pw")) {
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
  server_json_stations_attrib(PSTR("stn_dis"), os.actrelay_bits);
  bfill.emit_p(PSTR("],\"maxlen\":$D}"), STATION_NAME_SIZE);
}

// printing station names in json
byte server_json_stations(char *p)
{
  server_json_header();
  server_json_stations_main();
  return HTML_OK;
}

void server_change_stations_attrib(char *p, char header, byte *attrib)
{
  char tbuf2[3] = {0, 0, 0};
  byte bid;
  // process station master operation bits
  tbuf2[0]=header;
  for(bid=0;bid<os.nboards;bid++) {
    itoa(bid, tbuf2+1, 10);
    if(ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, tbuf2)) {
      attrib[bid] = atoi(tmp_buffer);
    }
  }
}

// server function for accepting station name changes
byte server_change_stations(char *p)
{
  p+=3;
  
  // check password
  if(check_password(p)==false)  return HTML_UNAUTHORIZED;
  
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
  
  //bfill.emit_p(PSTR("$F<script>alert(\"Changes saved.\");$F"), htmlOkHeader, htmlReturnHome);
  return HTML_SUCCESS;
}


// print page to set javascript url
byte server_view_scripturl(char *p) {
  bfill.emit_p(PSTR("$F"), htmlOkHeader);
  bfill.emit_p(PSTR("<hr /><form name=of action=cu method=get><p><b>Script URL:</b> <input type=text size=32 maxlength=127 value=\"$E\" name=jsp></p><p>Factory default url is $S<br />If local on uSD card, use ./</p><p><b>Password:</b><input type=password size=20 name=pw><input type=submit></p><hr /></form>"), ADDR_EEPROM_SCRIPTURL, DEFAULT_JAVASCRIPT_URL);  
  return HTML_OK;
}

// print program data in json
void server_json_programs_main() {
  bfill.emit_p(PSTR("\"nprogs\":$D,\"nboards\":$D,\"mnp\":$D,\"mnst\":$D,\"pnsize\":$D,\"pd\":["),
               pd.nprograms, os.nboards, MAX_NUMBER_PROGRAMS, MAX_NUM_STARTTIMES, PROGRAM_NAME_SIZE);
  byte pid, bid, i;
  ProgramStruct prog;
  for(pid=0;pid<pd.nprograms;pid++) {
    pd.read(pid, &prog);
    // to fix: convert interval remainder (absolute->relative)

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
    // durations
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
    if (pid%4==3) { // push out a packet every 4 programs
      ether.httpServerReply_with_flags(bfill.position(), TCP_FLAGS_ACK_V);
      bfill=ether.tcpOffset();
    } 
  }
  bfill.emit_p(PSTR("]}"));   
}

byte server_json_programs(char *p) 
{
  server_json_header();
  server_json_programs_main();
  return HTML_OK;
}

// server function to accept run-once program
#if 0
byte server_change_runonce(char *p) {
  p+=3;
  //ether.urlDecode(p);
  
  // check password
  if(check_password(p)==false)  return HTML_UNAUTHORIZED;
  
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
  reset_all_stations();
      
  byte sid;
  uint16_t dur;
  unsigned char *addr = (unsigned char*)ADDR_EEPROM_RUNONCE;
  boolean match_found = false;
  for(sid=0;sid<os.nstations;sid++, addr+=2) {
    dur=parse_listdata(&pv);
    eeprom_write_byte(addr, (dur>>8));
    eeprom_write_byte(addr+1, (dur&0xff));
    if (dur>0) {
      pd.scheduled_stop_time[sid] = dur;
      pd.scheduled_program_index[sid] = 254;      
      match_found = true;
    }
  }
  if(match_found) {
    schedule_all_stations(now(), os.options[OPTION_SEQUENTIAL].value);
  }

  return HTML_SUCCESS;
}
#endif

/*=============================================
  Delete Program
  
  HTTP GET command format:
  /dp?pw=xxx&pid=xxx
  
  pw: password
  pid:program index (-1 will delete all programs)
  =============================================*/
byte server_delete_program(char *p) {

  p+=3;
  //ether.urlDecode(p);
  
  // check password
  if(check_password(p)==false)  return HTML_UNAUTHORIZED;
  
  if (!ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, "pid"))
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

// parse one number from a comma separate list
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
  return atoi(tmp_buffer);
}

// server function to move up program
byte server_moveup_program(char *p) {
  p+=3;
  if(check_password(p)==false)  return HTML_UNAUTHORIZED;
  
  if (!ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, "pid")) {
    return HTML_DATA_MISSING;  
  }
  int pid=atoi(tmp_buffer);
  if (!(pid>=-1 && pid< pd.nprograms)) return HTML_DATA_OUTOFBOUND;
  
  pd.moveup(pid);
  
  return HTML_SUCCESS;  
}

// server function to accept program changes
byte server_change_program(char *p) {

  p+=3;
  //ether.urlDecode(p);
  
  // check password
  if(check_password(p)==false)  return HTML_UNAUTHORIZED;
    
  // parse program index
  if (!ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, "pid")) {
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
  if (ether.findKeyVal(pv, tmp_buffer, TMP_BUFFER_SIZE, "name")) {
    ether.urlDecode(tmp_buffer);
    strncpy(prog.name, tmp_buffer, PROGRAM_NAME_SIZE);
  }  

  // i should be equal to os.nstations at this point
  for(;i<MAX_NUM_STATIONS;i++) {
    prog.durations[i] = 0;     // clear unused field
  }

  // process interval day remainder (relative-> absolute)

  if (prog.type == PROGRAM_TYPE_INTERVAL && prog.days[1] > 1) { 
    pd.drem_to_absolute(prog.days);
  }
      
  //bfill.emit_p(PSTR("$F<script>"), htmlOkHeader);

  if (pid==-1) {
    if(!pd.add(&prog))
      return HTML_DATA_OUTOFBOUND;
    //bfill.emit_p(PSTR("alert(\"New program added.\");"));
  } else {
    if(!pd.modify(pid, &prog))
      return HTML_DATA_OUTOFBOUND;
    //bfill.emit_p(PSTR("alert(\"Program $D modified\");"), pid+1);
  }
  //bfill.emit_p(PSTR("window.location=\"/vp\";</script>\n"));
  return HTML_SUCCESS;
}

byte server_json_controller(char *p)
{
  server_json_header();
  server_json_controller_main();
  return HTML_OK;
}

// print controller variables in json
void server_json_controller_main()
{
  byte bid, sid;
  unsigned long curr_time = now();  
  //os.eeprom_string_get(ADDR_EEPROM_LOCATION, tmp_buffer);
  bfill.emit_p(PSTR("\"devt\":$L,\"nbrd\":$D,\"en\":$D,\"rd\":$D,\"rs\":$D,\"mm\":$D,"
                    "\"rdst\":$L,\"loc\":\"$E\",\"wtkey\":\"$E\",\"sbits\":["),
              curr_time,
              os.nboards,
              os.status.enabled,
              os.status.rain_delayed,
              os.status.rain_sensed,
              os.status.manual_mode,
              os.nvdata.rd_stop_time,
              ADDR_EEPROM_LOCATION,
              ADDR_EEPROM_WEATHER_KEY);
  // print sbits
  for(bid=0;bid<os.nboards;bid++)
    bfill.emit_p(PSTR("$D,"), os.station_bits[bid]);  
  bfill.emit_p(PSTR("0],\"ps\":["));
  // print ps
  for(sid=0;sid<os.nstations;sid++) {
    unsigned long rem = 0;
    if (pd.scheduled_program_index[sid] > 0) {
      rem = (curr_time >= pd.scheduled_start_time[sid]) ? (pd.scheduled_stop_time[sid]-curr_time) : (pd.scheduled_stop_time[sid]-pd.scheduled_start_time[sid]);
      if(pd.scheduled_stop_time[sid]==ULONG_MAX-1)  rem=0;
    }
    bfill.emit_p(PSTR("[$D,$L],"), pd.scheduled_program_index[sid], rem);
  }
  
  bfill.emit_p(PSTR("[0,0]]}"));
  
  //bfill.emit_p(PSTR("[0,0]],\"lrun\":[$D,$D,$D,$L],\"rodur\":["),pd.lastrun.station,
  //  pd.lastrun.program,pd.lastrun.duration,pd.lastrun.endtime);
  
  // output run-once timer values
  /*
  uint16_t dur;
  unsigned char *addr = (unsigned char*)ADDR_EEPROM_RUNONCE;
  for(sid=0;sid<os.nstations;sid++, addr+=2) {
    dur=eeprom_read_byte(addr);
    dur=(dur<<8)+eeprom_read_byte(addr+1);
    bfill.emit_p(PSTR("$D"),dur);
    if(sid!=os.nstations-1)
      bfill.emit_p(PSTR(","));    
  }
  bfill.emit_p(PSTR("]}"));    */
}

// print home page
byte server_home(char *p)
{
  byte bid, sid;
  unsigned long curr_time = now();

  bfill.emit_p(PSTR("$F<!DOCTYPE html>\n<html>\n<head>\n$F</head>\n<body>\n<script>"), htmlOkHeader, htmlMobileHeader);

  // send server variables and javascript packets
  bfill.emit_p(PSTR("var ver=$D,ipas=$D;</script>\n"),
               OS_FW_VERSION, os.options[OPTION_IGNORE_PASSWORD].value);
  bfill.emit_p(PSTR("<script src=\"$E/home.js\"></script>\n</body>\n</html>"), ADDR_EEPROM_SCRIPTURL);
  //ether.httpServerReply_with_flags(bfill.position(), TCP_FLAGS_ACK_V|TCP_FLAGS_FIN_V);
  return HTML_OK;
}

void server_json_header()
{
  bfill.emit_p(PSTR("$F{"), htmlJSONHeader);
}

// print options in json (main)
void server_json_options_main() {
  byte oid;
  for(oid=0;oid<NUM_OPTIONS;oid++) {
    int32_t v=os.options[oid].value;
    if (oid==OPTION_MASTER_OFF_ADJ) {v-=60;}
    if (oid==OPTION_RELAY_PULSE) {v*=10;}    
    if (oid==OPTION_STATION_DELAY_TIME) {v=water_time_decode(v);}
    if (oid==OPTION_STATION_DELAY_TIME) {
      bfill.emit_p(PSTR("\"$F\":$L"),
                 os.options[oid].json_str, v);  // account for value possibly larger than 32767
    } else {
      bfill.emit_p(PSTR("\"$F\":$D"),
                   os.options[oid].json_str, v);
    }
    if(oid!=NUM_OPTIONS-1)
      bfill.emit_p(PSTR(","));
  }

  bfill.emit_p(PSTR(",\"dexp\":$D,\"mexp\":$D}"), (int)os.detect_exp(), MAX_EXT_BOARDS);
}

// printing options in json
byte server_json_options(char *p)
{
  server_json_header();
  server_json_options_main();
  return HTML_OK;
}

/*=============================================
  Change Controller Values
  
  HTTP GET command format:
  /cv?pw=xxx&rsn=x&rbt=x&en=x&mm=x&rd=x
  
  pw:  password
  rsn: reset all stations (0 or 1)
  rbt: reboot controller (0 or 1)
  en:  enable (0 or 1)
  mm:  manual mode (0 or 1)
  rd:  rain delay hours (0 turns off rain delay)
  =============================================*/
byte server_change_values(char *p)
{
  p+=3;
  // if no password is attached, or password is incorrect
  if(check_password(p)==false)  return HTML_UNAUTHORIZED;

  if (ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, "rsn")) {
    reset_all_stations();
  }
#define TIME_REBOOT_DELAY  20

  if (ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, "rbt") && atoi(tmp_buffer) > 0) {
    //bfill.emit_p(PSTR("$F<meta http-equiv=\"refresh\" content=\"$D; url=/\">"), htmlOkHeader, TIME_REBOOT_DELAY);
    bfill.emit_p(PSTR("$FRebooting..."), htmlOkHeader);
    ether.httpServerReply_with_flags(bfill.position(), TCP_FLAGS_ACK_V|TCP_FLAGS_FIN_V);
    os.reboot();
  } 
  
  if (ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, "en")) {
    if (tmp_buffer[0]=='1' && !os.status.enabled)  os.enable();
    else if (tmp_buffer[0]=='0' &&  os.status.enabled)  os.disable();
  }   
  
  if (ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, "mm")) {
    if (tmp_buffer[0]=='1' && !os.status.manual_mode) {
      reset_all_stations();
      os.status.manual_mode = 1;
      //os.constatus_save();
      
    } else if (tmp_buffer[0]=='0' &&  os.status.manual_mode) {
      reset_all_stations();
      os.status.manual_mode = 0;
      //os.constatus_save();
    }
  }
  if (ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, "rd")) {
    int rd = atoi(tmp_buffer);
    if (rd>0) {
      os.nvdata.rd_stop_time = now() + (unsigned long) rd * 3600;    
      os.raindelay_start();
    } else if (rd==0){
      os.raindelay_stop();
    } else  return HTML_DATA_OUTOFBOUND;
  }  
 
  //bfill.emit_p(PSTR("$F<script>$F"), htmlOkHeader, htmlReturnHome);
  return HTML_SUCCESS;
}

// server function to accept script url changes
byte server_change_scripturl(char *p)
{
  p+=3;

  // if no password is attached, or password is incorrect
  if(check_password(p)==false)  return HTML_UNAUTHORIZED;
  if (ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, "jsp")) {
    ether.urlDecode(tmp_buffer);
    os.eeprom_string_set(ADDR_EEPROM_SCRIPTURL, tmp_buffer);
  }
  //bfill.emit_p(PSTR("$F<script>alert(\"Script url saved.\");$F"), htmlOkHeader, htmlReturnHome);  
  return HTML_SUCCESS;
}  
    

// server function to accept option changes
byte server_change_options(char *p)
{
  p+=3;

  // if no password is attached, or password is incorrect
  if(check_password(p)==false)  return HTML_UNAUTHORIZED;

  // temporarily save some old options values
  byte old_tz =  os.options[OPTION_TIMEZONE].value;
  byte old_ntp = os.options[OPTION_USE_NTP].value;
  // !!! p and bfill share the same buffer, so don't write
  // to bfill before you are done analyzing the buffer !!!
  
  // process option values
  byte err = 0;
  for (byte oid=0; oid<NUM_OPTIONS; oid++) {
    //if ((os.options[oid].flag&OPFLAG_WEB_EDIT)==0) continue;
    // skip binary options that do not appear in the UI
    if (oid==OPTION_USE_DHCP || oid==OPTION_RESET || oid==OPTION_DEVICE_ENABLE) continue;
    if (os.options[oid].max==1)  os.options[oid].value = 0;  // set a bool variable to 0 first
    char tbuf2[5] = {'o', 0, 0, 0, 0};
    itoa(oid, tbuf2+1, 10);
    if (ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, tbuf2)) {
      if (os.options[oid].max==1) {
        os.options[oid].value = 1;  // if the bool variable is detected, set to 1
        continue;
      }
      int32_t v = atol(tmp_buffer);
      if (oid==OPTION_MASTER_OFF_ADJ) {v+=60;} // master off time
      if (oid==OPTION_RELAY_PULSE) {v/=10;} // relay pulse time
      if (oid==OPTION_STATION_DELAY_TIME) {v=water_time_encode((uint16_t)v);} // encode station delay time
      if (v>=0 && v<=os.options[oid].max) {
        os.options[oid].value = v;
      } else {
        err = 1;
      }
    }
  }
  
  if (ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, "loc")) {
    ether.urlDecode(tmp_buffer);
    os.eeprom_string_set(ADDR_EEPROM_LOCATION, tmp_buffer);
  }
  uint8_t keyfound = 0;
  if (ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, "wtkey", &keyfound)) {
    ether.urlDecode(tmp_buffer);
    os.eeprom_string_set(ADDR_EEPROM_WEATHER_KEY, tmp_buffer);
  } else if (keyfound) {
    tmp_buffer[0]=0;
    os.eeprom_string_set(ADDR_EEPROM_WEATHER_KEY, tmp_buffer);
  }
  if (ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, "ttt")) {
    unsigned long t;
    //ether.urlDecode(tmp_buffer);
    t = atol(tmp_buffer);
    // before chaging time, reset all stations
    reset_all_stations();
    setTime(t);  
    if (os.status.has_rtc) RTC.set(t); // if rtc exists, update rtc
  }
  if (err) {
    //bfill.emit_p(PSTR("$F<script>alert(\"Values out of bound!\");window.location=\"/vo\";</script>\n"), htmlOkHeader);
    return HTML_DATA_OUTOFBOUND;
  } 

  os.options_save();
  
  //bfill.emit_p(PSTR("$F<script>alert(\"Options values saved.\");$F"), htmlOkHeader, htmlReturnHome);  
  if(os.options[OPTION_TIMEZONE].value != old_tz ||
     (!old_ntp && os.options[OPTION_USE_NTP].value)) {
    last_sync_time = 0;
  }
  return HTML_SUCCESS;
}

// server function to change password
byte server_change_password(char *p)
{
  p+=3;

  // if no password is attached, or password is incorrect
  if(check_password(p)==true) {
    if (ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, "npw")) {
      char tbuf2[TMP_BUFFER_SIZE];
      if (ether.findKeyVal(p, tbuf2, TMP_BUFFER_SIZE, "cpw") && strncmp(tmp_buffer, tbuf2, 16) == 0) {
        //os.password_set(tmp_buffer);
        ether.urlDecode(tmp_buffer);
        os.eeprom_string_set(ADDR_EEPROM_PASSWORD, tmp_buffer);
        //bfill.emit_p(PSTR("$F{\"result\":$D}"), htmlJSONHeader, 1); // success
        return HTML_SUCCESS;
      } else {
        //bfill.emit_p(PSTR("$F{\"result\":$D}"), htmlJSONHeader, 3); // password don't match
        return HTML_MISMATCH;
      }
    }
  }
  
  //bfill.emit_p(PSTR("$F{\"result\":$D}"), htmlJSONHeader, 2); // unauthorized
  return HTML_UNAUTHORIZED;
}

void server_json_status_main()
{
  bfill.emit_p(PSTR("\"sn\":["));
  byte sid;

  for (sid=0;sid<os.nstations;sid++) {
    bfill.emit_p(PSTR("$D"), (os.station_bits[(sid>>3)]>>(sid%8))&1);
    if(sid!=os.nstations-1) bfill.emit_p(PSTR(","));
  }
  bfill.emit_p(PSTR("],\"nstations\":$D}"), os.nstations);  
}

byte server_json_status(char *p)
{
  server_json_header();
  server_json_status_main();
  return HTML_OK;
}


/*=================================================
  Get/Set Station Bits:
  
  HTTP GET command format:
  /snx     -> get station bit (e.g. /sn1, /sn2 etc.)
  /sn0     -> get all bits
  
  The following will only work if controller is
  switched to manual mode:
  
  /snx=0   -> turn off station 
  /snx=1   -> turn on station
  /snx=1&t=xx -> turn on with timer (in seconds)
  =================================================*/
byte server_station_bits(char *p) {

  p+=2;
  int sid;
  byte i, sidmin, sidmax;  

  // parse station name
  i=0;
  while((*p)>='0'&&(*p)<='9'&&i<4) {
    tmp_buffer[i++] = (*p++);
  }
  tmp_buffer[i]=0;
  sid = atoi(tmp_buffer);
  if (!(sid>=0&&sid<=os.nstations)) return HTML_DATA_OUTOFBOUND;
  
  // parse get/set command
  if ((*p)=='=') {
    if (sid==0) return HTML_DATA_FORMATERROR;
    sid--;
    // this is a set command
    // can only do it when in manual mode
    if (!os.status.manual_mode) {
      //bfill.emit_p(PSTR("$F<script>alert(\"Station bits can only be set in manual mode.\")</script>\n"), htmlOkHeader);
      return HTML_NOT_PERMITTED;
    }
    // parse value
    p++;
    if ((*p)=='0') {
      manual_station_off(sid);
    } else if ((*p)=='1') {
      int ontimer = 0;
      if (ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, "t")) {
        ontimer = atoi(tmp_buffer);
        if (!(ontimer>=0))  return HTML_DATA_OUTOFBOUND;
      }
      manual_station_on((byte)sid, ontimer);
    } else {
      return HTML_DATA_OUTOFBOUND;
    }
    return HTML_SUCCESS;
  } else {
    // this is a get command
    bfill.emit_p(PSTR("$F"), htmlOkHeader);
    if (sid==0) { // print all station bits
      sidmin=0;sidmax=(os.nstations);
    } else {  // print one station bit
      sidmin=(sid-1);
      sidmax=sid;
    }
    for (sid=sidmin;sid<sidmax;sid++) {
      bfill.emit_p(PSTR("$D"), (os.station_bits[(sid>>3)]>>(sid%8))&1);
    }
    return HTML_OK;      
  }

  return HTML_UNAUTHORIZED;
}

/*=============================================
  Print log data in json
  
  HTTP GET command format:
  /jl?start=xxxxx&end=xxxxx
  
  start: start day (epoch time / 86400)
  end:   end day (epoch time / 874000
  start and end time are inclusive
  =============================================*/  
byte server_json_log(char *p) {

  // if no sd card, return false
  if (!os.status.has_sd)  return HTML_PAGE_NOT_FOUND;
  
  unsigned int start, end;
  if (!ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, "start"))
    return HTML_DATA_MISSING;
  start = atol(tmp_buffer) / 86400L;
  
  if (!ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, "end"))
    return HTML_DATA_MISSING;
  end = atol(tmp_buffer) / 86400L;
  
  // start must be prior to end, and can't retrieve more than 365 days of data
  if ((start>end) || (end-start)>365)  return HTML_DATA_OUTOFBOUND;
  
  bfill.emit_p(PSTR("$F["), htmlJSONHeader);
 
  char *s;
  int res, count = 0;
  bool first = true;
  for(int i=start;i<=end;i++) {
    itoa(i, tmp_buffer, 10);
    strcat(tmp_buffer, ".txt");
    
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
      
      // if this is the first record, do not print comma
      if (first)  { bfill.emit_p(PSTR("$S"), tmp_buffer); first=false;}
      else {  bfill.emit_p(PSTR(",$S"), tmp_buffer); }
      count ++;
      if (count % 25 == 0) {  // push out a packet every 25 records
        ether.httpServerReply_with_flags(bfill.position(), TCP_FLAGS_ACK_V);
        bfill=ether.tcpOffset();
        count = 0;
      }
    }
  }

  bfill.emit_p(PSTR("]"));   
  return HTML_OK; 
}

/*=============================================
  Delete Log
  
  HTTP GET command format:
  /dl?pw=xxx&day=xxx
  
  pw: password
  day:day (epoch time / 86400) to delete log for
  =============================================*/
byte server_delete_log(char *p) {

  p+=3;
  
  // check password
  if(check_password(p)==false)  return HTML_UNAUTHORIZED;
  
  if (!ether.findKeyVal(p, tmp_buffer, TMP_BUFFER_SIZE, "day"))
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
//prog_char _url_cr [] PROGMEM = "cr";
prog_char _url_up [] PROGMEM = "up";
prog_char _url_jp [] PROGMEM = "jp";

prog_char _url_co [] PROGMEM = "co";
prog_char _url_jo [] PROGMEM = "jo";
prog_char _url_sp [] PROGMEM = "sp";

prog_char _url_sn [] PROGMEM = "sn";
prog_char _url_js [] PROGMEM = "js";

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
  //{_url_cr,server_change_runonce},
  {_url_up,server_moveup_program},
  {_url_jp,server_json_programs},
  

  {_url_co,server_change_options},
  {_url_jo,server_json_options},
  {_url_sp,server_change_password},

  {_url_sn,server_station_bits},
  {_url_js,server_json_status},  

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
    //ether.urlDecode(str);
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
        //ether.urlDecode(str);
        byte ret = (urls[i].handler)(str);
        if (ret != HTML_OK) {
          bfill.emit_p(PSTR("$F{\"result\":$D}"), htmlJSONHeader, ret);
        }
        break;
      }
    }
    
    if((i==sizeof(urls)/sizeof(URLStruct)) && os.status.has_sd) {
      // no server funtion found, file handler
      byte k=0;  
      while (str[k]!=' ' && k<32) {tmp_buffer[k]=str[k];k++;}//search the end, indicated by space
      tmp_buffer[k]=0;
      if (streamfile ((char *)tmp_buffer)==0) {
        // file not found
        bfill.emit_p(PSTR("$F{\"result\":$D}"), htmlJSONHeader, HTML_PAGE_NOT_FOUND);
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

  bfill.emit_p(PSTR("$F"), htmlFileHeader);
  ether.httpServerReply_with_flags(bfill.position(), TCP_FLAGS_ACK_V);
  bfill = ether.tcpOffset();
  while(myfile.available()) {
    int nbytes = myfile.read(Ethernet::buffer+54, 512);
    cur = nbytes;
    if (cur>=512) {
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

// =============
// NTP Functions
// =============

/*
unsigned long ntp_wait_response()
{
  uint32_t time;
  unsigned long start = millis();
  do {
    ether.packetLoop(ether.packetReceive());
    if (ether.ntpProcessAnswer(&time, ntpclientportL))
    {
      if ((time & 0x80000000UL) ==0){
        time+=2085978496;
      }else{
        time-=2208988800UL;
      }
      return time + (int32_t)3600/4*(int32_t)(os.options[OPTION_TIMEZONE].value-48);
    }
  } while(millis() - start < 1000); // wait at most 1 seconds for ntp result
  return 0;
}
*/

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
