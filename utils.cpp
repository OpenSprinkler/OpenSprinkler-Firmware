/* OpenSprinkler Unified Firmware
 * Copyright (C) 2015 by Ray Wang (ray@opensprinkler.com)
 *
 * Utility functions
 * Feb 2015 @ OpenSprinkler.com
 *
 * This file is part of the OpenSprinkler library
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <http://www.gnu.org/licenses/>
 */

#include "utils.h"
#include "types.h"
#include "OpenSprinkler.h"
#include <math.h>
extern OpenSprinkler os;

bool parse_program_duration(const char *value, uint32_t *duration) {
	if (!value || !duration || !value[0]) return false;

	uint32_t parsed = 0;
	for (const char *p = value; *p; p++) {
		if (*p < '0' || *p > '9') return false;
		uint8_t digit = *p - '0';
		if (parsed > (MAX_PROGRAMMED_DURATION - digit) / 10) return false;
		parsed = parsed * 10 + digit;
	}

	if (!parsed) return false;
	*duration = parsed;
	return true;
}

#if defined(ESP8266)  // Arduino
	#include <FS.h>
	#include <LittleFS.h>

#else // RPI/LINUX

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

char* get_runtime_path() {
	static char path[PATH_MAX];
	static unsigned char query = 1;

	#ifdef __APPLE__
		strcpy(path, "./");
		return path;
	#endif

	if(query) {
		if(readlink("/proc/self/exe", path, PATH_MAX ) <= 0) {
			return NULL;
		}
		char* path_end = strrchr(path, '/');
		if(path_end == NULL) {
			return NULL;
		}
		path_end++;
		*path_end=0;
		query = 0;
	}
	return path;
}

static const char *data_dir = NULL;

const char* get_data_dir(void) {
	if (data_dir) {
		return data_dir;
	} else {
		return get_runtime_path();
	}
}

void set_data_dir(const char *new_data_dir) {
	data_dir = new_data_dir;
}

char* get_filename_fullpath(const char *filename) {
	static char fullpath[PATH_MAX];
	strcpy(fullpath, get_data_dir());
	if ('/' != fullpath[strlen(fullpath) - 1]) {
		strcat(fullpath, "/");
	}
	strcat(fullpath, filename);
	return fullpath;
}

#if defined(OSPI)
unsigned int detect_rpi_rev() {
	FILE * filp;
	unsigned int rev;
	char buf[512];
	char term;

	rev = 0;
	filp = fopen ("/proc/cpuinfo", "r");

	if (filp != NULL) {
		while (fgets(buf, sizeof(buf), filp) != NULL) {
			if (!strncasecmp("revision\t", buf, 9)) {
				if (sscanf(buf+strlen(buf)-5, "%x%c", &rev, &term) == 2) {
					if (term == '\n') break;
					rev = 0;
				}
			}
		}
		fclose(filp);
	}
	return rev;
}

route_t get_route() {
	route_t route;
	char iface[16];
	uint32_t dst, gw, mask;
	unsigned int flags, refcnt, use, metric, mtu, window, irtt;

	FILE *filp;
	char buf[512];
	char term;
	filp = fopen("/proc/net/route", "r");
	if(filp) {
		while(fgets(buf, sizeof(buf), filp) != NULL) {
			if(sscanf(buf, "%15s %x %x %X %d %d %d %x %d %d %d", iface, &dst, &gw, &flags, &refcnt, &use, &metric, &mask, &mtu, &window, &irtt) == 11) {
				if(flags & RTF_UP) {
					if(dst==0) {
						strcpy(route.iface, iface);
						route.gateway = gw;
						route.destination = dst;
					}
				}
			}
		}
		fclose(filp);
	}
	return route;
}

in_addr_t get_ip_address(char *iface) {
	struct ifaddrs *ifaddr;
	struct ifaddrs *ifa;
	in_addr_t ip = 0;
	if(getifaddrs(&ifaddr) == -1) {
		return 0;
	}

	ifa = ifaddr;

	while(ifa) {
		if(ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
			if(strcmp(ifa->ifa_name, iface)==0) {
				ip = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr;
				break;
			}
		}
		ifa = ifa->ifa_next;
	}
	freeifaddrs(ifaddr);
	return ip;
}
#endif

