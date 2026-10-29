#pragma once

#include "../defines.h"
#include "OpenThingsFramework.h"

void server_change_values(const OTF::Request&, OTF::Response&);
void server_json_controller(const OTF::Request&, OTF::Response&);
void server_delete_program(const OTF::Request&, OTF::Response&);
void server_change_program(const OTF::Request&, OTF::Response&);
void server_change_runonce(const OTF::Request&, OTF::Response&);
void server_manual_program(const OTF::Request&, OTF::Response&);
void server_moveup_program(const OTF::Request&, OTF::Response&);
void server_json_programs(const OTF::Request&, OTF::Response&);
void server_json_program_adj(const OTF::Request&, OTF::Response&);
void server_change_options(const OTF::Request&, OTF::Response&);
void server_json_options(const OTF::Request&, OTF::Response&);
void server_change_password(const OTF::Request&, OTF::Response&);
void server_json_status(const OTF::Request&, OTF::Response&);
void server_change_manual(const OTF::Request&, OTF::Response&);
void server_change_stations(const OTF::Request&, OTF::Response&);
void server_json_stations(const OTF::Request&, OTF::Response&);
void server_json_station_special(const OTF::Request&, OTF::Response&);
void server_json_log(const OTF::Request&, OTF::Response&);
void server_delete_log(const OTF::Request&, OTF::Response&);
void server_view_scripturl(const OTF::Request&, OTF::Response&);
void server_change_scripturl(const OTF::Request&, OTF::Response&);
void server_json_all(const OTF::Request&, OTF::Response&);
void server_pause_queue(const OTF::Request&, OTF::Response&);
void server_json_debug(const OTF::Request&, OTF::Response&);
#if defined(ARDUINO)
void server_list_files(const OTF::Request&, OTF::Response&);
#if defined(ENABLE_DEBUG)
void server_delete_file(const OTF::Request&, OTF::Response&);
#endif
#endif
void server_json_sensors(const OTF::Request&, OTF::Response&);
void server_change_sensor(const OTF::Request&, OTF::Response&);
void server_delete_sensor(const OTF::Request&, OTF::Response&);
void server_json_sensor_log(const OTF::Request&, OTF::Response&);
void server_delete_sensor_log(const OTF::Request&, OTF::Response&);
void server_json_sensor_desc(const OTF::Request&, OTF::Response&);
void server_json_sitemap(const OTF::Request&, OTF::Response&);
void server_change_sitemap_pins(const OTF::Request&, OTF::Response&);
#if defined(SITEMAP_DEVICE_IMAGE)
void server_change_sitemap_image(const OTF::Request&, OTF::Response&);
void server_relay_join(const OTF::Request&, OTF::Response&);
void server_relay_claim(const OTF::Request&, OTF::Response&);
void server_relay_upload(const OTF::Request&, OTF::Response&);
void server_relay_download(const OTF::Request&, OTF::Response&);
void server_relay_confirm(const OTF::Request&, OTF::Response&);
void server_relay_leave(const OTF::Request&, OTF::Response&);
#else
void server_upload_sitemap_image(const OTF::Request&, OTF::Response&);
void server_json_sitemap_image(const OTF::Request&, OTF::Response&);
#endif
void server_delete_sitemap(const OTF::Request&, OTF::Response&);
void server_upload_sitemap_drawings(const OTF::Request&, OTF::Response&);
void server_json_sitemap_drawings(const OTF::Request&, OTF::Response&);

void server_json_stations_main(const OTF::Request&, OTF::Response&);
void server_json_options_main();
void server_json_programs_main(const OTF::Request&, OTF::Response&);
void server_json_controller_main(const OTF::Request&, OTF::Response&);
void server_json_status_main();
void server_json_sensors_main(const OTF::Request&, OTF::Response&);
