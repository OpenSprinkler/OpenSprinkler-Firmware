#include <assert.h>
#include <stdint.h>

#include "services/remote_station.h"

int main() {
	uint8_t result = 0;
	assert(remote_station_parse_command_response(
		"HTTP/1.0 200 OK\r\nContent-Type: application/json\r\n\r\n{\"result\":1,\"item\":\"\"}",
		&result));
	assert(result == 1);
	assert(remote_station_http_success("HTTP/1.1 204 No Content\r\n\r\n"));
	assert(!remote_station_http_success("HTTP/1.1 503 Unavailable\r\n\r\n"));
	assert(!remote_station_parse_command_response(
		"HTTP/1.0 401 Unauthorized\r\n\r\n{\"result\":2}", &result));
	assert(!remote_station_parse_command_response("HTTP/1.0 200 OK\r\n\r\n{}", &result));

	bool active = false;
	assert(remote_station_parse_status_response(
		"HTTP/1.1 200 OK\r\n\r\n{\"sn\":[0,1,0],\"nstations\":3}", 1, &active));
	assert(active);
	assert(remote_station_parse_status_response(
		"HTTP/1.1 200 OK\n\n{\"sn\": [0, 1, 0]}", 2, &active));
	assert(!active);
	assert(!remote_station_parse_status_response(
		"HTTP/1.1 500 Error\r\n\r\n{\"sn\":[1]}", 0, &active));

	remote_station_clear(3);
	remote_station_schedule(3, true, 100);
	assert(remote_station_runtime[3].status == REMOTE_STATUS_PENDING);
	assert(remote_station_runtime[3].phase == REMOTE_PHASE_COMMAND);
	assert(remote_station_due(3, 100));

	remote_station_command_finished(3, REMOTE_ERROR_NONE, 200);
	assert(remote_station_runtime[3].phase == REMOTE_PHASE_VERIFY);
	assert(!remote_station_due(3, 549));
	assert(remote_station_due(3, 550));
	remote_station_verification_finished(3, true, true, REMOTE_ERROR_NONE, 550);
	assert(remote_station_runtime[3].status == REMOTE_STATUS_CONFIRMED);
	assert(remote_station_runtime[3].phase == REMOTE_PHASE_VERIFY);
	assert(!remote_station_due(3, 30549));
	assert(remote_station_due(3, 30550));
	remote_station_verification_finished(3, true, false, REMOTE_ERROR_NONE, 30550);
	assert(remote_station_runtime[3].status == REMOTE_STATUS_RETRYING);
	assert(remote_station_runtime[3].error == REMOTE_ERROR_STATE_MISMATCH);

	remote_station_schedule(3, false, 600);
	remote_station_command_finished(3, REMOTE_ERROR_TRANSPORT, 600);
	remote_station_verification_finished(3, false, false, REMOTE_ERROR_TRANSPORT, 950);
	assert(remote_station_runtime[3].status == REMOTE_STATUS_RETRYING);
	assert(remote_station_runtime[3].phase == REMOTE_PHASE_COMMAND);

	return 0;
}
