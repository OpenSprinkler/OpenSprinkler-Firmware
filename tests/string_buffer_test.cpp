#include <cassert>
#include <cstring>
#include <string>

#include "api/http.h"
#include "util/utils.h"

int main() {
	std::string long_value(300, 'a');
	long_value[10] = '"';
	long_value[290] = '\\';
	strReplaceQuoteBackslash(&long_value[0]);
	assert(long_value.size() == 300);
	assert(long_value[10] == '\'');
	assert(long_value[290] == '/');

	char copied[301];
	strncpy_P0(copied, long_value.c_str(), 300);
	assert(copied[300] == 0);
	assert(memcmp(copied, long_value.data(), 300) == 0);

	std::string request = "loc=" + long_value + "&next=1";
	char parsed[321];
	uint8_t found = 0;
	uint16_t length = findKeyVal(request.c_str(), parsed, sizeof(parsed), "loc", false, &found);
	assert(found == 1);
	assert(length == 300);
	assert(strlen(parsed) == 300);
	assert(memcmp(parsed, long_value.data(), 300) == 0);

	std::string exact_value(256, 'x');
	request = "loc=" + exact_value;
	length = findKeyVal(request.c_str(), parsed, sizeof(parsed), "loc", false, &found);
	assert(found == 1);
	assert(length == 256);

	const char mqtt_payload[] = {
		'c', 'm', '?', 'p', 'w', '=', 'a', 'b', 'c', '&',
		's', 'i', 'd', '=', '7', '&', 'e', 'n', '=', '1'
	};
	length = findKeyVal(mqtt_payload + 2, sizeof(mqtt_payload) - 2, parsed, sizeof(parsed),
		"sid", false, &found);
	assert(found == 1);
	assert(length == 1);
	assert(strcmp(parsed, "7") == 0);

	const char legacy_payload[] = {'c', 'm', 's', 'i', 'd', '=', '3', '&', 'e', 'n', '=', '1'};
	length = findKeyVal(legacy_payload + 2, sizeof(legacy_payload) - 2, parsed, sizeof(parsed),
		"sid", false, &found);
	assert(found == 1);
	assert(length == 1);
	assert(strcmp(parsed, "3") == 0);

	const char bounded_payload[] = {'r', 'd', '=', '4', '8', '&', 'r', 'd', '=', '9', '9'};
	length = findKeyVal(bounded_payload, 5, parsed, sizeof(parsed), "rd", false, &found);
	assert(found == 1);
	assert(length == 2);
	assert(strcmp(parsed, "48") == 0);

	const char empty_payload[] = {'c', 'v', '?', 'r', 's', 'n', '=', '&', 'e', 'n', '=', '1'};
	length = findKeyVal(empty_payload + 2, sizeof(empty_payload) - 2, parsed, sizeof(parsed),
		"rsn", false, &found);
	assert(found == 1);
	assert(length == 0);
	assert(parsed[0] == 0);

	char too_small[3];
	length = findKeyVal(mqtt_payload + 2, sizeof(mqtt_payload) - 2, too_small, sizeof(too_small),
		"pw", false, &found);
	assert(found == 0);
	assert(length == 0);
	assert(too_small[0] == 0);

	const char runonce_query[] = "?pw=abc&uwt=1&cnt=2&int=60&t=[600,0]";
	length = findKeyVal(runonce_query, sizeof(runonce_query) - 1, parsed, sizeof(parsed),
		"t", false, &found);
	assert(found == 1);
	assert(strcmp(parsed, "[600,0]") == 0);

	const char blocked_action_query[] = "?rrsn=1&en=1";
	length = findKeyVal(blocked_action_query, sizeof(blocked_action_query) - 1,
		parsed, sizeof(parsed), "rsn", false, &found);
	assert(found == 0);
	assert(length == 0);

	const char exact_action_query[] = "?rrsn=1&rsn=0";
	length = findKeyVal(exact_action_query, sizeof(exact_action_query) - 1,
		parsed, sizeof(parsed), "rsn", false, &found);
	assert(found == 1);
	assert(strcmp(parsed, "0") == 0);
	return 0;
}
