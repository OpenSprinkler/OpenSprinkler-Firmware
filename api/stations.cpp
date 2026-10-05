#include "handler_context.h"
#include "handlers.h"

#include "core/bundle.h"
#include "core/output_sequencer.h"
#include "storage/station_extra.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void emit_board_attribute(const char* name, unsigned char* attribute) {
	bfill.emit_p(PSTR("\"$F\":["), name);
	for (unsigned char index = 0; index < os.nboards; index++) {
		bfill.emit_p(PSTR("$D"), attribute[index]);
		if (index != os.nboards - 1) bfill.emit_p(PSTR(","));
	}
	bfill.emit_p(PSTR("],"));
}

void emit_station_attribute(const char* name, unsigned char* attribute) {
	bfill.emit_p(PSTR("\"$F\":["), name);
	for (unsigned char board = 0; board < os.nboards; board++) {
		for (unsigned char station = 0; station < 8; station++) {
			bfill.emit_p(PSTR("$D"), attribute[board * 8 + station]);
			if (board != os.nboards - 1 || station < 7) bfill.emit_p(PSTR(","));
		}
	}
	bfill.emit_p(PSTR("],"));
}

void change_board_attribute(const OTF::Request& request, char prefix, unsigned char* attribute) {
	char key[6] = {0};
	key[0] = prefix;
	for (unsigned char board = 0; board < os.nboards; board++) {
		snprintf(key + 1, 4, "%d", board);
		if (findKeyVal(request, tmp_buffer, TMP_BUFFER_SIZE, key)) attribute[board] = atoi(tmp_buffer);
	}
}

bool change_station_groups(const OTF::Request& request) {
	char key[6] = {'g', 0};
	for (unsigned char board = 0; board < os.nboards; board++) {
		for (unsigned char station = 0; station < 8; station++) {
			unsigned char sid = board * 8 + station;
			snprintf(key + 1, 4, "%d", sid);
			if (findKeyVal(request, tmp_buffer, TMP_BUFFER_SIZE, key)) {
				char* end = nullptr;
				long group = strtol(tmp_buffer, &end, 10);
				if (!tmp_buffer[0] || *end ||
					!((group >= 0 && group < NUM_SEQ_GROUPS) || group == PARALLEL_GROUP_ID)) {
					return false;
				}
				os.attrib_grp[sid] = (unsigned char)group;
			}
		}
	}
	return true;
}

} // namespace

void server_json_stations_main(OTF_PARAMS_DEF) {
	bfill.emit_p(PSTR("\"csver\":1,\"qmax\":$D,\"cs\":["), RUNTIME_QUEUE_SIZE);
	for (uint16_t sid = 0; sid < os.nstations; sid++) {
		CycleSoakPolicy policy;
		const auto result = station_extra_load(sid, MAX_NUM_STATIONS, policy);
		if (result != StationExtraResult::Ok && result != StationExtraResult::Missing) pd.cycle_config_error = true;
		bfill.emit_p(PSTR("[$D,$D,$D]"), policy.enabled, policy.cycle_seconds, policy.soak_seconds);
		if (sid + 1 < os.nstations) bfill.emit_p(PSTR(","));
	}
	bfill.emit_p(PSTR("],"));
	emit_board_attribute(PSTR("masop"), os.attrib_mas);
	emit_board_attribute(PSTR("masop2"), os.attrib_mas2);
	emit_board_attribute(PSTR("masop3"), os.attrib_mas3);
	emit_board_attribute(PSTR("masop4"), os.attrib_mas4);
	emit_board_attribute(PSTR("ignore_rain"), os.attrib_igrd);
	emit_board_attribute(PSTR("ignore_sn1"), os.attrib_igs[0]);
	emit_board_attribute(PSTR("ignore_sn2"), os.attrib_igs[1]);
	if (sensor_available(2)) {
		emit_board_attribute(PSTR("ignore_sn3"), os.attrib_igs[2]);
		emit_board_attribute(PSTR("ignore_sn4"), os.attrib_igs[3]);
	}
	emit_board_attribute(PSTR("stn_dis"), os.attrib_dis);
	emit_board_attribute(PSTR("stn_spe"), os.attrib_spe);
	emit_board_attribute(PSTR("stn_bnd"), os.attrib_bundle);
	bfill.emit_p(PSTR("\"bmt\":$L,"), BUNDLE_MEMBER_TYPE_MASK);
	emit_station_attribute(PSTR("stn_grp"), os.attrib_grp);

	bfill.emit_p(PSTR("\"snames\":["));
	for (unsigned char sid = 0; sid < os.nstations; sid++) {
		os.get_station_name(sid, tmp_buffer);
		bfill.emit_p(PSTR("\"$S\""), tmp_buffer);
		if (sid != os.nstations - 1) bfill.emit_p(PSTR(","));
	}
	bfill.emit_p(PSTR("],\"maxlen\":$D}"), STATION_NAME_SIZE);
}

