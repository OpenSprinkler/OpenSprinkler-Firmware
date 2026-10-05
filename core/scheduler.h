#pragma once

#include "../types.h"

struct ProgramStruct;
class RuntimeQueueStruct;

enum class StationStopReason { Cancelled, Completed, Pause };
enum CycleQueueFlags : uint8_t {
	CYCLE_INITIALIZED = 1, CYCLE_STARTED = 2, CYCLE_TAIL = 4,
	CYCLE_PAUSED = 8, CYCLE_MANUAL_OPT_IN = 16, CYCLE_APPLIED = 32,
	CYCLE_GOVERNED = 64,
};

uint8_t get_program_water_percent(const ProgramStruct &prog);
float get_program_sensor_adj(uint8_t pid);

void turn_on_station(unsigned char sid, uint32_t duration);
void turn_off_station(unsigned char sid, time_os_t curr_time, unsigned char shift=0,
	StationStopReason reason=StationStopReason::Cancelled);
void turn_off_running_station_immediate(unsigned char sid, time_os_t curr_time, unsigned char shift=0);
void schedule_all_stations(time_os_t curr_time, unsigned char qo=0);
void process_dynamic_events(time_os_t curr_time);
void reset_all_stations(bool running_ones_only=false);
void reset_all_stations_immediate(bool running_ones_only=false);
enum class RunAdmission { Accepted, Empty, Full };
void prepare_program_run(const ProgramStruct& program, uint8_t water_percent, float sensor_factor,
	bool skip_short, uint32_t* durations);
RunAdmission scheduler_check_admission(const uint32_t* durations, bool replace);
RunAdmission scheduler_enqueue_run(const uint32_t* durations, const uint8_t* order, uint8_t pid, bool replace);
RunAdmission manual_start_program(unsigned char pid, unsigned char uwt, unsigned char qo, unsigned char usa=0);

bool scheduler_has_cycles();
void scheduler_select_jobs();
void scheduler_tick_stations(time_os_t now);
void scheduler_output_applied(uint8_t sid, time_os_t now);
bool scheduler_queue_expired(const RuntimeQueueStruct& q, time_os_t now);
bool scheduler_master_active(uint8_t master, time_os_t now);
bool scheduler_pause_cycles(time_os_t now);
bool scheduler_resume_cycles(time_os_t now);
void scheduler_enter_remote_extension(time_os_t now);
void scheduler_rebuild_reservations(time_os_t now);
