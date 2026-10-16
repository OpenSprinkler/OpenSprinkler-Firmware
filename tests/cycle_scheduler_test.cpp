#include "core/program.h"
#include "core/scheduler.h"
#include "core/bundle.h"
#include "services/notifier.h"
#include "storage/station_extra.h"
#include "storage/logging.h"

#include <cassert>
#include <cstdio>
#include <vector>

// Run the production scheduler with deterministic time and output boundaries.
ConStatus OpenSprinkler::status = {};
unsigned char OpenSprinkler::nboards = 1, OpenSprinkler::nstations = 8;
unsigned char OpenSprinkler::iopts[NUM_IOPTS] = {};
unsigned char OpenSprinkler::station_bits[MAX_NUM_BOARDS] = {};
unsigned char OpenSprinkler::attrib_igrd[MAX_NUM_BOARDS] = {};
unsigned char OpenSprinkler::attrib_spe[MAX_NUM_BOARDS] = {};
unsigned char OpenSprinkler::attrib_igs[NUM_SENSORS][MAX_NUM_BOARDS] = {};
unsigned char OpenSprinkler::masters[NUM_MASTER_ZONES][NUM_MASTER_OPTS] = {};
SensorState OpenSprinkler::sn_sensors[NUM_SENSORS] = {};
uint32_t OpenSprinkler::pause_timer = 0, OpenSprinkler::flowcount_log_start = 0;
const SensorIoptKeys sensor_iopt_keys[NUM_SENSORS] = {};
uint32_t flow_begin = 0, flow_start = 0, flow_stop = 0, flow_gallons = 0, flow_count = 0;
float flow_last_gpm = 0;

static time_os_t clock_now = 1000;
static uint8_t groups[MAX_NUM_STATIONS], bindings[MAX_NUM_STATIONS];
static uint16_t bundle_min[MAX_NUM_STATIONS];
static int16_t on_adj[NUM_MASTER_ZONES], off_adj[NUM_MASTER_ZONES], station_delay;
static CycleSoakPolicy policies[MAX_NUM_STATIONS];
static unsigned policy_reader_opens = 0, policy_reads = 0, active_readers = 0;
static StationExtraResult policy_result = StationExtraResult::Ok;
struct Event { uint32_t time; uint8_t sid; bool on; };
static std::vector<Event> events;
static std::vector<LogStruct> logs;
static bool delay_output = false;
static uint32_t commanded_duration = 0;

time_os_t OpenSprinkler::now_tz() { return clock_now; }
unsigned char OpenSprinkler::is_sequential_station(unsigned char sid) { return groups[sid] < NUM_SEQ_GROUPS; }
unsigned char OpenSprinkler::get_station_gid(unsigned char sid) { return groups[sid]; }
unsigned char OpenSprinkler::is_master_station(unsigned char sid) {
	for (uint8_t m = 0; m < NUM_MASTER_ZONES; m++) if (masters[m][MASOPT_SID] == sid + 1) return 1;
	return 0;
}
int16_t OpenSprinkler::get_on_adj(unsigned char m) { return on_adj[m]; }
int16_t OpenSprinkler::get_off_adj(unsigned char m) { return off_adj[m]; }
unsigned char OpenSprinkler::is_running(unsigned char sid) { return (station_bits[sid >> 3] >> (sid & 7)) & 1; }
unsigned char OpenSprinkler::get_applied_station_bit(unsigned char sid) { return !delay_output && is_running(sid); }
unsigned char OpenSprinkler::set_station_bit(unsigned char sid, unsigned char value, uint32_t duration) {
	assert(active_readers == 0); // policy handles must close before output side effects
	if (value) commanded_duration = duration;
	const bool changed = is_running(sid) != value;
	if (changed) events.push_back({clock_now, sid, value != 0});
	if (value) station_bits[sid >> 3] |= 1 << (sid & 7);
	else station_bits[sid >> 3] &= ~(1 << (sid & 7));
	return changed;
}
void OpenSprinkler::apply_all_station_bits(void (*)()) {}
void OpenSprinkler::clear_all_station_bits() { memset(station_bits, 0, sizeof(station_bits)); }
int16_t water_time_decode_signed(unsigned char) { return station_delay; }
bool sensor_available(uint8_t) { return false; }
bool bundle_is_station(uint8_t sid) { return bundle_min[sid] != 0; }
uint16_t bundle_minimum_duration(uint8_t sid) { return bundle_min[sid]; }
bool bundle_bound_to_master(uint8_t sid, uint8_t master) { return bindings[sid] & (1 << master); }
bool NotifQueue::add(uint16_t, uint32_t, float, uint8_t, float) { return true; }
void write_log(unsigned char, time_os_t) { logs.push_back(ProgramData::lastrun); }
FileReader::FileReader(const char*) { active_readers++; }
FileReader::~FileReader() { assert(active_readers); active_readers--; }
StationExtraReader::StationExtraReader(uint16_t count)
	: file_(nullptr), count_(count), status_(policy_result) { policy_reader_opens++; }
StationExtraResult StationExtraReader::load(uint16_t sid, CycleSoakPolicy& out) {
	assert(sid < count_ && active_readers == 1);
	policy_reads++;
	out = status_ == StationExtraResult::Ok ? policies[sid] : CycleSoakPolicy{};
	return status_;
}

