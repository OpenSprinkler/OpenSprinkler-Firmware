#include "scheduler.h"

#include "core/bundle.h"
#include "core/output_sequencer.h"
#include "program.h"
#include "../OpenSprinkler.h"
#include "../services/notifier.h"
#include "../sensors/sensor.h"
#include "sensors/flow_rate.h"
#include "../storage/logging.h"
#include "../services/weather.h"
#include "platform/clock.h"
#include "storage/station_extra.h"

#include <algorithm>
#include <limits>

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

bool governed(const RuntimeQueueStruct& q) { return q.cycle_flags & CYCLE_GOVERNED; }

void prepare_cycle(RuntimeQueueStruct& q, StationExtraReader& policies) {
	if (q.cycle_flags & CYCLE_INITIALIZED) return;
	q.cycle_flags |= CYCLE_INITIALIZED;
	CycleSoakPolicy policy;
	const auto result = policies.load(q.sid, policy);
	if (result != StationExtraResult::Ok && result != StationExtraResult::Missing) {
		pd.cycle_config_error = true;
	}
	if (os.iopts[IOPT_REMOTE_EXT_MODE]) return;
	const bool apply = q.pid != MANUAL_PID || (q.cycle_flags & CYCLE_MANUAL_OPT_IN);
	uint32_t first;
	const uint16_t minimum = bundle_is_station(q.sid) ? bundle_minimum_duration(q.sid) : 0;
	if (!cycle_soak_begin(policy, q.dur, apply, minimum, q.cycle, first)) {
		// Configuration faults do not inhibit ordinary watering.
		pd.cycle_config_error = true;
		q.cycle = {};
		return;
	}
	q.dur = first;
	if (apply && policy.enabled) q.cycle_flags |= CYCLE_GOVERNED;
	// A bypass ignores the deadline itself, but contributes cooldown for later work.
	if (!apply && policy.enabled) q.cycle.soak_seconds = policy.soak_seconds;
}

void remember_off(RuntimeQueueStruct& q, time_os_t now) {
	if (!q.cycle.soak_seconds) return;
	cycle_soak_note_off(q.cycle, now);
	pd.last_off_until[q.sid] = max(pd.last_off_until[q.sid], q.cycle.soak_until);
	pd.cooldown_until[q.sid] = max(pd.cooldown_until[q.sid], q.cycle.soak_until);
}

void record_stop(RuntimeQueueStruct& q, time_os_t now) {
	#if defined(ARDUINO)
	const int16_t current = static_cast<int16_t>(os.read_current(true));
	if (current < os.get_imin() && (os.hw_type == HW_TYPE_AC || os.hw_type == HW_TYPE_DC)) {
		notif.add(NOTIFY_CURR_ALERT, q.sid, current, CURR_ALERT_TYPE_UNDER);
	}
	#endif
	flow_last_gpm = flow_gallons > 1 ?
		flow_rate_from_pulse_span(flow_gallons - 1, flow_stop - flow_begin) : 0.0f;
	const time_os_t start = governed(q) ? q.cycle_on_time : q.st;
	if (now < start || os.is_master_station(q.sid)) return;
	pd.lastrun.station = q.sid;
	pd.lastrun.program = q.pid;
	pd.lastrun.duration = now - start;
	pd.lastrun.endtime = now;
	write_log(LOGDATA_STATION, now);
	notif.add(NOTIFY_STATION_OFF, q.sid, pd.lastrun.duration, 0, flow_last_gpm);
	notif.add(NOTIFY_FLOW_ALERT, q.sid, pd.lastrun.duration, 0, flow_last_gpm);
}

void retain_tail(RuntimeQueueStruct& q) {
	q.deque_time = q.cycle.prev_end;
	for (uint8_t m = 0; m < NUM_MASTER_ZONES; m++) {
		uint32_t end;
		if ((q.cycle_tail_mask & (1 << m)) && bundle_bound_to_master(q.sid, m) &&
			cycle_soak_master_tail(q.cycle.prev_end, os.get_off_adj(m), end)) {
			q.deque_time = max(q.deque_time, end);
		}
	}
}

void capture_tail(RuntimeQueueStruct& q, time_os_t now) {
	q.cycle.prev_start = q.st;
	q.cycle.prev_end = now;
	q.cycle_tail_mask = 0;
	for (uint8_t m = 0; m < NUM_MASTER_ZONES; m++) {
		if (os.masters[m][MASOPT_SID] && bundle_bound_to_master(q.sid, m) &&
			int64_t(now) + os.get_off_adj(m) >= int64_t(q.st) + os.get_on_adj(m)) q.cycle_tail_mask |= 1 << m;
	}
}

time_os_t previous_tail_end(const RuntimeQueueStruct& q) {
	time_os_t end = q.cycle.prev_end;
	for (uint8_t m = 0; m < NUM_MASTER_ZONES; m++) {
		uint32_t deadline;
		if ((q.cycle_tail_mask & (1 << m)) && bundle_bound_to_master(q.sid, m) &&
			cycle_soak_master_tail(q.cycle.prev_end, os.get_off_adj(m), deadline)) end = max(end, deadline);
	}
	return end;
}

bool place_cycle(RuntimeQueueStruct& q, time_os_t now, time_os_t earliest = 0, bool fill_gap = false);

void sort_jobs(uint8_t* order, uint16_t count) {
	std::sort(order, order + count, [](uint8_t a, uint8_t b) {
		return cycle_soak_resume_before(pd.queue[a].st, pd.queue[a].sid, pd.queue[b].st, pd.queue[b].sid);
	});
}

