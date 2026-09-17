#pragma once

#include <stdint.h>

class FlowRateWindow {
public:
	void reset() {
		next_ = 0;
		size_ = 0;
		event_next_ = 0;
		event_size_ = 0;
		timeout_ms_ = 1000;
		last_rate_ = 0;
		initialized_ = false;
	}

	uint32_t update(uint32_t now_ms, uint32_t total_pulses, uint32_t rate_window) {
		if (!initialized_) {
			initialized_ = true;
			last_total_pulses_ = total_pulses;
		}

		uint32_t new_pulses = total_pulses - last_total_pulses_;
		if (new_pulses) {
			// Batched counter updates may still represent continuous flow.
			if (new_pulses == 1 && stale(now_ms)) {
				event_size_ = 0;
				last_rate_ = 0;
			}
			events_[event_next_] = {now_ms, total_pulses};
			event_next_ = (event_next_ + 1) % EVENT_SAMPLE_COUNT;
			if (event_size_ < EVENT_SAMPLE_COUNT) event_size_++;
			last_total_pulses_ = total_pulses;
			if (event_size_ >= 2) {
				const Sample& first = events_[(event_next_ + EVENT_SAMPLE_COUNT - event_size_) % EVENT_SAMPLE_COUNT];
				const Sample& last = latest_event();
				uint32_t pulses = last.pulses - first.pulses;
				uint64_t timeout = static_cast<uint64_t>(last.ms - first.ms) * 10 / pulses;
				if (timeout < 1000) timeout = 1000;
				timeout_ms_ = timeout > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(timeout);
			}
		}

		bool sampled = false;
		if (!size_ || now_ms - latest().ms >= SAMPLE_INTERVAL_MS) {
			samples_[next_] = {now_ms, total_pulses};
			next_ = (next_ + 1) % SAMPLE_COUNT;
			if (size_ < SAMPLE_COUNT) size_++;
			sampled = true;
		}

		if (event_size_ < 2 || stale(now_ms)) {
			last_rate_ = 0;
			return 0;
		}
		if (!new_pulses && !sampled) return last_rate_;

		const Sample& oldest = samples_[(next_ + SAMPLE_COUNT - size_) % SAMPLE_COUNT];
		uint32_t elapsed_ms = now_ms - oldest.ms;
		uint32_t pulses = total_pulses - oldest.pulses;
		if (elapsed_ms >= 1000 && pulses >= MIN_WINDOW_PULSES) {
			last_rate_ = rate(pulses, elapsed_ms, rate_window);
			return last_rate_;
		}

		const Sample& first_event = events_[(event_next_ + EVENT_SAMPLE_COUNT - event_size_) % EVENT_SAMPLE_COUNT];
		const Sample& last_event = latest_event();
		last_rate_ = rate(last_event.pulses - first_event.pulses, last_event.ms - first_event.ms, rate_window);
		return last_rate_;
	}

private:
	// Use pulse intervals for slow flows and five seconds of counts for faster flows.
	static constexpr uint8_t SAMPLE_COUNT = 11;
	static constexpr uint8_t EVENT_SAMPLE_COUNT = 6;
	static constexpr uint32_t SAMPLE_INTERVAL_MS = 500;
	// Forty pulses limit count-window quantization to about 2.5%.
	static constexpr uint32_t MIN_WINDOW_PULSES = 40;

	struct Sample {
		uint32_t ms;
		uint32_t pulses;
	};

	const Sample& latest() const {
		return samples_[(next_ + SAMPLE_COUNT - 1) % SAMPLE_COUNT];
	}
	const Sample& latest_event() const {
		return events_[(event_next_ + EVENT_SAMPLE_COUNT - 1) % EVENT_SAMPLE_COUNT];
	}
	bool stale(uint32_t now_ms) const {
		return event_size_ >= 2 && now_ms - latest_event().ms > timeout_ms_;
	}
	static uint32_t rate(uint32_t pulses, uint32_t elapsed_ms, uint32_t rate_window) {
		if (!elapsed_ms) return 0;
		return (static_cast<uint64_t>(pulses) * rate_window * 1000 + elapsed_ms / 2) / elapsed_ms;
	}

	Sample samples_[SAMPLE_COUNT] = {};
	Sample events_[EVENT_SAMPLE_COUNT] = {};
	uint8_t next_ = 0;
	uint8_t size_ = 0;
	uint8_t event_next_ = 0;
	uint8_t event_size_ = 0;
	uint32_t last_total_pulses_ = 0;
	uint32_t timeout_ms_ = 1000;
	uint32_t last_rate_ = 0;
	bool initialized_ = false;
};