static void reset() {
	assert(active_readers == 0);
	policy_reader_opens = policy_reads = 0;
	policy_result = StationExtraResult::Ok;
	ProgramData::cycle_config_error = false;
	ProgramData::reset_runtime();
	OpenSprinkler::clear_all_station_bits();
	OpenSprinkler::status = {};
	OpenSprinkler::status.enabled = 1;
	memset(OpenSprinkler::iopts, 0, sizeof(OpenSprinkler::iopts));
	memset(OpenSprinkler::masters, 0, sizeof(OpenSprinkler::masters));
	memset(OpenSprinkler::attrib_spe, 0, sizeof(OpenSprinkler::attrib_spe));
	memset(groups, 0, sizeof(groups));
	memset(bindings, 0, sizeof(bindings));
	memset(bundle_min, 0, sizeof(bundle_min));
	memset(on_adj, 0, sizeof(on_adj));
	memset(off_adj, 0, sizeof(off_adj));
	for (auto& policy : policies) policy = {};
	station_delay = 0;
	OpenSprinkler::pause_timer = 0;
	clock_now = 1000;
	events.clear(); logs.clear();
	delay_output = false;
	commanded_duration = 0;
}
static void policy(uint8_t sid, uint16_t cycle, uint16_t soak) {
	policies[sid].enabled = true;
	policies[sid].cycle_seconds = cycle;
	policies[sid].soak_seconds = soak;
}
static RuntimeQueueStruct& admit(uint8_t sid, uint32_t duration, uint8_t pid = 1) {
	auto* q = ProgramData::enqueue(); assert(q);
	q->sid = sid; q->dur = duration; q->pid = pid;
	return *q;
}
static void tick(uint32_t now) {
	clock_now = now;
	scheduler_tick_stations(now);
	for (int i = ProgramData::nqueue; i-- > 0;) {
		if (scheduler_queue_expired(ProgramData::queue[i], now)) ProgramData::dequeue(i);
	}
	scheduler_rebuild_reservations(now);
}

static void test_same_second_sequential_enqueue_without_cycles() {
	// PR #440 / issue #243: separate requests must see reservations immediately,
	// even with no cycle policies and no scheduler tick between the requests.
	const uint8_t batches[][2] = {{1, 1}, {1, 2}, {2, 1}};
	for (const auto& batch : batches) {
		for (int16_t delay : {-10, 0, 10}) {
			for (int16_t lead : {0, -15}) {
				reset();
				station_delay = delay;
				if (lead) {
					OpenSprinkler::masters[0][MASOPT_SID] = 8;
					on_adj[0] = lead;
					off_adj[0] = 20;
					memset(bindings, 1, sizeof(bindings));
				}
				uint8_t sid = 0;
				for (uint8_t count : batch) {
					for (uint8_t i = 0; i < count; i++) admit(sid++, 60);
					schedule_all_stations(clock_now, QUEUE_OPTION_APPEND);
					assert(clock_now == 1000 && events.empty());
					assert(!scheduler_has_cycles());
					const auto& last = ProgramData::queue[sid - 1];
					assert(ProgramData::last_seq_stop_times[0] == last.st + last.dur);
				}
				const uint32_t first_start = 1001 + uint32_t(-lead);
				for (uint8_t i = 0; i < sid; i++) {
					const auto& q = ProgramData::queue[i];
					assert(!(q.cycle_flags & CYCLE_GOVERNED));
					assert(q.dur == 60);
					assert(q.st == first_start + uint32_t(i) * (60 + delay));
				}
				// Negative delay intentionally overlaps; otherwise verify actual
				// output transitions as well as the precomputed start times.
				if (delay >= 0) {
					for (uint32_t now = 1001; now <= first_start + 240; now++) {
						tick(now);
						unsigned running = 0;
						for (uint8_t i = 0; i < sid; i++) running += OpenSprinkler::is_running(i);
						assert(running <= 1);
					}
					assert(ProgramData::nqueue == 0 && logs.size() == sid);
					for (uint8_t i = 0; i < sid; i++) {
						assert(logs[i].station == i && logs[i].duration == 60);
					}
				}
			}
		}
	}
}

static void test_batched_policy_admission() {
	reset();
	schedule_all_stations(clock_now);
	assert(policy_reader_opens == 0 && policy_reads == 0);
	for (uint8_t sid = 0; sid < 72; sid++) {
		policy(sid, 60, 120);
		admit(sid, 130);
	}
	schedule_all_stations(clock_now);
	assert(policy_reader_opens == 1 && policy_reads == 72 && active_readers == 0);
	for (uint8_t i = 0; i < 72; i++) {
		const auto& q = ProgramData::queue[i];
		assert(q.dur == 44 && q.cycle.remaining == 86 && q.cycle.soak_seconds == 120);
	}
	schedule_all_stations(clock_now);
	assert(policy_reader_opens == 1 && policy_reads == 72); // no new jobs, no lookup
	policy(0, 120, 180);
	admit(0, 120, 2);
	schedule_all_stations(clock_now);
	assert(policy_reader_opens == 2 && policy_reads == 73 && active_readers == 0);
	assert(ProgramData::queue[0].cycle.soak_seconds == 120); // admitted snapshot unchanged
	assert(ProgramData::queue[72].cycle.soak_seconds == 180); // next pass sees edits
	for (auto result : {StationExtraResult::Missing, StationExtraResult::Malformed}) {
		reset();
		policy_result = result;
		admit(0, 130); admit(1, 130);
		schedule_all_stations(clock_now);
		assert(policy_reader_opens == 1 && policy_reads == 2 && active_readers == 0);
		assert(ProgramData::queue[0].dur == 130 && ProgramData::queue[1].dur == 130);
		assert(ProgramData::cycle_config_error == (result == StationExtraResult::Malformed));
	}
}

