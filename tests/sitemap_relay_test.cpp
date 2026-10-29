#include "api/sitemap_relay.h"

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace {

constexpr uint32_t REQUEST = 6;
constexpr uint32_t MAX_WINDOW = 18;
constexpr uint32_t SENDER_LEASE = 30000;
constexpr uint32_t RECEIVER_LEASE = 60000;

constexpr uint32_t SENDER = 0xA1;
constexpr uint32_t OTHER_SENDER = 0xA2;
constexpr uint32_t RECEIVER = 0xB1;
constexpr uint32_t SECOND_RECEIVER = 0xB2;

class FakeStorage : public SiteMapRelayStorage {
public:
	uint32_t available = 1024 * 1024;
	bool fail_next_write = false;
	std::vector<uint8_t> window;

	uint32_t available_bytes() override {
		return available;
	}

	bool append(const uint8_t* data, uint32_t length) override {
		if (fail_next_write) {
			fail_next_write = false;
			return false;
		}
		window.insert(window.end(), data, data + length);
		return true;
	}

	void clear() override {
		window.clear();
	}
};

std::vector<uint8_t> image_bytes(uint32_t size, unsigned seed) {
	std::vector<uint8_t> bytes(size);
	srand(seed);
	for (auto& byte : bytes) byte = (uint8_t)rand();
	return bytes;
}

struct Fixture {
	FakeStorage storage;
	SiteMapRelay relay;
	std::vector<uint8_t> image;
	uint32_t now = 1000;

	explicit Fixture(uint32_t size = 40) :
		relay(storage, REQUEST, MAX_WINDOW, SENDER_LEASE, RECEIVER_LEASE), image(image_bytes(size, size)) {
		relay.set_image_size(size);
	}

	// Sends the current window in request-sized pieces, as the sender app does
	void fill(uint32_t client = SENDER) {
		SiteMapRelay::Status claimed = relay.claim(client, now);
		assert(claimed.result == SiteMapRelay::SUCCESS && claimed.state == SiteMapRelay::FILLING);
		uint32_t position = claimed.offset + claimed.length;
		uint32_t end = claimed.offset + claimed.window;
		if (end > image.size()) end = image.size();
		while (position < end) {
			uint32_t piece = end - position < claimed.request ? end - position : claimed.request;
			assert(relay.upload(client, position, image.data() + position, piece, now).result == SiteMapRelay::SUCCESS);
			position += piece;
		}
		assert(relay.claim(client, now).state == SiteMapRelay::READY);
	}

	// Downloads and confirms the current window, as the receiver app does, appending it to received
	void drain(std::vector<uint8_t>& received, uint32_t client = RECEIVER) {
		SiteMapRelay::Status ready = relay.batch(client, now);
		assert(ready.result == SiteMapRelay::SUCCESS && ready.state == SiteMapRelay::READY);
		assert(ready.offset == received.size() && ready.length == storage.window.size());
		received.insert(received.end(), storage.window.begin(), storage.window.end());
		assert(relay.confirm(client, ready.offset + ready.length, now).result == SiteMapRelay::SUCCESS);
		assert(storage.window.empty());
	}

	std::vector<uint8_t> transfer(uint32_t client = RECEIVER) {
		std::vector<uint8_t> received;
		while (received.size() < image.size()) {
			fill();
			drain(received, client);
		}
		return received;
	}
};

void test_join_and_claim() {
	Fixture f;
	assert(f.relay.summary() == 0);
	assert(f.relay.claim(SENDER, f.now).result == SiteMapRelay::DATA_MISSING);

	SiteMapRelay::Status joined = f.relay.join(RECEIVER, f.now);
	assert(joined.result == SiteMapRelay::SUCCESS && joined.state == SiteMapRelay::WAITING && joined.position == 0);
	assert(f.relay.summary() == 1);

	SiteMapRelay::Status claimed = f.relay.claim(SENDER, f.now);
	assert(claimed.result == SiteMapRelay::SUCCESS && claimed.state == SiteMapRelay::FILLING);
	assert(claimed.offset == 0 && claimed.length == 0 && claimed.total == 40);
	assert(claimed.window == MAX_WINDOW && claimed.request == REQUEST);
	assert(f.relay.summary() == 2);

	// Another app with the image is turned away while the first sender's claim is current
	assert(f.relay.claim(OTHER_SENDER, f.now).result == SiteMapRelay::NOT_PERMITTED);
	f.now += SENDER_LEASE - 1;
	assert(f.relay.claim(OTHER_SENDER, f.now).result == SiteMapRelay::NOT_PERMITTED);
	f.now += 2;
	f.relay.join(RECEIVER, f.now);
	assert(f.relay.claim(OTHER_SENDER, f.now).result == SiteMapRelay::SUCCESS);
}

void test_window_size() {
	Fixture f;
	f.relay.join(RECEIVER, f.now);

	// Rounded down to whole requests, and refused when not even one request fits
	f.storage.available = 17;
	assert(f.relay.claim(SENDER, f.now).window == 12);
	f.relay.leave(SENDER, f.now);
	f.storage.available = 5;
	SiteMapRelay::Status refused = f.relay.claim(SENDER, f.now);
	assert(refused.result == SiteMapRelay::STORAGE_FULL && f.relay.summary() == 1);

	// Worked out again for every window as free space changes
	f.storage.available = 1000;
	std::vector<uint8_t> received;
	f.fill();
	assert(f.storage.window.size() == MAX_WINDOW);
	f.drain(received);
	f.storage.available = 6;
	assert(f.relay.claim(SENDER, f.now).window == 6);
}

void test_upload_rules() {
	Fixture f;
	f.relay.join(RECEIVER, f.now);
	assert(f.relay.upload(SENDER, 0, f.image.data(), 6, f.now).result == SiteMapRelay::NOT_PERMITTED);
	f.relay.claim(SENDER, f.now);
	assert(f.relay.upload(OTHER_SENDER, 0, f.image.data(), 6, f.now).result == SiteMapRelay::NOT_PERMITTED);
	assert(f.relay.upload(SENDER, 6, f.image.data(), 6, f.now).result == SiteMapRelay::OUT_OF_BOUND);
	assert(f.relay.upload(SENDER, 0, f.image.data(), 7, f.now).result == SiteMapRelay::OUT_OF_BOUND);
	assert(f.relay.upload(SENDER, 0, f.image.data(), 0, f.now).result == SiteMapRelay::DATA_MISSING);
	assert(f.relay.upload(SENDER, 0, f.image.data(), 6, f.now).result == SiteMapRelay::SUCCESS);
	assert(f.relay.upload(SENDER, 6, f.image.data() + 6, 6, f.now).state == SiteMapRelay::FILLING);
	assert(f.relay.upload(SENDER, 12, f.image.data() + 12, 6, f.now).state == SiteMapRelay::READY);
	assert(f.relay.upload(SENDER, 18, f.image.data() + 18, 6, f.now).result == SiteMapRelay::OUT_OF_BOUND);

	// The last window ends with the image
	Fixture g(20);
	g.relay.join(RECEIVER, g.now);
	std::vector<uint8_t> received;
	g.fill();
	g.drain(received);
	SiteMapRelay::Status last = g.relay.claim(SENDER, g.now);
	assert(last.offset == 18 && last.window == MAX_WINDOW);
	assert(g.relay.upload(SENDER, 18, g.image.data() + 18, 3, g.now).result == SiteMapRelay::OUT_OF_BOUND);
	assert(g.relay.upload(SENDER, 18, g.image.data() + 18, 2, g.now).state == SiteMapRelay::READY);
}

void test_confirm() {
	Fixture f;
	f.relay.join(RECEIVER, f.now);
	f.relay.join(SECOND_RECEIVER, f.now);
	assert(f.relay.batch(RECEIVER, f.now).result == SiteMapRelay::OUT_OF_BOUND);
	f.fill();
	assert(f.relay.batch(SECOND_RECEIVER, f.now).result == SiteMapRelay::NOT_PERMITTED);
	assert(f.relay.confirm(SECOND_RECEIVER, MAX_WINDOW, f.now).result == SiteMapRelay::NOT_PERMITTED);
	assert(f.relay.confirm(RECEIVER, MAX_WINDOW - 1, f.now).result == SiteMapRelay::OUT_OF_BOUND);
	assert(f.relay.confirm(RECEIVER, MAX_WINDOW, f.now).result == SiteMapRelay::SUCCESS);
	SiteMapRelay::Status next = f.relay.join(RECEIVER, f.now);
	assert(next.offset == MAX_WINDOW && next.length == 0 && next.state == SiteMapRelay::FILLING);
	assert(f.relay.join(SECOND_RECEIVER, f.now).position == 1);
}

void test_whole_transfers() {
	for (uint32_t size : {1u, 6u, 18u, 19u, 40u, 1000u}) {
		Fixture f(size);
		f.relay.join(RECEIVER, f.now);
		f.relay.join(SECOND_RECEIVER, f.now);
		f.relay.join(0xB3, f.now);
		assert(f.transfer(RECEIVER) == f.image);

		// The next receiver in the queue starts from the beginning, with the same sender
		SiteMapRelay::Status second = f.relay.join(SECOND_RECEIVER, f.now);
		assert(second.position == 0 && second.offset == 0);
		assert(f.transfer(SECOND_RECEIVER) == f.image);
		assert(f.transfer(0xB3) == f.image);
		assert(f.relay.summary() == 0);
		assert(f.relay.claim(SENDER, f.now).result == SiteMapRelay::DATA_MISSING);
	}
}

void test_sender_lease() {
	Fixture f;
	f.relay.join(RECEIVER, f.now);
	std::vector<uint8_t> received;
	f.fill();
	f.drain(received);

	// The sender writes part of a window, then goes quiet
	f.relay.claim(SENDER, f.now);
	f.relay.upload(SENDER, MAX_WINDOW, f.image.data() + MAX_WINDOW, REQUEST, f.now);
	f.now += SENDER_LEASE / 2;
	f.relay.join(RECEIVER, f.now);
	f.now += SENDER_LEASE / 2 + 1;
	SiteMapRelay::Status waiting = f.relay.join(RECEIVER, f.now);
	assert(waiting.state == SiteMapRelay::WAITING && waiting.offset == MAX_WINDOW && waiting.length == 0);
	assert(f.storage.window.empty() && f.relay.summary() == 1);

	// Another app with the same image resumes where the receiver left off
	SiteMapRelay::Status resumed = f.relay.claim(OTHER_SENDER, f.now);
	assert(resumed.result == SiteMapRelay::SUCCESS && resumed.offset == MAX_WINDOW);
	while (received.size() < f.image.size()) {
		f.fill(OTHER_SENDER);
		f.drain(received);
	}
	assert(received == f.image);
}

void test_sender_lease_keeps_finished_batch() {
	Fixture f;
	f.relay.join(RECEIVER, f.now);

	// The sender finishes a window, then goes quiet before the receiver has downloaded it
	f.fill();
	f.now += SENDER_LEASE + 1;
	SiteMapRelay::Status ready = f.relay.join(RECEIVER, f.now);
	assert(ready.state == SiteMapRelay::READY && ready.length == MAX_WINDOW && f.relay.summary() == 1);

	// The receiver still gets the batch, then waits for another app to send the rest
	std::vector<uint8_t> received;
	f.drain(received);
	SiteMapRelay::Status waiting = f.relay.join(RECEIVER, f.now);
	assert(waiting.state == SiteMapRelay::WAITING && waiting.offset == MAX_WINDOW);
	while (received.size() < f.image.size()) {
		f.fill(OTHER_SENDER);
		f.drain(received);
	}
	assert(received == f.image);
}

void test_receiver_lease() {
	Fixture f;
	f.relay.join(RECEIVER, f.now);
	f.relay.join(SECOND_RECEIVER, f.now);
	f.relay.join(0xB3, f.now);
	f.fill();

	// The current receiver goes quiet: its window is dropped and the next receiver starts over
	f.now += RECEIVER_LEASE / 2;
	f.relay.join(SECOND_RECEIVER, f.now);
	f.relay.claim(SENDER, f.now);
	f.now += RECEIVER_LEASE / 2 + 1;
	SiteMapRelay::Status second = f.relay.join(SECOND_RECEIVER, f.now);
	assert(second.position == 0 && second.offset == 0 && second.length == 0);
	assert(f.storage.window.empty());

	// A queued receiver that goes quiet just leaves
	f.relay.claim(SENDER, f.now);
	assert(f.relay.join(SECOND_RECEIVER, f.now).result == SiteMapRelay::SUCCESS);
	assert(f.transfer(SECOND_RECEIVER) == f.image);
	assert(f.relay.summary() == 0);

	// A receiver that comes back after its lease lapsed joins again at the end of the queue
	assert(f.relay.join(RECEIVER, f.now).position == 0);
}

void test_reset_and_image_change() {
	Fixture f;
	f.relay.join(RECEIVER, f.now);
	f.fill();
	f.relay.set_image_size(40);
	assert(f.relay.summary() == 2 && !f.storage.window.empty());
	f.relay.set_image_size(41);
	assert(f.relay.summary() == 0 && f.storage.window.empty());
	assert(f.relay.claim(SENDER, f.now).result == SiteMapRelay::DATA_MISSING);

	f.relay.join(RECEIVER, f.now);
	f.fill();
	f.relay.reset();
	assert(f.relay.summary() == 0 && f.storage.window.empty());
	assert(f.relay.batch(RECEIVER, f.now).result == SiteMapRelay::NOT_PERMITTED);

	// Nothing to send without an image
	f.relay.set_image_size(0);
	assert(f.relay.join(RECEIVER, f.now).result == SiteMapRelay::DATA_MISSING);
}

void test_queue() {
	Fixture f;
	for (uint32_t index = 0; index < SiteMapRelay::MAX_RECEIVERS; index++) {
		assert(f.relay.join(0xC0 + index, f.now).position == index);
	}
	assert(f.relay.join(0xD0, f.now).result == SiteMapRelay::OUT_OF_BOUND);
	assert(f.relay.join(0xC2, f.now).position == 2);
	assert(f.relay.leave(0xC1, f.now).result == SiteMapRelay::SUCCESS);
	assert(f.relay.join(0xC2, f.now).position == 1);
	assert(f.relay.join(0xD0, f.now).position == 3);
	assert(f.relay.join(0, f.now).result == SiteMapRelay::DATA_MISSING);
}

void test_leave() {
	Fixture f;
	f.relay.join(RECEIVER, f.now);
	f.relay.join(SECOND_RECEIVER, f.now);
	std::vector<uint8_t> received;
	f.fill();
	f.drain(received);
	f.fill();

	// The sender cancelling leaves its finished batch, and the receiver's progress, for the next sender
	f.relay.leave(SENDER, f.now);
	SiteMapRelay::Status ready = f.relay.join(RECEIVER, f.now);
	assert(ready.state == SiteMapRelay::READY && ready.offset == MAX_WINDOW && ready.length == MAX_WINDOW);
	f.drain(received);
	SiteMapRelay::Status waiting = f.relay.join(RECEIVER, f.now);
	assert(waiting.state == SiteMapRelay::WAITING && waiting.offset == 2 * MAX_WINDOW && f.storage.window.empty());

	// The current receiver cancelling hands the transfer to the next one from the start
	f.fill(OTHER_SENDER);
	f.relay.leave(RECEIVER, f.now);
	SiteMapRelay::Status next = f.relay.join(SECOND_RECEIVER, f.now);
	assert(next.position == 0 && next.offset == 0 && next.state == SiteMapRelay::FILLING && f.storage.window.empty());
	f.relay.leave(SECOND_RECEIVER, f.now);
	assert(f.relay.summary() == 0);
	assert(f.relay.leave(0xEE, f.now).result == SiteMapRelay::SUCCESS);
}

void test_storage_failure() {
	Fixture f;
	f.relay.join(RECEIVER, f.now);
	f.relay.claim(SENDER, f.now);
	assert(f.relay.upload(SENDER, 0, f.image.data(), REQUEST, f.now).result == SiteMapRelay::SUCCESS);
	f.storage.fail_next_write = true;
	assert(f.relay.upload(SENDER, REQUEST, f.image.data() + REQUEST, REQUEST, f.now).result ==
		SiteMapRelay::STORAGE_FULL);
	assert(f.storage.window.empty());

	// The window is worked out again from the space now free, and the transfer carries on
	f.storage.available = 12;
	SiteMapRelay::Status reclaimed = f.relay.claim(SENDER, f.now);
	assert(reclaimed.result == SiteMapRelay::SUCCESS && reclaimed.offset == 0 && reclaimed.length == 0);
	assert(reclaimed.window == 12);
	assert(f.transfer() == f.image);
}

void test_clock_wraparound() {
	Fixture f;
	f.now = 0xFFFFFF00u;
	f.relay.join(RECEIVER, f.now);
	f.relay.claim(SENDER, f.now);
	f.now += 0x200;
	assert(f.relay.claim(OTHER_SENDER, f.now).result == SiteMapRelay::NOT_PERMITTED);
	f.now += SENDER_LEASE;
	f.relay.join(RECEIVER, f.now);
	assert(f.relay.claim(OTHER_SENDER, f.now).result == SiteMapRelay::SUCCESS);
}

} // namespace

int main() {
	test_join_and_claim();
	test_window_size();
	test_upload_rules();
	test_confirm();
	test_whole_transfers();
	test_sender_lease();
	test_sender_lease_keeps_finished_batch();
	test_receiver_lease();
	test_reset_and_image_change();
	test_queue();
	test_leave();
	test_storage_failure();
	test_clock_wraparound();
	return 0;
}
