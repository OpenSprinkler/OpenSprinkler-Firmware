// Example code for OpenSprinkler Generation 2

/* This is a program-based sprinkler schedule algorithm.
 Programs are set similar to calendar schedule.
 Each program specifies the days, stations,
 start time, end time, interval and duration.
 The number of programs you can create are subject to EEPROM size.
 
 Creative Commons Attribution-ShareAlike 3.0 license
 Sep 2014 @ Rayshobby.net
 */
#include <limits.h>
#include <OpenSprinklerGen2.h>
#include <SdFat.h>
#include <Wire.h>
#include "program.h"

#define NTP_SYNC_INTERVAL       86400L  // NYP sync interval, 24 hrs
#define RTC_SYNC_INTERVAL       60      // RTC sync interval, 60 secs
#define CHECK_NETWORK_INTERVAL  30      // Network checking interval, 30 secs
#define DHCP_RENEW_INTERVAL     43200L  // DHCP renewal interval: 12 hrs
#define CHECK_WEATHER_INTERVAL  900     // Weather check interval: 15 mins
#define LCD_DIMMING_TIMEOUT      15     // LCD dimming timeout: 15 secs
#define PING_TIMEOUT            200     // Ping test timeout: 200 ms

byte Ethernet::buffer[ETHER_BUFFER_SIZE]; // Ethernet packet buffer
char tmp_buffer[TMP_BUFFER_SIZE+1];       // scratch buffer
BufferFiller bfill;                       // buffer filler

// ====== Object defines ======
OpenSprinkler os; // OpenSprinkler object
ProgramData pd;   // ProgramdData object 
SdFat sd;         // SD card object

// ====== UI defines ======
static char ui_anim_chars[3] = {'.', 'o', 'O'};
  
#define UI_STATE_DEFAULT   0
#define UI_STATE_DISP_IP   1
#define UI_STATE_DISP_GW   2
#define UI_STATE_RUNPROG   3

static byte ui_state = UI_STATE_DEFAULT;
static byte ui_state_runprog = 0;

void ui_state_machine(time_t curr_time) {

  if (os.button_lasttime && os.button_lasttime + LCD_DIMMING_TIMEOUT < curr_time) {
    analogWrite(PIN_LCD_BACKLIGHT, 255-os.options[OPTION_LCD_DIMMING].value); 
    os.button_lasttime = 0;
    ui_state = UI_STATE_DEFAULT;  // also recover to default state
  }
      
  // read button, if something is pressed, wait till release
  byte button = os.button_read(BUTTON_WAIT_HOLD);

  if (button & BUTTON_FLAG_DOWN) {   // repond only to button down events
    os.button_lasttime = curr_time;
    analogWrite(PIN_LCD_BACKLIGHT, 255-os.options[OPTION_LCD_BACKLIGHT].value); // button is pressed, turn on LCD right away
  } else {
    return;
  }

  switch(ui_state) {
  case UI_STATE_DEFAULT:
    switch (button & BUTTON_MASK) {
    case BUTTON_1:
      if (button & BUTTON_FLAG_HOLD) {  // holding B1: stop all stations
        if (digitalRead(PIN_BUTTON_2)==0) { // if B2 is pressed, run a short test (internal test)
          manual_start_program(255);
        } else {
          reset_all_stations();
        }
      } else {  // clicking B1: display device IP and port
        os.lcd_print_ip(ether.myip, ether.hisport);
        ui_state = UI_STATE_DISP_IP;
      }
      break;
    case BUTTON_2:
      if (button & BUTTON_FLAG_HOLD) {  // holding B2: reboot
        os.reboot();
      } else {  // clicking B2: display gateway IP
        os.lcd_print_ip(ether.gwip, 0);
        ui_state = UI_STATE_DISP_GW;
      }
      break;
    case BUTTON_3:
      if (button & BUTTON_FLAG_HOLD) {  // holding B3: go to main menu
        os.lcd_print_line_clear_pgm(PSTR("Run a Program:"), 0);
        os.lcd_print_line_clear_pgm(PSTR("Click B3 to list"), 1);
        ui_state = UI_STATE_RUNPROG;
      } else {  // clicking B3: switch board display (cycle through master and all extension boards)
        os.status.display_board = (os.status.display_board + 1) % (os.nboards);
      }
      break;
    }
    break;
  case UI_STATE_DISP_IP:
  case UI_STATE_DISP_GW:
    ui_state = UI_STATE_DEFAULT;
    break;
  case UI_STATE_RUNPROG:
    if ((button & BUTTON_MASK)==BUTTON_3) {
      if (button & BUTTON_FLAG_HOLD) {
        // start
        manual_start_program(ui_state_runprog);
        ui_state = UI_STATE_DEFAULT;
      } else {
        ui_state_runprog = (ui_state_runprog+1) % (pd.nprograms+1);
        os.lcd_print_line_clear_pgm(PSTR("Hold B3 to start"), 0);
        if(ui_state_runprog > 0) {
          ProgramStruct prog;
          pd.read(ui_state_runprog-1, &prog);
          os.lcd_print_line_clear_pgm(PSTR(" "), 1);
          os.lcd.setCursor(0, 1);
          os.lcd.print((int)ui_state_runprog);
          os.lcd_print_pgm(PSTR(". "));
          os.lcd.print(prog.name);
        } else {
          os.lcd_print_line_clear_pgm(PSTR("0. Test (1 min)"), 1);
        }
      }
    }
    break;
  }
}

