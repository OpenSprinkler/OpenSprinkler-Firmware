#pragma once

#include <Wire.h>
#include <stdint.h>

// Maximum number of Power Data Objects (PDOs) the chip can report.
#define CH224_MAX_PDO_COUNT 10
// Size of the source capabilities data buffer.
#define CH224_SRC_CAP_SIZE 0x40

// PDO Types for identifying power supply capabilities
#define PD_TYPE_FIXED_SUPPLY 1
#define PD_TYPE_PPS_SUPPLY 2
#define PD_TYPE_AVS_SUPPLY 3

/**
 * @brief Represents the status of various charging protocols.
 */
typedef struct {
	 bool reserved;
	 bool avs_exists;
	 bool epr_exists;
	 bool epr_activation;
	 bool pd_activation;
	 bool qc3_activation;
	 bool qc2_activation;
	 bool bc_activation;
} ch224_status_t;

/**
 * @brief Represents a Fixed Supply Power Data Object (PDO).
 * All values are in standard units (mV, mA).
 */
typedef struct {
	 uint16_t voltage_mv;
	 uint16_t max_current_ma;
} ch224_fixed_pdo_t;

/**
 * @brief Represents a Programmable Power Supply (PPS) Augmented PDO (APDO).
 * All values are in standard units (mV, mA).
 */
typedef struct {
		bool power_limited;
		uint16_t max_voltage_mv;
		uint16_t min_voltage_mv;
		uint16_t max_current_ma;
} ch224_pps_apdo_t;

/**
 * @brief Represents an Adjustable Voltage Supply (AVS) Augmented PDO (APDO).
 * All values are in standard units (mV, W).
 */
typedef struct {
		uint16_t max_voltage_mv;
		uint16_t min_voltage_mv;
		uint8_t max_power_w;
} ch224_avs_apdo_t;

/**
 * @brief Enumerates the voltage/mode settings for the CH224.
 */
enum class CH224VoltageMode {
		Voltage_5V = 0,
		Voltage_9V = 1,
		Voltage_12V = 2,
		Voltage_15V = 3,
		Voltage_20V = 4,
		Voltage_28V = 5,
		Mode_PPS = 6,
		Mode_AVS = 7,
};


/**
 * @class CH224
 * @brief An object-oriented library for the CH224 USB PD Sink controller.
 * This class combines a clean interface with a full feature set for interacting
 * with the CH224 chip via I2C.
 */
class CH224 {
public:
	/**
	 * @brief Default constructor. Uses I2C address 0x23 and the default Wire interface.
	 */
	CH224();
	/**
	 * @brief Constructor with a specific I2C address. Uses the default Wire interface.
	 * @param address The 7-bit I2C address of the CH224 chip.
	 */
	CH224(uint8_t address);
	/**
	 * @brief Constructor with a specific I2C address and TwoWire interface.
	 * @param address The 7-bit I2C address of the CH224 chip.
	 * @param wire A reference to the TwoWire interface to use (e.g., Wire, Wire1).
	 */
	CH224(uint8_t address, TwoWire &wire);

	/**
	 * @brief Initializes the I2C communication. Should be called in setup().
	 */
	void begin();

	/**
	 * @brief Reads the status register from the chip.
	 * @return A ch224_status_t struct with the current status flags.
	 */
	ch224_status_t get_status();

	/**
	 * @brief Reads the source capabilities from the power adapter and parses them.
	 * This must be called before checking for available voltages or requesting power.
	 * @return true if successful and a PD contract is active, false otherwise.
	 */
	bool update_power_data();

	/**
	 * @brief Checks if the connected source supports USB Power Delivery.
	 * @return true if PD is active, false otherwise.
	 */
	bool is_pd_supported();

	/**
	 * @brief Checks if the connected source offers any Programmable Power Supply (PPS) capabilities.
	 * Must call update_power_data() first.
	 * @return true if PPS is available, false otherwise.
	 */
	bool has_pps();

