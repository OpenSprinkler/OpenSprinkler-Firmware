/* OpenSprinkler Unified (AVR/RPI/BBB/LINUX) Firmware
 * Copyright (C) 2015 by Ray Wang (ray@opensprinkler.com)
 *
 * Notifier data structures and functions header file
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

#ifndef _NOTIFIER_H
#define _NOTIFIER_H

#if !defined(ARDUINO) || defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__)
void push_program_schedule(RuntimePgmStruct * pgm);
void push_program_start(RuntimePgmStruct * pgm);
void push_program_stop(RuntimePgmStruct * pgm);
void push_station_schedule(RuntimeQueueStruct * q);
void push_station_open(RuntimeQueueStruct * q);
void push_station_close(RuntimeQueueStruct * q);
void push_weather_update(boolean success); 
void push_waterlevel_update(byte water_level);
void push_flow_update(float volume, ulong duration);
void push_ip_update(ulong ip);
void push_raindelay_start(ulong duration);
void push_raindelay_stop(ulong duration);
void push_rainsensor_on(void);
void push_rainsensor_off(ulong duration);
void push_reboot_complete(void);
#else
#define push_program_schedule(pgm) {};
#define push_program_start(pgm) {};
#define push_program_stop(pgm) {};
#define push_station_schedule(q) {};
#define push_station_open(q) {};
#define push_station_close(q) {};
#define push_weather_update(success) {};
#define push_waterlevel_update(water_level) {};
#define push_flow_update(volume, duration) {};
#define push_ip_update(ip) {};
#define push_raindelay_start(duration) {};
#define push_raindelay_stop(duration) {};
#define push_rainsensor_on() {};
#define push_rainsensor_off(duration) {};
#define push_reboot_complete() {};
#endif
#endif  // _NOTIFIER_H
