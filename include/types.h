#ifndef TYPES_H
#define TYPES_H
#include <stdint.h>

#if defined(ARDUINO)
typedef unsigned long time_os_t;
#else
#include <time.h>
typedef time_t time_os_t;
#endif

#endif