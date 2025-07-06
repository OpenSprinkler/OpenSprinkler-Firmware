/* OpenSprinkler Unified Firmware
 * Copyright (C) 2015 by Ray Wang (ray@opensprinkler.com)
 *
 * Header file containing declarations of functions defined in main.cpp,
 * but called by other translation units.
 * 
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


#ifndef _MAIN_H
#define _MAIN_H 1

void turn_off_station(unsigned char sid, time_os_t curr_time, unsigned char shift=0);
void schedule_all_stations(time_os_t curr_time);
void process_dynamic_events(time_os_t curr_time);
void reset_all_stations();
void reset_all_stations_immediate();
void delete_log(char *name);
void write_log(unsigned char type, time_os_t curr_time);
void make_logfile_name(char *name);

bool is_notif_enabled(uint16_t type);
uint16_t get_notif_enabled();
void set_notif_enabled(uint16_t notif);

#endif // _MAIN_H
