#pragma once

#include "OpenThingsFramework.h"

void server_json_stations_main(const OTF::Request&, OTF::Response&);
void server_json_options_main();
void server_json_programs_main(const OTF::Request&, OTF::Response&);
void server_json_controller_main(const OTF::Request&, OTF::Response&);
void server_json_status_main();
void server_json_sensors_main(const OTF::Request&, OTF::Response&);
