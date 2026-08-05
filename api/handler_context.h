#pragma once

#include "http.h"
#include "../OpenSprinkler.h"
#include "../main.h"
#include "../program.h"

extern OTF::OpenThingsFramework* otf;
extern char tmp_buffer[];
extern char ether_buffer[];
extern OpenSprinkler os;
extern ProgramData pd;
extern uint32_t flow_count;

#define OTF_PARAMS_DEF const OTF::Request &req, OTF::Response &res
#define OTF_PARAMS req, res
#define FKV_SOURCE req
#define handle_return(code) { if ((code) != HTML_OK) otf_send_result(req, res, (code)); return; }
