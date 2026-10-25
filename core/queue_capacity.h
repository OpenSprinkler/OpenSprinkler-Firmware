#pragma once

#include <cstdint>

#if defined(ESP8266)
#define RUNTIME_QUEUE_SIZE 72
#elif defined(ESP32)
#define RUNTIME_QUEUE_SIZE 120
#else
#define RUNTIME_QUEUE_SIZE 255
#endif

static_assert(RUNTIME_QUEUE_SIZE > 0 && RUNTIME_QUEUE_SIZE <= 255,
	"Queue indices must leave 0xFF available as the invalid index");

inline bool runtime_queue_can_admit(uint16_t used, uint16_t requested) {
	return used <= RUNTIME_QUEUE_SIZE && requested <= RUNTIME_QUEUE_SIZE - used;
}
