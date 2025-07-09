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
extern OpenSprinkler os;

#if defined(ARDUINO)  // Arduino

	#if defined(ESP8266)
		#include <FS.h>
		#include <LittleFS.h>
	#else
		#include <avr/eeprom.h>
		#include "SdFat.h"
		extern SdFat sd;
	#endif

#else // RPI/LINUX

#include <stdio.h>

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

void delay(ulong howLong)
{
	struct timespec sleeper, dummy ;

	sleeper.tv_sec  = (time_os_t)(howLong / 1000) ;
	sleeper.tv_nsec = (long)(howLong % 1000) * 1000000 ;

	nanosleep (&sleeper, &dummy) ;
}

void delayMicroseconds (ulong howLong)
{
	struct timespec sleeper ;
	unsigned int uSecs = howLong % 1000000 ;
	unsigned int wSecs = howLong / 1000000 ;

	/**/ if (howLong ==		0)
		return ;
	else if (howLong < 100)
		delayMicrosecondsHard (howLong) ;
	else
	{
		sleeper.tv_sec  = wSecs ;
		sleeper.tv_nsec = (long)(uSecs * 1000L) ;
		nanosleep (&sleeper, NULL) ;
	}
}

void delayMicrosecondsHard (ulong howLong)
{
	struct timeval tNow, tLong, tEnd ;

	gettimeofday (&tNow, NULL) ;
	tLong.tv_sec  = howLong / 1000000 ;
	tLong.tv_usec = howLong % 1000000 ;
	timeradd (&tNow, &tLong, &tEnd) ;

	while (timercmp (&tNow, &tEnd, <))
		gettimeofday (&tNow, NULL) ;
}

static uint64_t epochMilli, epochMicro ;

void initialiseEpoch()
{
	struct timeval tv ;

	gettimeofday (&tv, NULL) ;
	epochMilli = (uint64_t)tv.tv_sec * (uint64_t)1000    + (uint64_t)(tv.tv_usec / 1000) ;
	epochMicro = (uint64_t)tv.tv_sec * (uint64_t)1000000 + (uint64_t)(tv.tv_usec) ;
}

// ulong millis (void)
// {
// 	struct timeval tv ;
// 	uint64_t now ;

// 	gettimeofday (&tv, NULL) ;
// 	now  = (uint64_t)tv.tv_sec * (uint64_t)1000 + (uint64_t)(tv.tv_usec / 1000) ;

// 	return (ulong)(now - epochMilli) ;
// }

ulong micros (void)
{
	struct timeval tv ;
	uint64_t now ;

	gettimeofday (&tv, NULL) ;
	now  = (uint64_t)tv.tv_sec * (uint64_t)1000000 + (uint64_t)tv.tv_usec ;

	return (ulong)(now - epochMicro) ;
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
	unsigned long dst, gw;
	unsigned int flags, refcnt, use, metric, mask, mtu, window, irtt;

	FILE *filp;
	char buf[512];
	char term;
	filp = fopen("/proc/net/route", "r");
	if(filp) {
		while(fgets(buf, sizeof(buf), filp) != NULL) {
			if(sscanf(buf, "%s %lx %lx %X %d %d %d %lx %d %d %d", iface, &dst, &gw, &flags, &refcnt, &use, &metric, &mask, &mtu, &window, &irtt) == 11) {
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

    char buffer[100];

    BoardType res = BoardType::Unknown;

    int total = fread(buffer, 1, sizeof(buffer), file);

    if (prefix("raspberrypi", buffer)) {
        res = BoardType::RaspberryPi_Unknown;
        const char *cpu_buf = buffer;
        size_t index = 0;

        // model and cpu is seperated by a null byte
        while (index < (total - 1) && cpu_buf[index]) {
            index += 1;
        }

        cpu_buf += index + 1;  
        
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

#elif defined(ARDUINO)

	sd.chdir("/");
	if (!sd.exists(fn))  return;
	sd.remove(fn);

#else

	remove(get_filename_fullpath(fn));

#endif
}

bool file_exists(const char *fn) {
#if defined(ESP8266)

	return LittleFS.exists(fn);

#elif defined(ARDUINO)

	sd.chdir("/");
	return sd.exists(fn);

#else

	FILE *file;
	file = fopen(get_filename_fullpath(fn), "rb");
	if(file) {fclose(file); return true;}
	else {return false;}

#endif
}

// file functions
void file_read_block(const char *fn, void *dst, ulong pos, ulong len) {
#if defined(ESP8266)

	// do not use File.read_byte or read_byteUntil because it's very slow
	File f = LittleFS.open(fn, "r");
	if(f) {
		f.seek(pos, SeekSet);
		f.read((unsigned char*)dst, len);
		f.close();
	}

#elif defined(ARDUINO)

	sd.chdir("/");
	SdFile file;
	if(file.open(fn, O_READ)) {
		file.seekSet(pos);
		file.read(dst, len);
		file.close();
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

void file_write_block(const char *fn, const void *src, ulong pos, ulong len) {
#if defined(ESP8266)

	File f = LittleFS.open(fn, "r+");
	if(!f) f = LittleFS.open(fn, "w");
	if(f) {
		f.seek(pos, SeekSet);
		f.write((unsigned char*)src, len);
		f.close();
	}

#elif defined(ARDUINO)

	sd.chdir("/");
	SdFile file;
	int ret = file.open(fn, O_CREAT | O_RDWR);
	if(!ret) return;
	file.seekSet(pos);
	file.write(src, len);
	file.close();

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

void file_copy_block(const char *fn, ulong from, ulong to, ulong len, void *tmp) {
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

#elif defined(ARDUINO)

	sd.chdir("/");
	SdFile file;
	int ret = file.open(fn, O_RDWR);
	if(!ret) return;
	file.seekSet(from);
	file.read(tmp, len);
	file.seekSet(to);
	file.write(tmp, len);
	file.close();

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
unsigned char file_cmp_block(const char *fn, const char *buf, ulong pos) {
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

#elif defined(ARDUINO)

	sd.chdir("/");
	SdFile file;
	if(file.open(fn, O_READ)) {
		file.seekSet(pos);
		char c = file.read();
		while(*buf && (c==*buf)) {
			buf++;
			c=file.read();
		}
		file.close();
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

unsigned char file_read_byte(const char *fn, ulong pos) {
	unsigned char v = 0;
	file_read_block(fn, &v, pos, 1);
	return v;
}

void file_write_byte(const char *fn, ulong pos, unsigned char v) {
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
ulong water_time_resolve(uint16_t v) {
	if(v==65534) {
		return (os.nvdata.sunset_time-os.nvdata.sunrise_time) * 60L;
	} else if(v==65535) {
		return (os.nvdata.sunrise_time+1440-os.nvdata.sunset_time) * 60L;
	} else	{
		return v;
	}
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