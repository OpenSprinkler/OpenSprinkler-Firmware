/* OpenSprinkler Unified (AVR/RPI/BBB/LINUX) Firmware
 * Copyright (C) 2014 by Ray Wang (ray@opensprinkler.com)
 *
 * GPIO functions
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
 
#include "gpio.h"

#if defined(ARDUINO)

#if defined(ESP8266)

#include <Wire.h>
#include "defines.h"

byte IOEXP::detectType(uint8_t address) {
	Wire.beginTransmission(address);
	if(Wire.endTransmission()!=0) return IOEXP_TYPE_NONEXIST; // this I2C address does not exist
	
	Wire.beginTransmission(address);
	Wire.write(NXP_INVERT_REG); // ask for polarity register
	Wire.endTransmission();
	
	if(Wire.requestFrom(address, (uint8_t)2) != 2) return IOEXP_TYPE_UNKNOWN;
	uint8_t low = Wire.read();
	uint8_t high = Wire.read();
	if(low==0x00 && high==0x00) {
		return IOEXP_TYPE_9555; // PCA9555 has polarity register which inits to 0
	}
	return IOEXP_TYPE_8575;  
}

void PCA9555::pinMode(uint8_t pin, uint8_t IOMode) {
	uint16_t config = i2c_read(NXP_CONFIG_REG);
	if(IOMode == OUTPUT) {
			config &= ~(1 << pin); // config bit set to 0 for output pin
	} else {
			config |= (1 << pin);  // config bit set to 1 for input pin
	}
	i2c_write(NXP_CONFIG_REG, config);
}

uint16_t PCA9555::i2c_read(uint8_t reg) {
	if(address==255)	return 0xFFFF;
	Wire.beginTransmission(address);
	Wire.write(reg);
	Wire.endTransmission();
	if(Wire.requestFrom(address, (uint8_t)2) != 2) {return 0xFFFF;}
	uint16_t data0 = Wire.read();
	uint16_t data1 = Wire.read();
	return data0+(data1<<8);
}

void PCA9555::i2c_write(uint8_t reg, uint16_t v){
	if(address==255)	return;
	Wire.beginTransmission(address);
	Wire.write(reg);
	Wire.write(v&0xff);
	Wire.write(v>>8);
	Wire.endTransmission();
}

void PCA9555::shift_out(uint8_t plat, uint8_t pclk, uint8_t pdat, uint8_t v) {
	if(plat<IOEXP_PIN || pclk<IOEXP_PIN || pdat<IOEXP_PIN)
		return;	// the definition of each pin must be offset by IOEXP_PIN to begin with
	
	plat-=IOEXP_PIN;
	pclk-=IOEXP_PIN;
	pdat-=IOEXP_PIN;
	
	uint16_t output = i2c_read(NXP_OUTPUT_REG); // keep a copy of the current output registers
	
	output &= ~(1<<plat); i2c_write(NXP_OUTPUT_REG, output); // set latch low

	for(uint8_t s=0;s<8;s++) {
		output &= ~(1<<pclk); i2c_write(NXP_OUTPUT_REG, output); // set clock low

		if(v & ((byte)1<<(7-s))) {
			output |= (1<<pdat);
		} else {
			output &= ~(1<<pdat);
		}
		i2c_write(NXP_OUTPUT_REG, output); // set data pin according to bits in v

		output |= (1<<pclk); i2c_write(NXP_OUTPUT_REG, output); // set clock high
	}
	
	output |= (1<<plat); i2c_write(NXP_OUTPUT_REG, output); // set latch high
}

uint16_t PCF8575::i2c_read(uint8_t reg) {
	if(address==255)	return 0xFFFF;
	Wire.beginTransmission(address);
	if(Wire.requestFrom(address, (uint8_t)2) != 2) return 0xFFFF;
	uint16_t data0 = Wire.read();
	uint16_t data1 = Wire.read();
	Wire.endTransmission();
	return data0+(data1<<8);
}

void PCF8575::i2c_write(uint8_t reg, uint16_t v) {
	if(address==255)	return;
	Wire.beginTransmission(address);
	// todo: handle inputmask (not necessary unless if using any pin as input)
	Wire.write(v&0xff);
	Wire.write(v>>8);
	Wire.endTransmission();
}

uint16_t PCF8574::i2c_read(uint8_t reg) {
	if(address==255)	return 0xFFFF;
	Wire.beginTransmission(address);
	if(Wire.requestFrom(address, (uint8_t)1) != 1) return 0xFFFF;
	uint16_t data = Wire.read();
	Wire.endTransmission();
	return data; 
}

void PCF8574::i2c_write(uint8_t reg, uint16_t v) {
	if(address==255)	return;
	Wire.beginTransmission(address);
	Wire.write((uint8_t)(v&0xFF) | inputmask);
	Wire.endTransmission();  
}

#include "OpenSprinkler.h"

extern OpenSprinkler os;

void pinModeExt(byte pin, byte mode) {
	if(pin==255) return;
	if(pin>=IOEXP_PIN) {
		os.mainio->pinMode(pin-IOEXP_PIN, mode);
	} else {
		pinMode(pin, mode);
	}
}

void digitalWriteExt(byte pin, byte value) {
	if(pin==255) return;
	if(pin>=IOEXP_PIN) {

		os.mainio->digitalWrite(pin-IOEXP_PIN, value);
	/*
		// a pin on IO expander
		byte data=pcf_read(MAIN_I2CADDR);
		if(value) data|=(1<<(pin-IOEXP_PIN));
		else		 data&=~(1<<(pin-IOEXP_PIN));
		data |= MAIN_INPUTMASK; // make sure to enforce 1 for input pins
		pcf_write(MAIN_I2CADDR, data);*/
	} else {
		digitalWrite(pin, value);
	}
}

