#include "scheduler.h"

#include "program.h"
#include "../OpenSprinkler.h"
#include "../notifier.h"
#include "../sensors/sensor.h"
#include "../storage/logging.h"
#include "../weather.h"

extern OpenSprinkler os;
extern ProgramData pd;
extern NotifQueue notif;
extern uint32_t flow_begin;
extern uint32_t flow_start;
extern uint32_t flow_stop;
extern uint32_t flow_gallons;
extern uint32_t flow_count;
extern float flow_last_gpm;

namespace {

// After removing q, update remaining stations in its sequential group.
void shift_remaining_stations(RuntimeQueueStruct* q, unsigned char gid, time_os_t curr_time) {
	if (gid >= NUM_SEQ_GROUPS) return;

	RuntimeQueueStruct *s = pd.queue;
	time_os_t q_end_time = q->st + q->dur;
	uint32_t remainder = 0;

	if (q_end_time > curr_time) {
		remainder = (q->st < curr_time) ? q_end_time - curr_time : q->dur;
		for ( ; s < pd.queue + pd.nqueue; s++) {
			if (s == q || os.get_station_gid(s->sid) != gid || !os.is_sequential_station(s->sid)) {
				continue;
			}
			if (s->st >= q_end_time) {
				s->st -= remainder;
				s->deque_time -= remainder;
			}
		}
	}
	pd.last_seq_stop_times[gid] -= remainder;
	pd.last_seq_stop_times[gid] += 1;
}

void apply_master_adjustments(time_os_t curr_time, RuntimeQueueStruct *q, unsigned char gid, uint32_t *seq_start_times) {
	int16_t start_adj = 0;
	int16_t dequeue_adj = 0;

	for (unsigned char mas = MASTER_1; mas < NUM_MASTER_ZONES; mas++) {
		unsigned char masid = os.masters[mas][MASOPT_SID];
		if (masid && os.bound_to_master(q->sid, mas)) {
			int16_t mas_on_adj = os.get_on_adj(mas);
			int16_t mas_off_adj = os.get_off_adj(mas);
			start_adj = min(start_adj, mas_on_adj);
			dequeue_adj = max(dequeue_adj, mas_off_adj);
		}
	}

	if (q->st - curr_time <= abs(start_adj)) {
		q->st += abs(start_adj);
		if (os.is_sequential_station(q->sid)) {
			seq_start_times[gid] += abs(start_adj);
		}
	}

	q->deque_time = q->st + q->dur + dequeue_adj;
}

} // namespace

void turn_on_station(unsigned char sid, uint32_t duration) {
	flow_start = 0;
	flow_gallons = 0;

	if (os.set_station_bit(sid, 1, duration)) {
		notif.add(NOTIFY_STATION_ON, sid, duration);
	}
}

void turn_off_running_station_immediate(unsigned char sid, time_os_t curr_time, unsigned char shift) {
	os.set_station_bit(sid, 0);
	os.apply_all_station_bits();

	unsigned char qid = pd.station_qid[sid];
	RuntimeQueueStruct *q = pd.queue + qid;
	unsigned char gid = os.get_station_gid(q->sid);
	bool sequential = os.is_sequential_station(sid) && !os.iopts[IOPT_REMOTE_EXT_MODE];

	if (shift && sequential) {
		shift_remaining_stations(q, gid, curr_time);
	}

	int16_t station_delay = water_time_decode_signed(os.iopts[IOPT_STATION_DELAY_TIME]);
	if (sequential && q->st + q->dur + station_delay == pd.last_seq_stop_times[gid]) {
		pd.last_seq_stop_times[gid] = 0;
	}
	pd.dequeue(qid);
	pd.station_qid[sid] = 0xFF;
}

