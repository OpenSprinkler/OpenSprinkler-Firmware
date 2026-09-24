#pragma once

#include "api/http.h"

#include <cstddef>
#include <cstdint>

class ParamSource {
public:
	explicit ParamSource(const OTF::Request& request);
	// data must begin with the query portion: either '?' or the first parameter key.
	ParamSource(const uint8_t* data, size_t length);

	uint16_t get(char* buffer, uint16_t max_length, const char* key,
		bool key_in_program_memory = false, uint8_t* key_found = nullptr) const;

private:
	const OTF::Request* request;
	const char* data;
	size_t length;
};

enum ChangeValuesAction : uint16_t {
	CV_ACTION_RESET_STATIONS = 1U << 0,
	CV_ACTION_RESET_RUNNING = 1U << 1,
	CV_ACTION_RESET_OVERCURRENT = 1U << 2,
	CV_ACTION_UPDATE = 1U << 3,
	CV_ACTION_REBOOT = 1U << 4,
	CV_ACTION_ENABLE = 1U << 5,
	CV_ACTION_RAIN_DELAY = 1U << 6,
	CV_ACTION_REMOTE_EXTENSION = 1U << 7,
	CV_ACTION_RESET_AP = 1U << 8,
};

constexpr uint16_t CV_ACTIONS_HTTP =
	CV_ACTION_RESET_STATIONS | CV_ACTION_RESET_RUNNING | CV_ACTION_RESET_OVERCURRENT |
	CV_ACTION_UPDATE | CV_ACTION_REBOOT | CV_ACTION_ENABLE | CV_ACTION_RAIN_DELAY |
	CV_ACTION_REMOTE_EXTENSION | CV_ACTION_RESET_AP;

constexpr uint16_t CV_ACTIONS_MQTT =
	CV_ACTION_RESET_STATIONS | CV_ACTION_REBOOT | CV_ACTION_ENABLE | CV_ACTION_RAIN_DELAY;

uint8_t execute_change_values(const ParamSource& params, uint16_t allowed_actions);
uint8_t execute_manual_station(const ParamSource& params);
uint8_t execute_manual_program(const ParamSource& params);
uint8_t execute_runonce(const ParamSource& params);