// ======================
// Arduino Setup Function
// ======================
void setup() { 
  /* Clear WDT reset flag. */
  MCUSR &= ~(1<<WDRF);
  
  DEBUG_BEGIN(9600);
  DEBUG_PRINTLN("start");
    
  os.begin();          // OpenSprinkler init
  os.options_setup();  // Setup options
 
  pd.init();            // ProgramData init

  setSyncInterval(RTC_SYNC_INTERVAL);  // RTC sync interval
  // if rtc exists, sets it as time sync source
  setSyncProvider(os.status.has_rtc ? RTC.get : NULL);
  delay(500);
  os.lcd_print_time(0);  // display time to LCD

  // enable WDT
  /* In order to change WDE or the prescaler, we need to
   * set WDCE (This will allow updates for 4 clock cycles).
   */
  WDTCSR |= (1<<WDCE) | (1<<WDE);
  /* set new watchdog timeout prescaler value */
  WDTCSR = 1<<WDP3 | 1<<WDP0;  // 8.0 seconds
  /* Enable the WD interrupt (note no reset). */
  WDTCSR |= _BV(WDIE);  
    
  // attempt to detect SD card
  os.lcd_print_line_clear_pgm(PSTR("Detecting uSD..."), 1);

  if(sd.begin(PIN_SD_CS, SPI_HALF_SPEED)) {
    os.status.has_sd = 1;
  }
    
  if (os.start_network()) {  // initialize network
    os.status.network_fails = 0;
  } else  os.status.network_fails = 1;

  delay(500);

  os.apply_all_station_bits(); // reset station bits
  
  os.button_lasttime = now();
}

// Arduino software reset function
void(* sysReset) (void) = 0;