void turn_off_station(unsigned char sid, time_os_t curr_time, unsigned char shift) {
	unsigned char qid = pd.station_qid[sid];
	if (qid >= pd.nqueue) {
		return;
	}
	RuntimeQueueStruct *q = pd.queue + qid;
	unsigned char force_dequeue = 0;
	unsigned char station_bit = os.is_running(sid);
	unsigned char gid = os.get_station_gid(q->sid);
	bool sequential = os.is_sequential_station(sid) && !os.iopts[IOPT_REMOTE_EXT_MODE];

	if (shift && sequential) {
		shift_remaining_stations(q, gid, curr_time);
	}

	if (curr_time >= q->deque_time) {
		if (station_bit) {
			force_dequeue = 1;
		} else {
			pd.dequeue(qid);
			pd.station_qid[sid] = 0xFF;
			return;
		}
	} else if (curr_time >= q->st + q->dur) {
		if (!station_bit) return;
	}

	#if defined(ESP8266)
	int16_t current = (int16_t)os.read_current(true);
	int16_t imin = os.get_imin();
	if((current < imin) && (os.hw_type==HW_TYPE_AC || os.hw_type==HW_TYPE_DC)) {
		notif.add(NOTIFY_CURR_ALERT, sid, current, CURR_ALERT_TYPE_UNDER);
	}
	#endif

	os.set_station_bit(sid, 0);

	if (flow_gallons > 1) {
		if(flow_stop <= flow_begin) flow_last_gpm = 0;
		else flow_last_gpm = (float)60000 / (float)((flow_stop-flow_begin) / (flow_gallons - 1));
	} else {
		flow_last_gpm = 0;
	}

	if (curr_time >= q->st) {
		if (!os.is_master_station(sid)) {
			pd.lastrun.station = sid;
			pd.lastrun.program = q->pid;
			pd.lastrun.duration = curr_time - q->st;
			pd.lastrun.endtime = curr_time;

			write_log(LOGDATA_STATION, curr_time);
			notif.add(NOTIFY_STATION_OFF, sid, pd.lastrun.duration);
			notif.add(NOTIFY_FLOW_ALERT, sid, pd.lastrun.duration);
		}
	}

	int16_t station_delay = water_time_decode_signed(os.iopts[IOPT_STATION_DELAY_TIME]);
	if (sequential && q->st + q->dur + station_delay == pd.last_seq_stop_times[gid]) {
		pd.last_seq_stop_times[gid] = 0;
	}

	if (force_dequeue) {
		pd.dequeue(qid);
		pd.station_qid[sid] = 0xFF;
	}
}

void process_dynamic_events(time_os_t curr_time) {
	bool rd = os.status.rain_delayed;
	bool en = os.status.enabled;

	bool sn[NUM_SENSORS];
	for (uint8_t i = 0; i < NUM_SENSORS; i++) {
		uint8_t type = os.iopts[sensor_iopt_keys[i].type];
		sn[i] = sensor_available(i) && (type == SENSOR_TYPE_RAIN || type == SENSOR_TYPE_SOIL) && os.sn_sensors[i].active;
	}

	unsigned char sid, s, bid, qid, igrd;
	for(bid=0;bid<os.nboards;bid++) {
		igrd = os.attrib_igrd[bid];
		for(s=0;s<8;s++) {
			sid=bid*8+s;
			if (os.is_master_station(sid)) continue;
			qid = pd.station_qid[sid];
			if(qid==255) continue;
			RuntimeQueueStruct *q = pd.queue + qid;

			if(q->pid>=MANUAL_PID) continue;
			if(!en) {
				q->deque_time=curr_time;
				turn_off_station(sid, curr_time);
			}
			if(rd && !(igrd&(1<<s))) {
				q->deque_time=curr_time;
				turn_off_station(sid, curr_time);
			}
			for (uint8_t i = 0; i < NUM_SENSORS; i++) {
				if (sn[i] && !(os.attrib_igs[i][bid] & (1<<s))) {
					q->deque_time = curr_time;
					turn_off_station(sid, curr_time);
				}
			}
		}
	}
}