bool prefix(const char *pre, const char *str) {
	return strncmp(pre, str, strlen(pre)) == 0;
}

BoardType get_board_type() {
	FILE *file = fopen("/proc/device-tree/compatible", "rb");
	if (file == NULL) {
		return BoardType::Unknown;
	}

	char buffer[101] = {0};

	BoardType res = BoardType::Unknown;

	size_t total = fread(buffer, 1, sizeof(buffer) - 1, file);
	fclose(file);

	if (total >= strlen("raspberrypi") && prefix("raspberrypi", buffer)) {
		res = BoardType::RaspberryPi_Unknown;
		// Model and CPU identifiers are separated by a null byte.
		const char *separator = (const char*)memchr(buffer, '\0', total);
		if (!separator || separator + 1 >= buffer + total) return res;
		const char *cpu_buf = separator + 1;

		if (!strcmp("brcm,bcm2712", cpu_buf)) {
			// Pi 5
			res = BoardType::RaspberryPi_bcm2712;
		} else if (!strcmp("brcm,bcm2711", cpu_buf)) {
			// Pi 4
			res = BoardType::RaspberryPi_bcm2711;
		} else if (!strcmp("brcm,bcm2837", cpu_buf)) {
			// Pi 3 / Pi Zero 2
			res = BoardType::RaspberryPi_bcm2837;
		} else if (!strcmp("brcm,bcm2836", cpu_buf)) {
			// Pi 2
			res = BoardType::RaspberryPi_bcm2836;
		} else if (!strcmp("brcm,bcm2835", cpu_buf)) {
			// Pi / Pi Zero
			res = BoardType::RaspberryPi_bcm2835;
		}
	}

	return res;
}

#endif


void remove_file(const char *fn) {
#if defined(ESP8266)

	if(!LittleFS.exists(fn)) return;
	LittleFS.remove(fn);

#else

	remove(get_filename_fullpath(fn));

#endif
}

void ensure_log_dir() {
#if !defined(ESP8266)
	const char *dir = get_filename_fullpath(LOG_DIR);
	struct stat st;
	if (stat(dir, &st) != 0) {
		mkdir(dir, S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH);
	}
#endif
}

bool file_exists(const char *fn) {
#if defined(ESP8266)

	return LittleFS.exists(fn);

#else

	FILE *file;
	file = fopen(get_filename_fullpath(fn), "rb");
	if(file) {fclose(file); return true;}
	else {return false;}

#endif
}

os_file_type file_open(const char *fn, FileOpenMode mode) {
	#if defined(ESP8266)
	switch (mode) {
		default:
		case FileOpenMode::Read:
			return LittleFS.open(fn, "r");
		case FileOpenMode::ReadWrite:
			if (!LittleFS.exists(fn)) {
				File f = LittleFS.open(fn, "w");
				if (!f) return f;
				f.close();
			}
			return LittleFS.open(fn, "r+");
		case FileOpenMode::WriteTruncate:
			return LittleFS.open(fn, "w");
		case FileOpenMode::ReadWriteTruncate:
			return LittleFS.open(fn, "w+");
		case FileOpenMode::Append:
			return LittleFS.open(fn, "a");
		case FileOpenMode::ReadAppend:
			return LittleFS.open(fn, "a+");
	}
	#else
	char *full_file = get_filename_fullpath(fn);
	switch (mode) {
		default:
		case FileOpenMode::Read:
			return fopen(full_file, "rb");
		case FileOpenMode::ReadWrite: {
			int fd = open(full_file, O_RDWR | O_CREAT, 0644);
			if (fd == -1) return nullptr;
			FILE *file = fdopen(fd, "rb+");
			if (!file) close(fd);
			return file;
		}
		case FileOpenMode::WriteTruncate:
			return fopen(full_file, "wb");
		case FileOpenMode::ReadWriteTruncate:
			return fopen(full_file, "wb+");
		case FileOpenMode::Append:
			return fopen(full_file, "ab");
		case FileOpenMode::ReadAppend:
			return fopen(full_file, "ab+");
	}

	#endif
}