volatile byte wdt_timeout = 0;
// WDT interrupt service routine
ISR(WDT_vect)
{
  wdt_timeout += 1;
  // this isr is called every 8 seconds
  if (wdt_timeout > 15) {
    // reset after 120 seconds of timeout
    sysReset();
  }
}

  
// =================
// Arduino Main Loop
// =================
void loop()
{
  static unsigned long last_time = 0;
  static unsigned long last_minute = 0;
  static uint16_t pos;

  byte bid, sid, s, pid, bitvalue;
  ProgramStruct prog;

  os.status.seq = os.options[OPTION_SEQUENTIAL].value;
  os.status.mas = os.options[OPTION_MASTER_STATION].value;

  // ====== Process Ethernet packets ======
  pos=ether.packetLoop(ether.packetReceive());
  if (pos>0) {  // packet received
    analyze_get_url((char*)Ethernet::buffer+pos);
  }
  // ====== end of Process Ethernet packets
  
  wdt_reset();  // reset watchdog timer
  wdt_timeout = 0;
   
  time_t curr_time = now();
  ui_state_machine(curr_time);

  // if 1 second has passed
  if (last_time != curr_time) {

    last_time = curr_time;


    if (!ui_state)    
      os.lcd_print_time(0);       // print time

    // ====== Check raindelay status ======
    if (os.status.rain_delayed) {
      if (curr_time >= os.nvdata.rd_stop_time) {  // rain delay is over  
        os.raindelay_stop();
      }
    } else {
      if (os.nvdata.rd_stop_time > curr_time) {   // rain delay starts now
        os.raindelay_start();
      }
    }

    // ====== Check rain sensor status ======
    os.rainsensor_status();

    // ====== Check controller status changes and write log ======
    if (os.old_status.rain_delayed != os.status.rain_delayed) {
      if (os.status.rain_delayed) {
        // rain delay started, record time
        os.raindelay_start_time = curr_time;
      } else {
        // rain delay stopped, write log
        write_log(LOGDATA_RAINDELAY, curr_time);
      }
      os.old_status.rain_delayed = os.status.rain_delayed;
    }
    if (os.old_status.rain_sensed != os.status.rain_sensed) {
      if (os.status.rain_sensed) {
        // rain sensor on, record time
        os.rainsense_start_time = curr_time;
      } else {
        // rain sensor off, write log
        write_log(LOGDATA_RAINSENSE, curr_time);
      }
      os.old_status.rain_sensed = os.status.rain_sensed;
    }

    // ====== Schedule program data ======
    unsigned long curr_minute = curr_time / 60;
    boolean match_found = false;
    // since the granularity of start time is minute
    // we only need to check once every minute
    if (curr_minute != last_minute) {
      last_minute = curr_minute;
      // check through all programs
      for(pid=0; pid<pd.nprograms; pid++) {
        pd.read(pid, &prog);
        if(prog.check_match(curr_time)) {
          // program match found
          // process all selected stations
          for(sid=0;sid<os.nstations;sid++) {
            bid=sid>>3;
            s=sid&0x07;
            // skip if the station is:
            // - master station (because master cannot be scheduled independently
            // - currently running (cannot handle overlapping schedules of the same station)
            // - disabled
            if ((os.status.mas==sid+1) || (os.station_bits[bid]&(1<<s)) || (os.stndis_bits[bid]&(1<<s)))
              continue;
            
            // if station has non-zero water time and if it doesn't already have a scheduled stop time
            if (prog.durations[sid] && !pd.scheduled_stop_time[sid]) {
              // initialize schedule data by storing water time temporarily in stop_time
              // water time is scaled by watering percentage
              unsigned long water_time = (unsigned long)water_time_decode(prog.durations[sid]);
              // if the program is set to use weather scaling 
              if (prog.use_weather)
                water_time = water_time * os.options[OPTION_WATER_PERCENTAGE].value / 100;
              pd.scheduled_stop_time[sid] = water_time;
                                            
              if (pd.scheduled_stop_time[sid]) {  // water time may end up being zero after scaling
                pd.scheduled_program_index[sid] = pid+1;  
                match_found = true;
              }// if pd.scheduled_stop_time[sid]
            }// if prog.durations[sid]
          }// for sid
        }// if check_match
      }// for pid
      
      // calculate start and end time
      if (match_found) {
        schedule_all_stations(curr_time, os.status.seq);
      }
    }//if_check_current_minute
    
    // ====== Run program data ======
    // Check if a program is running currently
    // If so, do station run-time keeping
    if (os.status.program_busy){
      for(bid=0;bid<os.nboards; bid++) {
        bitvalue = os.station_bits[bid];
        for(s=0;s<8;s++) {
          byte sid = bid*8+s;
          
          // check if this station is scheduled, either running or waiting to run
          if (pd.scheduled_program_index[sid] > 0) {
            // if so, check if we should turn it off
            if (curr_time >= pd.scheduled_stop_time[sid]) {
              turn_off_station(sid, os.status.mas, curr_time);          
            }
          }
          // if current station is not running, check if we should turn it on
          if(!((bitvalue>>s)&1)) {
            if (curr_time >= pd.scheduled_start_time[sid] && curr_time < pd.scheduled_stop_time[sid]) {
              os.set_station_bit(sid, 1);
              
              // schedule master station here if
              // 1) master station is defined
              // 2) the station is non-master and is set to activate master
              // 3) controller is in sequential mode (if in parallel mode, master is handled differently)
              //if ((os.status.mas>0) && (os.status.mas!=sid+1) && (os.masop_bits[bid]&(1<<s)) && os.status.seq && os.status.manual_mode==0) {
              if ((os.status.mas>0) && (os.status.mas!=sid+1) && (os.masop_bits[bid]&(1<<s)) && os.status.seq) {
                byte masid=os.status.mas-1;
                // master will turn on when a station opens,
                // adjusted by the master on and off time
                pd.scheduled_start_time[masid] = pd.scheduled_start_time[sid]+os.options[OPTION_MASTER_ON_ADJ].value;
                pd.scheduled_stop_time[masid] = pd.scheduled_stop_time[sid]+os.options[OPTION_MASTER_OFF_ADJ].value-60;
                pd.scheduled_program_index[masid] = pd.scheduled_program_index[sid];
                // immediately check if we should turn on master now
                if (curr_time >= pd.scheduled_start_time[masid] && curr_time < pd.scheduled_stop_time[masid]) {
                  os.set_station_bit(masid, 1);
                }
              }
              // upon turning on station, process relay
              // if the station is set to activate / deactivate relay
              if(os.actrelay_bits[bid]&(1<<s)) {
                // turn relay on
                digitalWrite(PIN_RELAY, HIGH);
                if(os.options[OPTION_RELAY_PULSE].value > 0) {  // if relay is set to pulse
                  delay(os.options[OPTION_RELAY_PULSE].value*10);
                  digitalWrite(PIN_RELAY, LOW);
                }
              } // if activate relay
            } //if curr_time > scheduled_start_time
          } // if current station is not running
        }//end_s
      }//end_bid
      
      // process dynamic events
      process_dynamic_events(curr_time);
      
      // activate / deactivate valves
      os.apply_all_station_bits();

      // check through run-time data, calculate last_stop_time,
      boolean program_still_busy = false;
      pd.last_stop_time = 0;
      unsigned long sst;
      for(sid=0;sid<os.nstations;sid++) {
        // check if any station has a valid stop time
        // and the stop time must be larger than curr_time
        sst = pd.scheduled_stop_time[sid];
        if (sst > curr_time) {
          pd.last_stop_time = (sst > pd.last_stop_time ) ? sst : pd.last_stop_time;
          program_still_busy = true;
        }
      }
      // if no station has a schedule
      if (program_still_busy == false) {
        // turn off all stations
        os.clear_all_station_bits();
        os.apply_all_station_bits();
        // reset runtime
        pd.reset_runtime();
        // reset program busy bit
        os.status.program_busy = 0;
        
        // in case some options have changed while executing the program        
        os.status.mas = os.options[OPTION_MASTER_STATION].value; // update master station
        os.status.seq = os.options[OPTION_SEQUENTIAL].value;
      }
    }//if_some_program_is_running

    // handle master station for parallel mode
    //if ((os.status.mas>0) && os.status.manual_mode==1 || os.status.seq==0) {
    if ((os.status.mas>0) && os.status.seq==0) {
      // master will be on / off together with stations
      byte masbit = 0;
      for(sid=0;sid<os.nstations;sid++) {
        bid = sid>>3;
        s = sid&0x07;
        // check there is any non-master station that activates master and is currently turned on
        if ((os.status.mas!=sid+1) && (os.station_bits[bid]&(1<<s)) && (os.masop_bits[bid]&(1<<s))) {
          masbit = 1;
          break;
        }
      }
      os.set_station_bit(os.status.mas-1, masbit);
    }    
                  
    // process dynamic events
    process_dynamic_events(curr_time);
      
    // activate/deactivate valves
    os.apply_all_station_bits();
    
    // process LCD display
    if (!ui_state)    
      os.lcd_print_station(1, ui_anim_chars[curr_time%3]);
    
    // check network connection
    check_network(curr_time);
    
    // perform ntp sync
    perform_ntp_sync(curr_time);
    
    // check weather
    check_weather(curr_time);
  }
}

