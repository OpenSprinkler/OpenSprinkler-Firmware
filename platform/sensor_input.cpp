#include "platform/sensor_input.h"
#include "platform/gpio.h"
#include "boards/board_profile.h"
#include <cmath>

namespace {
uint8_t initialized_adc = 0;
}

void sensor_input_configure(uint8_t input, bool high_impedance) {
	const auto &board = osboard::active();
	if (input >= board.sensor_count) return;
	const uint8_t pin = board.pins.sensors[input];
	const uint8_t pullup = board.pins.sensor_pullups[input];
	initialized_adc &= ~(1U << input);
#if defined(ARDUINO)
	if (pullup != osboard::UNUSED_PIN) {
		pinModeExt(pullup, high_impedance ? INPUT : OUTPUT);
		if (!high_impedance) digitalWriteExt(pullup, HIGH);
		pinModeExt(pin, INPUT);
	} else {
		pinModeExt(pin, high_impedance ? INPUT : INPUT_PULLUP);
	}
#else
	(void)pullup;
	pinMode(pin, high_impedance ? INPUT : INPUT_PULLUP);
#endif
}

float sensor_input_voltage(uint8_t input) {
#if defined(ESP32)
	const auto &board = osboard::active();
	if (input >= board.sensor_count || !(board.analog_sensor_mask & (1U << input))) return NAN;
	const uint8_t pin = board.pins.sensors[input];
	if (!(initialized_adc & (1U << input))) {
		// Arduino owns ADC1. Its millivolt calibration is per unit and uses the
		// global default (ADC_11db), not the per-pin setting. Keep that default;
		// current sensing must remain raw analogRead() at its own 0 dB setting.
		(void)analogRead(pin);
		analogSetPinAttenuation(pin, ADC_11db);
		delayMicroseconds(100);
		(void)analogReadMilliVolts(pin);
		initialized_adc |= 1U << input;
	}
	uint32_t samples[5];
	for (uint8_t i = 0; i < 5; i++) {
		samples[i] = analogReadMilliVolts(pin);
		for (uint8_t j = i; j > 0 && samples[j] < samples[j - 1]; j--) {
			const uint32_t v = samples[j];
			samples[j] = samples[j - 1];
			samples[j - 1] = v;
		}
	}
	// Arduino reports zero for some ADC errors too; zero is a valid signal.
	return samples[2] / 1000.0f;
#else
	(void)input;
	return NAN;
#endif
}