void file_close(os_file_type f) {
	#if defined(ESP8266)
	f.close();
	#else
	fclose(f);
	#endif
}

bool file_seek(os_file_type f, uint32_t position, FileSeekMode mode) {
	#if defined(ESP8266)
	switch (mode) {
		case FileSeekMode::Set:
			return f.seek(position, fs::SeekMode::SeekSet);
		case FileSeekMode::Current:
			return f.seek(position, fs::SeekMode::SeekCur);
		case FileSeekMode::End:
			return f.seek(position, fs::SeekMode::SeekEnd);
	}
	#else
	switch (mode) {
		case FileSeekMode::Set:
			return fseek(f, position, SEEK_SET) == 0;
		case FileSeekMode::Current:
			return fseek(f, position, SEEK_CUR) == 0;
		case FileSeekMode::End:
			return fseek(f, position, SEEK_END) == 0;
	}
	#endif

	return false;
}

bool file_seek(os_file_type f, uint32_t position) {
	return file_seek(f, position, FileSeekMode::Set);
}

int file_read(os_file_type f, void *target, uint32_t len) {
	#if defined(ESP8266)
	return f.read((uint8_t*)target, len);
	#else
	return fread(target, 1, len, f);
	#endif
}

int file_write(os_file_type f, const void *source, uint32_t len) {
	#if defined(ESP8266)
	return f.write((const uint8_t*)source, len);
	#else
	return fwrite(source, 1, len, f);
	#endif
}

uint32_t file_size(os_file_type f) {
	#if defined(ESP8266)
	return f.size();
	#else
	long cur = ftell(f);
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, cur, SEEK_SET);
	return (uint32_t)(sz >= 0 ? sz : 0);
	#endif
}

// file functions
void file_read_block(const char *fn, void *dst, uint32_t pos, uint32_t len) {
#if defined(ESP8266)

	// do not use File.read_byte or read_byteUntil because it's very slow
	File f = LittleFS.open(fn, "r");
	if(f) {
		f.seek(pos, SeekSet);
		f.read((unsigned char*)dst, len);
		f.close();
	}

#else

	FILE *fp = fopen(get_filename_fullpath(fn), "rb");
	if(fp) {
		fseek(fp, pos, SEEK_SET);
		fread(dst, 1, len, fp);
		fclose(fp);
	}

#endif
}

void file_write_block(const char *fn, const void *src, uint32_t pos, uint32_t len) {
#if defined(ESP8266)

	File f = LittleFS.open(fn, "r+");
	if(!f) f = LittleFS.open(fn, "w");
	if(f) {
		f.seek(pos, SeekSet);
		f.write((unsigned char*)src, len);
		f.close();
	}

#else

	FILE *fp = fopen(get_filename_fullpath(fn), "rb+");
	if(!fp) {
		fp = fopen(get_filename_fullpath(fn), "wb+");
	}
	if(fp) {
		fseek(fp, pos, SEEK_SET); //this fails silently without the above change
		fwrite(src, 1, len, fp);
		fclose(fp);
	}

#endif

}

void file_copy_block(const char *fn, uint32_t from, uint32_t to, uint32_t len, void *tmp) {
	// assume tmp buffer is provided and is larger than len
	// todo future: if tmp buffer is not provided, do unsigned char-to-unsigned char copy
	if(tmp==NULL) { return; }
#if defined(ESP8266)

	File f = LittleFS.open(fn, "r+");
	if(!f) return;
	f.seek(from, SeekSet);
	f.read((unsigned char*)tmp, len);
	f.seek(to, SeekSet);
	f.write((unsigned char*)tmp, len);
	f.close();

#else

	FILE *fp = fopen(get_filename_fullpath(fn), "rb+");
	if(!fp) return;
	fseek(fp, from, SEEK_SET);
	fread(tmp, 1, len, fp);
	fseek(fp, to, SEEK_SET);
	fwrite(tmp, 1, len, fp);
	fclose(fp);

#endif

}

