/* OpenSprinkler Unified (AVR/RPI/BBB/LINUX) Firmware
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
 * <http://www.gnu.org/licenses/>.
 */

#include "utils.h"
#include "OpenSprinkler.h"
extern OpenSprinkler os;

#if defined(ARDUINO)	// Arduino

	#if defined(ESP8266)
		#include <FS.h>
	#else
		#include <avr/eeprom.h>
		#include "SdFat.h"
		extern SdFat sd;
	#endif

#else // RPI/BBB

char* get_runtime_path() {
	static char path[PATH_MAX];
	static byte query = 1;

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

char* get_filename_fullpath(const char *filename) {
	static char fullpath[PATH_MAX];
	strcpy(fullpath, get_runtime_path());
	strcat(fullpath, filename);
	return fullpath;
}

void delay(ulong howLong)
{
	struct timespec sleeper, dummy ;

	sleeper.tv_sec	= (time_t)(howLong / 1000) ;
	sleeper.tv_nsec = (long)(howLong % 1000) * 1000000 ;

	nanosleep (&sleeper, &dummy) ;
}

void delayMicrosecondsHard (ulong howLong)
{
	struct timeval tNow, tLong, tEnd ;

	gettimeofday (&tNow, NULL) ;
	tLong.tv_sec	= howLong / 1000000 ;
	tLong.tv_usec = howLong % 1000000 ;
	timeradd (&tNow, &tLong, &tEnd) ;

	while (timercmp (&tNow, &tEnd, <))
		gettimeofday (&tNow, NULL) ;
}

void delayMicroseconds (ulong howLong)
{
	struct timespec sleeper ;
	unsigned int uSecs = howLong % 1000000 ;
	unsigned int wSecs = howLong / 1000000 ;

	/**/ if (howLong ==		0)
		return ;
	else if (howLong	< 100)
		delayMicrosecondsHard (howLong) ;
	else
	{
		sleeper.tv_sec	= wSecs ;
		sleeper.tv_nsec = (long)(uSecs * 1000L) ;
		nanosleep (&sleeper, NULL) ;
	}
}

static uint64_t epochMilli, epochMicro ;

void initialiseEpoch()
{
	struct timeval tv ;

	gettimeofday (&tv, NULL) ;
	epochMilli = (uint64_t)tv.tv_sec * (uint64_t)1000		 + (uint64_t)(tv.tv_usec / 1000) ;
	epochMicro = (uint64_t)tv.tv_sec * (uint64_t)1000000 + (uint64_t)(tv.tv_usec) ;
}

ulong millis (void)
{
	struct timeval tv ;
	uint64_t now ;

	gettimeofday (&tv, NULL) ;
	now  = (uint64_t)tv.tv_sec * (uint64_t)1000 + (uint64_t)(tv.tv_usec / 1000) ;

	return (ulong)(now - epochMilli) ;
}

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
#endif

#endif

void write_to_file(const char *fn, const char *data, ulong size, ulong pos, bool trunc) {

#if defined(ESP8266)

	File f;
	if(trunc) {
		f = SPIFFS.open(fn, "w");
	} else {
		f = SPIFFS.open(fn, "r+");
		if(!f) f = SPIFFS.open(fn, "w");
	}		 
	if(!f) return;
	if(pos) f.seek(pos, SeekSet);
	if(size==0) {
		f.write((byte*)" ", 1);  // hack to circumvent SPIFFS bug involving writing empty file
	} else {
		f.write((byte*)data, size);
	}
	f.close();
	
#elif defined(ARDUINO)

	sd.chdir("/");
	SdFile file;
	int flag = O_CREAT | O_RDWR;
	if(trunc) flag |= O_TRUNC;
	int ret = file.open(fn, flag);
	if(!ret) return;
	file.seekSet(pos);
	file.write(data, size);
	file.close();
	
#else

	FILE *file;
	if(trunc) {
		file = fopen(get_filename_fullpath(fn), "wb");
	} else {
		file = fopen(get_filename_fullpath(fn), "r+b");
		if(!file) file = fopen(get_filename_fullpath(fn), "wb");
	}
	if(!file)  return;
	fseek(file, pos, SEEK_SET);
	fwrite(data, 1, size, file);
	fclose(file);
	
#endif
}

void read_from_file(const char *fn, char *data, ulong maxsize, ulong pos) {
#if defined(ESP8266)

	File f = SPIFFS.open(fn, "r");
	if(!f) {
		data[0]=0;
		return;  // return with empty string
	}
	if(pos)  f.seek(pos, SeekSet);
	int len = f.read((byte*)data, maxsize);
	if(len>0) data[len]=0;
	if(len==1 && data[0]==' ') data[0] = 0;  // hack to circumvent SPIFFS bug involving writing empty file
	data[maxsize-1]=0;
	f.close();
	return;

#elif defined(ARDUINO)

	sd.chdir("/");
	SdFile file;
	int ret = file.open(fn, O_READ);
	if(!ret) {
		data[0]=0;
		return;  // return with empty string
	}
	file.seekSet(pos);
	ret = file.fgets(data, maxsize);
	data[maxsize-1]=0;
	file.close();
	return;

#else

	FILE *file;
	file = fopen(get_filename_fullpath(fn), "rb");
	if(!file) {
		data[0] = 0;
		return;
	}

	int res;
	fseek(file, pos, SEEK_SET);
	if(fgets(data, maxsize, file)) {
		res = strlen(data);
	} else {
		res = 0;
	}
	if (res <= 0) {
		data[0] = 0;
	}

	data[maxsize-1]=0;
	fclose(file);
	return;

#endif
}

void remove_file(const char *fn) {
#if defined(ESP8266)

	if(!SPIFFS.exists(fn)) return;
	SPIFFS.remove(fn);

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

	return SPIFFS.exists(fn);

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

	// do not use File.readBytes or readBytesUntil because it's very slow  
	File f = SPIFFS.open(fn, "r");
	if(f) {
		f.seek(pos, SeekSet);
		f.read((byte*)dst, len);
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

	File f = SPIFFS.open(fn, "r+");
	if(!f) f = SPIFFS.open(fn, "w");
	if(f) {
		f.seek(pos, SeekSet);
		f.write((byte*)src, len);
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
	// todo future: if tmp buffer is not provided, do byte-to-byte copy
	if(tmp==NULL) { return; }
#if defined(ESP8266)

	File f = SPIFFS.open(fn, "r+");
	if(!f) return;
	f.seek(from, SeekSet);
	f.read((byte*)tmp, len);
	f.seek(to, SeekSet);
	f.write((byte*)tmp, len);
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
byte file_cmp_block(const char *fn, const char *buf, ulong pos) {
	char c;
#if defined(ESP8266)

	File f = SPIFFS.open(fn, "r");
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

byte file_read_byte(const char *fn, ulong pos) {
	byte v = 0;
	file_read_block(fn, &v, pos, 1);
	return v;
}

void file_write_byte(const char *fn, ulong pos, byte v) {
	file_write_block(fn, &v, pos, 1);
}

// copy n-character string from program memory with ending 0
void strncpy_P0(char* dest, const char* src, int n) {
	byte i;
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
byte water_time_encode_signed(int16_t i) {
	i=(i>600)?600:i;
	i=(i<-600)?-600:i;
	return (i+600)/5;
}

// decode a 8-bit unsigned byte (0 to 240)
// to a 16-bit signed water time (-600 to 600)
int16_t water_time_decode_signed(byte i) {
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

/** Decode a url string e.g "hello%20joe" or "hello+joe" becomes "hello joe" */
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

void peel_http_header(char* buffer) { // remove the HTTP header
	int i=0;
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
