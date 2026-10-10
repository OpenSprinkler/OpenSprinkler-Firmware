#pragma once

#include "analog_sensor.h"
#include "drivers/ads1115.h"

#define ADS1115_VOLTS_PER_COUNT (ADS1115_SCALE_FACTOR / 1000.0f)

class ADS1115Sensor : public AnalogSensor {
	public:
	ADS1115Sensor(uint32_t interval, float min, float max, const char *name, SensorUnit unit, uint8_t flag,
	              ADS1115 **sensors, uint8_t sensor_index, uint8_t pin,
	              float scale, float offset,
	              ADS1115Subtype subtype, uint8_t num_points, const sensor_adjustment_point_t *points);
	ADS1115Sensor(ADS1115 **sensors, char *buf, uint32_t len);

	void emit_extra_json(BufferFiller *bfill);
	static void emit_description_json(BufferFiller *bfill);

	SensorType get_sensor_type() {
		return SensorType::ADS1115;
	}

	uint8_t sensor_index;
	uint8_t pin;
	private:
	float _get_raw_value();
	uint32_t _serialize_internal(char *buf);

	ADS1115 **sensors;
};
