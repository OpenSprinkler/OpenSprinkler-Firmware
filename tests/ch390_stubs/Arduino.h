#pragma once
#include <stdint.h>
#include <cstring>

constexpr int SS = 16, OUTPUT = 1, HIGH = 1, LOW = 0;
extern uint32_t fake_us;
extern void (*delay_hook)();
inline uint32_t millis() { return fake_us / 1000; }
inline uint32_t micros() { return fake_us; }
inline void delay(unsigned ms) { fake_us += ms * 1000; if (delay_hook) delay_hook(); }
inline void delayMicroseconds(unsigned us) { fake_us += us; }
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
#define F(x) x
