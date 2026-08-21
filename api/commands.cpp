#include "api/commands.h"

#include "OpenSprinkler.h"
#include "core/program.h"
#include "core/scheduler.h"
#include "util/utils.h"

#include <cstdlib>
#include <cstring>

extern OpenSprinkler os;
extern ProgramData pd;
extern char tmp_buffer[];
extern uint32_t reboot_timer;

ParamSource::ParamSource(const OTF::Request& request)
	: request(&request), data(nullptr), length(0) {
}

ParamSource::ParamSource(const uint8_t* data, size_t length)
	: request(nullptr), data(reinterpret_cast<const char*>(data)), length(length) {
}

uint16_t ParamSource::get(char* buffer, uint16_t max_length, const char* key,
	bool key_in_program_memory, uint8_t* key_found) const {
	if (request) {
		return findKeyVal(*request, buffer, max_length, key, key_in_program_memory, key_found);
	}
	return findKeyVal(data, length, buffer, max_length, key, key_in_program_memory, key_found);
}

namespace {

bool parse_duration_list(char* value, uint16_t* durations, uint8_t count) {
	memset(durations, 0, sizeof(*durations) * count);
	if (!value || *value != '[') return false;
	value++;

	uint8_t index = 0;
	for (; index < count && *value && *value != ']'; index++) {
		char* end = value;
		long duration = strtol(value, &end, 10);
		if (end == value) return false;
		durations[index] = static_cast<uint16_t>(duration);
		value = end;
		if (*value == ',') {
			value++;
		} else if (*value != ']' && *value != 0) {
			return false;
		}
	}
	if (index < count || *value == ']') return *value == ']';

	// Ignore valid entries left over from a previously larger station setup.
	while (*value && *value != ']') {
		char* end = value;
		strtol(value, &end, 10);
		if (end == value) return false;
		value = end;
		if (*value == ',') value++;
		else if (*value != ']') return false;
	}
	return *value == ']';
}

bool get_positive_flag(const ParamSource& params, const char* key) {
	return params.get(tmp_buffer, TMP_BUFFER_SIZE, key, true) && atoi(tmp_buffer) > 0;
}

} // namespace

uint8_t execute_change_values(const ParamSource& params, uint16_t allowed_actions) {
	if ((allowed_actions & CV_ACTION_RESET_STATIONS) && get_positive_flag(params, PSTR("rsn"))) {
		reset_all_stations();
	}

	if ((allowed_actions & CV_ACTION_RESET_RUNNING) && get_positive_flag(params, PSTR("rrsn"))) {
		reset_all_stations(true);
	}

	if ((allowed_actions & CV_ACTION_RESET_OVERCURRENT) && get_positive_flag(params, PSTR("rocs"))) {
		os.status.overcurrent_sid = 0;
	}

#if !defined(ARDUINO)
	if ((allowed_actions & CV_ACTION_UPDATE) && get_positive_flag(params, PSTR("update"))) {
		os.update_dev();
	}
#endif

	if ((allowed_actions & CV_ACTION_REBOOT) && get_positive_flag(params, PSTR("rbt"))) {
		os.status.safe_reboot = 0;
		reboot_timer = os.now_tz() + 1;
		return HTML_SUCCESS;
	}

	if ((allowed_actions & CV_ACTION_ENABLE) &&
		params.get(tmp_buffer, TMP_BUFFER_SIZE, PSTR("en"), true)) {
		if (tmp_buffer[0] == '1' && !os.status.enabled) os.enable();
		else if (tmp_buffer[0] == '0' && os.status.enabled) os.disable();
	}

	if ((allowed_actions & CV_ACTION_RAIN_DELAY) &&
		params.get(tmp_buffer, TMP_BUFFER_SIZE, PSTR("rd"), true)) {
		int rain_delay = atoi(tmp_buffer);
		if (rain_delay > 0) {
			os.nvdata.rd_stop_time = os.now_tz() + static_cast<uint32_t>(rain_delay) * 3600;
			os.raindelay_start();
		} else if (rain_delay == 0) {
			os.raindelay_stop();
		} else {
			return HTML_DATA_OUTOFBOUND;
		}
	}

	if ((allowed_actions & CV_ACTION_REMOTE_EXTENSION) &&
		params.get(tmp_buffer, TMP_BUFFER_SIZE, PSTR("re"), true)) {
		if (tmp_buffer[0] == '1' && !os.iopts[IOPT_REMOTE_EXT_MODE]) {
			os.iopts[IOPT_REMOTE_EXT_MODE] = 1;
			os.iopts_save();
		} else if (tmp_buffer[0] == '0' && os.iopts[IOPT_REMOTE_EXT_MODE]) {
			os.iopts[IOPT_REMOTE_EXT_MODE] = 0;
			os.iopts_save();
		}
	}

#if defined(ARDUINO)
	if ((allowed_actions & CV_ACTION_RESET_AP) &&
		params.get(tmp_buffer, TMP_BUFFER_SIZE, PSTR("ap"), true)) {
		os.reset_to_ap();
	}
#endif

	return HTML_SUCCESS;
}

