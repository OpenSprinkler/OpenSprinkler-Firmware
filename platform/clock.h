#pragma once

#include "types.h"

// Milliseconds elapsed on a monotonic clock. This deliberately does not use
// OTF's Linux millis(), which is based on wall-clock time.
uint32_t monotonic_millis();

#if !defined(ARDUINO)

#include <cstdint>

void delay(uint32_t milliseconds);
void delayMicroseconds(uint32_t microseconds);
void delayMicrosecondsHard(uint32_t microseconds);

// OTF provides millis() for non-Arduino targets so it remains standalone.
uint32_t millis();
uint32_t micros();
void initialiseEpoch();

int hour(time_os_t timestamp);
int minute(time_os_t timestamp);
int second(time_os_t timestamp);
int day(time_os_t timestamp);
int month(time_os_t timestamp);

#endif
