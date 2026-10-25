#pragma once

#include "sensors/analog_sensor.h"

class OnboardAnalogSensor : public AnalogSensor {
public:
	OnboardAnalogSensor(uint32_t interval, float min, float max, const char *name,
		SensorUnit unit, uint8_t flag, uint8_t input, float scale, float offset,
		ADS1115Subtype subtype, uint8_t num_points, const sensor_adjustment_point_t *points);
	OnboardAnalogSensor(char *buf, uint32_t len);
	SensorType get_sensor_type() override { return SensorType::OnboardAnalog; }
	void emit_extra_json(BufferFiller *bfill) override;
	static void emit_description_json(BufferFiller *bfill);
	uint8_t input;
private:
	float _get_raw_value() override;
	uint32_t _serialize_internal(char *buf) override;
};
