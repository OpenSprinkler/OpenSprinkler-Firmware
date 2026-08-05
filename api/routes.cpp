#include "routes.h"

#include "handlers.h"
#include "http.h"
#include "../defines.h"

#include <cstring>

using ApiHandler = void (*)(const OTF::Request&, OTF::Response&);

namespace {

const char* route_uris[] PROGMEM = {
	"cv", "jc", "dp", "cp", "cr", "mp", "up", "jp", "jpa", "co", "jo", "sp",
	"js", "cm", "cs", "jn", "je", "jl", "dl", "su", "cu", "ja", "pq", "db",
#if defined(ARDUINO)
	"lf",
#if defined(ENABLE_DEBUG)
	"df",
#endif
#endif
	"jsn", "csn", "dsn", "jsl", "dsl", "jsd",
};

ApiHandler route_handlers[] = {
	server_change_values,
	server_json_controller,
	server_delete_program,
	server_change_program,
	server_change_runonce,
	server_manual_program,
	server_moveup_program,
	server_json_programs,
	server_json_program_adj,
	server_change_options,
	server_json_options,
	server_change_password,
	server_json_status,
	server_change_manual,
	server_change_stations,
	server_json_stations,
	server_json_station_special,
	server_json_log,
	server_delete_log,
	server_view_scripturl,
	server_change_scripturl,
	server_json_all,
	server_pause_queue,
	server_json_debug,
#if defined(ARDUINO)
	server_list_files,
#if defined(ENABLE_DEBUG)
	server_delete_file,
#endif
#endif
	server_json_sensors,
	server_change_sensor,
	server_delete_sensor,
	server_json_sensor_log,
	server_delete_sensor_log,
	server_json_sensor_desc,
};

static_assert(sizeof(route_uris) / sizeof(route_uris[0]) ==
	sizeof(route_handlers) / sizeof(route_handlers[0]), "API route table mismatch");

} // namespace

void register_api_routes(OTF::OpenThingsFramework& framework) {
	char uri[10] = {'/', 0};
	for (size_t index = 0; index < sizeof(route_handlers) / sizeof(route_handlers[0]); index++) {
#if defined(ARDUINO)
		strncpy_P(uri + 1, route_uris[index], sizeof(uri) - 1);
#else
		strncpy(uri + 1, route_uris[index], sizeof(uri) - 1);
#endif
		uri[sizeof(uri) - 1] = 0;
		framework.on(uri, route_handlers[index]);
	}
}
