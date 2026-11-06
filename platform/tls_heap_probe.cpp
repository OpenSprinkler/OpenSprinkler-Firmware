#include "platform/tls_heap_probe.h"

#if defined(ESP8266) && defined(OS_TLS_HEAP_PROBE)
#include <umm_malloc/umm_malloc.h>
#include <umm_malloc/umm_malloc_cfg.h>

#if !defined(UMM_STATS_FULL)
#error "TLS heap probe requires UMM_STATS_FULL across the entire core build"
#endif

namespace {
TlsHeapSample samples[static_cast<unsigned>(TlsHeapPhase::Count)] = {};
uint32_t oom_before;
}

const TlsHeapSample& tls_heap_sample(TlsHeapPhase phase) {
	return samples[static_cast<unsigned>(phase)];
}

// These spans are synchronous and non-nested. No logging or allocation occurs
// inside the probe; the core updates the low-water counter in its allocator.
void tls_heap_begin(TlsHeapPhase phase) {
	auto& sample = samples[static_cast<unsigned>(phase)];
	oom_before = umm_get_oom_count();
	sample.before = umm_free_heap_size_min_reset();
}

void tls_heap_end(TlsHeapPhase phase, bool success) {
	auto& sample = samples[static_cast<unsigned>(phase)];
	sample.minimum = umm_free_heap_size_min();
	sample.after = umm_free_heap_size();
	sample.oom += umm_get_oom_count() - oom_before;
	if (!sample.calls || sample.minimum < sample.lowest) sample.lowest = sample.minimum;
	++sample.calls;
	if (success) ++sample.successes;
}
#endif
