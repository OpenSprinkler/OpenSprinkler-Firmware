#pragma once

#include <stdint.h>

bool sensor_port_analog_capable(uint8_t input);
bool sensor_port_is_analog(uint8_t input);
void sensor_ports_initialize();
void sensor_ports_apply();

// Read references in one scoped operation. Includes disabled logical sensors.
bool sensor_port_references(uint8_t &analog_mask, uint8_t &digital_mask);
bool sensor_port_transition_allowed(uint8_t input, uint8_t current, uint8_t requested,
	uint8_t analog_mask, uint8_t digital_mask);