// compare a block of content
unsigned char file_cmp_block(const char *fn, const char *buf, uint32_t pos) {
#if defined(ESP8266)

	File f = LittleFS.open(fn, "r");
	if(f) {
		f.seek(pos, SeekSet);
		char c = f.read();
		while(*buf && (c==*buf)) {
			buf++;
			c=f.read();
		}
		f.close();
		return (*buf==c)?0:1;
	}

#else

	FILE *fp = fopen(get_filename_fullpath(fn), "rb");
	if(fp) {
		fseek(fp, pos, SEEK_SET);
		char c = fgetc(fp);
		while(*buf && (c==*buf)) {
			buf++;
			c=fgetc(fp);
		}
		fclose(fp);
		return (*buf==c)?0:1;
	}

#endif
	return 1;
}

unsigned char file_read_byte(const char *fn, uint32_t pos) {
	unsigned char v = 0;
	file_read_block(fn, &v, pos, 1);
	return v;
}

void file_write_byte(const char *fn, uint32_t pos, unsigned char v) {
	file_write_block(fn, &v, pos, 1);
}

// copy n-character string from program memory with ending 0
void strncpy_P0(char* dest, const char* src, int n) {
	unsigned char i;
	for(i=0;i<n;i++) {
		*dest=pgm_read_byte(src++);
		dest++;
	}
	*dest=0;
}

// resolve water time
/* special values:
 * 65534: sunrise to sunset duration
 * 65535: sunset to sunrise duration
 */
uint32_t water_time_resolve(uint16_t v) {
	if(v==65534) {
		return (os.nvdata.sunset_time-os.nvdata.sunrise_time) * 60L;
	} else if(v==65535) {
		return (os.nvdata.sunrise_time+1440-os.nvdata.sunset_time) * 60L;
	} else	{
		return v;
	}
}

uint32_t water_time_scale(uint32_t duration, uint8_t weather_percent, float sensor_factor) {
	if (!duration || !weather_percent || !isfinite(sensor_factor) || sensor_factor <= 0.f) return 0;
	double scaled = (double)duration * weather_percent / 100.0 * sensor_factor;
	if (scaled >= MAX_RUNTIME_DURATION) return MAX_RUNTIME_DURATION;
	return (uint32_t)scaled;
}

// encode a 16-bit signed water time (-600 to 600)
// to unsigned byte (0 to 240)
unsigned char water_time_encode_signed(int16_t i) {
	i=(i>600)?600:i;
	i=(i<-600)?-600:i;
	return (i+600)/5;
}

// decode a 8-bit unsigned byte (0 to 240)
// to a 16-bit signed water time (-600 to 600)
int16_t water_time_decode_signed(unsigned char i) {
	i=(i>240)?240:i;
	return ((int16_t)i-120)*5;
}


/** Convert a single hex digit character to its integer value */
static unsigned char h2int(char c) {
		if (c >= '0' && c <='9'){
				return((unsigned char)c - '0');
		}
		if (c >= 'a' && c <='f'){
				return((unsigned char)c - 'a' + 10);
		}
		if (c >= 'A' && c <='F'){
				return((unsigned char)c - 'A' + 10);
		}
		return(0);
}

/** Decode a url string in place, e.g "hello%20joe" or "hello+joe" becomes "hello joe"*/
void urlDecode (char *urlbuf) {
	if(!urlbuf) return;
	char c;
	char *dst = urlbuf;
	while ((c = *urlbuf) != 0) {
		if (c == '+') c = ' ';
		if (c == '%') {
			c = *++urlbuf;
			c = (h2int(c) << 4) | h2int(*++urlbuf);
		}
		*dst++ = c;
		urlbuf++;
	}
	*dst = '\0';
}

/** Encode a url string in place, e.g "hello joe" to "hello%20joe"
  * IMPORTANT: assume the buffer is large enough to fit the output
  */
