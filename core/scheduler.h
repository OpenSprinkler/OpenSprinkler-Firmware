#pragma once

#include "../types.h"

struct ProgramStruct;

uint8_t get_program_water_percent(const ProgramStruct &prog);
float get_program_sensor_adj(uint8_t pid);

void turn_on_station(unsigned char sid, uint32_t duration);
void turn_off_station(unsigned char sid, time_os_t curr_time, unsigned char shift=0);
void turn_off_running_station_immediate(unsigned char sid, time_os_t curr_time, unsigned char shift=0);
void schedule_all_stations(time_os_t curr_time, unsigned char qo=0);
void process_dynamic_events(time_os_t curr_time);
void reset_all_stations(bool running_ones_only=false);
void reset_all_stations_immediate(bool running_ones_only=false);
void manual_start_program(unsigned char pid, unsigned char uwt, unsigned char qo, unsigned char usa=0);
