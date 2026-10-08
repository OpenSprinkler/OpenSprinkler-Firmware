#include "services/remote_station.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

RemoteStationRuntime remote_station_runtime[MAX_NUM_STATIONS] = {};

namespace {

constexpr uint32_t REMOTE_VERIFY_DELAY_MS = 350;
constexpr uint32_t REMOTE_VERIFY_RETRY_MS = 750;
constexpr uint32_t REMOTE_CONFIRMED_POLL_MS = 30000;
constexpr uint8_t REMOTE_VERIFY_POLLS = 3;
constexpr uint8_t REMOTE_FAST_RETRIES = 3;

uint32_t retry_delay(uint8_t attempts) {
	if (attempts <= 1) return 1000;
	if (attempts == 2) return 5000;
	if (attempts == 3) return 15000;
	return 60000;
}

const char *http_body(const char *response) {
	if (!response) return nullptr;
	const char *body = strstr(response, "\r\n\r\n");
	if (body) return body + 4;
	body = strstr(response, "\n\n");
	return body ? body + 2 : nullptr;
}

const char *find_json_value(const char *body, const char *key) {
	const char *position = strstr(body, key);
	if (!position) return nullptr;
	position += strlen(key);
	while (*position && isspace(static_cast<unsigned char>(*position))) position++;
	if (*position++ != ':') return nullptr;
	while (*position && isspace(static_cast<unsigned char>(*position))) position++;
	return position;
}

} // namespace

bool remote_station_http_success(const char *response) {
	if (!response || strncmp(response, "HTTP/", 5) != 0) return false;
	const char *space = strchr(response, ' ');
	if (!space) return false;
	char *end = nullptr;
	const long status = strtol(space + 1, &end, 10);
	return end != space + 1 && status >= 200 && status < 300;
}

void remote_station_schedule(uint8_t sid, bool target, uint32_t now_ms, bool refresh) {
	if (sid >= MAX_NUM_STATIONS) return;
	RemoteStationRuntime& runtime = remote_station_runtime[sid];
	const bool same_target = runtime.status != REMOTE_STATUS_NONE && runtime.target == target;

	if (same_target && runtime.phase != REMOTE_PHASE_IDLE &&
		runtime.status != REMOTE_STATUS_CONFIRMED) return;
	if (same_target && !refresh && runtime.status == REMOTE_STATUS_CONFIRMED) return;

	if (!same_target) {
		runtime.attempts = 0;
		runtime.error = REMOTE_ERROR_NONE;
	}
	runtime.target = target;
	runtime.phase = REMOTE_PHASE_COMMAND;
	runtime.verify_polls = 0;
	runtime.next_action_ms = now_ms;
	if (runtime.status != REMOTE_STATUS_FAILED || !same_target) {
		runtime.status = REMOTE_STATUS_PENDING;
	}
}

void remote_station_clear(uint8_t sid) {
	if (sid >= MAX_NUM_STATIONS) return;
	remote_station_runtime[sid] = {};
}

bool remote_station_due(uint8_t sid, uint32_t now_ms) {
	if (sid >= MAX_NUM_STATIONS) return false;
	const RemoteStationRuntime& runtime = remote_station_runtime[sid];
	return runtime.phase != REMOTE_PHASE_IDLE &&
		static_cast<int32_t>(now_ms - runtime.next_action_ms) >= 0;
}

void remote_station_command_finished(uint8_t sid, uint8_t error, uint32_t now_ms) {
	if (sid >= MAX_NUM_STATIONS) return;
	RemoteStationRuntime& runtime = remote_station_runtime[sid];
	if (runtime.attempts < 255) runtime.attempts++;
	runtime.error = error;
	runtime.phase = REMOTE_PHASE_VERIFY;
	// A transport failure may still mean the remote received the request, but one
	// verification is enough before resending. A received response gets three
	// polls so the remote main loop has time to apply an accepted command.
	runtime.verify_polls = error == REMOTE_ERROR_TRANSPORT ? REMOTE_VERIFY_POLLS - 1 : 0;
	runtime.next_action_ms = now_ms + REMOTE_VERIFY_DELAY_MS;
	runtime.status = runtime.attempts > 1 ? REMOTE_STATUS_RETRYING : REMOTE_STATUS_PENDING;
}

void remote_station_verification_finished(uint8_t sid, bool received, bool actual,
	uint8_t error, uint32_t now_ms) {
	if (sid >= MAX_NUM_STATIONS) return;
	RemoteStationRuntime& runtime = remote_station_runtime[sid];

	if (received && actual == (runtime.target != 0)) {
		runtime.status = REMOTE_STATUS_CONFIRMED;
		runtime.error = REMOTE_ERROR_NONE;
		runtime.attempts = 0;
		// Keep checking confirmed software state. A later mismatch (for example,
		// after a satellite reboot) moves back through the retry path.
		runtime.phase = REMOTE_PHASE_VERIFY;
		runtime.verify_polls = 0;
		runtime.next_action_ms = now_ms + REMOTE_CONFIRMED_POLL_MS;
		return;
	}

	runtime.error = received ? REMOTE_ERROR_STATE_MISMATCH : error;
	if (received && ++runtime.verify_polls < REMOTE_VERIFY_POLLS) {
		runtime.status = REMOTE_STATUS_RETRYING;
		runtime.phase = REMOTE_PHASE_VERIFY;
		runtime.next_action_ms = now_ms + REMOTE_VERIFY_RETRY_MS;
		return;
	}

	runtime.phase = REMOTE_PHASE_COMMAND;
	runtime.verify_polls = 0;
	runtime.status = runtime.attempts >= REMOTE_FAST_RETRIES ?
		REMOTE_STATUS_FAILED : REMOTE_STATUS_RETRYING;
	runtime.next_action_ms = now_ms + retry_delay(runtime.attempts);
}

bool remote_station_parse_command_response(const char *response, uint8_t *result) {
	if (!result || !remote_station_http_success(response)) return false;
	const char *body = http_body(response);
	if (!body) return false;
	const char *value = find_json_value(body, "\"result\"");
	if (!value) return false;
	char *end = nullptr;
	long parsed = strtol(value, &end, 10);
	if (end == value || parsed < 0 || parsed > 255) return false;
	*result = static_cast<uint8_t>(parsed);
	return true;
}

bool remote_station_parse_status_response(const char *response, uint8_t remote_sid,
	bool *active) {
	if (!active || !remote_station_http_success(response)) return false;
	const char *body = http_body(response);
	if (!body) return false;
	const char *value = find_json_value(body, "\"sn\"");
	if (!value || *value++ != '[') return false;

	for (uint16_t index = 0; index <= remote_sid; index++) {
		while (*value && isspace(static_cast<unsigned char>(*value))) value++;
		char *end = nullptr;
		long parsed = strtol(value, &end, 10);
		if (end == value || parsed < 0 || parsed > 1) return false;
		if (index == remote_sid) {
			*active = parsed != 0;
			return true;
		}
		value = end;
		while (*value && isspace(static_cast<unsigned char>(*value))) value++;
		if (*value++ != ',') return false;
	}
	return false;
}
