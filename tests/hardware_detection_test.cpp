#include "boards/hardware_detection.h"

#include <assert.h>
#include <string.h>

using namespace osboard;

namespace {

bool present[256];
bool revision1_strap;
uint8_t probed[16];
uint8_t probe_count;

bool probe_i2c(uint8_t address) {
	probed[probe_count++] = address;
	return present[address];
}

bool probe_revision1() {
	return revision1_strap;
}

void reset_probes() {
	memset(present, 0, sizeof(present));
	memset(probed, 0, sizeof(probed));
	revision1_strap = false;
	probe_count = 0;
}

HardwareDetection detect() {
	return detect_hardware(probe_i2c, probe_revision1);
}

void check_driver(const HardwareDetection& result, uint8_t revision, PowerType power,
	ProfileId profile, DriverKind driver, uint8_t address) {
	assert(result.revision == revision);
	assert(result.power_type == power);
	assert(result.profile == profile);
	assert(result.driver_kind == driver);
	assert(result.driver_address == address);
}

} // namespace

int main() {
	reset_probes();
	present[MAIN_IO_EXPANDER_ADDRESS] = true;
	present[AC_DRIVER_ADDRESS] = true;
	HardwareDetection result = detect();
	check_driver(result, 0, POWER_AC, PROFILE_OS_30, DRIVER_PCF8574, AC_DRIVER_ADDRESS);
	assert(result.separate_main_io);
	assert(probe_count == 2);

	reset_probes();
	present[MAIN_IO_EXPANDER_ADDRESS] = true;
	present[DC_DRIVER_ADDRESS] = true;
	result = detect();
	check_driver(result, 0, POWER_DC, PROFILE_OS_30, DRIVER_PCF8574, DC_DRIVER_ADDRESS);
	assert(probe_count == 3);

	reset_probes();
	present[MAIN_IO_EXPANDER_ADDRESS] = true;
	present[LATCH_DRIVER_ADDRESS] = true;
	result = detect();
	check_driver(result, 0, POWER_LATCH, PROFILE_OS_30, DRIVER_PCF8574, LATCH_DRIVER_ADDRESS);

	reset_probes();
	present[MAIN_IO_EXPANDER_ADDRESS] = true;
	result = detect();
	check_driver(result, 0, POWER_UNKNOWN, PROFILE_OS_30, DRIVER_PCF8574, AC_DRIVER_ADDRESS);

	reset_probes();
	revision1_strap = true;
	present[DC_DRIVER_ADDRESS] = true;
	result = detect();
	check_driver(result, 1, POWER_DC, PROFILE_OS_31, DRIVER_PCA9555, DC_DRIVER_ADDRESS);
	assert(!result.separate_main_io);

	reset_probes();
	present[AC_DRIVER_ADDRESS] = true;
	result = detect();
	check_driver(result, 2, POWER_AC, PROFILE_OS_32_33, DRIVER_PCA9555, AC_DRIVER_ADDRESS);
	assert(probed[1] == EEPROM_ADDRESS + 2);
	assert(probed[2] == CH224_ADDRESS);
	assert(probed[3] == CH224_ADDRESS + 1);
	assert(probed[4] == EEPROM_ADDRESS);

	reset_probes();
	present[EEPROM_ADDRESS] = true;
	present[LATCH_DRIVER_ADDRESS] = true;
	result = detect();
	check_driver(result, 3, POWER_LATCH, PROFILE_OS_32_33, DRIVER_PCA9555, LATCH_DRIVER_ADDRESS);

	reset_probes();
	present[EEPROM_ADDRESS + 2] = true;
	result = detect();
	check_driver(result, 4, POWER_AC, PROFILE_OS_34, DRIVER_PCA9555, AC_DRIVER_ADDRESS);
	assert(!result.initialize_usb_pd);
	assert(probe_count == 4);

	reset_probes();
	present[CH224_ADDRESS] = true;
	present[CH224_ADDRESS + 1] = true;
	result = detect();
	check_driver(result, 4, POWER_DC, PROFILE_OS_34, DRIVER_PCA9555, AC_DRIVER_ADDRESS);
	assert(result.initialize_usb_pd);
	assert(probe_count == 4);

	return 0;
}
