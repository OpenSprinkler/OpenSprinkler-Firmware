#include "boards/board_profile.h"

#include <assert.h>

using namespace osboard;

static void check_sensor_pins(ProfileId id, uint8_t count, uint8_t sn1,
	uint8_t sn2, uint8_t sn3, uint8_t sn4) {
	select(id);
	assert(active().sensor_count == count);
	assert(active().pins.sensors[0] == sn1);
	assert(active().pins.sensors[1] == sn2);
	assert(active().pins.sensors[2] == sn3);
	assert(active().pins.sensors[3] == sn4);
}

int main() {
	select(PROFILE_UNKNOWN);
	assert(active().sensor_count == 0);
	assert(active().pins.rf_tx == UNUSED_PIN);
	assert(!has(CAP_ETHERNET));

	check_sensor_pins(PROFILE_OS_30, 2, 12, 13, UNUSED_PIN, UNUSED_PIN);
	assert(active().pins.buttons[0] == IO_EXPANDER_PIN_BASE + 1);
	assert(active().pins.buttons[1] == 0);
	assert(active().pins.rf_rx == 14);
	assert(active().pins.rf_tx == 16);
	assert(active().pins.boost == IO_EXPANDER_PIN_BASE + 6);
	assert(!has(CAP_ETHERNET));

	check_sensor_pins(PROFILE_OS_31, 2, IO_EXPANDER_PIN_BASE + 8,
		IO_EXPANDER_PIN_BASE + 9, UNUSED_PIN, UNUSED_PIN);
	assert(active().pins.buttons[0] == IO_EXPANDER_PIN_BASE + 10);
	assert(active().pins.io_expander_interrupt == 12);
	assert(active().pins.latch_common == IO_EXPANDER_PIN_BASE + 15);
	assert(!has(CAP_ETHERNET));

	check_sensor_pins(PROFILE_OS_32_33, 2, 3, 10, UNUSED_PIN, UNUSED_PIN);
	assert(active().pins.buttons[0] == 2);
	assert(active().pins.rf_tx == 15);
	assert(active().pins.latch_common_anode == IO_EXPANDER_PIN_BASE + 8);
	assert(active().pins.latch_common_cathode == IO_EXPANDER_PIN_BASE + 15);
	assert(has(CAP_ETHERNET));

	check_sensor_pins(PROFILE_OS_34, 4, 3, 10,
		IO_EXPANDER_PIN_BASE + 10, IO_EXPANDER_PIN_BASE + 11);
	assert(has(CAP_ETHERNET));

	check_sensor_pins(PROFILE_OS_40, 4, 1, 2, 3, 6);
	assert(active().pins.buttons[0] == IO_EXPANDER_PIN_BASE + 8);
	assert(active().pins.buttons[1] == 9);
	assert(active().pins.buttons[2] == IO_EXPANDER_PIN_BASE + 9);
	assert(active().pins.rf_tx == 7);
	assert(active().pins.boost == IO_EXPANDER_PIN_BASE + 13);
	assert(active().pins.boost_enable == IO_EXPANDER_PIN_BASE + 14);
	assert(has(CAP_ETHERNET));
	assert(OS40_CURRENT_SENSE_PIN == 0);
	assert(OS40_ETHERNET_CS_PIN == 18);
	assert(OS40_I2C_CLOCK_PIN == 22);
	assert(OS40_I2C_DATA_PIN == 23);

	check_sensor_pins(PROFILE_OSPI_BOARD, 2, 14, 23, UNUSED_PIN, UNUSED_PIN);
	assert(active().pins.buttons[0] == 24);
	assert(active().pins.rf_tx == 15);
	assert(OSPI_SHIFT_LATCH_PIN == 22);
	assert(OSPI_SHIFT_DATA_PIN == 27);
	assert(OSPI_SHIFT_DATA_ALT_PIN == 21);
	assert(OSPI_SHIFT_CLOCK_PIN == 4);
	assert(OSPI_SHIFT_OUTPUT_ENABLE_PIN == 17);

	check_sensor_pins(PROFILE_DEMO_BOARD, 2, 0, 0, 0, 0);
	assert(active().pins.rf_tx == 0);

	return 0;
}
