#pragma once

#include <stdint.h>

namespace osboard {

constexpr uint8_t UNUSED_PIN = 255;

enum ProfileId : uint8_t {
	PROFILE_UNKNOWN = 0,
	PROFILE_OS_30,
	PROFILE_OS_31,
	PROFILE_OS_32_33,
	PROFILE_OS_34,
	PROFILE_OS_40,
	PROFILE_OSPI_BOARD,
	PROFILE_DEMO_BOARD,
};

enum Capability : uint8_t {
	CAP_NONE = 0,
	CAP_ETHERNET = 1 << 0,
};

struct BoardPins {
	uint8_t buttons[3];
	uint8_t sensors[4];
	uint8_t rf_rx;
	uint8_t rf_tx;
	uint8_t boost;
	uint8_t boost_enable;
	uint8_t latch_common;
	uint8_t latch_common_anode;
	uint8_t latch_common_cathode;
	uint8_t io_expander_interrupt;
};

struct BoardProfile {
	BoardPins pins;
	uint8_t sensor_count;
	uint8_t capabilities;
};

static_assert(sizeof(BoardPins) == 15, "BoardPins must remain compact");
static_assert(sizeof(BoardProfile) == 17, "BoardProfile must remain compact");

const BoardProfile& active();
void select(ProfileId id);
bool has(Capability capability);

// Shared OpenSprinkler v3 hardware constants.
constexpr uint8_t IO_EXPANDER_PIN_BASE = 0x80;
constexpr uint8_t MAIN_IO_EXPANDER_ADDRESS = 0x20;
constexpr uint8_t AC_DRIVER_ADDRESS = 0x21;
constexpr uint8_t DC_DRIVER_ADDRESS = 0x22;
constexpr uint8_t LATCH_DRIVER_ADDRESS = 0x23;
constexpr uint8_t EXPANDER_ADDRESS_BASE = 0x24;
constexpr uint8_t LCD_ADDRESS = 0x3C;
constexpr uint8_t EEPROM_ADDRESS = 0x50;
constexpr uint8_t CH224_ADDRESS = 0x22;
constexpr uint8_t RTC_ADDRESS = 0x51;
constexpr uint8_t REVISION1_DETECT_PIN = 16;
constexpr uint8_t ETHERNET_CS_PIN = 16;
constexpr uint32_t ETHERNET_SPI_CLOCK_HZ = 20000000UL;

constexpr uint8_t OS30_POWER_RX_PIN = IO_EXPANDER_PIN_BASE + 0;
constexpr uint8_t OS30_POWER_TX_PIN = IO_EXPANDER_PIN_BASE + 2;

constexpr uint16_t OS31_IO_CONFIG = 0x1F00;
constexpr uint16_t OS31_IO_OUTPUT = 0x1F00;

constexpr uint16_t OS32_IO_CONFIG = 0x1000;
constexpr uint16_t OS32_IO_OUTPUT = 0x1E00;
constexpr uint8_t OS32_SHIFT_LATCH_PIN = IO_EXPANDER_PIN_BASE + 9;
constexpr uint8_t OS32_SHIFT_CLOCK_PIN = IO_EXPANDER_PIN_BASE + 10;
constexpr uint8_t OS32_SHIFT_DATA_PIN = IO_EXPANDER_PIN_BASE + 11;
constexpr uint8_t OS32_BOOST_SELECT_PIN = IO_EXPANDER_PIN_BASE + 8;

// OpenSprinkler v4.0 (ESP32-C6). AC and DC use the same PCA9555 address;
// CH224 detection selects the power type at runtime.
constexpr uint8_t OS40_HARDWARE_VERSION = 40;
constexpr uint16_t OS40_IO_CONFIG = 0xFF00;
constexpr uint16_t OS40_IO_OUTPUT = 0x0000;
constexpr uint8_t OS40_CURRENT_SENSE_PIN = 0;
constexpr uint8_t OS40_EXTERNAL_FLASH_CS_PIN = 8;
constexpr uint8_t OS40_ETHERNET_IRQ_PIN = 10;
constexpr uint8_t OS40_ETHERNET_RESET_PIN = 11;
constexpr uint8_t OS40_ETHERNET_CS_PIN = 18;
constexpr uint8_t OS40_SPI_MOSI_PIN = 19;
constexpr uint8_t OS40_SPI_MISO_PIN = 20;
constexpr uint8_t OS40_SPI_CLOCK_PIN = 21;
constexpr uint8_t OS40_I2C_CLOCK_PIN = 22;
constexpr uint8_t OS40_I2C_DATA_PIN = 23;

constexpr uint8_t OSPI_SHIFT_LATCH_PIN = 22;
constexpr uint8_t OSPI_SHIFT_DATA_PIN = 27;
constexpr uint8_t OSPI_SHIFT_DATA_ALT_PIN = 21;
constexpr uint8_t OSPI_SHIFT_CLOCK_PIN = 4;
constexpr uint8_t OSPI_SHIFT_OUTPUT_ENABLE_PIN = 17;

} // namespace osboard
