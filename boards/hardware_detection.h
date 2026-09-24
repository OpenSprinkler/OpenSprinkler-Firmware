#pragma once

#include "board_profile.h"

#include <stdint.h>

namespace osboard {

enum PowerType : uint8_t {
	POWER_AC = 0xAC,
	POWER_DC = 0xDC,
	POWER_LATCH = 0x1A,
	POWER_UNKNOWN = 0xFF,
};

enum DriverKind : uint8_t {
	DRIVER_PCF8574,
	DRIVER_PCA9555,
};

struct HardwareDetection {
	uint8_t revision;
	PowerType power_type;
	ProfileId profile;
	DriverKind driver_kind;
	uint8_t driver_address;
	bool separate_main_io;
	bool initialize_usb_pd;
};

using I2cProbe = bool (*)(uint8_t address);
using Revision1Probe = bool (*)();

HardwareDetection detect_hardware(I2cProbe probe_i2c, Revision1Probe probe_revision1);
HardwareDetection detect_os4_hardware(I2cProbe probe_i2c);

} // namespace osboard
