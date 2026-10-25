#pragma once

#include <stdint.h>

// High impedance releases both the terminal and the external pull-up control.
void sensor_input_configure(uint8_t input, bool high_impedance);
float sensor_input_voltage(uint8_t input);