void perform_ntp_sync(time_t curr_time) {
  // do not perform sync if this option is disabled, or if network is not available, or if a program is running
  if (!os.options[OPTION_USE_NTP].value || os.status.network_fails>0 || os.status.program_busy) return;   

  if (os.ntpsync_lasttime == 0 || (curr_time - os.ntpsync_lasttime > NTP_SYNC_INTERVAL)) {
    os.ntpsync_lasttime = curr_time;
    if (!ui_state) {
      os.lcd_print_line_clear_pgm(PSTR("NTP Syncing..."),1);
    }
    unsigned long t = getNtpTime();   
    if (t>0) {    
      setTime(t);
      if (os.status.has_rtc) RTC.set(t); // if rtc exists, update rtc
    }
  }
}

void check_weather(time_t curr_time) {
  // do not check weather if the Use Weather option is disabled, or if network is not available, or if a program is running
  if (os.status.network_fails>0 || os.status.program_busy) return;

  uint16_t inv = 10;
  if (os.status.wt_received)  inv = CHECK_WEATHER_INTERVAL;
  if (!os.checkwt_lasttime || ((curr_time - os.checkwt_lasttime) > inv)) {
    os.checkwt_lasttime = curr_time;
    GetWeather();
  }
}

void check_network(time_t curr_time) {
  if (os.status.program_busy) {return;}

  // do not perform network checking if the controller has just started, or if a program is running
  //if (os.network_lasttime == 0) { os.network_lasttime = curr_time; os.dhcpnew_lasttime = curr_time;}
  if (!os.network_lasttime) {
    os.start_network();
  }
  
  // check network condition periodically
  // check interval depends on the fail times
  // the more time it fails, the longer the gap between two checks
  unsigned long interval = 1 << (os.status.network_fails);
  interval *= CHECK_NETWORK_INTERVAL;
  if (curr_time - os.network_lasttime > interval) {
    // change LCD icon to indicate it's checking network
    if (!ui_state) {
      os.lcd.setCursor(15, 1);
      os.lcd.write(4);
    }
      
    os.network_lasttime = curr_time;
   
    // ping gateway ip
    ether.clientIcmpRequest(ether.gwip);
    
    unsigned long start = millis();
    boolean failed = true;
    // wait at most PING_TIMEOUT milliseconds for ping result
    do {
      ether.packetLoop(ether.packetReceive());
      if (ether.packetLoopIcmpCheckReply(ether.gwip)) {
        failed = false;
        break;
      }
    } while(millis() - start < PING_TIMEOUT);
    if (failed)  {
      os.status.network_fails++;
      // clamp it to 6
      if (os.status.network_fails > 6) os.status.network_fails = 6;
    }
    else os.status.network_fails=0;
    // if failed more than once, reconnect
    if ((os.status.network_fails>2 || (curr_time - os.dhcpnew_lasttime > DHCP_RENEW_INTERVAL))
        &&os.options[OPTION_NETFAIL_RECONNECT].value) {
      os.dhcpnew_lasttime = curr_time;
      //os.lcd_print_line_clear_pgm(PSTR(""),0);
      if (os.start_network())
        os.status.network_fails=0;
    }
  } 
}

