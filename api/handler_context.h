#pragma once

#include "http.h"
#include "../OpenSprinkler.h"
#include "../core/scheduler.h"
#include "../storage/logging.h"
#include "../core/program.h"

extern OTF::OpenThingsFramework* otf;
extern char tmp_buffer[];
extern char ether_buffer[];
extern OpenSprinkler os;
extern ProgramData pd;
extern uint32_t flow_count;

#define OTF_PARAMS_DEF const OTF::Request &req, OTF::Response &res
#define OTF_PARAMS req, res
#define FKV_SOURCE req
#define handle_return(expression) { \
	const uint8_t result_code = static_cast<uint8_t>(expression); \
	if (result_code != HTML_OK) otf_send_result(req, res, result_code); \
	return; \
}