static void test_insert_running_duplicate() {
	reset(); policy(1, 60, 60);
	admit(0, 120); schedule_all_stations(clock_now); tick(1001);
	clock_now = 1021;
	admit(0, 30, 2); admit(1, 60, 2);
	scheduler_select_jobs();
	assert(ProgramData::station_qid[0] == 0); // unscheduled duplicate cannot own the output
	schedule_all_stations(clock_now, QUEUE_OPTION_INSERT_FRONT);
	assert(!OpenSprinkler::is_running(0));
	assert(logs.size() == 1 && logs[0].duration == 20);
	assert(ProgramData::queue[0].dur == 100);
	for (uint32_t now = 1022; ProgramData::nqueue && now < 1600; now++) {
		tick(now);
		assert(!(OpenSprinkler::is_running(0) && OpenSprinkler::is_running(1)));
	}
	assert(!ProgramData::nqueue);
	uint32_t original = 0, inserted = 0;
	for (const auto& log : logs) if (log.station == 0) {
		if (log.program == 1) original += log.duration;
		if (log.program == 2) inserted += log.duration;
	}
	assert(original == 120 && inserted == 30);
}

static void test_replace_cooldown() {
	uint32_t durations[MAX_NUM_STATIONS] = {};
	uint8_t order[MAX_NUM_STATIONS];
	for (uint16_t sid = 0; sid < MAX_NUM_STATIONS; sid++) order[sid] = sid;
	durations[0] = 60;
	// Replacement while soaking preserves the actual OFF, not the canceled reservation.
	reset(); policy(0, 60, 120); admit(0, 120);
	schedule_all_stations(clock_now); tick(1001); tick(1061);
	clock_now = 1076;
	assert(scheduler_enqueue_run(durations, order, 2, true) == RunAdmission::Accepted);
	schedule_all_stations(clock_now);
	assert(ProgramData::queue[0].st == 1181);
	tick(1180); assert(!OpenSprinkler::is_running(0));
	tick(1181); assert(OpenSprinkler::is_running(0));

	// Active governed and manual-bypass runs both establish cooldown on replacement.
	for (uint8_t pid : {uint8_t(1), uint8_t(MANUAL_PID)}) {
		reset(); policy(0, 60, 120); admit(0, 120, pid);
		schedule_all_stations(clock_now); tick(1001); clock_now = 1016;
		assert(scheduler_enqueue_run(durations, order, 2, true) == RunAdmission::Accepted);
		assert(!OpenSprinkler::is_running(0));
		assert(ProgramData::last_off_until[0] == 1136);
		schedule_all_stations(clock_now);
		assert(ProgramData::queue[0].st == 1136);
	}

	// A canceled station that never ran must not leave a predicted soak behind.
	reset(); policy(0, 60, 120); admit(1, 300); admit(0, 60);
	schedule_all_stations(clock_now); tick(1001); clock_now = 1016;
	assert(scheduler_enqueue_run(durations, order, 2, true) == RunAdmission::Accepted);
	assert(!ProgramData::last_off_until[0]);
	schedule_all_stations(clock_now);
	assert(ProgramData::queue[0].st == 1017);

	// Ordinary immediate reset preserves OFF history; explicit clock reset clears it.
	tick(1017); clock_now = 1020; reset_all_stations_immediate();
	assert(!ProgramData::nqueue && ProgramData::last_off_until[0] == 1140);
	assert(ProgramData::cooldown_until[0] == 1140);
	ProgramData::reset_runtime();
	assert(!ProgramData::last_off_until[0] && !ProgramData::cooldown_until[0]);
}

static uint32_t random_state = 1;
static uint32_t random_below(uint32_t limit) {
	random_state = random_state * 1664525U + 1013904223U;
	return random_state % limit;
}

static void test_gap_boundaries() {
	// Exact fit, or one second too short, including delay on both sides.
	for (int delay : {0, 5}) for (int short_by : {0, 1}) {
		reset(); policy(0, 60, 60); station_delay = delay;
		admit(0, 120); admit(1, 60); schedule_all_stations(clock_now);
		ProgramData::queue[1].st = 1181 + delay - short_by;
		ProgramData::queue[1].deque_time = ProgramData::queue[1].st + 60;
		tick(1001); tick(1061);
		assert(ProgramData::queue[0].st == (short_by ? 1240U + 2 * delay : 1121U));
	}
	// The raw window fits, but the negative master ON lead moves it into a booking.
	reset(); policy(0, 60, 60); admit(0, 120); admit(1, 60);
	schedule_all_stations(clock_now); tick(1001);
	bindings[0] = 1; OpenSprinkler::masters[0][MASOPT_SID] = 8; on_adj[0] = -90;
	ProgramData::queue[1].st = 1241; ProgramData::queue[1].deque_time = 1301;
	tick(1061); assert(ProgramData::queue[0].st == 1301);

	// Another station's master tail does not occupy a watering gap.
	reset(); policy(0, 60, 60); admit(0, 120); auto& tail = admit(1, 1);
	tail.cycle_flags = CYCLE_TAIL; tail.dur = 0; tail.st = 1000;
	tail.cycle.prev_end = tail.deque_time = 2000;
	schedule_all_stations(clock_now); tick(1001); tick(1061);
	assert(ProgramData::queue[0].st == 1121);

	// Duplicate requests, including paused ones, opt out even if a gap would fit.
	for (bool paused : {false, true}) {
		reset(); policy(0, 60, 60); admit(0, 120); admit(1, 60); admit(0, 60);
		schedule_all_stations(clock_now);
		ProgramData::queue[1].st = 1401; ProgramData::queue[1].deque_time = 1461;
		ProgramData::queue[2].st = 1601; ProgramData::queue[2].deque_time = 1661;
		if (paused) ProgramData::queue[2].cycle_flags |= CYCLE_PAUSED;
		tick(1001); tick(1061);
		assert(ProgramData::queue[0].st == (paused ? 1461U : 1721U));
	}
	// A missed first window is not a naturally completed cycle: still append.
	reset(); policy(0, 60, 60); admit(0, 120); admit(1, 60);
	schedule_all_stations(clock_now);
	ProgramData::queue[1].st = 1401; ProgramData::queue[1].deque_time = 1461;
	tick(1061); assert(ProgramData::queue[0].st == 1461);

	// Full-queue fragmented gaps, in either physical queue order. The scan needs
	// no scratch array and cannot depend on swap-delete's storage order.
	for (bool reverse : {false, true}) {
		reset(); policy(0, 60, 60); admit(0, 120); schedule_all_stations(clock_now);
		for (uint16_t i = 1; i < RUNTIME_QUEUE_SIZE; i++) {
			auto& reservation = admit(1, 1);
			const uint16_t slot = reverse ? RUNTIME_QUEUE_SIZE - i - 1 : i - 1;
			reservation.st = 1121 + 2 * slot;
			reservation.deque_time = reservation.st + 1;
		}
		tick(1001); tick(1061);
		assert(ProgramData::queue[0].st == 1122U + 2 * (RUNTIME_QUEUE_SIZE - 2));
	}
}