	/**
	 * @brief Checks if the connected source offers any Adjustable Voltage Supply (AVS) capabilities.
	 * Must call update_power_data() first.
	 * @return true if AVS is available, false otherwise.
	 */
	bool has_avs();

	/**
	 * @brief Checks if the connected source supports Extended Power Range (EPR).
	 * @return true if EPR is available, false otherwise.
	 */
	bool has_epr();

	/**
	 * @brief Requests a specific fixed voltage from the source.
	 * @param vol The desired voltage in volts (e.g., 5, 9, 12, 15, 20, 28).
	 * @return true on success, false if the voltage is not supported or the request fails.
	 */
	bool request_fixed_voltage(uint8_t vol);

	/**
	 * @brief Requests a specific voltage in PPS mode.
	 * @param vol The desired voltage in volts. The value will be clamped to the supported PPS range.
	 * @return true on success, false if PPS is not supported or the request fails.
	 */
	bool request_pps_voltage(float vol);

	/**
	 * @brief Requests a specific voltage in AVS mode.
	 * @param vol The desired voltage in volts. The value will be clamped to the supported AVS range.
	 * @return true on success, false if AVS is not supported or the request fails.
	 */
	bool request_avs_voltage(float vol);

	/**
	 * @brief Requests the best possible voltage match for the given target.
	 * It prioritizes exact fixed voltages, then PPS/AVS ranges. If no exact match is found,
	 * it requests the voltage closest to the target from all available options.
	 * @param voltage_mv The desired voltage in millivolts.
	 * @return true if an exact match was found and set, false if a closest match was used instead.
	 */
	bool request_voltage_closest(uint16_t voltage_mv);

	/**
	 * @brief Gets the currently configured output voltage.
	 * Works for Fixed, PPS, and AVS modes.
	 * @return The current output voltage in millivolts (mV).
	 */
	uint16_t get_output_voltage_mv();

	/**
	 * @brief Gets the current being drawn from the source.
	 * @return The current in milliamps (mA).
	 */
	uint16_t get_current_ma();

	/**
	 * @brief Gets the number of valid Fixed Supply PDOs found.
	 * @return The count of valid fixed voltage options.
	 */
	uint8_t get_fixed_pdo_count() const;

	/**
	 * @brief Gets the number of valid PPS APDOs found.
	 * @return The count of valid PPS ranges.
	 */
	uint8_t get_pps_apdo_count() const;

	/**
	 * @brief Gets the number of valid AVS APDOs found.
	 * @return The count of valid AVS ranges.
	 */
	uint8_t get_avs_apdo_count() const;

	// Public members to inspect parsed power capabilities
	ch224_fixed_pdo_t supported_fixed_voltages[CH224_MAX_PDO_COUNT];
	ch224_pps_apdo_t supported_pps_ranges[CH224_MAX_PDO_COUNT];
	ch224_avs_apdo_t supported_avs_ranges[CH224_MAX_PDO_COUNT];


private:
	// I2C communication methods
	void send_byte(uint8_t reg, uint8_t data);
	uint8_t read_byte(uint8_t reg);
	void read_sequential(uint8_t reg, uint8_t* buffer, uint8_t len);

	// Raw configuration methods
	void set_voltage_mode(CH224VoltageMode voltage);
	void set_avs_voltage_mv(uint16_t voltage_mv);
	void set_pps_voltage_mv(uint16_t voltage_mv);

	// Helper to check if a requested voltage is within the advertised capabilities
	bool is_voltage_supported(uint8_t type, uint16_t req_mv);

	uint8_t _address;
	TwoWire *_wire;
	uint8_t _src_cap_buffer[CH224_SRC_CAP_SIZE];

	// Counters for valid PDOs
	uint8_t _fixed_pdo_count;
	uint8_t _pps_apdo_count;
	uint8_t _avs_apdo_count;
};
