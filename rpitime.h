#ifndef RPI_TIME_H
#define RPI_TIME_H

#include <time.h>

static int hour(time_t ct) {
	struct tm *ti = gmtime(&ct);
	return ti->tm_hour;
}

static int minute(time_t ct) {
	struct tm *ti = gmtime(&ct);
	return ti->tm_min;
}

static int second(time_t ct) {
	struct tm *ti = gmtime(&ct);
	return ti->tm_sec;
}

static int day(time_t ct) {
	struct tm *ti = gmtime(&ct);
	return ti->tm_mday;
}

static int month(time_t ct) {
	struct tm *ti = gmtime(&ct);
	return ti->tm_mon+1;
}

#endif