static void test_gap_randomized() {
	for (uint32_t seed = 1; seed <= 400; seed++) {
		reset(); random_state = seed;
		station_delay = random_below(11);
		uint32_t requested[4], last_off[4] = {}, watered[4] = {};
		for (uint8_t sid = 0; sid < 4; sid++) {
			policy(sid, 60 + random_below(181), 60 + random_below(541));
			requested[sid] = 61 + random_below(900);
			admit(sid, requested[sid]);
		}
		schedule_all_stations(clock_now);
		size_t checked = 0;
		for (uint32_t now = 1001; ProgramData::nqueue && now < 30000; now++) {
			tick(now);
			unsigned running = 0;
			for (uint8_t sid = 0; sid < 4; sid++) running += bool(OpenSprinkler::is_running(sid));
			assert(running <= 1);
			while (checked < events.size()) {
				const auto& event = events[checked++];
				if (event.on && last_off[event.sid]) assert(event.time >= last_off[event.sid] + policies[event.sid].soak_seconds);
				if (!event.on) last_off[event.sid] = event.time;
			}
		}
		assert(!ProgramData::nqueue);
		for (const auto& log : logs) {
			assert(log.duration <= policies[log.station].cycle_seconds);
			watered[log.station] += log.duration;
		}
		for (uint8_t sid = 0; sid < 4; sid++) assert(watered[sid] == requested[sid]);
	}
}

// Compile this same harness against the pre-gap scheduler to compare complete
// feature-off output traces, including insertion, replacement and interruption.
static void feature_off_traces() {
	for (unsigned mode = 0; mode < 5; mode++) for (unsigned seed = 1; seed <= 100; seed++) {
		reset(); random_state = seed;
		station_delay = int16_t(random_below(21)) - 10;
		for (uint8_t sid = 0; sid < 4; sid++) {
			groups[sid] = random_below(3);
			admit(sid, 30 + random_below(180));
		}
		schedule_all_stations(clock_now);
		for (unsigned now = 1001; now < 3000; now++) {
			clock_now = now;
			if (now == 1050 || now == 1100 || now == 1200) {
				if (mode < 3) {
					if (mode == 2) reset_all_stations_immediate();
					for (uint8_t sid = 0; sid < 4; sid++) admit(sid, 10 + random_below(120), 2);
					schedule_all_stations(now, mode == 1 ? QUEUE_OPTION_INSERT_FRONT : QUEUE_OPTION_APPEND);
				} else if (mode == 3) {
					turn_off_station(random_below(4), now, 1);
				} else ProgramData::toggle_pause(20);
			}
			if (mode == 4 && (now == 1060 || now == 1110 || now == 1210)) ProgramData::toggle_pause(0);
			tick(now);
		}
		assert(!ProgramData::nqueue);
		printf("%u:%u", mode, seed);
		for (const auto& event : events) printf(" %u,%u,%u", event.time, event.sid, event.on);
		puts("");
	}
}

static void preview_fixtures() {
	puts("[");
	for (unsigned scenario = 0; scenario < 50; scenario++) {
		reset(); policy(0, 60, 60); policy(1, 60, 60);
		if (scenario == 2) { policy(0, 600, 1800); policy(1, 300, 600); }
		if (scenario == 3) station_delay = -10;
		if (scenario == 4 || scenario == 5) {
			bindings[0] = bindings[1] = 1; OpenSprinkler::masters[0][MASOPT_SID] = 8;
			on_adj[0] = scenario == 4 ? 90 : -10;
			off_adj[0] = scenario == 4 ? 120 : 15;
		}
		if (scenario == 6) groups[0] = groups[1] = PARALLEL_GROUP_ID;
		if (scenario == 7) policies[0].enabled = policies[1].enabled = false;
		if (scenario == 8) OpenSprinkler::iopts[IOPT_REMOTE_EXT_MODE] = 1;
		if (scenario == 9) {
			bindings[0] = bindings[1] = 1; OpenSprinkler::masters[0][MASOPT_SID] = 8;
			on_adj[0] = 1000; off_adj[0] = 120;
		}
		if (scenario == 10) { policy(0, 900, 600); policy(1, 60, 60); }
		if (scenario > 10) {
			random_state = scenario;
			policy(0, 60 + random_below(541), 60 + random_below(1801));
			policy(1, 60 + random_below(181), 60 + random_below(601));
			station_delay = scenario % 3 ? random_below(31) : -10;
			if (scenario % 4 == 0) {
				bindings[0] = bindings[1] = 1; OpenSprinkler::masters[0][MASOPT_SID] = 8;
				on_adj[0] = -int16_t(random_below(151)); off_adj[0] = random_below(91);
			}
		}
		admit(0, scenario == 10 ? 3600 : scenario > 10 ? 61 + random_below(1800) : scenario == 0 ? 121 : scenario == 2 ? 1200 : 120);
		if (scenario != 0) admit(scenario == 1 ? 0 : 1, scenario == 10 ? 3600 : scenario > 10 ? 61 + random_below(900) : scenario == 2 ? 600 : 60, 2);
		printf("%s{\"scenario\":%u,\"delay\":%d,\"remote\":%s,\"policies\":[", scenario ? "," : "", scenario, station_delay,
			OpenSprinkler::iopts[IOPT_REMOTE_EXT_MODE] ? "true" : "false");
		for (unsigned sid = 0; sid < 2; sid++) printf("%s[%u,%u,%u]", sid ? "," : "", policies[sid].enabled, policies[sid].cycle_seconds, policies[sid].soak_seconds);
		printf("],\"masters\":[");
		if (bindings[0]) printf("{\"on\":%d,\"off\":%d}", on_adj[0], off_adj[0]);
		printf("],\"requests\":[");
		for (unsigned i = 0; i < ProgramData::nqueue; i++) {
			const auto& q = ProgramData::queue[i];
			printf("%s{\"sid\":%u,\"pid\":%u,\"gid\":%u,\"dur\":%u}", i ? "," : "", q.sid, q.pid, groups[q.sid], unsigned(q.dur));
		}
		schedule_all_stations(clock_now);
		for (unsigned now = 1001; ProgramData::nqueue && now < 100000; now++) tick(now);
		assert(!ProgramData::nqueue);
		printf("],\"segments\":[");
		for (unsigned i = 0; i < logs.size(); i++) {
			const auto& log = logs[i];
			printf("%s{\"sid\":%u,\"pid\":%u,\"st\":%u,\"dur\":%u}", i ? "," : "", log.station, log.program,
				unsigned(log.endtime - log.duration), unsigned(log.duration));
		}
		puts("]}");
	}
	puts("]");
}