void turn_off_station(byte sid, byte mas, unsigned long curr_time) {
  byte bid = sid>>3;
  byte s = sid&0x07;
  os.set_station_bit(sid, 0);

  // ignore if we are turning off a station that's not running or scheduled to run
  if (!pd.scheduled_start_time[sid])  return;
  
  // check if the current time is past the scheduled start time,
  // because we may be turning off a station that hasn't started yet
  if (curr_time > pd.scheduled_start_time[sid]) {
    // record lastrun log (only for non-master stations)
    if(mas!=(sid+1)) {
      pd.lastrun.station = sid;
      pd.lastrun.program = pd.scheduled_program_index[sid];
      pd.lastrun.duration = curr_time - pd.scheduled_start_time[sid];
      pd.lastrun.endtime = curr_time;
      write_log(LOGDATA_STATION, curr_time);
    }      

    // upon turning off station, update master station stop time
    // because we may be manually turning station off earlier than scheduled
    if ((mas!=sid+1) && (os.masop_bits[bid]&(1<<s)) && os.status.seq) {
      pd.scheduled_stop_time[mas-1] = curr_time+os.options[OPTION_MASTER_OFF_ADJ].value-60;
    }

    // upon turning off station, process relay
    // if the station is set to active / deactivate relay
    if(os.actrelay_bits[bid]&(1<<s)) {
      // turn relay off
      if(os.options[OPTION_RELAY_PULSE].value > 0) {  // if relay is set to pulse
        digitalWrite(PIN_RELAY, HIGH);
        delay(os.options[OPTION_RELAY_PULSE].value*10);
      } 
      digitalWrite(PIN_RELAY, LOW);
    }
  }
  
  // reset program run-time data variables
  pd.scheduled_start_time[sid] = 0;
  pd.scheduled_stop_time[sid] = 0;
  pd.scheduled_program_index[sid] = 0;  
  
}

