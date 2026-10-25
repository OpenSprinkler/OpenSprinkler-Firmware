#pragma once

#include "sensor.h"

#define ADS1115_DEFAULT_SCALE 1
#define ADS1115_DEFAULT_OFFSET 0
#define ADS1115_PIECEWISE_POINTS SENSOR_ADJUSTMENT_POINTS

// Subtype selects how raw voltage is converted to a physical value.
// Output is then trimmed via the universal scale/offset:
//     y_final = scale * transform(subtype, voltage) + offset
//
// Numbering convention: 0-9 reserved for general / vendor-agnostic transforms;
// 10+ for vendor-specific baked curves. Stored as one byte; gaps are free.
enum class ADS1115Subtype : uint8_t {
	LINEAR           = 0,    // transform(v) = v          → y = scale*v + offset
	PIECEWISE_LINEAR = 1,    // transform(v) = piecewise interpolation across user-supplied points
	// (2-9 reserved for future general transforms)

	// 10+: vendor-specific, baked in firmware. Numbers reserved; v2 work.
	SMT50_TEMPERATURE     = 10,
	SMT50_MOISTURE        = 11,
	SMT100_TEMPERATURE    = 12,
	SMT100_MOISTURE       = 13,
	SMT100_PERMITTIVITY   = 14,
	VH400_MOISTURE        = 15,
	THERM200_TEMPERATURE  = 16,
	AQUAPLUMB_LEVEL       = 17,

	MAX_VALUE,
};

// Returns true only for subtype values whose transform is actually implemented in
// this build. Reserved / not-yet-implemented values (the gap at 2..9 and any
// vendor entries past v1) return false, so the API rejects them at the boundary
// instead of silently storing a value that produces NAN at runtime.
// Returns the unit the subtype's transform natively produces. Used to convert
// from the formula's native unit to the user's chosen unit (e.g., Celsius -> Fahrenheit).
// Returns SensorUnit::None for subtypes whose output unit is user-defined
// (LINEAR, PIECEWISE_LINEAR) — those skip conversion entirely.
SensorUnit ads1115_subtype_native_unit(ADS1115Subtype subtype);

inline bool ads1115_subtype_is_supported(ADS1115Subtype subtype) {
	switch (subtype) {
		case ADS1115Subtype::LINEAR:
		case ADS1115Subtype::PIECEWISE_LINEAR:
		case ADS1115Subtype::SMT50_TEMPERATURE:
		case ADS1115Subtype::SMT50_MOISTURE:
		case ADS1115Subtype::SMT100_TEMPERATURE:
		case ADS1115Subtype::SMT100_MOISTURE:
		case ADS1115Subtype::SMT100_PERMITTIVITY:
		case ADS1115Subtype::VH400_MOISTURE:
		case ADS1115Subtype::THERM200_TEMPERATURE:
		case ADS1115Subtype::AQUAPLUMB_LEVEL:
			return true;
		default:
			return false;
	}
}


// Common voltage conversion and metadata; acquisition and input identity stay in each driver.
class AnalogSensor : public Sensor {
public:
	AnalogSensor(uint32_t interval, float min, float max, const char *name, SensorUnit unit, uint8_t flag,
		float scale, float offset, ADS1115Subtype subtype, uint8_t num_points,
		const sensor_adjustment_point_t *points);
	AnalogSensor() = default;
	static void emit_analog_description(BufferFiller *bfill);
	void emit_analog_json(BufferFiller *bfill);
	float scale;
	float offset;
	ADS1115Subtype subtype;
	uint8_t num_points;
	sensor_adjustment_point_t points[ADS1115_PIECEWISE_POINTS];
protected:
	float convert_voltage(float voltage);
	uint32_t serialize_analog(char *buf);
	void deserialize_analog(char *buf, uint32_t i, uint32_t end);
};
