#pragma once
#include "Arduino.h"
#include <deque>
#include <vector>
#include <utility>

constexpr int MSBFIRST = 1, SPI_MODE0 = 0;
struct SPISettings {
    uint32_t hz;
    SPISettings(uint32_t h, int, int): hz(h) {}
};
class SPIClass {
public:
    uint8_t regs[128] = {};
    std::deque<uint8_t> rx;
    std::vector<std::pair<uint8_t, uint8_t>> writes;
    bool absent = false, stuck_reset = false;
    uint32_t clock = 20000000;
    uint8_t command = 0;
    bool first = true;
    SPIClass() { regs[0x29] = 0x1c; regs[0x2a] = 0x51; regs[0x2b] = 0x91; }
    void begin() {}
    void beginTransaction(SPISettings s) { clock = s.hz; first = true; }
    void endTransaction() {} // Matches ESP8266: frequency is not restored.
    uint8_t transfer(uint8_t v) {
        if (first) { command = v; first = false; return 0; }
        uint8_t a = command & 127;
        if (command & 128) {
            writes.emplace_back(a, v);
            if (absent) return 0xff;
            regs[a] = v;
            if (a == 0 && v == 1 && !stuck_reset) regs[a] = 0;
            if (a == 0x55 && (v & 1)) { rx.clear(); regs[0x70] = 0; }
            if (a == 0x0b) {
                regs[a] = 0;
                uint8_t phy = regs[0x0c] & 31;
                if (v == 0x0c) {
                    uint16_t word = phy == 2 ? 0x7371 : phy == 3 ? 0x9010 : 0;
                    regs[0x0d] = word & 255; regs[0x0e] = word >> 8;
                }
            }
            return 0;
        }
        if (absent) return 0xff;
        if (a == 0x72) {
            if (rx.empty()) return 0;
            uint8_t result = rx.front(); rx.pop_front();
            if (rx.empty()) regs[0x70] = 0;
            return result;
        }
        return regs[a];
    }
    void frame(uint16_t length, uint8_t status = 0, uint8_t flag = 1) {
        rx = {flag, status, uint8_t(length), uint8_t(length >> 8)};
        for (unsigned i = 0; i < length && i < 1523; ++i) rx.push_back(uint8_t(i));
        regs[0x70] = 1;
    }
};
extern SPIClass SPI;