void process_dynamic_events(unsigned long curr_time)
{
  // check if rain is detected
  bool rain = false;
  bool en = os.status.enabled ? true : false;
  if (os.status.rain_delayed || (os.options[OPTION_USE_RAINSENSOR].value && os.status.rain_sensed)) {
    rain = true;
  }

  byte sid, s, bid, rbits, sbits;
  for(bid=0;bid<os.nboards;bid++) {
    rbits = os.ignrain_bits[bid];
    sbits = os.station_bits[bid];
    for(s=0;s<8;s++) {
      sid=bid*8+s;
      // If this is a normal program (not a run-once or test program)
      // and either the controller is disabled, or
      // if raining and ignore rain bit is cleared
      //if (!mm && (pd.scheduled_program_index[sid] != 254) &&
      if ((pd.scheduled_program_index[sid]<99) &&
          (!en || (rain && !(rbits&(1<<s)))) ) {
        if (sbits&(1<<s)) { // if station is currently running
          turn_off_station(sid, os.status.mas, curr_time);
                   
        } else if (pd.scheduled_program_index[sid] > 0) { // if station is currently not running but is waiting to run
        
          // reset program data variables
          pd.scheduled_start_time[sid] = 0;
          pd.scheduled_stop_time[sid] = 0;
          pd.scheduled_program_index[sid] = 0;             
        }
      }
    }
  }      
}

void schedule_all_stations(unsigned long curr_time, byte seq)
{
  unsigned long accumulate_time = curr_time + 1;
  // if we are in sequential mode, and if there are 
  // existing running/scheduled programs
  int16_t station_delay = water_time_decode(os.options[OPTION_STATION_DELAY_TIME].value);
  if (seq && pd.last_stop_time > 0) {
    accumulate_time = pd.last_stop_time + station_delay;
  }

  DEBUG_PRINT("current time:");
  DEBUG_PRINTLN(curr_time);
  
  byte sid;
    
  // calculate start / stop time of each station
  for(sid=0;sid<os.nstations;sid++) {
    // skip master station because it's not scheduled independently
    if (os.status.mas==sid+1) continue;
    byte bid=sid>>3;
    byte s=sid&0x07;    
    
    // if the station is not scheduled to run, or is already scheduled (i.e. start_time > 0) or is already running
    // skip
    if(!pd.scheduled_stop_time[sid] || pd.scheduled_start_time[sid] || (os.station_bits[bid]&(1<<s)))
      continue;
    
    if (seq) {  // in sequential mode, stations are serialized
      pd.scheduled_start_time[sid] = accumulate_time;
      accumulate_time += pd.scheduled_stop_time[sid];
      pd.scheduled_stop_time[sid] = accumulate_time;
      accumulate_time += station_delay; // add station delay time
      DEBUG_PRINT("[");
      DEBUG_PRINT(sid);
      DEBUG_PRINT(":");
      DEBUG_PRINT(pd.scheduled_start_time[sid]);
      DEBUG_PRINT(",");
      DEBUG_PRINT(pd.scheduled_stop_time[sid]);
      DEBUG_PRINTLN("]");
	  } else {
      pd.scheduled_start_time[sid] = accumulate_time;
      pd.scheduled_stop_time[sid] = accumulate_time + pd.scheduled_stop_time[sid];
  	}
    os.status.program_busy = 1;  // set program busy bit
	}
}

