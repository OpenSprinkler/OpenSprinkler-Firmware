#include "logging.h"

#include "../OpenSprinkler.h"
#include "../core/program.h"
#include "storage/files.h"
#include "storage/maintenance.h"
#include "storage/sprinkler_log.h"

#if defined(ARDUINO)
	#include <FS.h>
	#include <LittleFS.h>
#else
	#include <dirent.h>
	#include <cerrno>
	#include <limits.h>
	#include <sys/stat.h>
	#include <unistd.h>
#endif

extern char tmp_buffer[];
extern OpenSprinkler os;
extern ProgramData pd;
extern uint32_t flow_count;
extern float flow_last_gpm;

void make_logfile_name(char *name) {
	strcpy(tmp_buffer+TMP_BUFFER_SIZE-10, name);
	strcpy(tmp_buffer, LOG_DIR);
	strcat(tmp_buffer, tmp_buffer+TMP_BUFFER_SIZE-10);
	strcat_P(tmp_buffer, PSTR(".txt"));
}

void write_log(unsigned char type, time_os_t curr_time) {
	if (!os.iopts[IOPT_ENABLE_LOGGING]) return;
	SprinklerLogRecord record = {};
	record.timestamp = (uint32_t)curr_time;
	record.type = type;

	if (type == LOGDATA_STATION) {
		record.program = pd.lastrun.program;
		record.station = pd.lastrun.station;
		record.value = pd.lastrun.duration;
		if (os.iopts[IOPT_SENSOR1_TYPE] == SENSOR_TYPE_FLOW) {
			record.flags |= SPRINKLER_LOG_FLAG_FLOW;
			float scaled = flow_last_gpm * 100.0f;
			if (scaled > 0.0f) {
				record.aux = scaled >= 4294967295.0f ? UINT32_MAX : (uint32_t)(scaled + 0.5f);
			}
		}
	} else {
		if (type == LOGDATA_FLOWSENSE) {
			record.aux = flow_count > os.flowcount_log_start ?
				flow_count - os.flowcount_log_start : 0;
		}
		switch (type) {
		case LOGDATA_FLOWSENSE:
		case LOGDATA_SENSOR1:
		case LOGDATA_SENSOR2:
		case LOGDATA_SENSOR3:
		case LOGDATA_SENSOR4: {
			int8_t sidx = sensor_index_from_log_code(type);
			time_os_t t = sidx >= 0 ? os.sn_sensors[sidx].active_lasttime : 0;
			record.value = curr_time > t ? curr_time - t : 0;
			break;
		}
		case LOGDATA_RAINDELAY:
			record.value = curr_time > os.raindelay_on_lasttime ?
				curr_time - os.raindelay_on_lasttime : 0;
			break;
		case LOGDATA_WATERLEVEL:
			record.value = os.iopts[IOPT_WATER_PERCENTAGE];
			break;
		}
	}
	if (!sprinkler_log_append(record)) DEBUG_PRINTLN(F("sprinkler log write failed"));
}

bool delete_log(char *name) {
	bool ok = true;
	bool delete_all = strncmp(name, "all", 3) == 0;
	uint32_t day = delete_all ? 0 : (uint32_t)strtoul(name, nullptr, 10);
	#if defined(ESP8266)
	if (delete_all) {
		while (true) {
			String filename;
			Dir dir = LittleFS.openDir(LOG_DIR);
			while (dir.next()) {
				if (is_sprinkler_log_filename(dir.fileName().c_str())) {
					filename = String(LOG_DIR) + dir.fileName();
					break;
				}
			}
			if (filename.length() == 0) break;
			if (!LittleFS.remove(filename)) { ok = false; break; }
		}
	} else {
		make_logfile_name(name);
		if (LittleFS.exists(tmp_buffer)) ok = LittleFS.remove(tmp_buffer);
	}
	#elif defined(ESP32)
	if (delete_all) {
		while (true) {
			String filename;
			File dir = LittleFS.open(LOG_DIR);
			if (!dir || !dir.isDirectory()) break;
			for (File file = dir.openNextFile(); file; file = dir.openNextFile()) {
				String candidate = file.name();
				file.close();
				if (is_sprinkler_log_filename(candidate.c_str())) { filename = candidate; break; }
			}
			dir.close();
			if (filename.length() == 0) break;
			if (!LittleFS.remove(filename)) { ok = false; break; }
		}
	} else {
		make_logfile_name(name);
		if (LittleFS.exists(tmp_buffer)) ok = LittleFS.remove(tmp_buffer);
	}
	#else
	if (delete_all) {
		char log_dir[PATH_MAX];
		strcpy(log_dir, get_filename_fullpath(LOG_DIR));
		DIR *d = opendir(log_dir);
		if (d) {
			int log_dir_fd = dirfd(d);
			struct dirent *ent;
			while ((ent = readdir(d)) != NULL) {
				if (is_sprinkler_log_filename(ent->d_name)) {
					if (unlinkat(log_dir_fd, ent->d_name, 0) != 0) ok = false;
				}
			}
			closedir(d);
		} else if (errno != ENOENT) ok = false;
	} else {
		make_logfile_name(name);
		if (remove(get_filename_fullpath(tmp_buffer)) != 0 && errno != ENOENT) ok = false;
	}
	#endif
	bool ring_ok = delete_all ? sprinkler_log_clear() : sprinkler_log_delete_day(day);
	return ok && ring_ok;
}

bool delete_logs_before(uint32_t day) {
	bool ok = true;
	#if defined(ESP8266)
	while (true) {
		String filename;
		Dir dir = LittleFS.openDir(LOG_DIR);
		while (dir.next()) {
			if (!is_sprinkler_log_filename(dir.fileName().c_str())) continue;
			if ((uint32_t)strtoul(dir.fileName().c_str(), nullptr, 10) >= day) continue;
			filename = String(LOG_DIR) + dir.fileName();
			break;
		}
		if (filename.length() == 0) break;
		if (!LittleFS.remove(filename)) { ok = false; break; }
		yield();
	}
	#elif defined(ESP32)
	while (true) {
		String filename;
		File dir = LittleFS.open(LOG_DIR);
		if (!dir || !dir.isDirectory()) break;
		for (File file = dir.openNextFile(); file; file = dir.openNextFile()) {
			String candidate = file.name();
			file.close();
			if (!is_sprinkler_log_filename(candidate.c_str())) continue;
			const char* base = strrchr(candidate.c_str(), '/');
			base = base ? base + 1 : candidate.c_str();
			if ((uint32_t)strtoul(base, nullptr, 10) >= day) continue;
			filename = candidate;
			break;
		}
		dir.close();
		if (filename.length() == 0) break;
		if (!LittleFS.remove(filename)) { ok = false; break; }
		yield();
	}
	#else
	char log_dir[PATH_MAX];
	strcpy(log_dir, get_filename_fullpath(LOG_DIR));
	DIR *directory = opendir(log_dir);
	if (directory) {
		int log_dir_fd = dirfd(directory);
		struct dirent *entry;
		while ((entry = readdir(directory)) != nullptr) {
			if (!is_sprinkler_log_filename(entry->d_name)) continue;
			if ((uint32_t)strtoul(entry->d_name, nullptr, 10) >= day) continue;
			if (unlinkat(log_dir_fd, entry->d_name, 0) != 0) ok = false;
		}
		closedir(directory);
	} else if (errno != ENOENT) {
		ok = false;
	}
	#endif
	return sprinkler_log_delete_before(day) && ok;
}
