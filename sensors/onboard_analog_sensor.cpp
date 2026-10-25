#include "sensors/onboard_analog_sensor.h"
#include "services/sensor_ports.h"
#include "platform/sensor_input.h"
#include <cmath>

OnboardAnalogSensor::OnboardAnalogSensor(uint32_t interval, float min, float max,
	const char *name, SensorUnit unit, uint8_t flag, uint8_t input, float scale,
	float offset, ADS1115Subtype subtype, uint8_t num_points,
	const sensor_adjustment_point_t *points) :
	AnalogSensor(interval, min, max, name, unit, flag, scale, offset, subtype, num_points, points),
	input(input) {}

OnboardAnalogSensor::OnboardAnalogSensor(char *buf, uint32_t len) {
	uint8_t extra_len = 0;
	uint32_t i = Sensor::_deserialize(buf, len, &extra_len);
	const uint32_t end = i + extra_len;
	input = i < end ? static_cast<uint8_t>(buf[i]) : 255;
	deserialize_analog(buf, i + 1, end);
	// Unlike old ADS1115 records, this type has no pre-subtype legacy format.
	if (extra_len < 10 || !ads1115_subtype_is_supported(static_cast<ADS1115Subtype>(buf[i + 9]))) input = 255;
}

float OnboardAnalogSensor::_get_raw_value() {
	if (!sensor_port_analog_capable(input) || !sensor_port_is_analog(input)) return NAN;
	return convert_voltage(sensor_input_voltage(input));
}

void OnboardAnalogSensor::emit_extra_json(BufferFiller *bfill) {
	bfill->emit_p(PSTR("{\"input\":$D"), input);
	emit_analog_json(bfill);
	bfill->emit_p(PSTR("}"));
}

uint32_t OnboardAnalogSensor::_serialize_internal(char *buf) {
	buf[0] = input;
	return 1 + serialize_analog(buf + 1);
}

void OnboardAnalogSensor::emit_description_json(BufferFiller *bfill) {
	uint8_t mask = 0;
	for (uint8_t i = 0; i < 4; i++) if (sensor_port_analog_capable(i)) mask |= 1U << i;
	if (!mask) {
		bfill->emit_p(PSTR("{\"dis\":1}"));
		return;
	}
	bfill->emit_p(PSTR("{\"n\":\"Onboard Analog\",\"as\":[{\"n\":\"Input\",\"a\":\"input\",\"t\":\"enum\",\"d\":\"0\",\"o\":["));
	bool comma = false;
	for (uint8_t i = 0; i < 4; i++) {
		if (!(mask & (1U << i))) continue;
		if (comma) bfill->emit_p(PSTR(","));
		comma = true;
		bfill->emit_p(PSTR("{\"id\":$D,\"l\":\"SN$D\",\"dfl\":{\"unit\":$D,\"min\":\"0\",\"max\":\"3.3\"}}"),
			i, i + 1, static_cast<uint8_t>(SensorUnit::Volt));
	}
	bfill->emit_p(PSTR("]},"));
	emit_analog_description(bfill);
	bfill->emit_p(PSTR("]}"));
}
