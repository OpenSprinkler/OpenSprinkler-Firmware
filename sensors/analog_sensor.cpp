#include "analog_sensor.h"
#include <cmath>
#include <cstring>

AnalogSensor::AnalogSensor(uint32_t interval, float min, float max, const char *name,
	SensorUnit unit, uint8_t flag, float scale, float offset, ADS1115Subtype subtype,
	uint8_t num_points, const sensor_adjustment_point_t *points) :
	Sensor(interval, min, max, name, unit, flag), scale(scale), offset(offset),
	subtype(subtype), num_points(num_points) {
	if (this->num_points > ADS1115_PIECEWISE_POINTS) this->num_points = ADS1115_PIECEWISE_POINTS;
	memset(this->points, 0, sizeof(this->points));
	if (points && this->num_points) {
		for (uint8_t i = 0; i < this->num_points; i++) {
			this->points[i] = points[i];
		}
	}
}

void AnalogSensor::emit_analog_json(BufferFiller *bfill) {
	bfill->emit_p(PSTR(",\"scale\":$E,\"offset\":$E,\"subtype\":$D"),
		scale, offset, static_cast<uint8_t>(subtype));
	if (this->subtype == ADS1115Subtype::PIECEWISE_LINEAR) {
		bfill->emit_p(PSTR(",\"points\":["));
		for (uint8_t p = 0; p < this->num_points; p++) {
			if (p) bfill->emit_p(PSTR(","));
			bfill->emit_p(PSTR("{\"x\":$E,\"y\":$E}"), this->points[p].x, this->points[p].y);
		}
		bfill->emit_p(PSTR("]"));
	}
}

void AnalogSensor::emit_analog_description(BufferFiller *bfill) {
	bfill->emit_p(PSTR(
			"{\"n\":\"Subtype\","
			 "\"a\":\"subtype\","
			 "\"t\":\"enum\","
			 "\"d\":\"0\","
			 "\"o\":["
	));

	// LINEAR — uses base defaults entirely; just hide the points editor.
	bfill->emit_p(PSTR("{\"id\":0,\"l\":\"Linear\",\"hd\":[\"points\"]}"));

	// PIECEWISE_LINEAR — points define the entire transform; scale/offset are
	// redundant (any post-trim can be baked into the y values), so hide them.
	bfill->emit_p(PSTR(",{\"id\":1,\"l\":\"Piecewise Linear\",\"hd\":[\"scale\",\"offset\"]}"));

	// Baked subtypes — each emits its own option with unit/min/max defaults and unit lock.
	// Unit IDs are substituted at runtime since SensorUnit values aren't preprocessor-stringifiable.
	bfill->emit_p(PSTR(",{\"id\":10,\"l\":\"SMT50 Temperature\","
	                    "\"hd\":[\"points\"],"
	                    "\"dfl\":{\"unit\":$D,\"min\":\"-40\",\"max\":\"185\"},"
	                    "\"ug\":$D}"),
	              static_cast<uint8_t>(SensorUnit::Celsius),
	              static_cast<uint8_t>(SensorUnitGroup::Temperature));

	bfill->emit_p(PSTR(",{\"id\":11,\"l\":\"SMT50 Moisture\","
	                    "\"hd\":[\"points\"],"
	                    "\"dfl\":{\"unit\":$D,\"max\":\"50\"},"
	                    "\"lk\":[\"unit\"]}"),
	              static_cast<uint8_t>(SensorUnit::VolumetricMoistureContent));

	bfill->emit_p(PSTR(",{\"id\":12,\"l\":\"SMT100 (0-3V) Temperature\","
	                    "\"hd\":[\"points\"],"
	                    "\"dfl\":{\"unit\":$D,\"min\":\"-40\",\"max\":\"185\"},"
	                    "\"ug\":$D}"),
	              static_cast<uint8_t>(SensorUnit::Celsius),
	              static_cast<uint8_t>(SensorUnitGroup::Temperature));

	bfill->emit_p(PSTR(",{\"id\":13,\"l\":\"SMT100 (0-3V) Moisture\","
	                    "\"hd\":[\"points\"],"
	                    "\"dfl\":{\"unit\":$D,\"max\":\"100\"},"
	                    "\"lk\":[\"unit\"]}"),
	              static_cast<uint8_t>(SensorUnit::VolumetricMoistureContent));

	bfill->emit_p(PSTR(",{\"id\":14,\"l\":\"SMT100 (0-3V) Permittivity\","
	                    "\"hd\":[\"points\"],"
	                    "\"dfl\":{\"unit\":$D,\"min\":\"1\",\"max\":\"80\"},"
	                    "\"lk\":[\"unit\"]}"),
	              static_cast<uint8_t>(SensorUnit::DielectricConstant));

	bfill->emit_p(PSTR(",{\"id\":15,\"l\":\"VH400 Soil Moisture\","
	                    "\"hd\":[\"points\"],"
	                    "\"dfl\":{\"unit\":$D,\"max\":\"100\"},"
	                    "\"lk\":[\"unit\"]}"),
	              static_cast<uint8_t>(SensorUnit::VolumetricMoistureContent));

	bfill->emit_p(PSTR(",{\"id\":16,\"l\":\"THERM200 Temperature\","
	                    "\"hd\":[\"points\"],"
	                    "\"dfl\":{\"unit\":$D,\"min\":\"-40\",\"max\":\"185\"},"
	                    "\"ug\":$D}"),
	              static_cast<uint8_t>(SensorUnit::Celsius),
	              static_cast<uint8_t>(SensorUnitGroup::Temperature));

	bfill->emit_p(PSTR(",{\"id\":17,\"l\":\"AquaPlumb Water Level\","
	                    "\"hd\":[\"points\"],"
	                    "\"dfl\":{\"unit\":$D,\"max\":\"100\"},"
	                    "\"lk\":[\"unit\"]}"),
	              static_cast<uint8_t>(SensorUnit::Percent));

	// Closing of options array + remaining args
	bfill->emit_p(PSTR(
			 "]},"
			"{\"n\":\"Scale\","
			 "\"a\":\"scale\","
			 "\"t\":\"float\","
			 "\"d\":\"" SENSOR_DEFAULT_STR(ADS1115_DEFAULT_SCALE) "\","
			 "\"h\":\"Sensor Value = Scale * Voltage + Offset\"},"
			"{\"n\":\"Offset\","
			 "\"a\":\"offset\","
			 "\"t\":\"float\","
			 "\"d\":\"" SENSOR_DEFAULT_STR(ADS1115_DEFAULT_OFFSET) "\","
			 "\"h\":\"Applied after Scale\"},"
			"{\"n\":\"Points\","
			 "\"a\":\"points\","
			 "\"t\":\"points::[0,8]\","
			 "\"h\":\"Up to 8 (voltage, value) pairs, sorted by voltage\"}"
		""
	));
}

