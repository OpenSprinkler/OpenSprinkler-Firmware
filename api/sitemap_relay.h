#pragma once

#include <cstdint>

/*
 * Relays the site map image from an app that has it to apps that don't, on controllers that leave the image to the
 * apps (SITEMAP_DEVICE_IMAGE). Apps poll; the controller only answers. Each app identifies itself with a random
 * client id. Receivers queue up and are served one at a time; the first app with the image to claim the transfer
 * becomes the sender, and others are turned away until its claim lapses. The image moves in batches ("windows")
 * that the sender writes in requests of at most request_bytes and the current receiver downloads and confirms in one
 * go. A window is as large as the free storage allows, up to max_window.
 *
 * This class holds only the bookkeeping, so it can be tested without a web server or file system; the handlers in
 * sitemap.cpp check the password and image fingerprint, keep it told of the image size, and stream the window.
 */

// Where the window is kept between the sender writing it and the receiver downloading it
class SiteMapRelayStorage {
public:
	virtual ~SiteMapRelayStorage() {}

	// Bytes a window may use without cutting into the space kept free for logs
	virtual uint32_t available_bytes() = 0;
	virtual bool append(const uint8_t* data, uint32_t length) = 0;
	virtual void clear() = 0;
};

class SiteMapRelay {
public:
	static constexpr uint8_t MAX_RECEIVERS = 4;

	enum State : uint8_t {
		IDLE = 0,     // nobody is waiting for the image
		WAITING = 1,  // a receiver is waiting for a sender
		FILLING = 2,  // the sender is writing a window
		READY = 3,    // a window is waiting for the current receiver
	};

	// Result codes, the same as the HTML_* values in http.h
	static constexpr uint8_t SUCCESS = 0x01;
	static constexpr uint8_t DATA_MISSING = 0x10;
	static constexpr uint8_t OUT_OF_BOUND = 0x11;
	static constexpr uint8_t NOT_PERMITTED = 0x30;
	static constexpr uint8_t STORAGE_FULL = 0x40;

	struct Status {
		uint8_t result;
		uint8_t state;
		uint8_t position;  // receiver's place in the queue, 0 for the one being served
		uint32_t offset;   // image offset where the current window starts
		uint32_t length;   // bytes in the current window
		uint32_t total;    // image size
		uint32_t window;   // size of the current window, 0 until the sender claims it
		uint32_t request;  // most image bytes the sender may send in one request
	};

	SiteMapRelay(SiteMapRelayStorage& storage, uint32_t request_bytes, uint32_t max_window, uint32_t sender_lease_ms,
		uint32_t receiver_lease_ms);

	// Keeps the relay told of the stored image size; a different size means a different image and ends any transfer
	void set_image_size(uint32_t size);

	// Ends any transfer, for example when the image is replaced or removed
	void reset();

	// Releases the sender and receivers that haven't been heard from within their lease
	void expire(uint32_t now);

	Status join(uint32_t client, uint32_t now);
	Status claim(uint32_t client, uint32_t now);
	Status upload(uint32_t client, uint32_t offset, const uint8_t* data, uint32_t length, uint32_t now);
	Status batch(uint32_t client, uint32_t now);
	Status confirm(uint32_t client, uint32_t offset, uint32_t now);
	Status leave(uint32_t client, uint32_t now);

	// 0 when idle, 1 while a receiver waits for a sender, 2 while the image is being sent ("smx" in /jo)
	uint8_t summary() const;

private:
	struct Client {
		uint32_t id;
		uint32_t seen;
	};

	SiteMapRelayStorage& storage;
	const uint32_t request_bytes;
	const uint32_t max_window;
	const uint32_t sender_lease_ms;
	const uint32_t receiver_lease_ms;

	State state = IDLE;
	Client sender = {0, 0};
	Client receivers[MAX_RECEIVERS] = {};
	uint8_t receiver_count = 0;
	uint32_t offset = 0;
	uint32_t length = 0;
	uint32_t window = 0;
	uint32_t total = 0;

	Status status(uint8_t result, uint8_t position = 0) const;
	int find_receiver(uint32_t client) const;
	void remove_receiver(int index);
	void drop_window();
	void release_sender();
	void settle();
};
