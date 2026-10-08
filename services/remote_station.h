#pragma once

#include <stddef.h>
#include <stdint.h>

#include "defines.h"

enum RemoteStationStatus : uint8_t {
	REMOTE_STATUS_NONE = 0,
	REMOTE_STATUS_PENDING = 1,
	REMOTE_STATUS_CONFIRMED = 2,
	REMOTE_STATUS_RETRYING = 3,
	REMOTE_STATUS_FAILED = 4,
};

enum RemoteStationError : uint8_t {
	REMOTE_ERROR_NONE = 0,
	REMOTE_ERROR_TRANSPORT = 1,
	REMOTE_ERROR_HTTP = 2,
	REMOTE_ERROR_RESPONSE = 3,
	REMOTE_ERROR_REJECTED = 4,
	REMOTE_ERROR_STATE_MISMATCH = 5,
};

enum RemoteStationPhase : uint8_t {
	REMOTE_PHASE_IDLE = 0,
	REMOTE_PHASE_COMMAND = 1,
	REMOTE_PHASE_VERIFY = 2,
};

struct RemoteStationRuntime {
	uint32_t next_action_ms;
	uint8_t status;
	uint8_t target;
	uint8_t attempts;
	uint8_t error;
	uint8_t phase;
	uint8_t verify_polls;
};

extern RemoteStationRuntime remote_station_runtime[MAX_NUM_STATIONS];

void remote_station_schedule(uint8_t sid, bool target, uint32_t now_ms, bool refresh = false);
void remote_station_clear(uint8_t sid);
bool remote_station_due(uint8_t sid, uint32_t now_ms);
void remote_station_command_finished(uint8_t sid, uint8_t error, uint32_t now_ms);
void remote_station_verification_finished(uint8_t sid, bool received, bool actual,
	uint8_t error, uint32_t now_ms);

bool remote_station_parse_command_response(const char *response, uint8_t *result);
bool remote_station_parse_status_response(const char *response, uint8_t remote_sid,
	bool *active);
bool remote_station_http_success(const char *response);
