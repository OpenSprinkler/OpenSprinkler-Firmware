#include "platform/clock.h"

#if defined(ARDUINO)
  #include <Arduino.h>
#else
  #include <time.h>
#endif

uint32_t monotonic_millis() {
#if defined(ARDUINO)
	return millis();
#else
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	const uint64_t elapsed_ms =
		static_cast<uint64_t>(now.tv_sec) * 1000ULL +
		static_cast<uint64_t>(now.tv_nsec) / 1000000ULL;
	return static_cast<uint32_t>(elapsed_ms);
#endif
}
