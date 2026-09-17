#include "onboard_digital_sensor.h"
#include "../OpenSprinkler.h"

#include <cmath>

extern OpenSprinkler os;

OnboardDigitalSensor::OnboardDigitalSensor(uint32_t interval, float min, float max, const char* name, SensorUnit unit, uint8_t flag,
                                            OnboardInput input) :
	Sensor(interval, min, max, name, unit, flag),
	input(input) {
}

void OnboardDigitalSensor::emit_extra_json(BufferFiller *bfill) {
	bfill->emit_p(PSTR("{\"input\":$D}"), static_cast<uint8_t>(this->input));
}

void OnboardDigitalSensor::emit_description_json(BufferFiller* bfill) {
	// SN1 and SN2 are always present on every hardware variant.
	bfill->emit_p(PSTR(
		"{\"n\":\"Onboard Digital\","
		"\"as\":["
			"{\"n\":\"Input\","
			 "\"a\":\"input\","
			 "\"t\":\"enum\","
			 "\"d\":\"0\","
			 "\"o\":["
				"{\"id\":0,\"l\":\"SN1\","
				 "\"dfl\":{\"unit\":$D,\"max\":\"1\"},"
				 "\"lk\":[\"unit\"]},"
				"{\"id\":1,\"l\":\"SN2\","
				 "\"dfl\":{\"unit\":$D,\"max\":\"1\"},"
				 "\"lk\":[\"unit\"]}"
		),
		static_cast<uint8_t>(SensorUnit::None),
		static_cast<uint8_t>(SensorUnit::None));

	// Advertise SN3/SN4 from the selected board profile so OS3.4 and OS4 expose
	// them while earlier hardware continues to omit non-functional inputs.
	if (sensor_available(2)) {
		bfill->emit_p(PSTR(
				",{\"id\":2,\"l\":\"SN3\","
				 "\"dfl\":{\"unit\":$D,\"max\":\"1\"},"
				 "\"lk\":[\"unit\"]},"
				"{\"id\":3,\"l\":\"SN4\","
				 "\"dfl\":{\"unit\":$D,\"max\":\"1\"},"
				 "\"lk\":[\"unit\"]}"
			),
			static_cast<uint8_t>(SensorUnit::None),
			static_cast<uint8_t>(SensorUnit::None));
	}

	bfill->emit_p(PSTR(
			 "]}"
		"]}"
	));
}


float OnboardDigitalSensor::_get_raw_value() {
	switch (this->input) {
		case OnboardInput::SN1:
			return os.sn_sensors[0].active ? 1.0f : 0.0f;
		case OnboardInput::SN2:
			return os.sn_sensors[1].active ? 1.0f : 0.0f;
		case OnboardInput::SN3:
			return os.sn_sensors[2].active ? 1.0f : 0.0f;
		case OnboardInput::SN4:
			return os.sn_sensors[3].active ? 1.0f : 0.0f;
		default:
			return NAN;
	}
}

uint32_t OnboardDigitalSensor::_serialize_internal(char *buf) {
	uint32_t i = 0;
	buf[i++] = static_cast<uint8_t>(this->input);
	return i;
}

OnboardDigitalSensor::OnboardDigitalSensor(char *buf, uint32_t len) {
	uint8_t subclass_len = 0;
	uint32_t i = Sensor::_deserialize(buf, len, &subclass_len);
	uint32_t end = i + subclass_len;

	this->input = OnboardInput::MAX_VALUE;   // sentinel: "unknown"
	if (i + 1 <= end) {
		uint8_t v = static_cast<uint8_t>(buf[i]);
		if (v < static_cast<uint8_t>(OnboardInput::MAX_VALUE)) {
			this->input = static_cast<OnboardInput>(v);
		}
	}
}
