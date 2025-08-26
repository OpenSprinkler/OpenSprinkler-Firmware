/* OpenSprinkler Unified Firmware
 * Copyright (C) 2015 by Ray Wang (ray@opensprinkler.com)
 *
 * Weather functions header file
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
 * <http://www.gnu.org/licenses/>
 */


#ifndef _WEATHER_H
#define _WEATHER_H

#define WEATHER_UPDATE_SUNRISE  0x01
#define WEATHER_UPDATE_SUNSET   0x02
#define WEATHER_UPDATE_EIP      0x04
#define WEATHER_UPDATE_WL       0x08
#define WEATHER_UPDATE_TZ       0x10
#define WEATHER_UPDATE_RD       0x20

#define MAX_N_MD_SCALES 14 // maximum number of days that can be stored in md_scales array

void GetWeather();

extern char wt_rawData[];
extern int wt_errCode;
extern unsigned char md_scales[]; // multiday watering scales
extern unsigned char md_N; // number of elements in the md_scales array
extern unsigned char mda;
extern unsigned char wt_monthly[];
extern unsigned char wt_restricted;
void parse_wto(char* wto);
void apply_monthly_adjustment(time_os_t curr_time);
#endif  // _WEATHER_H