int main(int argc, char** argv) {
	if (argc == 2 && !strcmp(argv[1], "--preview-fixtures")) { preview_fixtures(); return 0; }
	if (argc == 2 && !strcmp(argv[1], "--feature-off-traces")) { feature_off_traces(); return 0; }
	test_same_second_sequential_enqueue_without_cycles();
	test_batched_policy_admission();
	test_insert_running_duplicate();
	test_replace_cooldown();
	test_gap_boundaries();
	test_gap_randomized();
	// Waiting on the physical output must not consume the cycle's watering time.
	reset(); policy(0, 60, 60); delay_output = true;
	admit(0, 120); schedule_all_stations(clock_now); tick(1001); tick(1010);
	assert(logs.empty() && !(ProgramData::queue[0].cycle_flags & CYCLE_APPLIED));
	scheduler_output_applied(0, 1010);
	assert(ProgramData::queue[0].st + ProgramData::queue[0].dur == 1070);
	tick(1070);
	assert(logs.size() == 1 && logs[0].duration == 60);
	assert(ProgramData::queue[0].st == 1130);
	reset(); policy(0, 60, 60); delay_output = true;
	admit(0, 120); schedule_all_stations(clock_now); tick(1001);
	clock_now = 1010; ProgramData::toggle_pause(30);
	assert(logs.empty() && ProgramData::queue[0].dur == 60);

	// Special stations time the local command, even without a local applied bit.
	reset(); policy(0, 60, 60); delay_output = true;
	OpenSprinkler::attrib_spe[0] = 1;
	admit(0, 120); schedule_all_stations(clock_now); tick(1001);
	assert(ProgramData::queue[0].cycle_flags & CYCLE_APPLIED);
	tick(1061);
	assert(logs.size() == 1 && logs[0].duration == 60 && ProgramData::queue[0].st == 1121);

	// Cancelling before application must not manufacture watering or another cycle.
	reset(); policy(0, 60, 60); delay_output = true;
	admit(0, 120); schedule_all_stations(clock_now); tick(1001);
	turn_off_station(0, 1002);
	assert(logs.empty() && !ProgramData::nqueue && !OpenSprinkler::is_running(0));

	// Positive master ON adjustment may begin only after the valve has closed.
	reset(); policy(0, 60, 60); bindings[0] = 1;
	OpenSprinkler::masters[0][MASOPT_SID] = 8;
	on_adj[0] = 90; off_adj[0] = 120;
	admit(0, 120); schedule_all_stations(clock_now); tick(1001); tick(1061);
	assert(!scheduler_master_active(0, 1080));
	assert(scheduler_master_active(0, 1091));
	assert(ProgramData::queue[0].st == 1182);
	assert(scheduler_master_active(0, 1181));

	reset(); policy(0, 60, 60);
	auto& a = admit(0, 121);
	schedule_all_stations(clock_now);
	assert(a.st == 1001 && a.dur == 41 && a.cycle.remaining == 80);
	tick(1001); assert(OpenSprinkler::is_running(0));
	tick(1042); assert(!OpenSprinkler::is_running(0));
	assert(a.st == 1102 && a.dur == 40 && a.cycle.remaining == 40);
	tick(1102); tick(1142);
	assert(a.st == 1202 && a.dur == 40);
	tick(1202); tick(1242); tick(1243);
	assert(!ProgramData::nqueue && logs.size() == 3);
	assert(logs[0].duration + logs[1].duration + logs[2].duration == 121);
	assert(ProgramData::last_off_until[0] == 1302);

	// A natural completion after deque_time still re-arms; a coincident inhibit wins.
	reset(); policy(0, 60, 60); admit(0, 120); schedule_all_stations(clock_now);
	tick(1001); tick(1065);
	assert(ProgramData::queue[0].st == 1125 && ProgramData::nqueue == 1);
	reset(); policy(0, 60, 60); admit(0, 120); schedule_all_stations(clock_now);
	tick(1001); OpenSprinkler::status.rain_delayed = 1; tick(1061);
	assert(!ProgramData::nqueue && !OpenSprinkler::is_running(0));
	reset(); policy(0, 60, 60); admit(0, 120); schedule_all_stations(clock_now);
	tick(1001); tick(1061); OpenSprinkler::status.enabled = 0; tick(1062);
	assert(!ProgramData::nqueue && logs.size() == 1);
	reset(); policy(0, 60, 60); auto& manual_disabled = admit(0, 120, MANUAL_PID);
	manual_disabled.cycle_flags |= CYCLE_MANUAL_OPT_IN;
	schedule_all_stations(clock_now); tick(1001); tick(1061);
	OpenSprinkler::status.enabled = 0; tick(1062);
	assert(ProgramData::nqueue == 1); // Existing manual-run inhibit bypass remains.

	// Same SID requests reserve distinct segments, including across re-arms.
	reset(); policy(0, 60, 60); admit(0, 120); admit(0, 60); schedule_all_stations(clock_now);
	assert(ProgramData::queue[0].st == 1001 && ProgramData::queue[1].st == 1121);
	tick(1001); tick(1061);
	assert(ProgramData::queue[0].st == 1241);
	tick(1121); assert(ProgramData::station_qid[0] == 1 && OpenSprinkler::is_running(0));

	// Preserve bound masters' individual tails while the next cycle waits.
	reset(); policy(0, 60, 60); bindings[0] = 3;
	OpenSprinkler::masters[0][MASOPT_SID] = 7; OpenSprinkler::masters[1][MASOPT_SID] = 8;
	on_adj[0] = -10; off_adj[0] = 15; off_adj[1] = 30;
	admit(0, 120); schedule_all_stations(clock_now);
	assert(ProgramData::queue[0].st == 1011);
	assert(scheduler_master_active(0, 1001));
	tick(1011); tick(1071);
	assert(scheduler_master_active(0, 1080));
	assert(!scheduler_master_active(0, 1087) && scheduler_master_active(1, 1087));
	assert(!scheduler_master_active(1, 1102));

	// An unstarted window must not be logged as delivered or consume a cycle.
	// A one-second late start is still inside the window, not a reason to reorder.
	reset(); policy(0, 60, 60); policy(1, 60, 60); policy(2, 60, 60);
	admit(0, 60); admit(1, 60); admit(2, 60); schedule_all_stations(clock_now);
	assert(ProgramData::queue[1].st == 1061 && ProgramData::queue[2].st == 1121);
	tick(1001); tick(1062);
	assert(OpenSprinkler::is_running(1));
	assert(ProgramData::queue[1].st == 1061 && ProgramData::queue[1].dur == 61);
	assert(ProgramData::queue[2].st == 1121 && commanded_duration == 60);
	tick(1121); tick(1122);
	assert(logs.size() == 2 && logs[1].station == 1 && logs[1].duration == 60);
	reset(); policy(0, 60, 60); policy(1, 60, 60);
	admit(0, 120); admit(1, 60); schedule_all_stations(clock_now);
	tick(1001); tick(1061); tick(1122);
	assert(OpenSprinkler::is_running(0) && ProgramData::queue[0].st == 1121);
	assert(ProgramData::queue[0].st + ProgramData::queue[0].dur == 1182);

	// Remote/special commands also get the full late-start segment, not its remainder.
	reset(); policy(0, 60, 60); OpenSprinkler::attrib_spe[0] = 1; delay_output = true;
	admit(0, 120); schedule_all_stations(clock_now); tick(1060);
	assert(OpenSprinkler::is_running(0) && commanded_duration == 60);
	assert(ProgramData::queue[0].st + ProgramData::queue[0].dur == 1120);
	tick(1120);
	assert(logs.size() == 1 && logs[0].duration == 60);
	assert(ProgramData::queue[0].cycle.remaining == 0 && ProgramData::queue[0].dur == 60);

	// Exactly at the end, the whole window is missed and no watering is invented.
	reset(); policy(0, 60, 60); admit(0, 120); schedule_all_stations(clock_now);
	tick(1061);
	assert(logs.empty() && !OpenSprinkler::is_running(0));
	assert(ProgramData::queue[0].st == 1062 && ProgramData::queue[0].dur == 60);

	reset(); policy(0, 60, 60); admit(0, 120); schedule_all_stations(clock_now);
	tick(1100);
	assert(logs.empty() && ProgramData::queue[0].dur == 60 && ProgramData::queue[0].cycle.remaining == 60);
	assert(ProgramData::queue[0].st == 1101);

	// Pause during ON keeps the partial segment separate from later cycles.
	reset(); policy(0, 60, 60); admit(0, 120); schedule_all_stations(clock_now);
	tick(1001); clock_now = 1021; ProgramData::toggle_pause(30);
	assert(!OpenSprinkler::is_running(0) && ProgramData::queue[0].dur == 40);
	clock_now = 1031; ProgramData::toggle_pause(0);
	assert(ProgramData::queue[0].st == 1081 && ProgramData::queue[0].cycle.remaining == 60);
	tick(1081); tick(1121);
	assert(ProgramData::queue[0].st == 1181 && ProgramData::queue[0].dur == 60);

	// Paused soak is absolute, not extended by the full pause twice.
	reset(); policy(0, 60, 60); admit(0, 120); schedule_all_stations(clock_now);
	tick(1001); tick(1061); clock_now = 1070; ProgramData::toggle_pause(120);
	clock_now = 1130; ProgramData::toggle_pause(0);
	assert(ProgramData::queue[0].st == 1131);

	// RE transition cancels sleeping work; active work finishes once.
	reset(); policy(0, 60, 60); policy(1, 60, 60); groups[1] = PARALLEL_GROUP_ID;
	admit(0, 120); admit(1, 120); schedule_all_stations(clock_now);
	tick(1001); tick(1061); OpenSprinkler::iopts[IOPT_REMOTE_EXT_MODE] = 1;
	scheduler_enter_remote_extension(1062); assert(!ProgramData::nqueue);
	reset(); policy(0, 60, 60); admit(0, 120); schedule_all_stations(clock_now);
	tick(1001); OpenSprinkler::iopts[IOPT_REMOTE_EXT_MODE] = 1; scheduler_enter_remote_extension(1020);
	assert(OpenSprinkler::is_running(0) && !ProgramData::queue[0].cycle.remaining);
	tick(1061); tick(1062); assert(!ProgramData::nqueue);

	// Ordinary runs retain their initial timetable and duration.
	reset(); admit(0, 30); admit(1, 45); schedule_all_stations(clock_now);
	assert(ProgramData::queue[0].st == 1001 && ProgramData::queue[1].st == 1031);
	tick(1001); tick(1031); tick(1076);
	assert(logs.size() == 2 && logs[0].duration == 30 && logs[1].duration == 45);

	// Direct manual runs bypass by default; opt-in and disabled policy are distinct.
	reset(); policy(0, 60, 60); auto& bypass = admit(0, 120, MANUAL_PID);
	schedule_all_stations(clock_now);
	assert(bypass.dur == 120 && !(bypass.cycle_flags & CYCLE_GOVERNED) && bypass.cycle.soak_seconds == 60);
	reset(); policy(0, 60, 60); auto& opted = admit(0, 120, MANUAL_PID);
	opted.cycle_flags |= CYCLE_MANUAL_OPT_IN; schedule_all_stations(clock_now);
	assert(opted.dur == 60 && opted.cycle.remaining == 60);
	reset(); auto& disabled = admit(0, 120, MANUAL_PID);
	disabled.cycle_flags |= CYCLE_MANUAL_OPT_IN; schedule_all_stations(clock_now);
	assert(disabled.dur == 120 && !disabled.cycle.remaining);
	reset(); policy(0, 60, 60); OpenSprinkler::iopts[IOPT_REMOTE_EXT_MODE] = 1;
	auto& remote = admit(0, 120); schedule_all_stations(clock_now);
	assert(remote.dur == 120 && !(remote.cycle_flags & CYCLE_GOVERNED));

	// Cancelling while soaking cannot log a phantom run or re-arm later.
	reset(); policy(0, 60, 60); admit(0, 120); schedule_all_stations(clock_now);
	tick(1001); tick(1061); scheduler_select_jobs();
	turn_off_station(0, 1070);
	assert(!ProgramData::nqueue && logs.size() == 1);
	tick(1200); assert(!OpenSprinkler::is_running(0));
	assert(ProgramData::last_off_until[0] == 1121);
	ProgramData::reset_runtime(false); assert(ProgramData::last_off_until[0] == 1121);
	ProgramData::reset_runtime(); assert(!ProgramData::last_off_until[0]);

	// Master tails survive the final cycle and the RE cancellation of a soak.
	reset(); policy(0, 60, 60); bindings[0] = 1;
	OpenSprinkler::masters[0][MASOPT_SID] = 8; off_adj[0] = 90;
	admit(0, 60); schedule_all_stations(clock_now); tick(1001); tick(1061);
	assert(ProgramData::nqueue == 1 && scheduler_master_active(0, 1100));
	tick(1151); assert(ProgramData::nqueue == 1); tick(1152); assert(!ProgramData::nqueue);
	reset(); policy(0, 60, 60); bindings[0] = 1;
	OpenSprinkler::masters[0][MASOPT_SID] = 8; off_adj[0] = 90;
	admit(0, 120); schedule_all_stations(clock_now); tick(1001); tick(1061);
	OpenSprinkler::iopts[IOPT_REMOTE_EXT_MODE] = 1; scheduler_enter_remote_extension(1062);
	assert(ProgramData::nqueue == 1 && !ProgramData::queue[0].dur);
	assert(scheduler_master_active(0, 1100)); tick(1152); assert(!ProgramData::nqueue);

	// Late completions in the same pass publish group tails immediately.
	reset(); policy(0, 60, 60); policy(1, 60, 60); station_delay = -30;
	admit(0, 120); admit(1, 120); schedule_all_stations(clock_now);
	assert(ProgramData::queue[1].st == 1031);
	tick(1001); tick(1031); tick(1100);
	assert(ProgramData::queue[0].st == 1160 && ProgramData::queue[1].st == 1190);

	// A natural re-arm fills an idle gap without moving the longer-soak reservation.
	reset(); policy(0, 600, 1800); policy(1, 300, 600);
	admit(0, 1200); admit(1, 600); schedule_all_stations(clock_now);
	tick(1001); tick(1601); tick(1901);
	assert(ProgramData::queue[0].st == 3401 && ProgramData::queue[1].st == 2501);

	// Bundle minimum changes do not reject the invocation at runtime.
	reset(); policy(0, 60, 60); bundle_min[0] = 3;
	admit(0, 1); schedule_all_stations(clock_now); assert(ProgramData::queue[0].dur == 3);
	reset(); policy(0, 60, 60); bundle_min[0] = 31;
	ProgramData::cycle_config_error = false;
	admit(0, 120); schedule_all_stations(clock_now);
	assert(ProgramData::cycle_config_error && ProgramData::queue[0].dur == 120);
	assert(!(ProgramData::queue[0].cycle_flags & CYCLE_GOVERNED));

	// Early resume also clears pause for ordinary runs.
	reset(); admit(0, 120); schedule_all_stations(clock_now); tick(1001);
	clock_now = 1011; ProgramData::toggle_pause(60);
	assert(OpenSprinkler::status.pause_state);
	clock_now = 1021; OpenSprinkler::pause_timer = 50; ProgramData::toggle_pause(0);
	assert(!OpenSprinkler::status.pause_state);

	// RE may remove the last cycling job while an ordinary job is still paused.
	reset(); policy(0, 60, 60); admit(0, 120); admit(1, 30);
	schedule_all_stations(clock_now); tick(1001);
	clock_now = 1011; ProgramData::toggle_pause(60);
	OpenSprinkler::iopts[IOPT_REMOTE_EXT_MODE] = 1; scheduler_enter_remote_extension(1012);
	clock_now = 1020; ProgramData::toggle_pause(0);
	assert(ProgramData::nqueue == 1 && ProgramData::queue[0].sid == 1);
	assert(!(ProgramData::queue[0].cycle_flags & CYCLE_PAUSED));
	tick(1021); assert(OpenSprinkler::is_running(1));

	// Insert-front interrupts the ON segment, not its future water, and respects soak.
	reset(); policy(0, 60, 60); admit(0, 120); admit(1, 30);
	schedule_all_stations(clock_now); tick(1001); clock_now = 1021;
	admit(2, 10); schedule_all_stations(clock_now, QUEUE_OPTION_INSERT_FRONT);
	assert(!OpenSprinkler::is_running(0) && logs.size() == 1 && logs[0].duration == 20);
	assert(ProgramData::queue[2].st == 1022);
	assert(ProgramData::queue[0].st == 1081 && ProgramData::queue[0].dur == 40);
	assert(ProgramData::queue[0].cycle.remaining == 60 && ProgramData::queue[1].st == 1121);
	tick(1022); tick(1032); tick(1081); tick(1121); tick(1151); tick(1181); tick(1241);
	uint32_t delivered = 0;
	for (const auto& log : logs) if (log.station == 0) delivered += log.duration;
	assert(delivered == 120);

	// Inserting another job for the same SID removes old future reservations first.
	reset(); policy(0, 60, 60); admit(0, 120); schedule_all_stations(clock_now); tick(1001);
	clock_now = 1021; admit(0, 60); schedule_all_stations(clock_now, QUEUE_OPTION_INSERT_FRONT);
	assert(ProgramData::queue[1].st == 1081 && ProgramData::queue[0].st == 1201);
	assert(ProgramData::queue[0].dur == 40 && ProgramData::queue[0].cycle.remaining == 60);

	// Cancellation with shift removes only the selected chain; a sibling keeps cooldown.
	reset(); policy(0, 60, 60); admit(0, 120); admit(0, 60); admit(1, 30);
	schedule_all_stations(clock_now); tick(1001); clock_now = 1021;
	turn_off_station(0, clock_now, 1);
	assert(ProgramData::nqueue == 2);
	for (uint16_t i = 0; i < ProgramData::nqueue; i++) {
		const auto& q = ProgramData::queue[i];
		assert(q.st == (q.sid == 0 ? 1081U : 1141U));
	}

	// A request submitted while paused participates in the early-resume pass.
	reset(); policy(0, 60, 60); admit(0, 120); schedule_all_stations(clock_now); tick(1001);
	clock_now = 1011; ProgramData::toggle_pause(300);
	admit(1, 10); schedule_all_stations(clock_now, QUEUE_OPTION_INSERT_FRONT);
	clock_now = 1021; ProgramData::toggle_pause(0);
	assert(ProgramData::queue[1].st == 1022 && ProgramData::queue[0].st == 1071);
	assert(!OpenSprinkler::status.pause_state && ProgramData::queue[0].dur == 50);

	// Feature-off insert and shift retain the ordinary timetable.
	reset(); admit(0, 120); admit(1, 30); schedule_all_stations(clock_now); tick(1001);
	clock_now = 1021; admit(2, 10); schedule_all_stations(clock_now, QUEUE_OPTION_INSERT_FRONT);
	assert(ProgramData::queue[2].st == 1022 && ProgramData::queue[0].st == 1032);
	assert(ProgramData::queue[0].dur == 100 && ProgramData::queue[1].st == 1132);
	tick(1022); turn_off_station(2, 1025, 1);
	assert(ProgramData::queue[0].st == 1025 && ProgramData::queue[1].st == 1125);

	// All-or-none admission at the last slot, including a full 255-entry Linux queue.
	reset(); uint32_t durations[MAX_NUM_STATIONS] = {};
	uint8_t order[MAX_NUM_STATIONS];
	for (uint16_t i = 0; i < MAX_NUM_STATIONS; i++) order[i] = i;
	for (uint16_t i = 0; i < RUNTIME_QUEUE_SIZE - 1; i++) admit(0, 120);
	const std::vector<RuntimeQueueStruct> before(ProgramData::queue, ProgramData::queue + ProgramData::nqueue);
	const uint32_t rejected_before = ProgramData::queue_rejections;
	durations[1] = 10; durations[2] = 20;
	assert(scheduler_enqueue_run(durations, order, 4, false) == RunAdmission::Full);
	assert(ProgramData::nqueue == RUNTIME_QUEUE_SIZE - 1);
	assert(!memcmp(before.data(), ProgramData::queue, before.size() * sizeof(RuntimeQueueStruct)));
	assert(ProgramData::queue_rejections == rejected_before + 1);
	durations[2] = 0;
	assert(scheduler_enqueue_run(durations, order, 4, false) == RunAdmission::Accepted);
	assert(ProgramData::nqueue == RUNTIME_QUEUE_SIZE && ProgramData::queue[RUNTIME_QUEUE_SIZE - 1].sid == 1);
	assert(scheduler_enqueue_run(durations, order, 4, false) == RunAdmission::Full);
	durations[1] = 0;
	assert(scheduler_enqueue_run(durations, order, 4, true) == RunAdmission::Empty);
	assert(ProgramData::nqueue == RUNTIME_QUEUE_SIZE);
	durations[1] = 10; durations[2] = 20;
	assert(scheduler_enqueue_run(durations, order, 4, true) == RunAdmission::Accepted);
	assert(ProgramData::nqueue == 2 && ProgramData::queue[0].sid == 1 && ProgramData::queue[1].sid == 2);
	puts("Production cycle scheduler PASS");
}
