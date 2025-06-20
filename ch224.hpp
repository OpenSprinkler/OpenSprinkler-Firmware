#pragma once

#include <Wire.h>
#include <stdint.h>

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

typedef struct {
   uint16_t voltage;
   uint16_t max_current;
} ch224_fixed_data_t;

typedef struct {
    uint16_t max_voltage;
    uint16_t min_voltage;
    uint16_t max_current;
} ch224_pps_data_t;

enum class CH224VoltageMode {
    Voltage_5V = 0,
    Voltage_9V = 1,
    Voltage_12V = 2,
    Voltage_15V = 3,
    Voltage_20V = 4,
    Voltage_28V = 5,
    Voltage_PPS = 6,
    Voltage_AVS = 7,
};

#define CH224_PDO_COUNT (0x30 / 4)

class CH224 {
public:
  // 0x23
  CH224();
  CH224(uint8_t address);
  CH224(uint8_t address, TwoWire &wire);

  // 0x09
  ch224_status_t get_status();
  // 0x0A
  void set_voltage_mode(CH224VoltageMode voltage);
  // 0x50 (50mA)
  uint8_t get_current();
  uint16_t get_current_ma();
  // high: 0x51, low: 0x52 (25mV)
  void set_avs_voltage(uint16_t voltage);
  void set_avs_voltage_mv(uint32_t voltage);
  // high: 0x53 (100mV)
  void set_pps_voltage(uint8_t voltage);
  void set_pps_voltage_mv(uint16_t voltage);
  // high: 0x60
  void update_power_data();
  ch224_fixed_data_t supported_voltages[CH224_PDO_COUNT];
  ch224_pps_data_t supported_pps_ranges[CH224_PDO_COUNT];

private:
  void send_byte(uint8_t address, uint8_t data);
  uint8_t read_byte(uint8_t address);
  uint8_t _address;
  TwoWire *_wire;
};