void urlEncode(char *urlbuf) {
	if(!urlbuf) return;

	// First, find the original length
	size_t len = strlen(urlbuf);

	// Compute new length
	size_t extra = 0;
	for (size_t i = 0; i < len; i++) {
		unsigned char c = urlbuf[i];
		if (c == ' ' || c == '\"' || c == '\'' || c == '<' || c == '>' || c > 127) {
			extra += 2; // encoded, extra 2
		}
	}

	size_t newlen = len + extra;
	urlbuf[newlen] = 0; // Null-terminate the new string

	// Process in reverse to avoid overwriting
	for (int i = len - 1, j = newlen - 1; i >= 0; i--) {
		unsigned char c = urlbuf[i];
		if (c == ' ' || c == '\"' || c == '\'' || c == '<' || c == '>' || c > 127) {
			static const char hex[] = "0123456789ABCDEF";
			urlbuf[j--] = hex[c & 0xF];
			urlbuf[j--] = hex[(c >> 4) & 0xF];
			urlbuf[j--] = '%';
		} else {
			urlbuf[j--] = c;
		}
	}
}


void peel_http_header(char* buffer) { // remove the HTTP header
	uint16_t i=0;
	bool eol=true;
	while(i<ETHER_BUFFER_SIZE) {
		char c = buffer[i];
		if(c==0)	return;
		if(c=='\n' && eol) {
			// copy
			i++;
			int j=0;
			while(i<ETHER_BUFFER_SIZE) {
				buffer[j]=buffer[i];
				if(buffer[j]==0)	break;
				i++;
				j++;
			}
			return;
		}
		if(c=='\n') {
			eol=true;
		} else if (c!='\r') {
			eol=false;
		}
		i++;
	}
}

void strReplace(char *str, char c, char r) {
	for(unsigned char i=0;i<strlen(str);i++) {
		if(str[i]==c) str[i]=r;
	}
}

void strReplaceQuoteBackslash(char *buf) {
	strReplace(buf, '\"', '\'');
	strReplace(buf, '\\', '/');
}

static const unsigned char month_days[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

bool isLastDayofMonth(unsigned char month, unsigned char day) {
	return day == month_days[month-1];
}

bool isValidDate(unsigned char m, unsigned char d) {
	if(m<1 || m>12) return false;
	if(d<1 || d>month_days[m-1]) return false;
	return true;
}

bool isValidDate(uint16_t date) {
	if (date < MIN_ENCODED_DATE || date > MAX_ENCODED_DATE) {
		return false;
	}
	unsigned char month = date >> 5;
	unsigned char day = date & 31;
	return isValidDate(month, day);
}

bool isLeapYear(uint16_t y){ // Accepts 4 digit year and returns if leap year
	return (y%400==0) || ((y%4==0) && (y%100!=0));
}

#if defined(ESP8266)
unsigned char hex2dec(const char *hex) {
	return strtol(hex, NULL, 16);
}

bool isHex(char c) {
	if(c>='0' && c<='9') return true;
	if(c>='a' && c<='f') return true;
	if(c>='A' && c<='F') return true;
	return false;
}

bool isValidMAC(const char *_mac) {
	char mac[18], *hex;
	strncpy(mac, _mac, 18);
	mac[17] = 0;
	unsigned char count = 0;
	hex = strtok(mac, ":");
	if(strlen(hex)!=2) return false;
	if(!isHex(hex[0]) || !isHex(hex[1])) return false;
	count++;
	while(true) {
		hex = strtok(NULL, ":");
		if(hex==NULL) break;
		if(strlen(hex)!=2) return false;
		if(!isHex(hex[0]) || !isHex(hex[1])) return false;
		count++;
		yield();
	}
	if(count!=6) return false;
	else return true;
}

void str2mac(const char *_str, unsigned char mac[]) {
	char str[18], *hex;
	strncpy(str, _str, 18);
	str[17] = 0;
	unsigned char count=0;
	hex = strtok(str, ":");
	mac[count] = hex2dec(hex);
	count++;
	while(true) {
		hex = strtok(NULL, ":");
		if(hex==NULL) break;
		mac[count++] = hex2dec(hex);
		yield();
	}
}
#endif

char dec2hexchar(unsigned char dec) {
	if(dec<10) return '0'+dec;
	else return 'A'+(dec-10);
}
