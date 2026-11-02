#pragma once

#include <stddef.h>
#include <stdint.h>

// Clock is injected so the production byte/line reader can run in native tests.
template<class Clock> class UpdateHttpReaderBase {
public:
	static constexpr size_t HEADER_LIMIT = 8192;
	static constexpr size_t LINE_LIMIT = 1024;
	static constexpr size_t BODY_LIMIT = 3 * 1024 * 1024;
	static constexpr size_t DESCRIPTOR_LIMIT = 1024;
	static constexpr uint32_t HEADER_MS = 5000;
	static constexpr uint32_t BODY_IDLE_MS = 10000;
	static constexpr uint32_t REQUEST_MS = 300000;
	static bool valid_query(const char *text, size_t length) {
		unsigned arguments = 1;
		for (size_t i = 0; i < length; ++i) {
			if (!text[i] || ((text[i] == '&' || text[i] == ';') && ++arguments > 32)) return false;
		}
		return true;
	}
	static bool parse_length(const char *text, size_t &length) {
		length = 0;
		if (!text || !*text) return false;
		for (; *text; ++text) {
			if (*text < '0' || *text > '9' || length > (BODY_LIMIT - size_t(*text - '0')) / 10)
				return false;
			length = length * 10 + size_t(*text - '0');
		}
		return true;
	}

	void begin() {
		_started = _phase_started = _last_byte = Clock::now();
		_header_bytes = _body_bytes = _body_length = 0;
		_body = _multipart = _failed = false;
	}

	bool begin_body(size_t length, bool multipart, size_t boundary_length) {
		if (_failed || !length || length > (multipart ? BODY_LIMIT : DESCRIPTOR_LIMIT) ||
			(multipart && (!boundary_length || boundary_length > 70))) return false;
		_body = true;
		_multipart = multipart;
		_body_length = length;
		_phase_started = _last_byte = Clock::now();
		return true;
	}

	template<class Client> int read_byte(Client &client) {
		for (;;) {
			uint32_t now = Clock::now();
			uint32_t phase_limit = _body && _multipart ? REQUEST_MS : HEADER_MS;
			uint32_t idle_limit = _body ? BODY_IDLE_MS : HEADER_MS;
			if (_failed || uint32_t(now - _started) >= REQUEST_MS ||
				uint32_t(now - _phase_started) >= phase_limit ||
				uint32_t(now - _last_byte) >= idle_limit ||
				(_body && _body_bytes >= _body_length)) return stop(client);
			int value = client.read();
			if (value >= 0) {
				_last_byte = now;
				if (_body) ++_body_bytes;
				else if (++_header_bytes > HEADER_LIMIT) return stop(client);
				return value;
			}
			if (!client.connected()) return stop(client);
			Clock::idle();
		}
	}

	template<class Client, class Text> bool read_line(Client &client, Text &line, char delimiter) {
		line = "";
		for (;;) {
			int value = read_byte(client);
			if (value < 0) return false;
			if (value == delimiter) return true;
			if (line.length() >= LINE_LIMIT) { stop(client); return false; }
			line += char(value);
		}
	}

	bool failed() const { return _failed; }

private:
	template<class Client> int stop(Client &client) {
		_failed = true;
		client.stop();
		return -1;
	}
	uint32_t _started = 0, _phase_started = 0, _last_byte = 0;
	size_t _header_bytes = 0, _body_bytes = 0, _body_length = 0;
	bool _body = false, _multipart = false, _failed = false;
};

#if defined(ARDUINO)
#include <Arduino.h>
struct UpdateHttpClock {
	static uint32_t now() { return millis(); }
	static void idle() { yield(); }
};
using UpdateHttpReader = UpdateHttpReaderBase<UpdateHttpClock>;
#endif
