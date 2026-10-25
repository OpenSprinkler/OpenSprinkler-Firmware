#include "core/queue_capacity.h"

#include <cassert>

int main() {
	static_assert(RUNTIME_QUEUE_SIZE == EXPECTED_QUEUE_CAPACITY, "Platform queue capacity");
	for (uint16_t used = 0; used <= RUNTIME_QUEUE_SIZE; used++) {
		assert(runtime_queue_can_admit(used, RUNTIME_QUEUE_SIZE - used));
		assert(!runtime_queue_can_admit(used, RUNTIME_QUEUE_SIZE - used + 1));
	}
	assert(!runtime_queue_can_admit(RUNTIME_QUEUE_SIZE + 1, 0));
	assert(!runtime_queue_can_admit(0, UINT16_MAX));
	assert(!runtime_queue_can_admit(UINT16_MAX, 1));
}
