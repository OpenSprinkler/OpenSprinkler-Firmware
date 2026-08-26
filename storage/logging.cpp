#include "logging.h"

#include "../OpenSprinkler.h"
#include "../core/program.h"
#include "storage/maintenance.h"

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

// Each fixed-width name occupies three bytes in program memory.
static const char log_type_names[] PROGMEM =
	"  \0"
	"s1\0"
	"rd\0"
	"wl\0"
	"fl\0"
	"s2\0"
	"s3\0"
	"s4\0"
	"cu\0";

void write_log(unsigned char type, time_os_t curr_time) {
	if (!os.iopts[IOPT_ENABLE_LOGGING]) return;

	snprintf(tmp_buffer, TMP_BUFFER_SIZE, "%" PRIu32, (uint32_t)curr_time / 86400);
	make_logfile_name(tmp_buffer);

	#if defined(ARDUINO)
	bool new_file = !LittleFS.exists(tmp_buffer);
	if (!prepare_log_write(new_file)) return;
	File file = LittleFS.open(tmp_buffer, "r+");
	if(!file) {
		file = LittleFS.open(tmp_buffer, "w");
		if(!file) return;
	}
	file.seek(0, SeekEnd);
	#else
	struct stat st;
	if(stat(get_filename_fullpath(LOG_DIR), &st)) {
		if(mkdir(get_filename_fullpath(LOG_DIR), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH)) {
			return;
		}
	}
	FILE *file = fopen(get_filename_fullpath(tmp_buffer), "rb+");
	if(!file) {
		file = fopen(get_filename_fullpath(tmp_buffer), "wb");
		if (!file) return;
	}
	fseek(file, 0, SEEK_END);
	#endif

	strcpy_P(tmp_buffer, PSTR("["));

	if(type == LOGDATA_STATION) {
		size_t size = strlen(tmp_buffer);
		snprintf(tmp_buffer + size, TMP_BUFFER_SIZE - size, "%d", pd.lastrun.program);
		strcat_P(tmp_buffer, PSTR(","));
		size = strlen(tmp_buffer);
		snprintf(tmp_buffer + size, TMP_BUFFER_SIZE - size, "%d", pd.lastrun.station);
		strcat_P(tmp_buffer, PSTR(","));
		size = strlen(tmp_buffer);
		snprintf(tmp_buffer + size, TMP_BUFFER_SIZE - size, "%" PRIu32, (uint32_t)pd.lastrun.duration);
	} else {
		uint32_t lvalue=0;
		if(type==LOGDATA_FLOWSENSE) {
			lvalue = (flow_count>os.flowcount_log_start)?(flow_count-os.flowcount_log_start):0;
		}

		size_t size = strlen(tmp_buffer);
		snprintf(tmp_buffer + size, TMP_BUFFER_SIZE - size, "%" PRIu32, lvalue);
		strcat_P(tmp_buffer, PSTR(",\""));
		strcat_P(tmp_buffer, log_type_names+type*3);
		strcat_P(tmp_buffer, PSTR("\","));

		switch(type) {
			case LOGDATA_FLOWSENSE:
			case LOGDATA_SENSOR1:
			case LOGDATA_SENSOR2:
			case LOGDATA_SENSOR3:
			case LOGDATA_SENSOR4: {
				int8_t sidx = sensor_index_from_log_code(type);
				time_os_t t = (sidx >= 0) ? os.sn_sensors[sidx].active_lasttime : 0;
				lvalue = (curr_time>t) ? (curr_time-t) : 0;
				break;
			}
			case LOGDATA_RAINDELAY:
				lvalue = (curr_time>os.raindelay_on_lasttime)?(curr_time-os.raindelay_on_lasttime):0;
				break;
			case LOGDATA_WATERLEVEL:
				lvalue = os.iopts[IOPT_WATER_PERCENTAGE];
				break;
		}
		size = strlen(tmp_buffer);
		snprintf(tmp_buffer + size, TMP_BUFFER_SIZE - size, "%" PRIu32, lvalue);
	}
	strcat_P(tmp_buffer, PSTR(","));
	size_t size = strlen(tmp_buffer);
	snprintf(tmp_buffer + size, TMP_BUFFER_SIZE - size, "%" PRIu32, (uint32_t)curr_time);
	if((os.iopts[IOPT_SENSOR1_TYPE]==SENSOR_TYPE_FLOW) && (type==LOGDATA_STATION)) {
		strcat_P(tmp_buffer, PSTR(","));
		#if defined(ARDUINO)
		dtostrf(flow_last_gpm,5,2,tmp_buffer+strlen(tmp_buffer));
		#else
		snprintf(tmp_buffer+strlen(tmp_buffer), TMP_BUFFER_SIZE, "%5.2f", flow_last_gpm);
		#endif
	}
	strcat_P(tmp_buffer, PSTR("]\r\n"));

	#if defined(ARDUINO)
	const size_t length = strlen(tmp_buffer);
	if (file.write((const uint8_t*)tmp_buffer, length) != length) {
		DEBUG_PRINTLN(F("sprinkler log write failed"));
	}
	file.close();
	#else
	fwrite(tmp_buffer, 1, strlen(tmp_buffer), file);
	fclose(file);
	#endif
}

bool delete_log(char *name) {
	bool ok = true;
	#if defined(ESP8266)
	if (strncmp(name, "all", 3) == 0) {
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
	if (strncmp(name, "all", 3) == 0) {
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
	if (strncmp(name, "all", 3) == 0) {
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
	return ok;
}
