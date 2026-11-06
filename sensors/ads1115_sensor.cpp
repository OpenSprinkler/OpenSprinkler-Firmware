#include "ads1115_sensor.h"
#include "OpenSprinkler.h"
#include <cmath>

ADS1115Sensor::ADS1115Sensor(uint32_t interval, float min, float max, const char *name,
	SensorUnit unit, uint8_t flag, ADS1115 **sensors, uint8_t sensor_index, uint8_t pin,
	float scale, float offset, ADS1115Subtype subtype, uint8_t num_points,
	const sensor_adjustment_point_t *points) :
	AnalogSensor(interval, min, max, name, unit, flag, scale, offset, subtype, num_points, points),
	sensor_index(sensor_index), pin(pin), sensors(sensors) {}

void ADS1115Sensor::emit_extra_json(BufferFiller *bfill) {
	bfill->emit_p(PSTR("{\"pin\":$D"), ((sensor_index << 2) + pin + 1));
	emit_analog_json(bfill);
	bfill->emit_p(PSTR("}"));
}

void ADS1115Sensor::emit_description_json(BufferFiller* bfill) {
	// Opening + (hardware-detected flag on real targets only) + pin + subtype enum start.
	// "hwd" is a generic capability bit consumed by the UI: when present and
	// equal to 0, the UI shows a "required hardware not detected" message.
	// On DEMO/SIM the mock backend is always present, so the field is
	// omitted entirely — UI default is "available."
#if defined(ARDUINO) || defined(OSPI)
	bfill->emit_p(PSTR(
		"{\"n\":\"ADS1115 Sensor\","
		"\"hwd\":$D,"
		"\"as\":["
		),
		OpenSprinkler::has_ads1115() ? 1 : 0);
#else
	bfill->emit_p(PSTR(
		"{\"n\":\"ADS1115 Sensor (simulated)\","
		"\"as\":["
		));
#endif
	bfill->emit_p(PSTR(
			"{\"n\":\"Pin\","
			 "\"a\":\"pin\","
			 "\"t\":\"int::[1,16]\","
			 "\"d\":\"1\","
			 "\"h\":\"Analog pin number (1-16)\"},"
	));
	emit_analog_description(bfill);
	bfill->emit_p(PSTR("]}"));
}

float ADS1115Sensor::_get_raw_value() {
	if (sensor_index >= 4 || pin >= 4 || !sensors || !sensors[sensor_index]) return NAN;
	int16_t counts = sensors[sensor_index]->get_pin_value(pin);
	return counts < 0 ? NAN : convert_voltage(counts * ADS1115_VOLTS_PER_COUNT);
}

uint32_t ADS1115Sensor::_serialize_internal(char *buf) {
	buf[0] = sensor_index;
	buf[1] = pin;
	return 2 + serialize_analog(buf + 2);
}

ADS1115Sensor::ADS1115Sensor(ADS1115 **sensors, char *buf, uint32_t len) : sensors(sensors) {
	uint8_t subclass_len = 0;
	uint32_t i = Sensor::_deserialize(buf, len, &subclass_len);
	uint32_t end = i + subclass_len;
	sensor_index = i < end ? static_cast<uint8_t>(buf[i]) : 0;
	i++;
	pin = i < end ? static_cast<uint8_t>(buf[i]) : 0;
	i++;
	deserialize_analog(buf, i, end);
}
