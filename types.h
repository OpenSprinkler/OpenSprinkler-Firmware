#pragma once

#include <stdint.h>

#if defined(ARDUINO)
typedef uint32_t time_os_t;
#else
#include <time.h>
typedef uint32_t time_os_t;
#endif
