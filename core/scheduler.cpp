#include "scheduler.h"

#include "program.h"
#include "../OpenSprinkler.h"
#include "../notifier.h"
#include "../sensors/sensor.h"
#include "../weather.h"

extern OpenSprinkler os;
extern ProgramData pd;
extern NotifQueue notif;

uint8_t get_program_water_percent(const ProgramStruct &prog) {
	if (!prog.use_weather) return 100;
	if (wt_restricted > 0) return 0;
	uint8_t wl = os.iopts[IOPT_WATER_PERCENTAGE];
	if (mda == 100 && prog.type == PROGRAM_TYPE_INTERVAL && md_N > 0) {
		wl = ((unsigned int)prog.days[1] - 1 < md_N) ? md_scales[prog.days[1] - 1] : md_scales[md_N - 1];
	}
	return wl;
}

float get_program_sensor_adj(uint8_t pid) {
	SensorAdjustment *adj = SensorAdjustment::read(pid, pd.nprograms);
	if (adj) return adj->get_adjustment_factor(os.sensors);
	return 1.f;
}

void manual_start_program(unsigned char pid, unsigned char uwt, unsigned char qo, unsigned char usa) {
	boolean match_found = false;
	ProgramStruct prog;
	uint32_t dur;
	float sensor_adj = 1.f;
	unsigned char sid, bid, s;
	unsigned char ns = os.nstations;
	unsigned char order[ns];
	// prefill with default order: ascending by index
	for(sid=0;sid<ns;sid++) {
		order[sid] = sid;
	}

	unsigned char wl = 100;
	if ((pid>0)&&(pid<255)) {
		pd.read(pid-1, &prog);
		if(usa) sensor_adj = get_program_sensor_adj(pid-1);
		if(uwt) wl = get_program_water_percent(prog);
		notif.add(NOTIFY_PROGRAM_SCHED, pid-1, wl, 1, sensor_adj);
		// get station ordering from program name
		prog.gen_station_runorder(1, order);
	}

	for(unsigned char oi=0;oi<ns;oi++) {
		sid=order[oi];
		bid=sid>>3;
		s=sid&0x07;
		// skip if the station is a master station (because master cannot be scheduled independently
		if (os.is_master_station(sid))
			continue;
		dur = 60;
		if(pid==255) {
			dur=2;
		} else if (pid>0) {
			dur = water_time_resolve(prog.durations[sid]);
		}

		dur = water_time_scale(dur, wl, sensor_adj);
		if(dur>0 && !(os.attrib_dis[bid]&(1<<s))) {
			RuntimeQueueStruct *q = pd.enqueue();
			if (q) {
				q->st = 0;
				q->dur = dur;
				q->sid = sid;
				q->pid = RUNONCE_PID;
				match_found = true;
			}
		}
	}
	if(match_found) {
		schedule_all_stations(os.now_tz(), qo);
	}
}