void schedule_all_stations(time_os_t curr_time, unsigned char qo) {
	uint32_t con_start_time = curr_time;
	if (os.status.pause_state) {
		con_start_time += os.pause_timer;
	}
	int16_t station_delay = water_time_decode_signed(os.iopts[IOPT_STATION_DELAY_TIME]);
	unsigned char re = os.iopts[IOPT_REMOTE_EXT_MODE];

	RuntimeQueueStruct *q = NULL;
	unsigned char gid;
	unsigned char stagger[NUM_SEQ_GROUPS];
	memset(stagger, 0, NUM_SEQ_GROUPS);
	for(q=pd.queue;q<pd.queue+pd.nqueue;q++) {
		if(q->st || (!q->dur)) continue;
		if (!os.is_sequential_station(q->sid) || re) continue;
		gid = os.get_station_gid(q->sid);
		stagger[gid] = 1;
	}
	for(unsigned char i=1;i<NUM_SEQ_GROUPS;i++) {
		stagger[i] += stagger[i-1];
	}

	uint32_t seq_start_times[NUM_SEQ_GROUPS];
	uint32_t seq_adjustments[NUM_SEQ_GROUPS];
	memset(seq_adjustments, 0, sizeof(seq_adjustments));

	if (qo>0) {
		for(q=pd.queue;q<pd.queue+pd.nqueue;q++) {
			if(q->st) continue;
			if(!q->dur) continue;

			gid = os.get_station_gid(q->sid);
			if (os.is_sequential_station(q->sid) && !re) {
				seq_adjustments[gid] += q->dur + station_delay;
			}
		}

		for(q=pd.queue;q<pd.queue+pd.nqueue;q++) {
			if(!q->st) continue;
			if(!q->dur) continue;
			if (!os.is_sequential_station(q->sid) || re) continue;

			gid = os.get_station_gid(q->sid);
			uint32_t adjustment = seq_adjustments[gid] + stagger[gid];
			if (adjustment == 0) continue;

			if (curr_time >= q->st && curr_time < q->st + q->dur) {
				turn_off_station(q->sid, curr_time);
				uint32_t remaining = q->dur - (curr_time - q->st);
				q->st = curr_time + adjustment;
				q->dur = remaining;
				q->deque_time += adjustment;
			} else if (curr_time < q->st) {
				q->st += adjustment;
				q->deque_time += adjustment;
			}
			if (q->st + q->dur > pd.last_seq_stop_times[gid]) {
				pd.last_seq_stop_times[gid] = q->st + q->dur;
			}
		}

		for(unsigned char i=0;i<NUM_SEQ_GROUPS;i++) {
			seq_start_times[i] = con_start_time + stagger[i];
		}
	} else {
		for(unsigned char i=0;i<NUM_SEQ_GROUPS;i++) {
			seq_start_times[i] = con_start_time + stagger[i];
			if (pd.last_seq_stop_times[i] > curr_time) {
				seq_start_times[i] = pd.last_seq_stop_times[i] + station_delay;
			}
		}
	}

	con_start_time += (stagger[NUM_SEQ_GROUPS-1] + 1);

	for(q=pd.queue;q<pd.queue+pd.nqueue;q++) {
		if(q->st) continue;
		if(!q->dur) continue;
		gid = os.get_station_gid(q->sid);

		if (os.is_sequential_station(q->sid) && !re) {
			q->st = seq_start_times[gid];
			seq_start_times[gid] += q->dur;
			seq_start_times[gid] += station_delay;
		} else {
			q->st = con_start_time;
			con_start_time+=1;
		}

		apply_master_adjustments(curr_time, q, gid, seq_start_times);

		if (!os.status.program_busy) {
			os.status.program_busy = 1;
			if(os.iopts[IOPT_SENSOR1_TYPE] == SENSOR_TYPE_FLOW) {
				os.flowcount_log_start = flow_count;
				os.sn_sensors[0].active_lasttime = curr_time;
			}
		}
	}

	#if defined(ENABLE_DEBUG)
	DEBUG_PRINTLN("queue:");
	for(q=pd.queue;q<pd.queue+pd.nqueue;q++) {
		DEBUG_PRINT("[");
		DEBUG_PRINT(q->sid);
		DEBUG_PRINT(",");
		DEBUG_PRINT(q->dur);
		DEBUG_PRINT(",");
		DEBUG_PRINT(q->st);
		DEBUG_PRINT("(");
		DEBUG_PRINT(hour(q->st));
		DEBUG_PRINT(":");
		DEBUG_PRINT(minute(q->st));
		DEBUG_PRINT(":");
		DEBUG_PRINT(second(q->st));
		DEBUG_PRINTLN(")]");
	}
	DEBUG_PRINTLN("");
	#endif
}

void reset_all_stations_immediate(bool running_ones_only) {
	if(running_ones_only) {
		RuntimeQueueStruct *q = NULL;
		time_os_t currtime = os.now_tz();
		for(q=pd.queue;q<pd.queue+pd.nqueue;q++) {
			unsigned char sid = q->sid;
			if(os.is_running(sid)) {
				q->deque_time = currtime;
				os.set_station_bit(sid, 0);
			}
			os.apply_all_station_bits();
		}
		for(int qi = pd.nqueue; qi-- > 0;) {
			q = &pd.queue[qi];
			if(q->deque_time == currtime) {
				turn_off_running_station_immediate(q->sid, currtime, 0);
			}
		}
	} else {
		os.clear_all_station_bits();
		os.apply_all_station_bits();
		pd.reset_runtime();
		pd.clear_pause();
	}
}

void reset_all_stations(bool running_ones_only) {
	if(running_ones_only) {
		RuntimeQueueStruct *q;
		time_os_t currtime = os.now_tz();
		for(int qi = pd.nqueue; qi-- > 0;) {
			q = &pd.queue[qi];
			if(os.is_running(q->sid)) {
				q->deque_time = currtime;
				turn_off_station(q->sid, currtime, 0);
			}
		}
	} else {
		RuntimeQueueStruct *q;
		for(q=pd.queue;q<pd.queue+pd.nqueue;q++) {
			q->dur = 0;
		}
	}
}

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