uint8_t execute_manual_station(const ParamSource& params) {
	if (!params.get(tmp_buffer, TMP_BUFFER_SIZE, PSTR("sid"), true)) return HTML_DATA_MISSING;
	int sid = atoi(tmp_buffer);
	if (sid < 0 || sid >= os.nstations) return HTML_DATA_OUTOFBOUND;

	if (!params.get(tmp_buffer, TMP_BUFFER_SIZE, PSTR("en"), true)) return HTML_DATA_MISSING;
	bool enable = atoi(tmp_buffer) != 0;
	uint32_t curr_time = os.now_tz();

	if (enable) {
		if (!params.get(tmp_buffer, TMP_BUFFER_SIZE, PSTR("t"), true)) return HTML_DATA_MISSING;
		uint32_t duration = 0;
		if (!parse_program_duration(tmp_buffer, &duration)) return HTML_DATA_OUTOFBOUND;
		if (os.is_master_station(sid)) return HTML_NOT_PERMITTED;
		if (pd.station_qid[sid] != 0xFF) return HTML_NOT_PERMITTED;

		uint8_t queue_option = QUEUE_OPTION_APPEND;
		if (params.get(tmp_buffer, TMP_BUFFER_SIZE, PSTR("qo"), true)) {
			queue_option = static_cast<uint8_t>(atoi(tmp_buffer));
		}

		RuntimeQueueStruct* entry = pd.enqueue();
		if (!entry) return HTML_NOT_PERMITTED;
		entry->st = 0;
		entry->dur = duration;
		entry->sid = static_cast<uint8_t>(sid);
		entry->pid = MANUAL_PID;
		schedule_all_stations(curr_time, queue_option);
	} else {
		uint8_t shift = 0;
		if (params.get(tmp_buffer, TMP_BUFFER_SIZE, PSTR("ssta"), true)) {
			shift = static_cast<uint8_t>(atoi(tmp_buffer));
		}
		uint8_t queue_id = pd.station_qid[sid];
		if (queue_id == 0xFF) return HTML_DATA_OUTOFBOUND;
		RuntimeQueueStruct* entry = pd.queue + queue_id;
		entry->deque_time = curr_time;
		turn_off_station(static_cast<uint8_t>(sid), curr_time, shift);
	}

	return HTML_SUCCESS;
}

uint8_t execute_manual_program(const ParamSource& params) {
	if (!params.get(tmp_buffer, TMP_BUFFER_SIZE, PSTR("pid"), true)) return HTML_DATA_MISSING;
	int pid = atoi(tmp_buffer);
	if (pid < 0 || pid >= pd.nprograms) return HTML_DATA_OUTOFBOUND;

	uint8_t use_weather = 0;
	if (params.get(tmp_buffer, TMP_BUFFER_SIZE, PSTR("uwt"), true) && tmp_buffer[0] == '1') {
		use_weather = 1;
	}

	uint8_t use_sensor_adjustment = 0;
	if (params.get(tmp_buffer, TMP_BUFFER_SIZE, PSTR("usa"), true) && tmp_buffer[0] == '1') {
		use_sensor_adjustment = 1;
	}

	uint8_t queue_option = QUEUE_OPTION_REPLACE;
	if (params.get(tmp_buffer, TMP_BUFFER_SIZE, PSTR("qo"), true)) {
		queue_option = static_cast<uint8_t>(atoi(tmp_buffer));
	}
	if (queue_option == QUEUE_OPTION_REPLACE) reset_all_stations_immediate();

	manual_start_program(static_cast<uint8_t>(pid + 1), use_weather, queue_option,
		use_sensor_adjustment);
	return HTML_SUCCESS;
}

