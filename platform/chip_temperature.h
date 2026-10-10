#pragma once

constexpr int CHIP_TEMPERATURE_MIN_C = -10;
constexpr int CHIP_TEMPERATURE_MAX_C = 80;

// Chip, not ambient, temperature. Returns NaN when unavailable or on failure.
// Called from the controller loop; ESP32 instances share one lazy driver handle.
float chip_temperature_celsius();