byte digitalReadExt(byte pin) {
	if(pin==255) return HIGH;
	if(pin>=IOEXP_PIN) {
		return os.mainio->digitalRead(pin-IOEXP_PIN);
		// a pin on IO expander
		//return pcf_read(MAIN_I2CADDR)&(1<<(pin-IOEXP_PIN));
	} else {
		return digitalRead(pin);
	}
}
#endif

#elif defined(OSPI) || defined(OSBO)

#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>
#include <pthread.h>

#define BUFFER_MAX 64
#define GPIO_MAX	 64

// GPIO file descriptors
static int sysFds[GPIO_MAX] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
} ;

// Interrupt service routine functions
static void (*isrFunctions [GPIO_MAX])(void);

static volatile int		 pinPass = -1 ;
static pthread_mutex_t pinMutex ;

/** Export gpio pin */
static byte GPIOExport(int pin) {
	char buffer[BUFFER_MAX];
	int fd, len;

	fd = open("/sys/class/gpio/export", O_WRONLY);
	if (fd < 0) {
		DEBUG_PRINTLN("failed to open export for writing");
		return 0;
	}

	len = snprintf(buffer, sizeof(buffer), "%d", pin);
	write(fd, buffer, len);
	close(fd);
	return 1;
}

/** Unexport gpio pin */
static byte GPIOUnexport(int pin) {
	char buffer[BUFFER_MAX];
	int fd, len;

	fd = open("/sys/class/gpio/unexport", O_WRONLY);
	if (fd < 0) {
		DEBUG_PRINTLN("failed to open unexport for writing");
		return 0;
	}

	len = snprintf(buffer, sizeof(buffer), "%d", pin);
	write(fd, buffer, len);
	close(fd);
	return 1;
}

/** Set interrupt edge mode */
static byte GPIOSetEdge(int pin, const char *edge) {
	char path[BUFFER_MAX];
	int fd, len;

	snprintf(path, BUFFER_MAX, "/sys/class/gpio/gpio%d/edge", pin);

	fd = open(path, O_WRONLY);
	if (fd < 0) {
		DEBUG_PRINTLN("failed to open gpio edge for writing");
		return 0;
	}
	write(fd, edge, strlen(edge)+1);
	close(fd);
	return 1;
}

/** Set pin mode, in or out */
void pinMode(int pin, byte mode) {
	static const char dir_str[]  = "in\0out";

	char path[BUFFER_MAX];
	int fd;

	snprintf(path, BUFFER_MAX, "/sys/class/gpio/gpio%d/direction", pin);

	struct stat st;
	if(stat(path, &st)) {
		if (!GPIOExport(pin)) return;
	}

	fd = open(path, O_WRONLY);
	if (fd < 0) {
		DEBUG_PRINTLN("failed to open gpio direction for writing");
		return;
	}

	if (-1 == write(fd, &dir_str[(INPUT==mode)||(INPUT_PULLUP==mode)?0:3], (INPUT==mode)||(INPUT_PULLUP==mode)?2:3)) {
		DEBUG_PRINTLN("failed to set direction");
		return;
	}

	close(fd);
#if defined(OSPI)
	if(mode==INPUT_PULLUP) {
		char cmd[BUFFER_MAX];
		snprintf(cmd, BUFFER_MAX, "gpio -g mode %d up", pin);
		system(cmd);
	}
#endif
	return;
}