uint8_t execute_runonce(const ParamSource& params) {
	if (!params.get(tmp_buffer, TMP_BUFFER_SIZE, "t", false)) return HTML_DATA_MISSING;

	ProgramStruct program{};
	ProgramStruct annotation_program{};
	uint8_t station_count = os.nstations;
	if (!parse_duration_list(tmp_buffer, program.durations, station_count)) {
		return HTML_DATA_FORMATERROR;
	}

	uint8_t order[MAX_NUM_STATIONS];
	annotation_program.name[0] = 0;
	params.get(annotation_program.name, sizeof(annotation_program.name), PSTR("anno"), true);
	annotation_program.gen_station_runorder(1, order);

	uint8_t count_found = 0;
	params.get(tmp_buffer, TMP_BUFFER_SIZE, PSTR("cnt"), true, &count_found);
	if (count_found) {
		long repeat_count = atol(tmp_buffer);
		if (repeat_count < 0 || repeat_count > 32768) return HTML_DATA_OUTOFBOUND;
		if (repeat_count > 0) {
			if (!params.get(tmp_buffer, TMP_BUFFER_SIZE, PSTR("int"), true)) {
				return HTML_DATA_MISSING;
			}
			long interval = atol(tmp_buffer);
			if (interval < 1 || interval > 32767) return HTML_DATA_OUTOFBOUND;

			uint32_t start_minute = os.now_tz() / 60 + static_cast<uint32_t>(interval) + 1;
			uint16_t epoch_day = start_minute / 1440;
			program.enabled = 1;
			program.use_weather = 0;
			if (params.get(tmp_buffer, TMP_BUFFER_SIZE, PSTR("uwt"), true) && atol(tmp_buffer)) {
				program.use_weather = 1;
			}
			program.oddeven = 0;
			program.type = PROGRAM_TYPE_SINGLERUN;
			program.starttime_type = 0;
			program.en_daterange = 0;
			program.days[0] = (epoch_day >> 8) & 0xFF;
			program.days[1] = epoch_day & 0xFF;
			program.starttimes[0] = start_minute % 1440;
			program.starttimes[1] = static_cast<int16_t>(repeat_count - 1);
			program.starttimes[2] = repeat_count == 1 ? 0 : static_cast<int16_t>(interval);
			strcpy_P(program.name, PSTR(RUNONCE_REPEAT_PREFIX));
			strncat(program.name, annotation_program.name,
				PROGRAM_NAME_SIZE - strlen(program.name) - 1);
			program.name[PROGRAM_NAME_SIZE - 1] = 0;
			if (!pd.add(&program)) return HTML_DATA_OUTOFBOUND;
		}
	}

	uint8_t watering_level = 100;
	if (params.get(tmp_buffer, TMP_BUFFER_SIZE, PSTR("uwt"), true) && tmp_buffer[0] == '1') {
		watering_level = os.iopts[IOPT_WATER_PERCENTAGE];
	}

	uint8_t queue_option = QUEUE_OPTION_REPLACE;
	if (params.get(tmp_buffer, TMP_BUFFER_SIZE, PSTR("qo"), true)) {
		queue_option = static_cast<uint8_t>(atoi(tmp_buffer));
	}
	if (queue_option == QUEUE_OPTION_REPLACE) reset_all_stations_immediate();

	bool match_found = false;
	for (uint8_t order_index = 0; order_index < station_count; order_index++) {
		uint8_t sid = order[order_index];
		uint32_t duration = water_time_scale(water_time_resolve(program.durations[sid]),
			watering_level, 1.0f);
		uint8_t board = sid >> 3;
		uint8_t station = sid & 0x07;
		if (duration == 0 || (os.attrib_dis[board] & (1 << station))) continue;

		RuntimeQueueStruct* entry = pd.enqueue();
		if (!entry) continue;
		entry->st = 0;
		entry->dur = duration;
		entry->pid = RUNONCE_PID;
		entry->sid = sid;
		match_found = true;
	}

	if (!match_found) return HTML_DATA_MISSING;
	schedule_all_stations(os.now_tz(), queue_option);
	return HTML_SUCCESS;
}