void placement_failed(RuntimeQueueStruct& q) {
	cycle_soak_cancel_future(q.cycle);
	q.dur = 0;
	q.cycle_flags |= CYCLE_TAIL;
	retain_tail(q);
	pd.cycle_config_error = true;
}

void stop_cycle(uint8_t qid, time_os_t now, StationStopReason reason) {
	RuntimeQueueStruct& q = pd.queue[qid];
	if (!q.dur && !(q.cycle_flags & CYCLE_TAIL)) reason = StationStopReason::Cancelled;
	const bool started = q.cycle_flags & CYCLE_STARTED;
	const bool delivered = q.cycle_flags & CYCLE_APPLIED;
	if (started) {
		os.set_station_bit(q.sid, 0);
		os.apply_all_station_bits();
		now = os.now_tz();
		if (delivered) record_stop(q, now);
		remember_off(q, now);
		q.cycle_flags &= ~(CYCLE_STARTED | CYCLE_APPLIED);
		capture_tail(q, now);
	}
	if (reason == StationStopReason::Cancelled) {
		pd.dequeue(qid);
		scheduler_rebuild_reservations(now);
		return;
	}
	if (reason == StationStopReason::Pause) {
		if (delivered) q.dur = now < q.st + q.dur ? q.st + q.dur - now : 0;
		return;
	}
	if (q.cycle_flags & CYCLE_TAIL) return;
	if (os.iopts[IOPT_REMOTE_EXT_MODE]) cycle_soak_cancel_future(q.cycle);
	if (delivered) {
		uint32_t next = 0;
		if (!cycle_soak_finish(q.cycle, now, now, CycleSoakStop::Natural, next)) {
			q.dur = 0;
			q.cycle_flags |= CYCLE_TAIL;
			retain_tail(q);
			scheduler_rebuild_reservations(now);
			return;
		}
		q.dur = next;
	}
	// A completely missed window was never watered: re-place the same segment.
	q.st = 0;
	scheduler_rebuild_reservations(now);
	if (!place_cycle(q, now, 0, delivered)) {
		cycle_soak_cancel_future(q.cycle);
		q.dur = 0;
		q.cycle_flags |= CYCLE_TAIL;
		retain_tail(q);
		pd.cycle_config_error = true;
	}
}

