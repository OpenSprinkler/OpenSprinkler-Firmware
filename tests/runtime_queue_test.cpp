#include "core/program.h"

#include <cassert>
#include <cstdio>

int main() {
	static_assert(RUNTIME_QUEUE_SIZE == 255, "Native queue capacity");
	ProgramData::reset_runtime();
	assert(runtime_queue_can_admit(0, 255));
	assert(!runtime_queue_can_admit(0, 256));
	assert(!runtime_queue_can_admit(255, 1));
	assert(!runtime_queue_can_admit(256, 0));
	assert(!runtime_queue_can_admit(120, 65535));
	for (uint16_t i = 0; i < RUNTIME_QUEUE_SIZE; i++) {
		auto* q = ProgramData::enqueue();
		assert(q && !q->st && !q->dur && !q->deque_time);
		q->st = 1000 + i;
		q->dur = 30;
		q->sid = i % MAX_NUM_STATIONS;
	}
	assert(ProgramData::nqueue == 255);
	assert(!ProgramData::enqueue());
	const uint8_t last_sid = ProgramData::queue[254].sid;
	ProgramData::station_qid[last_sid] = 254;
	ProgramData::dequeue(1);
	assert(ProgramData::nqueue == 254);
	assert(ProgramData::queue[1].st == 1254);
	assert(ProgramData::station_qid[last_sid] == 1);
	auto* q = ProgramData::enqueue();
	assert(q == ProgramData::queue + 254);
	assert(!q->st && !q->dur && !q->deque_time);
	ProgramData::dequeue(255); // sentinel must never remove a valid entry
	assert(ProgramData::nqueue == 255);
	ProgramData::dequeue(254);
	assert(ProgramData::nqueue == 254);
	ProgramData::reset_runtime();
	assert(!ProgramData::nqueue);
	for (uint16_t sid = 0; sid < MAX_NUM_STATIONS; sid++) assert(ProgramData::station_qid[sid] == 255);
	printf("Runtime queue PASS: %u entries, %zu-byte records, %zu-byte array\n",
		RUNTIME_QUEUE_SIZE, sizeof(RuntimeQueueStruct), RUNTIME_QUEUE_SIZE * sizeof(RuntimeQueueStruct));
}
