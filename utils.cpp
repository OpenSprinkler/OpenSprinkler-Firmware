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

#if defined(ARDUINO)  // AVR
#include <avr/eeprom.h>
#include "SdFat.h"
extern SdFat sd;

void write_to_file(const char *name, const char *data, int size, int pos, bool trunc) {
  if (!os.status.has_sd)  return;

  char fn[12];
  strcpy_P(fn, name);
  sd.chdir("/");
  SdFile file;
  int flag = O_CREAT | O_WRITE;
  if (trunc) flag |= O_TRUNC;
  int ret = file.open(fn, flag);
  if(!ret) {
    return;
  }
  file.seekSet(pos);
  file.write(data, size);
  file.close();
}

bool read_from_file(const char *name, char *data, int maxsize, int pos) {
  if (!os.status.has_sd)  { data[0]=0; return false; }

  char fn[12];
  strcpy_P(fn, name);
  sd.chdir("/");
  SdFile file;
  int ret = file.open(fn, O_READ );
  if(!ret) {
    data[0]=0;
    return true;  // return true but with empty string
  }
  file.seekSet(pos);
  ret = file.fgets(data, maxsize);
  data[maxsize-1]=0;
  file.close();
  return true;
}

void remove_file(const char *name) {
  if (!os.status.has_sd)  return;

  char fn[12];
  strcpy_P(fn, name);
  sd.chdir("/");
  if (!sd.exists(fn))  return;
  sd.remove(fn);
}

#else // RPI/BBB/LINUX
void nvm_read_block(void *dst, const void *src, int len) {
  FILE *fp = fopen(get_filename_fullpath(NVM_FILENAME), "rb");
  if(fp) {
    fseek(fp, (unsigned int)src, SEEK_SET);
    fread(dst, 1, len, fp);
    fclose(fp);
  }
}

void nvm_write_block(const void *src, void *dst, int len) {
  FILE *fp = fopen(get_filename_fullpath(NVM_FILENAME), "rb+");
  if(!fp) {
    fp = fopen(get_filename_fullpath(NVM_FILENAME), "wb");
  }
  if(fp) {
    fseek(fp, (unsigned int)dst, SEEK_SET);
    fwrite(src, 1, len, fp);
    fclose(fp);
  } else {
    // file does not exist
  }
}

byte nvm_read_byte(const byte *p) {
  FILE *fp = fopen(get_filename_fullpath(NVM_FILENAME), "rb");
  byte v = 0;
  if(fp) {
    fseek(fp, (unsigned int)p, SEEK_SET);
    fread(&v, 1, 1, fp);
    fclose(fp);
  } else {
   // file does not exist
  }
  return v;
}

void nvm_write_byte(const byte *p, byte v) {
  FILE *fp = fopen(get_filename_fullpath(NVM_FILENAME), "rb+");
  if(!fp) {
    fp = fopen(get_filename_fullpath(NVM_FILENAME), "wb");
  }
  if(fp) {
    fseek(fp, (unsigned int)p, SEEK_SET);
    fwrite(&v, 1, 1, fp);
    fclose(fp);
  } else {
    // file does not exist
  }
}

void write_to_file(const char *name, const char *data, int size, int pos, bool trunc) {
  FILE *file;
  if(trunc) {
    file = fopen(get_filename_fullpath(name), "wb");
  } else {
    file = fopen(get_filename_fullpath(name), "r+b");
    if(!file) {
        file = fopen(get_filename_fullpath(name), "wb");
    }
  }

  if (!file) { return; }

  fseek(file, pos, SEEK_SET);
  fwrite(data, 1, size, file);
  fclose(file);
}