SensorUnit ads1115_subtype_native_unit(ADS1115Subtype subtype) {
	switch (subtype) {
		case ADS1115Subtype::SMT50_TEMPERATURE:
		case ADS1115Subtype::SMT100_TEMPERATURE:
		case ADS1115Subtype::THERM200_TEMPERATURE:
			return SensorUnit::Celsius;
		case ADS1115Subtype::SMT50_MOISTURE:
		case ADS1115Subtype::SMT100_MOISTURE:
		case ADS1115Subtype::VH400_MOISTURE:
			return SensorUnit::VolumetricMoistureContent;
		case ADS1115Subtype::SMT100_PERMITTIVITY:
			return SensorUnit::DielectricConstant;
		case ADS1115Subtype::AQUAPLUMB_LEVEL:
			return SensorUnit::Percent;
		default:
			return SensorUnit::None;   // LINEAR, PIECEWISE_LINEAR: user-defined unit
	}
}

// Voltage -> physical-value transform, dispatched by subtype.
// scale/offset trim is applied by the caller (_get_raw_value).
// Output is clamped at the Sensor base level via min/max, so out-of-range inputs
// (e.g. above the sensor's spec'd voltage) get bounded into a sensible range.
static float analog_transform(ADS1115Subtype subtype, float voltage,
                                const sensor_adjustment_point_t *points, uint8_t num_points) {
	switch (subtype) {
		case ADS1115Subtype::LINEAR:
			return voltage;

		case ADS1115Subtype::PIECEWISE_LINEAR:
			if (num_points < 2) return NAN;
			return sensor_piecewise_interp(voltage, points, num_points);

		// Truebner SMT50 (output: 0-3V)
		case ADS1115Subtype::SMT50_TEMPERATURE:
			// T [°C] = (V - 0.5) * 100
			return (voltage - 0.5f) * 100.0f;
		case ADS1115Subtype::SMT50_MOISTURE:
			// VWC [%] = V * 50 / 3
			return voltage * 50.0f / 3.0f;

		// Truebner SMT100, 3V variant (common in irrigation; 1V variant uses different scaling)
		case ADS1115Subtype::SMT100_TEMPERATURE:
			// T [°C] = V * 100 / 3 - 40
			return voltage * 100.0f / 3.0f - 40.0f;
		case ADS1115Subtype::SMT100_MOISTURE:
			// VWC [%] = V * 100 / 3
			return voltage * 100.0f / 3.0f;
		case ADS1115Subtype::SMT100_PERMITTIVITY:
			// ε ≈ 3.03 + 3.1·U + 16.222·U² − 2.841·U³  (Horner form below)
			return 3.03f + voltage * (3.1f + voltage * (16.222f - 2.841f * voltage));

		// Vegetronix VH400 (soil moisture, piecewise linear baked from datasheet)
		case ADS1115Subtype::VH400_MOISTURE:
			if (voltage <= 1.10f)         return 10.00f * voltage -  1.00f;
			else if (voltage <  1.30f)    return 25.00f * voltage - 17.50f;
			else if (voltage <  1.82f)    return 48.08f * voltage - 47.50f;
			else if (voltage <  2.20f)    return 26.32f * voltage -  7.89f;
			else                          return 62.50f * voltage - 87.50f;

		// Vegetronix THERM200 (temperature)
		case ADS1115Subtype::THERM200_TEMPERATURE:
			// T [°C] = V * 41.67 - 40
			return voltage * 41.67f - 40.0f;

		// Vegetronix AquaPlumb (water level, output: 0-3V → 0-100%)
		case ADS1115Subtype::AQUAPLUMB_LEVEL:
			return voltage * 100.0f / 3.0f;

		default:
			return NAN;
	}
}


