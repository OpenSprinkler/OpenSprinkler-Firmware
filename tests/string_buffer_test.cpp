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
	return 0;
}
