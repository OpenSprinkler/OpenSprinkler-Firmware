#include "sitemap_relay.h"

namespace {

// Time since then, correct across the millisecond clock wrapping around
uint32_t elapsed(uint32_t now, uint32_t then) {
	return now - then;
}

} // namespace

SiteMapRelay::SiteMapRelay(SiteMapRelayStorage& storage, uint32_t request_bytes, uint32_t max_window,
	uint32_t sender_lease_ms, uint32_t receiver_lease_ms) :
	storage(storage), request_bytes(request_bytes), max_window(max_window), sender_lease_ms(sender_lease_ms),
	receiver_lease_ms(receiver_lease_ms) {}

void SiteMapRelay::set_image_size(uint32_t size) {
	if (size == total) return;
	reset();
	total = size;
}

void SiteMapRelay::reset() {
	drop_window();
	sender.id = 0;
	receiver_count = 0;
	offset = 0;
	state = IDLE;
}

void SiteMapRelay::expire(uint32_t now) {
	if (sender.id && elapsed(now, sender.seen) > sender_lease_ms) release_sender();
	for (int index = receiver_count - 1; index >= 0; index--) {
		if (elapsed(now, receivers[index].seen) > receiver_lease_ms) remove_receiver(index);
	}
	settle();
}

SiteMapRelay::Status SiteMapRelay::join(uint32_t client, uint32_t now) {
	expire(now);
	if (!client || !total) return status(DATA_MISSING);
	int index = find_receiver(client);
	if (index < 0) {
		if (receiver_count >= MAX_RECEIVERS) return status(OUT_OF_BOUND);
		index = receiver_count++;
		receivers[index].id = client;
		settle();
	}
	receivers[index].seen = now;
	return status(SUCCESS, (uint8_t)index);
}

SiteMapRelay::Status SiteMapRelay::claim(uint32_t client, uint32_t now) {
	expire(now);
	if (!client) return status(DATA_MISSING);
	if (sender.id && sender.id != client) return status(NOT_PERMITTED);
	if (!receiver_count) return status(DATA_MISSING);

	sender.id = client;
	sender.seen = now;
	if (state == WAITING) state = FILLING;

	// Each window is sized from the space free when it starts, in whole requests
	if (state == FILLING && !window) {
		uint32_t available = storage.available_bytes();
		uint32_t size = available < max_window ? available : max_window;
		size -= size % request_bytes;
		if (!size) {
			release_sender();
			settle();
			return status(STORAGE_FULL);
		}
		window = size;
	}
	return status(SUCCESS);
}

SiteMapRelay::Status SiteMapRelay::upload(uint32_t client, uint32_t position, const uint8_t* data, uint32_t size,
	uint32_t now) {
	expire(now);
	if (!client || client != sender.id) return status(NOT_PERMITTED);
	sender.seen = now;
	if (!size) return status(DATA_MISSING);
	if (state != FILLING || !window || size > request_bytes || position != offset + length ||
		length + size > window || position + size > total) {
		return status(OUT_OF_BOUND);
	}
	if (!storage.append(data, size)) {
		drop_window();
		return status(STORAGE_FULL);
	}
	length += size;
	if (length == window || offset + length == total) state = READY;
	return status(SUCCESS);
}

SiteMapRelay::Status SiteMapRelay::batch(uint32_t client, uint32_t now) {
	expire(now);
	if (!client || find_receiver(client) != 0) return status(NOT_PERMITTED);
	receivers[0].seen = now;
	if (state != READY) return status(OUT_OF_BOUND);
	return status(SUCCESS);
}

SiteMapRelay::Status SiteMapRelay::confirm(uint32_t client, uint32_t position, uint32_t now) {
	expire(now);
	if (!client || find_receiver(client) != 0) return status(NOT_PERMITTED);
	receivers[0].seen = now;
	if (state != READY || position != offset + length) return status(OUT_OF_BOUND);

	offset += length;
	drop_window();
	if (offset >= total) {
		remove_receiver(0);
	} else {
		state = FILLING;
	}
	settle();
	return status(SUCCESS);
}

SiteMapRelay::Status SiteMapRelay::leave(uint32_t client, uint32_t now) {
	expire(now);
	if (client && client == sender.id) release_sender();
	int index = find_receiver(client);
	if (client && index >= 0) remove_receiver(index);
	settle();
	return status(SUCCESS);
}

uint8_t SiteMapRelay::summary() const {
	if (!receiver_count) return 0;
	return sender.id ? 2 : 1;
}

SiteMapRelay::Status SiteMapRelay::status(uint8_t result, uint8_t position) const {
	return {result, (uint8_t)state, position, offset, length, total, window, request_bytes};
}

int SiteMapRelay::find_receiver(uint32_t client) const {
	for (uint8_t index = 0; index < receiver_count; index++) {
		if (receivers[index].id == client) return index;
	}
	return -1;
}

// Removing the receiver being served abandons its transfer; the next one starts from the beginning
void SiteMapRelay::remove_receiver(int index) {
	if (index == 0) {
		drop_window();
		offset = 0;
	}
	for (uint8_t next = index + 1; next < receiver_count; next++) receivers[next - 1] = receivers[next];
	receiver_count--;
}

void SiteMapRelay::drop_window() {
	storage.clear();
	length = 0;
	window = 0;
	if (state == READY) state = FILLING;
}

// A new sender carries on from offset, since every copy of the image with the same fingerprint has the same bytes.
// A finished batch stays for the receiver to download.
void SiteMapRelay::release_sender() {
	sender.id = 0;
	if (state != READY) drop_window();
}

// Works out the state after receivers or the sender came or went
void SiteMapRelay::settle() {
	if (!receiver_count) {
		sender.id = 0;
		drop_window();
		offset = 0;
		state = IDLE;
	} else if (!sender.id) {
		if (state != READY) state = WAITING;
	} else if (state == IDLE || state == WAITING) {
		state = FILLING;
	}
}