bool read_from_file(const char *name, char *data, int maxsize, int pos) {

  FILE *file;
  file = fopen(get_filename_fullpath(name), "rb");
  if(!file) {
    data[0] = 0;
    return true;
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
  return true;
}

void remove_file(const char *name) {
  remove(get_filename_fullpath(name));
}

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

void delay(ulong howLong)
{
  struct timespec sleeper, dummy ;

  sleeper.tv_sec  = (time_t)(howLong / 1000) ;
  sleeper.tv_nsec = (long)(howLong % 1000) * 1000000 ;

  nanosleep (&sleeper, &dummy) ;
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

void delayMicroseconds (ulong howLong)
{
  struct timespec sleeper ;
  unsigned int uSecs = howLong % 1000000 ;
  unsigned int wSecs = howLong / 1000000 ;

  /**/ if (howLong ==   0)
    return ;
  else if (howLong  < 100)
    delayMicrosecondsHard (howLong) ;
  else
  {
    sleeper.tv_sec  = wSecs ;
    sleeper.tv_nsec = (long)(uSecs * 1000L) ;
    nanosleep (&sleeper, NULL) ;
  }
}

static uint64_t epochMilli, epochMicro ;

void initialiseEpoch()
{
  struct timeval tv ;

  gettimeofday (&tv, NULL) ;
  epochMilli = (uint64_t)tv.tv_sec * (uint64_t)1000    + (uint64_t)(tv.tv_usec / 1000) ;
  epochMicro = (uint64_t)tv.tv_sec * (uint64_t)1000000 + (uint64_t)(tv.tv_usec) ;
}

ulong millis (void)
{
  struct timeval tv ;
  uint64_t now ;

  gettimeofday (&tv, NULL) ;
  now  = (uint64_t)tv.tv_sec * (uint64_t)1000 + (uint64_t)(tv.tv_usec / 1000) ;

  return (uint32_t)(now - epochMilli) ;
}

ulong micros (void)
{
  struct timeval tv ;
  uint64_t now ;

  gettimeofday (&tv, NULL) ;
  now  = (uint64_t)tv.tv_sec * (uint64_t)1000000 + (uint64_t)tv.tv_usec ;

  return (uint32_t)(now - epochMicro) ;
}


#endif

// copy n-character string from program memory with ending 0
void strncpy_P0(char* dest, const char* src, int n) {
  byte i;
  for(i=0;i<n;i++) {
    *dest=pgm_read_byte(src++);
    dest++;
  }
  *dest=0;
}

// compare a string to nvm
byte strcmp_to_nvm(const char* src, int _addr) {
  byte c1, c2;
  byte *addr = (byte*)_addr;
  while(1) {
    c1 = nvm_read_byte(addr++);
    c2 = *src++;
    if (c1==0 || c2==0)
      break;
    if (c1!=c2)  return 1;
  }
  return (c1==c2) ? 0 : 1;
}

// ================================================
// ====== Data Encoding / Decoding Functions ======
// ================================================
// encode a 16-bit unsigned water time to 8-bit byte
/* encoding scheme:
   byte value : water time
     [0.. 59]  : [0..59]  (seconds)
    [60..238]  : [1..179] (minutes), or 60 to 10740 seconds
   [239..252]  : [3..16]  (hours),   or 10800 to 57600 seconds
         253   : unused
         254   : sunrise to sunset
         255   : sunset to sunrise
*/
byte water_time_encode(uint16_t i) {
  if (i<60) {
    return (byte)(i);
  } else if (i<10800) {
    return (byte)(i/60+59);
  } else if (i<=57600) {
    return (byte)(i/3600+236);
  } else if(i==65534) {
    return 254;
  } else if(i==65535) {
    return 255;
  } else {
    return 254;
  }
}

// decode a 8-bit byte to a 16-bit unsigned water time
uint16_t water_time_decode(byte i) {
  uint16_t ii = i;
  if (i<60) {
    return ii;
  } else if (i<239) {
    return (ii-59)*60;
  } else if (i<=252) {
    return (ii-236)*3600;
  } else if (i==254) {
    return 65534;
  } else if (i==255) {
    return 65535;
  } else {
    return 65534;
  }
}

// resolve water time
ulong water_time_resolve(uint16_t v) {
  if(v==65534) {
    return (os.nvdata.sunset_time-os.nvdata.sunrise_time) * 60L;
  } else if(v==65535) {
    return (os.nvdata.sunrise_time+1440-os.nvdata.sunset_time) * 60L;
  } else  {
    return v;
  }
}

// encode a 16-bit signed water time to 8-bit unsigned byte (leading bit is the sign)
byte water_time_encode_signed(int16_t i) {
  byte ret = water_time_encode(i>=0?i : -i);
  return ((i>=0) ? (128+ret) : (128-ret));
}

// decode a 8-bit unsigned byte (leading bit is the sign) into a 16-bit signed water time
int16_t water_time_decode_signed(byte i) {
  int16_t ret = i;
  ret -= 128;
  ret = water_time_decode(ret>=0?ret:-ret);
  return (i>=128 ? ret : -ret);
}




