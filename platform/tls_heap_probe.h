#pragma once

// Opt-in bench instrumentation; normal builds evaluate only the original call.
#if defined(ESP8266) && defined(OS_TLS_HEAP_PROBE)
#include <cstdint>

enum class TlsHeapPhase : uint8_t { HttpProbe, HttpConnect, SmtpProbe, SmtpConnect, Count };
struct TlsHeapSample {
	uint32_t calls, successes, before, minimum, lowest, after, oom;
};
const TlsHeapSample& tls_heap_sample(TlsHeapPhase phase);
void tls_heap_begin(TlsHeapPhase phase);
void tls_heap_end(TlsHeapPhase phase, bool success);

template <typename Operation>
auto tls_heap_measure(TlsHeapPhase phase, Operation operation) -> decltype(operation()) {
	tls_heap_begin(phase);
	auto result = operation();
	tls_heap_end(phase, result);
	return result;
}
#define OS_TLS_MEASURE(phase, expression) \
	tls_heap_measure(TlsHeapPhase::phase, [&]() { return (expression); })
#else
#define OS_TLS_MEASURE(phase, expression) (expression)
#endif
