#pragma once
#include <cstdio>
#include <cstring>

struct TestStation {
	char name[33] = {};
	bool setHostname(const char* value) {
		strcpy(name, value);
		return true;
	}
};
struct TestWiFi {
	TestStation STA;
	char global_name[32] = {};
	bool setHostname(const char* value) {
		snprintf(global_name, sizeof(global_name), "%s", value);
		return true;
	}
	bool hostname(const char* value) { return STA.setHostname(value); }
};
extern TestWiFi WiFi;
