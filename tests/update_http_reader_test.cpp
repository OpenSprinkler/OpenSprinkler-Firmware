#include "platform/update_http_reader.h"

#include <cassert>
#include <cstdio>
#include <string>

struct Clock {
	static uint32_t time;
	static uint32_t now() { return time; }
	static void idle() { ++time; }
};
uint32_t Clock::time = 0;
using Reader = UpdateHttpReaderBase<Clock>;

struct Client {
	bool alive = true;
	std::string bytes;
	size_t pos = 0;
	uint32_t interval = 0, last = Clock::time;
	bool connected() { return alive; }
	void stop() { alive = false; }
	int read() {
		if (pos == bytes.size() || uint32_t(Clock::time - last) < interval) return -1;
		last = Clock::time;
		return uint8_t(bytes[pos++]);
	}
};

int main() {
	size_t length;
	assert(Reader::valid_query("pw=x&id=test", 12));
	std::string many;
	for (unsigned i = 0; i < 32; ++i) many += "a&";
	assert(!Reader::valid_query(many.data(), many.length()));
	assert(!Reader::valid_query("a\0b", 3));
	assert(Reader::parse_length("3145728", length) && length == Reader::BODY_LIMIT);
	for (const char *s : {"", "-1", "+1", "1x", "1 2", "3145729", "99999999999999999999"})
		assert(!Reader::parse_length(s, length));
	Reader r;
	r.begin();
	assert(!r.begin_body(0, true, 5));
	assert(!r.begin_body(1025, false, 0));
	assert(!r.begin_body(2048, true, 0));
	assert(!r.begin_body(2048, true, 71));
	assert(!r.begin_body(Reader::BODY_LIMIT + 1, true, 5));
	Client c;
	assert(r.read_byte(c) == -1 && !c.alive && Clock::time == 5000);

	// A connected peer cannot keep a header open by trickling bytes.
	r.begin(); c = Client{}; c.bytes = std::string(100, 'a'); c.interval = 400;
	auto start = Clock::time;
	std::string line;
	assert(!r.read_line(c, line, '\n') && Clock::time - start == 5000);
	r.begin(); c = Client{}; c.bytes = std::string(1025, 'x') + "\n";
	assert(!r.read_line(c, line, '\n') && !c.alive);
	r.begin(); c = Client{}; c.bytes = std::string(8193, 'x');
	for (size_t i = 0; i < Reader::HEADER_LIMIT; ++i) assert(r.read_byte(c) >= 0);
	assert(r.read_byte(c) == -1 && !c.alive);

	r.begin(); assert(r.begin_body(2048, true, 70)); c = Client{};
	start = Clock::time;
	assert(r.read_byte(c) == -1 && Clock::time - start == 10000);
	r.begin(); assert(r.begin_body(2048, true, 5)); c = Client{};
	c.bytes = std::string(2048, 'a'); c.interval = 1000; start = Clock::time;
	while (r.read_byte(c) >= 0) {}
	assert(Clock::time - start == Reader::REQUEST_MS && !c.alive);

	// Descriptor bodies have their own short total deadline, not just an idle limit.
	r.begin(); assert(r.begin_body(1000, false, 0)); c = Client{};
	c.bytes = std::string(1000, 'a'); c.interval = 100; start = Clock::time;
	while (r.read_byte(c) >= 0) {}
	assert(Clock::time - start == 5000);

	Clock::time = UINT32_MAX - 100;
	r.begin(); assert(r.begin_body(2048, true, 5)); c = Client{}; start = Clock::time;
	assert(r.read_byte(c) == -1 && uint32_t(Clock::time - start) == 10000);
	r.begin(); c = Client{}; c.alive = false;
	assert(r.read_byte(c) == -1);
	r.begin(); c = Client{}; c.bytes = "abc\r\n";
	assert(r.read_line(c, line, '\r') && line == "abc" && r.read_byte(c) == '\n');
	assert(r.begin_body(3, false, 0)); c.bytes += "xyzEXTRA";
	assert(r.read_byte(c) == 'x' && r.read_byte(c) == 'y' && r.read_byte(c) == 'z');
	assert(r.read_byte(c) == -1 && c.pos == 8);
	puts("Updater reader: deadlines, trickles, limits, EOF, exact length and rollover PASS");
}
