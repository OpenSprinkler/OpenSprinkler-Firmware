#pragma once

#if defined(ARDUINO)

#include <Arduino.h>

#include "../drivers/io_expander.h"

// The caller owns the expander and must keep it alive after registration.
void gpio_set_main_expander(IOEXP* expander);
void pinModeExt(unsigned char pin, unsigned char mode);
void digitalWriteExt(unsigned char pin, unsigned char value);
unsigned char digitalReadExt(unsigned char pin);

#else

#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>

#include "../defines.h"

#define OUTPUT 0
#define INPUT  1

#if defined(OSPI)
#define INPUT_PULLUP 2
#else
#define INPUT_PULLUP INPUT
#endif

#define HIGH 1
#define LOW  0

void pinMode(int pin, unsigned char mode);
void digitalWrite(int pin, unsigned char value);
int gpio_fd_open(int pin, int mode = O_WRONLY);
void gpio_fd_close(int fd);
void gpio_write(int fd, unsigned char value);
unsigned char digitalRead(int pin);
void attachInterrupt(int pin, const char* mode, void (*isr)(void));

#endif
