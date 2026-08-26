#pragma once

#include "../types.h"

void make_logfile_name(char *name);
void write_log(unsigned char type, time_os_t curr_time);
bool delete_log(char *name);
