#include "clock.h"

#if !defined(ARDUINO)

#include <sys/time.h>
#include <time.h>

namespace {

uint64_t epoch_microseconds = 0;

tm utc_components(time_os_t timestamp) {
	time_t value = timestamp;
	tm result = {};
	gmtime_r(&value, &result);
	return result;
}

} // namespace

void delay(uint32_t milliseconds) {
	struct timespec sleeper;
	sleeper.tv_sec = milliseconds / 1000;
	sleeper.tv_nsec = (long)(milliseconds % 1000) * 1000000;
	nanosleep(&sleeper, nullptr);
}

void delayMicroseconds(uint32_t microseconds) {
	if (microseconds == 0) return;
	if (microseconds < 100) {
		delayMicrosecondsHard(microseconds);
		return;
	}

	struct timespec sleeper;
	sleeper.tv_sec = microseconds / 1000000;
	sleeper.tv_nsec = (long)(microseconds % 1000000) * 1000;
	nanosleep(&sleeper, nullptr);
}

void delayMicrosecondsHard(uint32_t microseconds) {
	struct timeval now;
	struct timeval duration;
	struct timeval end;

	gettimeofday(&now, nullptr);
	duration.tv_sec = microseconds / 1000000;
	duration.tv_usec = microseconds % 1000000;
	timeradd(&now, &duration, &end);

	while (timercmp(&now, &end, <)) gettimeofday(&now, nullptr);
}

void initialiseEpoch() {
	struct timeval now;
	gettimeofday(&now, nullptr);
	epoch_microseconds = (uint64_t)now.tv_sec * 1000000 + (uint64_t)now.tv_usec;
}

uint32_t micros() {
	struct timeval tv;
	gettimeofday(&tv, nullptr);
	uint64_t now = (uint64_t)tv.tv_sec * 1000000 + (uint64_t)tv.tv_usec;
	return (uint32_t)(now - epoch_microseconds);
}

int hour(time_os_t timestamp) {
	return utc_components(timestamp).tm_hour;
}

int minute(time_os_t timestamp) {
	return utc_components(timestamp).tm_min;
}

int second(time_os_t timestamp) {
	return utc_components(timestamp).tm_sec;
}

int day(time_os_t timestamp) {
	return utc_components(timestamp).tm_mday;
}

int month(time_os_t timestamp) {
	return utc_components(timestamp).tm_mon + 1;
}

#endif