void server_json_stations(OTF_PARAMS_DEF) {
	if (!process_password(OTF_PARAMS)) return;
	begin_response(res);
	print_header(OTF_PARAMS);
	bfill.emit_p(PSTR("{"));
	server_json_stations_main(OTF_PARAMS);
	handle_return(HTML_OK);
}

void server_json_station_special(OTF_PARAMS_DEF) {
	if (!process_password(OTF_PARAMS)) return;
	begin_response(res);
	print_header(OTF_PARAMS);

	unsigned char comma = 0;
	StationData* data = (StationData*)tmp_buffer;
	bfill.emit_p(PSTR("{"));
	for (unsigned char sid = 0; sid < os.nstations; sid++) {
		unsigned char board = sid >> 3;
		unsigned char station = sid & 0x07;
		if (os.attrib_spe[board] & (1 << station)) {
			os.get_station_data(sid, data);
			if (comma) bfill.emit_p(PSTR(","));
			else comma = 1;
			bfill.emit_p(PSTR("\"$D\":{\"st\":$D,\"sd\":\"$S\"}"), sid, data->type, data->sped);
		}
	}
	bfill.emit_p(PSTR("}"));
	handle_return(HTML_OK);
}

void server_change_stations(OTF_PARAMS_DEF) {
	if (!process_password(OTF_PARAMS)) return;
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("csreset"), true)) {
		if (strcmp(tmp_buffer, "1")) handle_return(HTML_DATA_OUTOFBOUND);
		if (file_exists(STATION_EXTRA_FILENAME) && !remove_file(STATION_EXTRA_FILENAME)) handle_return(HTML_INTERNAL_ERROR);
		const auto result = station_extra_initialize(MAX_NUM_STATIONS);
		pd.cycle_config_error = result != StationExtraResult::Ok;
		handle_return(pd.cycle_config_error ? HTML_INTERNAL_ERROR : HTML_SUCCESS);
	}
	CycleSoakPolicy cycle_policy;
	bool change_cycle = false;
	long cycle_sid = -1;
	const char* cycle_keys[] = {"cse", "csc", "css"};
	uint16_t cycle_values[3] = {};
	bool cycle_present[3] = {};
	for (uint8_t i = 0; i < 3; i++) {
		uint8_t found = 0;
		findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, cycle_keys[i], false, &found);
		if (!found) continue;
		char* end;
		long value = strtol(tmp_buffer, &end, 10);
		if (!tmp_buffer[0] || *end || value < 0 || value > (i ? CYCLE_SOAK_MAX_SECONDS : 1)) handle_return(HTML_DATA_OUTOFBOUND);
		cycle_values[i] = value;
		cycle_present[i] = change_cycle = true;
	}
	if (change_cycle) {
		if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("sid"), true)) handle_return(HTML_DATA_MISSING);
		char* end;
		cycle_sid = strtol(tmp_buffer, &end, 10);
		if (!tmp_buffer[0] || *end || cycle_sid < 0 || cycle_sid >= os.nstations) handle_return(HTML_DATA_OUTOFBOUND);
		const auto loaded = station_extra_load(cycle_sid, MAX_NUM_STATIONS, cycle_policy);
		if (loaded != StationExtraResult::Ok && loaded != StationExtraResult::Missing &&
			!(cycle_present[0] && cycle_present[1] && cycle_present[2])) handle_return(HTML_INTERNAL_ERROR);
		if (cycle_present[0]) cycle_policy.enabled = cycle_values[0];
		if (cycle_present[1]) cycle_policy.cycle_seconds = cycle_values[1];
		if (cycle_present[2]) cycle_policy.soak_seconds = cycle_values[2];
		const bool changing_type = findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("st"), true);
		if (!cycle_soak_policy_valid(cycle_policy, !changing_type && bundle_is_station(cycle_sid) ? bundle_minimum_duration(cycle_sid) : 0)) handle_return(HTML_DATA_OUTOFBOUND);
	}
	uint8_t has_type = 0, has_data = 0;
	findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("st"), true, &has_type);
	findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("sd"), true, &has_data);
	if (has_type != has_data) handle_return(HTML_DATA_MISSING);
	// Validate a changed Bundle definition before any station writes. Count all
	// selected members, conservatively including temporarily ineligible members.
	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("st"), true) && !strcmp(tmp_buffer, "7")) {
		if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("sid"), true)) handle_return(HTML_DATA_MISSING);
		char* end;
		long leader = strtol(tmp_buffer, &end, 10);
		if (!tmp_buffer[0] || *end || leader < 0 || leader >= os.nstations) handle_return(HTML_DATA_OUTOFBOUND);
		CycleSoakPolicy policy = cycle_policy;
		if (!change_cycle) station_extra_load(leader, MAX_NUM_STATIONS, policy);
		if (!findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("sd"), true)) handle_return(HTML_DATA_MISSING);
		uint8_t members[MAX_NUM_BOARDS] = {};
		if (!bundle_validate_definition(leader, tmp_buffer) || !bundle_decode_hex(tmp_buffer, strlen(tmp_buffer), members, os.nboards)) handle_return(HTML_DATA_OUTOFBOUND);
		uint16_t count = 0;
		for (uint16_t sid = 0; sid < MAX_NUM_STATIONS; sid++) count += (members[sid >> 3] >> (sid & 7)) & 1;
		const uint16_t minimum = (count * OUTPUT_STAGGER_MS + 999) / 1000 + 1;
		if (!cycle_soak_policy_valid(policy, minimum)) handle_return(HTML_DATA_OUTOFBOUND);
	}

	bool storage_ok = true;
	unsigned char sid;
	char key[5] = {'s', 0, 0, 0, 0};
	for (sid = 0; sid < os.nstations; sid++) {
		snprintf(key + 1, 4, "%d", sid);
		if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, key)) {
			strReplaceQuoteBackslash(tmp_buffer);
			if (!os.set_station_name(sid, tmp_buffer)) storage_ok = false;
		}
	}

	change_board_attribute(FKV_SOURCE, 'm', os.attrib_mas);
	change_board_attribute(FKV_SOURCE, 'i', os.attrib_igrd);
	change_board_attribute(FKV_SOURCE, 'j', os.attrib_igs[0]);
	change_board_attribute(FKV_SOURCE, 'k', os.attrib_igs[1]);
	if (sensor_available(2)) {
		change_board_attribute(FKV_SOURCE, 'o', os.attrib_igs[2]);
		change_board_attribute(FKV_SOURCE, 'r', os.attrib_igs[3]);
	}
	change_board_attribute(FKV_SOURCE, 'n', os.attrib_mas2);
	change_board_attribute(FKV_SOURCE, 'u', os.attrib_mas3);
	change_board_attribute(FKV_SOURCE, 'v', os.attrib_mas4);
	change_board_attribute(FKV_SOURCE, 'd', os.attrib_dis);
	if (!change_station_groups(FKV_SOURCE)) handle_return(HTML_DATA_OUTOFBOUND);

	if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("sid"), true)) {
		char* end = nullptr;
		long sid_value = strtol(tmp_buffer, &end, 10);
		if (!tmp_buffer[0] || *end || sid_value < 0 || sid_value >= os.nstations) {
			handle_return(HTML_DATA_OUTOFBOUND);
		}
		sid = (unsigned char)sid_value;
		if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, PSTR("st"), true) &&
			findKeyVal(FKV_SOURCE, tmp_buffer + 1, TMP_BUFFER_SIZE - 1, PSTR("sd"), true)) {
			tmp_buffer[0] -= '0';
			tmp_buffer[STATION_SPECIAL_DATA_SIZE] = 0;
			const unsigned char station_type = static_cast<unsigned char>(tmp_buffer[0]);
			if (station_type != STN_TYPE_STANDARD && bundle_station_is_referenced(sid)) {
				handle_return(HTML_NOT_PERMITTED);
			}
			if (station_type == STN_TYPE_BUNDLE) {
				if (!bundle_validate_definition(sid, tmp_buffer + 1)) {
					handle_return(HTML_DATA_OUTOFBOUND);
				}
			}
			if (station_type == STN_TYPE_GPIO) {
				unsigned char gpio = (tmp_buffer[1] - '0') * 10 + tmp_buffer[2] - '0';
				unsigned char active_state = tmp_buffer[3] - '0';
				unsigned char gpio_list[] = PIN_FREE_LIST;
				bool found = false;
				for (unsigned char index = 0; index < sizeof(gpio_list) && !found; index++) {
					if (gpio_list[index] == gpio) found = true;
				}
				if (!found || active_state > 1) handle_return(HTML_DATA_OUTOFBOUND);
			} else if (station_type == STN_TYPE_HTTP || station_type == STN_TYPE_HTTPS ||
				station_type == STN_TYPE_REMOTE_OTC) {
				if (strlen(tmp_buffer + 1) > sizeof(HTTPStationData)) {
					handle_return(HTML_DATA_OUTOFBOUND);
				}
			}
			if (!file_write_block(STATIONS_FILENAME, tmp_buffer,
				(uint32_t)sid * sizeof(StationData) + offsetof(StationData, type),
				STATION_SPECIAL_DATA_SIZE + 1)) storage_ok = false;
			const unsigned char mask = static_cast<unsigned char>(1U << (sid & 7));
			if (station_type == STN_TYPE_BUNDLE) {
				os.attrib_bundle[sid >> 3] |= mask;
				os.attrib_spe[sid >> 3] |= mask;
			} else {
				os.attrib_bundle[sid >> 3] &= ~mask;
				if (station_type == STN_TYPE_STANDARD) os.attrib_spe[sid >> 3] &= ~mask;
			}
		} else if (!change_cycle) {
			handle_return(HTML_DATA_MISSING);
		}
	}

	change_board_attribute(FKV_SOURCE, 'p', os.attrib_spe);
	for (unsigned char board = 0; board < os.nboards; board++) {
		os.attrib_spe[board] |= os.attrib_bundle[board];
	}
	bundle_invalidate();
	if (!os.attribs_save()) storage_ok = false;
	if (change_cycle) {
		if (station_extra_initialize(MAX_NUM_STATIONS) != StationExtraResult::Ok ||
			station_extra_save(cycle_sid, MAX_NUM_STATIONS, cycle_policy) != StationExtraResult::Ok) storage_ok = false;
		pd.cycle_config_error = !storage_ok;
		for (uint16_t id = 0; id < os.nstations; id++) {
			CycleSoakPolicy check;
			if (station_extra_load(id, MAX_NUM_STATIONS, check) != StationExtraResult::Ok) pd.cycle_config_error = true;
		}
	}
	handle_return(storage_ok ? HTML_SUCCESS : HTML_INTERNAL_ERROR);
}