float AnalogSensor::convert_voltage(float voltage) {
	if (!std::isfinite(voltage)) return NAN;
	float transformed = analog_transform(this->subtype, voltage, this->points, this->num_points);
	if (isnan(transformed)) return NAN;

	// Convert from the subtype's native unit to the user's chosen unit (e.g.
	// Celsius -> Fahrenheit). No-op when native is None or matches user's unit.
	SensorUnit native = ads1115_subtype_native_unit(this->subtype);
	if (native != SensorUnit::None) {
		transformed = convert_unit(transformed, native, this->unit);
	}

	// PIECEWISE_LINEAR users define the entire output via points; post-trim
	// scale/offset would be redundant, so skip them. Stored scale/offset are
	// kept in the record (format unchanged) but ignored at runtime.
	if (this->subtype == ADS1115Subtype::PIECEWISE_LINEAR) {
		return transformed;
	}

	return this->scale * transformed + this->offset;
}
uint32_t AnalogSensor::serialize_analog(char *buf) {
	uint32_t i = 0;
	i += write_buf(buf + i, this->scale);
	i += write_buf(buf + i, this->offset);
	buf[i++] = static_cast<uint8_t>(this->subtype);
	if (this->subtype == ADS1115Subtype::PIECEWISE_LINEAR) {
		buf[i++] = this->num_points;
		for (uint8_t p = 0; p < this->num_points; p++) {
			i += write_buf(buf + i, this->points[p].x);
			i += write_buf(buf + i, this->points[p].y);
		}
	}
	return i;
}

void AnalogSensor::deserialize_analog(char *buf, uint32_t i, uint32_t end) {
	this->scale = ADS1115_DEFAULT_SCALE;
	this->offset = ADS1115_DEFAULT_OFFSET;
	this->subtype = ADS1115Subtype::LINEAR;
	this->num_points = 0;
	memset(this->points, 0, sizeof(this->points));

	read_buf(buf, &i, end, this->scale);
	read_buf(buf, &i, end, this->offset);

	// subtype byte — absent in pre-subtype records; defaults to LINEAR.
	// Reserved-but-unimplemented values (e.g. firmware downgrade after a v2 record
	// was saved) also fall back to LINEAR, so the sensor produces a sensible reading
	// instead of NAN. The user can re-pick a subtype via the UI.
	if (i + 1 <= end) {
		uint8_t st = static_cast<uint8_t>(buf[i]);
		if (st < static_cast<uint8_t>(ADS1115Subtype::MAX_VALUE)) {
			ADS1115Subtype candidate = static_cast<ADS1115Subtype>(st);
			if (ads1115_subtype_is_supported(candidate)) {
				this->subtype = candidate;
			}
		}
	}
	i++;

	// Points payload — present only for PIECEWISE_LINEAR records.
	if (this->subtype == ADS1115Subtype::PIECEWISE_LINEAR && i + 1 <= end) {
		uint8_t np = static_cast<uint8_t>(buf[i++]);
		if (np > ADS1115_PIECEWISE_POINTS) np = ADS1115_PIECEWISE_POINTS;
		for (uint8_t p = 0; p < np; p++) {
			if (i + 8 > end) break;
			read_buf(buf, &i, end, this->points[p].x);
			read_buf(buf, &i, end, this->points[p].y);
			this->num_points++;
		}
	}

}