/** Open file for digital pin */
int gpio_fd_open(int pin, int mode) {
	char path[BUFFER_MAX];
	int fd;

	snprintf(path, BUFFER_MAX, "/sys/class/gpio/gpio%d/value", pin);
	fd = open(path, mode);
	if (fd < 0) {
		DEBUG_PRINTLN("failed to open gpio");
		return -1;
	}
	return fd;
}

/** Close file */
void gpio_fd_close(int fd) {
	close(fd);
}

/** Read digital value */
byte digitalRead(int pin) {
	char value_str[3];

	int fd = gpio_fd_open(pin, O_RDONLY);
	if (fd < 0) {
		return 0;
	}

	if (read(fd, value_str, 3) < 0) {
		DEBUG_PRINTLN("failed to read value");
		return 0;
	}

	close(fd);
	return atoi(value_str);
}

/** Write digital value given file descriptor */
void gpio_write(int fd, byte value) {
	static const char value_str[] = "01";

	if (1 != write(fd, &value_str[LOW==value?0:1], 1)) {
		DEBUG_PRINT("failed to write value on pin ");
	}
}

/** Write digital value */
void digitalWrite(int pin, byte value) {
	int fd = gpio_fd_open(pin);
	if (fd < 0) {
		return;
	}
	gpio_write(fd, value);
	close(fd);
}

static int HiPri (const int pri) {
	struct sched_param sched ;

	memset (&sched, 0, sizeof(sched)) ;

	if (pri > sched_get_priority_max (SCHED_RR))
		sched.sched_priority = sched_get_priority_max (SCHED_RR) ;
	else
		sched.sched_priority = pri ;

	return sched_setscheduler (0, SCHED_RR, &sched) ;
}

static int waitForInterrupt (int pin, int mS)
{
	int fd, x ;
	uint8_t c ;
	struct pollfd polls ;

	if((fd=sysFds[pin]) < 0)
		return -2;

	polls.fd		 = fd ;
	polls.events = POLLPRI ;			// Urgent data!

	x = poll (&polls, 1, mS) ;
// Do a dummy read to clear the interrupt
//			A one character read appars to be enough.
//			Followed by a seek to reset it.

	(void)read (fd, &c, 1);
	lseek (fd, 0, SEEK_SET);

	return x ;
}

static void *interruptHandler (void *arg) {
	int myPin ;

	(void) HiPri (55) ;  // Only effective if we run as root

	myPin		= pinPass ;
	pinPass = -1 ;

	for (;;)
		if (waitForInterrupt (myPin, -1) > 0)
			isrFunctions[myPin]() ;

	return NULL ;
}

#include "utils.h"
/** Attach an interrupt function to pin */
void attachInterrupt(int pin, const char* mode, void (*isr)(void)) {
	if((pin<0)||(pin>GPIO_MAX)) {
		DEBUG_PRINTLN("pin out of range");
		return;
	}

	// set pin to INPUT mode and set interrupt edge mode
	pinMode(pin, INPUT);
	GPIOSetEdge(pin, mode);

	char path[BUFFER_MAX];
	snprintf(path, BUFFER_MAX, "/sys/class/gpio/gpio%d/value", pin);
	int fd;

	// open gpio file
	if(sysFds[pin]==-1) {
		if((sysFds[pin]=open(path, O_RDWR))<0) {
			DEBUG_PRINTLN("failed to open gpio value for reading");
			return;
		}
	}

	int count, i;
	char c;
	// clear any pending interrupts
	ioctl (sysFds[pin], FIONREAD, &count) ;
	for (i=0; i<count; i++)
		read (sysFds[pin], &c, 1) ;

	// record isr function
	isrFunctions[pin] = isr;

	pthread_t threadId ;
	pthread_mutex_lock (&pinMutex) ;
		pinPass = pin ;
		pthread_create (&threadId, NULL, interruptHandler, NULL) ;
		while (pinPass != -1)
			delay(1) ;
	pthread_mutex_unlock (&pinMutex) ;
}
#else

void pinMode(int pin, byte mode) {}
void digitalWrite(int pin, byte value) {}
byte digitalRead(int pin) {return 0;}
void attachInterrupt(int pin, const char* mode, void (*isr)(void)) {}
int gpio_fd_open(int pin, int mode) {return 0;}
void gpio_fd_close(int fd) {}
void gpio_write(int fd, byte value) {}

#endif