void reset_all_stations_immediate() {
  os.clear_all_station_bits();
  os.apply_all_station_bits();
  pd.reset_runtime();
}

void reset_all_stations() {
  // stop all running and scheduled stations
  unsigned long curr_time = now();
  for(byte sid=0;sid<os.nstations;sid++) {
    if(pd.scheduled_program_index[sid] > 0) {
      pd.scheduled_stop_time[sid] = curr_time;  
    }
  }
}


// Manually start a program
// If pid==0, this is a test program (1 minute per station)
// If pid==255, this is a short test program (2 second per station)
// If pid > 0. run program pid-1
void manual_start_program(byte pid) {
  boolean match_found = false;
  reset_all_stations_immediate();
  ProgramStruct prog;
  uint16_t dur;
  byte sid, bid, s;
  if ((pid>0)&&(pid<255)) {
    pd.read(pid-1, &prog);
  }
  for(sid=0;sid<os.nstations;sid++) {
    bid=sid>>3;
    s=sid&0x07;
    dur = 60;
    if(pid==255)  dur=2;
    else if(pid>0)
      dur = water_time_decode(prog.durations[sid]);
    if (dur>0 && !(os.stndis_bits[bid]&(1<<s))) {
      pd.scheduled_stop_time[sid] = dur;
      pd.scheduled_program_index[sid] = 254;      
      match_found = true;
    }
  }
  if(match_found) {
    schedule_all_stations(now(), os.status.seq);
  }  
}

// ================================
// ====== LOGGING FUNCTIONS =======
// ================================
// Log files will be named /logs/xxxxx.txt
const char LOG_PREFIX[] = "/logs/";
void make_logfile_name(char *name) {
  sd.chdir("/");
  String str = LOG_PREFIX;
  str += name;
  str += ".txt";
  str.toCharArray(tmp_buffer, TMP_BUFFER_SIZE);
}

// delete log file
// if name is 'all', delete all logs
void delete_log(char *name) {
  if (!os.status.has_sd) return;

  if (strncmp(name, "all", 3) == 0) {
    // delete the log folder
    SdFile file;

    if (sd.chdir(LOG_PREFIX)) {
      sd.vwd()->rmRfStar();
    }
    return;
  } else {
    make_logfile_name(name);
    if (!sd.exists(tmp_buffer))  return;
    sd.remove(tmp_buffer);
  }
}

// write run record to log on SD card
void write_log(byte type, unsigned long curr_time) {
  if (!os.status.has_sd)  return;

  // file name will be logs/xxxxx.tx where xxxxx is the day in epoch time
  ultoa(curr_time / 86400, tmp_buffer, 10);
  make_logfile_name(tmp_buffer);

  sd.chdir("/");
  if (sd.chdir(LOG_PREFIX) == false) {
    // create dir if it doesn't exist yet
    if (sd.mkdir(LOG_PREFIX) == false)
      return;    
  }

  SdFile file;
  file.open(tmp_buffer, O_CREAT | O_WRITE );
  file.seekEnd();
  String str;
  str = "[";

  switch(type) {
  case LOGDATA_STATION:
    str += pd.lastrun.program;
    str += ",";
    str += pd.lastrun.station;
    str += ",";
    str += pd.lastrun.duration;
    break;
  case LOGDATA_RAINSENSE:
    str += "0,\"rs\",";
    str += (curr_time - os.rainsense_start_time);
    break;
  case LOGDATA_RAINDELAY:
    str += "0,\"rd\",";
    str += (curr_time - os.raindelay_start_time);
    break;
  }
  str += ",";
  str += curr_time;  
  str += "]";
  str += "\r\n";
  str.toCharArray(tmp_buffer, TMP_BUFFER_SIZE);

  file.write(tmp_buffer);
  file.close();
}
