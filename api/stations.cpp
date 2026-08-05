#include "handler_context.h"
#include "handlers.h"

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

	unsigned char sid;
	char key[5] = {'s', 0, 0, 0, 0};
	for (sid = 0; sid < os.nstations; sid++) {
		snprintf(key + 1, 4, "%d", sid);
		if (findKeyVal(FKV_SOURCE, tmp_buffer, TMP_BUFFER_SIZE, key)) {
			strReplaceQuoteBackslash(tmp_buffer);
			os.set_station_name(sid, tmp_buffer);
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
			if (tmp_buffer[0] == STN_TYPE_GPIO) {
				unsigned char gpio = (tmp_buffer[1] - '0') * 10 + tmp_buffer[2] - '0';
				unsigned char active_state = tmp_buffer[3] - '0';
				unsigned char gpio_list[] = PIN_FREE_LIST;
				bool found = false;
				for (unsigned char index = 0; index < sizeof(gpio_list) && !found; index++) {
					if (gpio_list[index] == gpio) found = true;
				}
				if (!found || active_state > 1) handle_return(HTML_DATA_OUTOFBOUND);
			} else if (tmp_buffer[0] == STN_TYPE_HTTP || tmp_buffer[0] == STN_TYPE_HTTPS ||
				tmp_buffer[0] == STN_TYPE_REMOTE_OTC) {
				if (strlen(tmp_buffer + 1) > sizeof(HTTPStationData)) {
					handle_return(HTML_DATA_OUTOFBOUND);
				}
			}
			file_write_block(STATIONS_FILENAME, tmp_buffer,
				(uint32_t)sid * sizeof(StationData) + offsetof(StationData, type),
				STATION_SPECIAL_DATA_SIZE + 1);
		} else {
			handle_return(HTML_DATA_MISSING);
		}
	}

	change_board_attribute(FKV_SOURCE, 'p', os.attrib_spe);
	os.attribs_save();
	handle_return(HTML_SUCCESS);
}