// After removing q, update remaining stations in its sequential group.
void shift_remaining_stations(RuntimeQueueStruct* q, unsigned char gid, time_os_t curr_time) {
	if (gid >= NUM_SEQ_GROUPS) return;
	if (scheduler_has_cycles() || governed(*q)) {
		const time_os_t end = q->st + q->dur;
		if (end <= curr_time) return;
		const uint32_t remainder = q->st < curr_time ? end - curr_time : q->dur;
		uint8_t order[RUNTIME_QUEUE_SIZE];
		uint16_t count = 0;
		const uint8_t flags = q->cycle_flags;
		q->cycle_flags |= CYCLE_PAUSED; // exclude the removed reservation
		for (uint16_t i = 0; i < pd.nqueue; i++) {
			auto& other = pd.queue[i];
			if (&other == q || !other.dur || (other.cycle_flags & (CYCLE_STARTED | CYCLE_TAIL)) ||
				os.get_station_gid(other.sid) != gid || other.st < end) continue;
			order[count++] = i;
			other.cycle_flags |= CYCLE_PAUSED;
		}
		sort_jobs(order, count);
		scheduler_rebuild_reservations(curr_time);
		const time_os_t base = curr_time + (os.status.pause_state ? os.pause_timer : 0);
		for (uint16_t i = 0; i < count; i++) {
			auto& other = pd.queue[order[i]];
			const time_os_t earliest = other.st - remainder;
			other.cycle_flags &= ~CYCLE_PAUSED;
			if (!place_cycle(other, base, earliest)) placement_failed(other);
		}
		if (os.status.pause_state) {
			for (uint16_t i = 0; i < count; i++) pd.queue[order[i]].cycle_flags |= CYCLE_PAUSED;
		}
		q->cycle_flags = flags;
		return;
	}

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

uint16_t master_lead(const RuntimeQueueStruct& q) {
	int16_t start_adj = 0;
	for (uint8_t mas = MASTER_1; mas < NUM_MASTER_ZONES; mas++) {
		if (os.masters[mas][MASOPT_SID] && bundle_bound_to_master(q.sid, mas)) {
			start_adj = min(start_adj, os.get_on_adj(mas));
		}
	}
	return abs(start_adj);
}

void apply_master_adjustments(time_os_t curr_time, RuntimeQueueStruct *q, unsigned char gid, uint32_t *seq_start_times) {
	const uint16_t lead = master_lead(*q);
	int16_t dequeue_adj = 0;

	for (unsigned char mas = MASTER_1; mas < NUM_MASTER_ZONES; mas++) {
		unsigned char masid = os.masters[mas][MASOPT_SID];
		if (masid && bundle_bound_to_master(q->sid, mas)) {
			int16_t mas_off_adj = os.get_off_adj(mas);
			dequeue_adj = max(dequeue_adj, mas_off_adj);
		}
	}

	if (q->st - curr_time <= lead) {
		q->st += lead;
		if (os.is_sequential_station(q->sid) && gid < NUM_SEQ_GROUPS) {
			seq_start_times[gid] += lead;
		}
	}

	q->deque_time = q->st + q->dur + dequeue_adj;
	if (q->cycle.prev_end) {
		const uint32_t current_end = q->deque_time;
		retain_tail(*q);
		q->deque_time = max(q->deque_time, current_end);
	}
}

bool cycle_gap_allowed(const RuntimeQueueStruct& q) {
	for (uint16_t i = 0; i < pd.nqueue; i++) {
		const auto& other = pd.queue[i];
		if (&other != &q && other.sid == q.sid && other.dur && !(other.cycle_flags & CYCLE_TAIL)) return false;
	}
	return true;
}

bool first_cycle_gap(const RuntimeQueueStruct& q, time_os_t now, uint8_t gid,
	uint16_t delay, uint32_t& start) {
	const uint16_t lead = master_lead(q);
	// Advance past conflicts until the whole adjusted window fits. Each advance
	// passes a reservation, so this is bounded by nqueue + 1 scans, without a
	// queue-sized stack buffer. Queue storage order cannot change the result.
	for (uint16_t pass = 0; pass <= pd.nqueue; pass++) {
		const uint64_t adjusted = uint64_t(start) + (start - now <= lead ? lead : 0);
		if (adjusted + q.dur > UINT32_MAX) return false;
		bool conflict = false;
		for (uint16_t i = 0; i < pd.nqueue; i++) {
			const auto& other = pd.queue[i];
			if (&other == &q || !other.st || !other.dur ||
				(other.cycle_flags & (CYCLE_TAIL | CYCLE_PAUSED)) ||
				!os.is_sequential_station(other.sid) || os.get_station_gid(other.sid) != gid) continue;
			const uint64_t after = uint64_t(other.st) + other.dur + delay;
			if (adjusted < after && adjusted + q.dur + delay > other.st) {
				if (after > UINT32_MAX) return false;
				start = static_cast<uint32_t>(after);
				conflict = true;
				break;
			}
		}
		if (!conflict) return true;
	}
	return false;
}

bool place_cycle(RuntimeQueueStruct& q, time_os_t now, time_os_t earliest, bool fill_gap) {
	const uint8_t gid = os.get_station_gid(q.sid);
	const bool sequential = os.is_sequential_station(q.sid) && !os.iopts[IOPT_REMOTE_EXT_MODE];
	const uint32_t tail = sequential ? pd.last_seq_stop_times[gid] : 0;
	const int16_t delay = water_time_decode_signed(os.iopts[IOPT_STATION_DELAY_TIME]);
	const bool gap = fill_gap && governed(q) && sequential && delay >= 0 && cycle_gap_allowed(q);
	if (!cycle_soak_place(now, q.cycle.soak_until,
		governed(q) ? pd.cooldown_until[q.sid] : 0, sequential && !gap, tail, delay, q.st)) return false;
	q.st = max(q.st, earliest);
	// V1 keeps one previous master window. Retire it before another segment
	// can overwrite it, including windows whose ON adjustment is after OFF.
	if (q.cycle.prev_end) {
		uint32_t after_tail;
		if (!cycle_soak_deadline(previous_tail_end(q), 1, after_tail)) return false;
		q.st = max(q.st, after_tail);
	}
	if (gap && !first_cycle_gap(q, now, gid, delay, q.st)) return false;
	uint32_t starts[NUM_SEQ_GROUPS] = {};
	apply_master_adjustments(now, &q, gid, starts);
	if (q.cycle.soak_seconds && !cycle_soak_reserve(q.st, q.dur,
		q.cycle.soak_seconds, pd.cooldown_until[q.sid])) return false;
	if (sequential) pd.last_seq_stop_times[gid] = max(tail, q.st + q.dur);
	return true;
}

} // namespace

void turn_on_station(unsigned char sid, uint32_t duration) {
	flow_start = 0;
	flow_gallons = 0;
	const uint8_t qid = pd.station_qid[sid];
	if (qid < pd.nqueue && governed(pd.queue[qid])) {
		pd.queue[qid].cycle_flags |= CYCLE_STARTED;
	}

	if (os.set_station_bit(sid, 1, duration)) {
		notif.add(NOTIFY_STATION_ON, sid, duration);
	}
	// Network/RF stations acknowledge only the local command, not delivery.
	if (((os.attrib_spe[sid >> 3] & (1 << (sid & 7))) && !bundle_is_station(sid)) ||
		os.get_applied_station_bit(sid)) scheduler_output_applied(sid, os.now_tz());
}

void scheduler_output_applied(uint8_t sid, time_os_t now) {
	const uint8_t qid = pd.station_qid[sid];
	if (qid >= pd.nqueue) return;
	auto& q = pd.queue[qid];
	if (!governed(q) || !(q.cycle_flags & CYCLE_STARTED) || (q.cycle_flags & CYCLE_APPLIED)) return;
	q.cycle_flags |= CYCLE_APPLIED;
	q.cycle_on_time = now;
	if (now > q.st) {
		const uint32_t lag = now - q.st;
		if (!cycle_soak_deadline(q.dur, lag, q.dur) ||
			!cycle_soak_deadline(q.deque_time, lag, q.deque_time)) pd.cycle_config_error = true;
		scheduler_rebuild_reservations(now);
	}
}

void turn_off_running_station_immediate(unsigned char sid, time_os_t curr_time, unsigned char shift) {
	if (pd.station_qid[sid] >= pd.nqueue) return;
	if (governed(pd.queue[pd.station_qid[sid]])) {
		RuntimeQueueStruct stopped = pd.queue[pd.station_qid[sid]];
		stop_cycle(pd.station_qid[sid], curr_time, StationStopReason::Cancelled);
		if (shift && os.is_sequential_station(sid) && !os.iopts[IOPT_REMOTE_EXT_MODE]) {
			shift_remaining_stations(&stopped, os.get_station_gid(sid), curr_time);
		}
		os.apply_all_station_bits();
		return;
	}
	if (os.is_running(sid)) remember_off(pd.queue[pd.station_qid[sid]], curr_time);
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

void turn_off_station(unsigned char sid, time_os_t curr_time, unsigned char shift, StationStopReason reason) {
	unsigned char qid = pd.station_qid[sid];
	if (qid >= pd.nqueue) {
		return;
	}
	RuntimeQueueStruct *q = pd.queue + qid;
	if (governed(*q)) {
		RuntimeQueueStruct stopped = *q;
		stop_cycle(qid, curr_time, reason);
		if (shift && reason == StationStopReason::Cancelled && os.is_sequential_station(sid) &&
			!os.iopts[IOPT_REMOTE_EXT_MODE]) shift_remaining_stations(&stopped, os.get_station_gid(sid), curr_time);
		return;
	}
	unsigned char force_dequeue = 0;
	unsigned char station_bit = os.is_running(sid);
	unsigned char gid = os.get_station_gid(q->sid);
	bool sequential = os.is_sequential_station(sid) && !os.iopts[IOPT_REMOTE_EXT_MODE];

	if (shift && sequential) {
		if (station_bit) remember_off(*q, curr_time);
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

	#if defined(ARDUINO)
	int16_t current = (int16_t)os.read_current(true);
	int16_t imin = os.get_imin();
	if((current < imin) && (os.hw_type==HW_TYPE_AC || os.hw_type==HW_TYPE_DC)) {
		notif.add(NOTIFY_CURR_ALERT, sid, current, CURR_ALERT_TYPE_UNDER);
	}
	#endif

	os.set_station_bit(sid, 0);
	if (station_bit) remember_off(*q, curr_time);
	if (station_bit && reason == StationStopReason::Pause && scheduler_has_cycles()) capture_tail(*q, curr_time);

	// flow_gallons includes one settling-period sentinel; the rest are measured pulses.
	flow_last_gpm = flow_gallons > 1 ?
		flow_rate_from_pulse_span(flow_gallons - 1, flow_stop - flow_begin) : 0.0f;

	if (curr_time >= q->st) {
		if (!os.is_master_station(sid)) {
			pd.lastrun.station = sid;
			pd.lastrun.program = q->pid;
			pd.lastrun.duration = curr_time - q->st;
			pd.lastrun.endtime = curr_time;

			write_log(LOGDATA_STATION, curr_time);
			notif.add(NOTIFY_STATION_OFF, sid, pd.lastrun.duration, 0, flow_last_gpm);
			notif.add(NOTIFY_FLOW_ALERT, sid, pd.lastrun.duration, 0, flow_last_gpm);
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
	// Defensive: RE clears existing work and never admits scheduled starts.
	if (os.iopts[IOPT_REMOTE_EXT_MODE]) return;
	bool rd = os.status.rain_delayed;
	bool en = os.status.enabled;

	bool sn[NUM_SENSORS];
	for (uint8_t i = 0; i < NUM_SENSORS; i++) {
		uint8_t type = os.iopts[sensor_iopt_keys[i].type];
		sn[i] = sensor_available(i) && (type == SENSOR_TYPE_RAIN || type == SENSOR_TYPE_SOIL) && os.sn_sensors[i].active;
	}
	// Cancel every governed request, not just the currently selected job for a SID.
	for (int qi = pd.nqueue; qi-- > 0;) {
		RuntimeQueueStruct& q = pd.queue[qi];
		if (!governed(q) || q.pid >= MANUAL_PID) continue;
		const uint8_t b = q.sid >> 3, bit = 1 << (q.sid & 7);
		bool cancel = !en || (rd && !(os.attrib_igrd[b] & bit));
		for (uint8_t i = 0; i < NUM_SENSORS; i++) {
			cancel |= sn[i] && !(os.attrib_igs[i][b] & bit);
		}
		if (cancel) stop_cycle(qi, curr_time, StationStopReason::Cancelled);
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
			if (governed(*q)) continue;

			if(q->pid>=MANUAL_PID) continue;
			bool cancel = !en || (rd && !(igrd & (1 << s)));
			for (uint8_t i = 0; i < NUM_SENSORS; i++) {
				cancel |= sn[i] && !(os.attrib_igs[i][bid] & (1 << s));
			}
			if (cancel) {
				q->deque_time = curr_time;
				turn_off_station(sid, curr_time);
			}
		}
	}
}

void schedule_all_stations(time_os_t curr_time, unsigned char qo) {
	for (uint16_t i = 0; i < pd.nqueue; i++) {
		if (pd.queue[i].st || !pd.queue[i].dur || (pd.queue[i].cycle_flags & CYCLE_INITIALIZED)) continue;
		// Open only when there is new work, and close before scheduling can
		// interrupt running outputs and send special-station commands.
		StationExtraReader policies(MAX_NUM_STATIONS);
		for (; i < pd.nqueue; i++) {
			if (!pd.queue[i].st && pd.queue[i].dur) prepare_cycle(pd.queue[i], policies);
		}
		break;
	}
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
	unsigned char active_group_count = 0;
	for(unsigned char i=0;i<NUM_SEQ_GROUPS;i++) {
		if (stagger[i]) stagger[i] = 1 + active_group_count++ / OUTPUT_RISES_PER_SECOND;
	}

	// Bundle members are physically staggered after their leader. Extend short
	// runs before any queue calculations consume q->dur.
	for(q=pd.queue;q<pd.queue+pd.nqueue;q++) {
		if(q->st || !q->dur || !bundle_is_station(q->sid)) continue;
		const uint16_t minimum = bundle_minimum_duration(q->sid);
		if (q->dur < minimum) q->dur = minimum;
	}

	uint32_t seq_start_times[NUM_SEQ_GROUPS];
	uint32_t seq_adjustments[NUM_SEQ_GROUPS];
	memset(seq_adjustments, 0, sizeof(seq_adjustments));
	bool reappend_group[NUM_SEQ_GROUPS] = {};
	uint8_t interrupted[RUNTIME_QUEUE_SIZE];
	uint16_t interrupted_count = 0;
	if (qo > 0 && !re && scheduler_has_cycles()) {
		for (q = pd.queue; q < pd.queue + pd.nqueue; q++) {
			if (!q->dur || !os.is_sequential_station(q->sid)) continue;
			gid = os.get_station_gid(q->sid);
			if (stagger[gid] && governed(*q)) reappend_group[gid] = true;
		}
		scheduler_select_jobs();
		for (uint16_t i = 0; i < pd.nqueue; i++) {
			auto& old = pd.queue[i];
			gid = os.get_station_gid(old.sid);
			if (gid >= NUM_SEQ_GROUPS || !reappend_group[gid] || !old.st || !old.dur ||
				(old.cycle_flags & CYCLE_TAIL)) continue;
			if (!governed(old) && !(old.cycle_flags & CYCLE_PAUSED) && curr_time >= old.st + old.dur) continue;
			// Only actual ON jobs are interrupted. Waiting segments have no OFF/log.
			if (pd.station_qid[old.sid] == i && os.is_running(old.sid)) {
				if (curr_time >= old.st + old.dur) continue; // normal completion pass owns it
				const uint32_t remaining = governed(old) && !(old.cycle_flags & CYCLE_APPLIED) ?
					old.dur : old.st + old.dur - curr_time;
				turn_off_station(old.sid, curr_time, 0, StationStopReason::Pause);
				old.dur = remaining;
				if (bundle_is_station(old.sid)) old.dur = max(old.dur, uint32_t(bundle_minimum_duration(old.sid)));
			}
			interrupted[interrupted_count++] = i;
			old.cycle_flags |= CYCLE_PAUSED;
		}
		sort_jobs(interrupted, interrupted_count);
		// Remove old reservations before placing the inserted invocation.
		scheduler_rebuild_reservations(curr_time);
	}

	if (qo>0) {
		for(q=pd.queue;q<pd.queue+pd.nqueue;q++) {
			if(q->st) continue;
			if(!q->dur) continue;

			gid = os.get_station_gid(q->sid);
			if (os.is_sequential_station(q->sid) && !re && !reappend_group[gid]) {
				seq_adjustments[gid] += q->dur + station_delay;
			}
		}

		uint32_t resume_extensions[NUM_SEQ_GROUPS] = {};
		for(q=pd.queue;q<pd.queue+pd.nqueue;q++) {
			if(!q->st || !q->dur || !os.is_sequential_station(q->sid) || re) continue;
			if(curr_time < q->st || curr_time >= q->st + q->dur || !bundle_is_station(q->sid)) continue;
			gid = os.get_station_gid(q->sid);
			if (reappend_group[gid]) continue;
			const uint32_t remaining = q->dur - (curr_time - q->st);
			const uint16_t minimum = bundle_minimum_duration(q->sid);
			if (remaining < minimum) resume_extensions[gid] = minimum - remaining;
		}

		for(q=pd.queue;q<pd.queue+pd.nqueue;q++) {
			if(!q->st) continue;
			if(!q->dur) continue;
			if (!os.is_sequential_station(q->sid) || re) continue;

			gid = os.get_station_gid(q->sid);
			if (reappend_group[gid]) continue;
			uint32_t adjustment = seq_adjustments[gid] + stagger[gid];
			if (adjustment == 0) continue;
			uint32_t resume_extension = resume_extensions[gid];

			if (curr_time >= q->st && curr_time < q->st + q->dur) {
				uint32_t remaining = q->dur - (curr_time - q->st);
				turn_off_station(q->sid, curr_time, 0, StationStopReason::Pause);
				q->st = curr_time + adjustment;
				q->dur = remaining + resume_extension;
				q->deque_time += adjustment + resume_extension;
			} else if (curr_time < q->st) {
				q->st += adjustment + resume_extension;
				q->deque_time += adjustment + resume_extension;
			}
			if (q->st + q->dur > pd.last_seq_stop_times[gid]) {
				pd.last_seq_stop_times[gid] = q->st + q->dur;
			}
		}

		for(unsigned char i=0;i<NUM_SEQ_GROUPS;i++) {
			seq_start_times[i] = con_start_time + (stagger[i] ? stagger[i] : 1);
		}
	} else {
		for(unsigned char i=0;i<NUM_SEQ_GROUPS;i++) {
			seq_start_times[i] = con_start_time + (stagger[i] ? stagger[i] : 1);
			if (pd.last_seq_stop_times[i] > curr_time) {
				seq_start_times[i] = pd.last_seq_stop_times[i] + station_delay;
			}
		}
	}

	unsigned char parallel_slot = active_group_count;

	for(q=pd.queue;q<pd.queue+pd.nqueue;q++) {
		if(q->st) continue;
		if(!q->dur) continue;
		gid = os.get_station_gid(q->sid);

		if (os.is_sequential_station(q->sid) && !re) {
			q->st = seq_start_times[gid];
			if (governed(*q)) q->st = max(q->st, max(curr_time + 1,
				max(q->cycle.soak_until, pd.cooldown_until[q->sid])));
			seq_start_times[gid] = q->st;
			seq_start_times[gid] += q->dur;
			seq_start_times[gid] += station_delay;
		} else {
			q->st = con_start_time + 1 + parallel_slot / OUTPUT_RISES_PER_SECOND;
			if (governed(*q)) q->st = max(q->st, max(q->cycle.soak_until, pd.cooldown_until[q->sid]));
			if (q->dur == 1 && (parallel_slot % OUTPUT_RISES_PER_SECOND)) q->dur = 2;
			parallel_slot++;
		}

		apply_master_adjustments(curr_time, q, gid, seq_start_times);
		if (q->cycle.soak_seconds && !cycle_soak_reserve(q->st, q->dur,
			q->cycle.soak_seconds, pd.cooldown_until[q->sid])) pd.cycle_config_error = true;

		if (!os.status.program_busy) {
			os.status.program_busy = 1;
			if(os.iopts[IOPT_SENSOR1_TYPE] == SENSOR_TYPE_FLOW) {
				os.flowcount_log_start = flow_count;
				os.sn_sensors[0].active_lasttime = curr_time;
			}
		}
	}
	scheduler_rebuild_reservations(curr_time);
	for (uint16_t i = 0; i < interrupted_count; i++) {
		auto& old = pd.queue[interrupted[i]];
		old.cycle_flags &= ~CYCLE_PAUSED;
		if (!place_cycle(old, con_start_time)) placement_failed(old);
	}
	if (os.status.pause_state && scheduler_has_cycles()) {
		// Include work admitted during a pause in the same ordered resume pass.
		for (uint16_t i = 0; i < pd.nqueue; i++) {
			if (pd.queue[i].dur && !(pd.queue[i].cycle_flags & CYCLE_TAIL)) pd.queue[i].cycle_flags |= CYCLE_PAUSED;
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

bool scheduler_has_cycles() {
	for (uint16_t i = 0; i < pd.nqueue; i++) {
		if (governed(pd.queue[i]) || (pd.queue[i].cycle_flags & CYCLE_PAUSED)) return true;
	}
	return false;
}

void scheduler_rebuild_reservations(time_os_t now) {
	memcpy(pd.cooldown_until, pd.last_off_until, sizeof(time_os_t) * MAX_NUM_STATIONS);
	memset(pd.last_seq_stop_times, 0, sizeof(time_os_t) * NUM_SEQ_GROUPS);
	for (uint16_t i = 0; i < pd.nqueue; i++) {
		const auto& q = pd.queue[i];
		if (!q.st || !q.dur || (q.cycle_flags & (CYCLE_TAIL | CYCLE_PAUSED))) continue;
		if (q.cycle.soak_seconds) cycle_soak_reserve(q.st, q.dur, q.cycle.soak_seconds, pd.cooldown_until[q.sid]);
		if (q.st + q.dur > now && os.is_sequential_station(q.sid) && !os.iopts[IOPT_REMOTE_EXT_MODE]) {
			const uint8_t gid = os.get_station_gid(q.sid);
			pd.last_seq_stop_times[gid] = max(pd.last_seq_stop_times[gid], q.st + q.dur);
		}
	}
}

void scheduler_select_jobs() {
	memset(pd.station_qid, 0xff, MAX_NUM_STATIONS);
	for (uint16_t i = 0; i < pd.nqueue; i++) {
		const auto& q = pd.queue[i];
		// Newly admitted duplicates cannot own an output before placement.
		if (!q.st) continue;
		if (q.cycle_flags & (CYCLE_TAIL | CYCLE_PAUSED)) continue;
		const uint8_t old = pd.station_qid[q.sid];
		if (old < pd.nqueue) {
			const auto& previous = pd.queue[old];
			if (previous.cycle_flags & CYCLE_STARTED) continue;
			if (!(q.cycle_flags & CYCLE_STARTED) && previous.st < q.st) continue;
		}
		pd.station_qid[q.sid] = i;
	}
}

void scheduler_tick_stations(time_os_t now) {
	// A cancellation received at the completion boundary must win over re-arm.
	process_dynamic_events(now);
	scheduler_select_jobs();
	if (os.status.pause_state) return;
	for (uint16_t sid = 0; sid < os.nstations; sid++) {
		if (os.is_master_station(sid)) continue;
		const uint8_t qid = pd.station_qid[sid];
		if (qid >= pd.nqueue) continue;
		auto& q = pd.queue[qid];
		if (governed(q) && !(q.cycle_flags & CYCLE_STARTED) && q.cycle.prev_end &&
			now >= q.st && now <= previous_tail_end(q)) {
			// A master-offset edit must not overwrite an outstanding old window.
			if (!place_cycle(q, now)) stop_cycle(qid, now, StationStopReason::Cancelled);
			continue;
		}
		if (governed(q) && (q.cycle_flags & CYCLE_STARTED) && !(q.cycle_flags & CYCLE_APPLIED)) continue;
		if (governed(q) && !(q.cycle_flags & CYCLE_STARTED) && q.dur && now >= q.st + q.dur) {
			stop_cycle(qid, now, StationStopReason::Completed);
			continue;
		}
		if (!os.is_running(sid) && now >= q.st && now < q.st + q.dur) {
			// The applied-output hook extends a late cycling segment by its start
			// lag; remote stations must receive that full segment duration too.
			turn_on_station(sid, governed(q) ? q.dur : q.st + q.dur - now);
		}
		if (q.st && now >= q.st + q.dur) {
			turn_off_station(sid, now, 0, StationStopReason::Completed);
		}
	}
}

bool scheduler_queue_expired(const RuntimeQueueStruct& q, time_os_t now) {
	if (!governed(q)) return (!q.dur || now >= q.deque_time) &&
		(!q.cycle.prev_end || now > previous_tail_end(q));
	if (q.cycle_flags & CYCLE_TAIL) {
		return now > previous_tail_end(q);
	}
	return !q.dur && !(q.cycle_flags & CYCLE_PAUSED);
}

bool scheduler_master_active(uint8_t master, time_os_t now) {
	for (uint16_t i = 0; i < pd.nqueue; i++) {
		const auto& q = pd.queue[i];
		if (!bundle_bound_to_master(q.sid, master) || os.is_master_station(q.sid)) continue;
		uint32_t end;
		if ((q.cycle_tail_mask & (1 << master)) &&
			cycle_soak_master_tail(q.cycle.prev_end, os.get_off_adj(master), end) && now <= end &&
			int64_t(now) >= int64_t(q.cycle.prev_start) + os.get_on_adj(master)) return true;
		if (!q.st || !q.dur || os.status.pause_state || (q.cycle_flags & (CYCLE_TAIL | CYCLE_PAUSED))) continue;
		if (int64_t(now) >= int64_t(q.st) + os.get_on_adj(master) &&
			int64_t(now) <= int64_t(q.st) + q.dur + os.get_off_adj(master)) return true;
	}
	return false;
}

bool scheduler_pause_cycles(time_os_t now) {
	if (!scheduler_has_cycles()) return false;
	scheduler_select_jobs();
	for (uint16_t i = 0; i < pd.nqueue; i++) {
		auto& q = pd.queue[i];
		if (q.cycle_flags & (CYCLE_TAIL | CYCLE_PAUSED)) continue;
		if (governed(q) && (q.cycle_flags & CYCLE_STARTED)) {
			if (now >= q.st + q.dur) stop_cycle(i, now, StationStopReason::Completed);
			else stop_cycle(i, now, StationStopReason::Pause);
		} else if (!governed(q) && pd.station_qid[q.sid] == i && os.is_running(q.sid) && now < q.st + q.dur) {
			turn_off_station(q.sid, now, 0, StationStopReason::Pause);
			q.dur -= now - q.st;
		}
		if (q.cycle_flags & CYCLE_TAIL) continue;
		q.st = max(q.st, now) + os.pause_timer;
		q.deque_time = q.st + q.dur;
		q.cycle_flags |= CYCLE_PAUSED;
	}
	scheduler_rebuild_reservations(now);
	return true;
}

bool scheduler_resume_cycles(time_os_t now) {
	uint8_t order[RUNTIME_QUEUE_SIZE];
	uint16_t count = 0;
	for (uint16_t i = 0; i < pd.nqueue; i++) {
		if (pd.queue[i].cycle_flags & CYCLE_PAUSED) order[count++] = i;
	}
	if (!count) return false;
	sort_jobs(order, count);
	for (uint16_t i = 0; i < count; i++) pd.queue[order[i]].st = 0;
	scheduler_rebuild_reservations(now);
	for (uint16_t i = 0; i < count; i++) {
		auto& q = pd.queue[order[i]];
		q.cycle_flags &= ~CYCLE_PAUSED;
		if (!place_cycle(q, now + i / OUTPUT_RISES_PER_SECOND)) {
			cycle_soak_cancel_future(q.cycle);
			q.dur = 0;
			q.cycle_flags |= CYCLE_TAIL;
			retain_tail(q);
			pd.cycle_config_error = true;
		}
	}
	scheduler_select_jobs();
	return true;
}

void scheduler_enter_remote_extension(time_os_t now) {
	// A mode change must not carry autonomous work into the passive controller.
	// Use normal stops before clearing the queue so delivered water is logged.
	scheduler_select_jobs();
	for (int qi = pd.nqueue; qi-- > 0;) {
		auto& q = pd.queue[qi];
		if (governed(q)) {
			if (q.cycle_flags & CYCLE_STARTED) stop_cycle(qi, now, StationStopReason::Cancelled);
		} else if (pd.station_qid[q.sid] == qi && os.is_running(q.sid)) {
			q.deque_time = now;
			turn_off_station(q.sid, now);
		}
	}
	os.clear_all_station_bits();
	os.apply_all_station_bits();
	if (os.status.program_busy && os.iopts[IOPT_SENSOR1_TYPE] == SENSOR_TYPE_FLOW) {
		write_log(LOGDATA_FLOWSENSE, now);
		notif.add(NOTIFY_FLOWSENSOR, flow_pulses_since(os.flowcount_log_start, flow_count));
	}
	pd.reset_runtime();
	pd.clear_pause();
	os.status.program_busy = 0;
}

void reset_all_stations_immediate(bool running_ones_only) {
	if(running_ones_only) {
		time_os_t currtime = os.now_tz();
		for(int qi = pd.nqueue; qi-- > 0;) {
			auto& q = pd.queue[qi];
			if (governed(q)) {
				if (q.cycle_flags & CYCLE_STARTED) stop_cycle(qi, currtime, StationStopReason::Cancelled);
			} else if (pd.station_qid[q.sid] == qi && os.is_running(q.sid)) {
				turn_off_running_station_immediate(q.sid, currtime, 0);
			}
		}
		os.apply_all_station_bits();
	} else {
		const time_os_t now = os.now_tz();
		scheduler_select_jobs();
		for (uint16_t qi = 0; qi < pd.nqueue; qi++) {
			auto& q = pd.queue[qi];
			if (pd.station_qid[q.sid] == qi && os.is_running(q.sid)) remember_off(q, now);
		}
		os.clear_all_station_bits();
		os.apply_all_station_bits();
		pd.reset_runtime(false);
		// Keep actual OFF history, but discard reservations for canceled work.
		scheduler_rebuild_reservations(now);
		pd.clear_pause();
	}
}

void reset_all_stations(bool running_ones_only) {
	if(running_ones_only) {
		RuntimeQueueStruct *q;
		time_os_t currtime = os.now_tz();
		for(int qi = pd.nqueue; qi-- > 0;) {
			q = &pd.queue[qi];
			if (governed(*q)) {
				if (q->cycle_flags & CYCLE_STARTED) stop_cycle(qi, currtime, StationStopReason::Cancelled);
			} else if(pd.station_qid[q->sid] == qi && os.is_running(q->sid)) {
				q->deque_time = currtime;
				turn_off_station(q->sid, currtime, 0);
			}
		}
	} else {
		for (int i = pd.nqueue; i-- > 0;) {
			if (governed(pd.queue[i])) stop_cycle(i, os.now_tz(), StationStopReason::Cancelled);
			else pd.queue[i].dur = 0;
		}
	}
}

uint8_t get_program_water_percent(const ProgramStruct &prog) {
	if (!prog.use_weather) return 100;
	bool response_current = weather_response_is_current(monotonic_millis());
	if (response_current && wt_restricted > 0) return 0;
	uint8_t wl = effective_weather_water_percent(response_current);
	if (response_current && mda == 100 && prog.type == PROGRAM_TYPE_INTERVAL && md_N > 0) {
		wl = ((unsigned int)prog.days[1] - 1 < md_N) ? md_scales[prog.days[1] - 1] : md_scales[md_N - 1];
	}
	return wl;
}

float get_program_sensor_adj(uint8_t pid) {
	SensorAdjustment *adj = SensorAdjustment::read(pid, pd.nprograms);
	if (adj) return adj->get_adjustment_factor(os.sensors);
	return 1.f;
}

void prepare_program_run(const ProgramStruct& program, uint8_t water_percent, float sensor_factor,
	bool skip_short, uint32_t* durations) {
	for (uint16_t sid = 0; sid < os.nstations; sid++) {
		uint32_t duration = 0;
		if (program.durations[sid] && !os.is_master_station(sid) &&
			!(os.attrib_dis[sid >> 3] & (1 << (sid & 7)))) {
			duration = water_time_scale(water_time_resolve(program.durations[sid]), water_percent, sensor_factor);
			if (skip_short && water_percent < 20 && duration < 10) duration = 0;
		}
		durations[sid] = duration;
	}
}

RunAdmission scheduler_check_admission(const uint32_t* durations, bool replace) {
	uint16_t count = 0;
	for (uint16_t sid = 0; sid < os.nstations; sid++) count += durations[sid] != 0;
	if (!count) return RunAdmission::Empty;
	if (!runtime_queue_can_admit(replace ? 0 : pd.nqueue, count)) {
		if (pd.queue_rejections != UINT32_MAX) pd.queue_rejections++;
		return RunAdmission::Full;
	}
	return RunAdmission::Accepted;
}

RunAdmission scheduler_enqueue_run(const uint32_t* durations, const uint8_t* order, uint8_t pid, bool replace) {
	const auto result = scheduler_check_admission(durations, replace);
	if (result != RunAdmission::Accepted) return result;
	// All eligibility/scaling and capacity checks precede replacement. The main
	// loop owns the queue; no callbacks or yields occur during this commit.
	if (replace) reset_all_stations_immediate();
	for (uint16_t oi = 0; oi < os.nstations; oi++) {
		const uint8_t sid = order[oi];
		if (!durations[sid]) continue;
		auto* q = pd.enqueue();
		q->sid = sid;
		q->pid = pid;
		q->dur = durations[sid];
	}
	return RunAdmission::Accepted;
}

// Keep program/order/duration scratch off the scheduling and policy-read stack.
static __attribute__((noinline)) RunAdmission prepare_manual_program(unsigned char pid,
	unsigned char uwt, unsigned char qo, unsigned char usa) {
	ProgramStruct prog{};
	float sensor_adj = 1.f;
	unsigned char ns = os.nstations;
	unsigned char order[MAX_NUM_STATIONS];
	uint32_t durations[MAX_NUM_STATIONS];
	// prefill with default order: ascending by index
	for(unsigned char sid=0;sid<ns;sid++) {
		order[sid] = sid;
		prog.durations[sid] = pid == 255 ? 2 : 60;
	}

	unsigned char wl = 100;
	if ((pid>0)&&(pid<255)) {
		pd.read(pid-1, &prog);
		if(usa && !os.iopts[IOPT_REMOTE_EXT_MODE]) sensor_adj = get_program_sensor_adj(pid-1);
		if(uwt && !os.iopts[IOPT_REMOTE_EXT_MODE]) wl = get_program_water_percent(prog);
		// get station ordering from program name
		prog.gen_station_runorder(1, order);
	}

	prepare_program_run(prog, wl, sensor_adj, false, durations);
	const auto result = scheduler_enqueue_run(durations, order, RUNONCE_PID, qo == QUEUE_OPTION_REPLACE);
	if(result == RunAdmission::Accepted) {
		if (pid > 0 && pid < 255) notif.add(NOTIFY_PROGRAM_SCHED, pid-1, wl, 1, sensor_adj);
	}
	return result;
}

RunAdmission manual_start_program(unsigned char pid, unsigned char uwt, unsigned char qo, unsigned char usa) {
	const auto result = prepare_manual_program(pid, uwt, qo, usa);
	if (result == RunAdmission::Accepted) schedule_all_stations(os.now_tz(), qo);
	return result;
}
