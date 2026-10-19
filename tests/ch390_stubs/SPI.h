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
    uint8_t memory[0x4000] = {};
    std::deque<uint8_t> write_high_samples;
    unsigned memory_reads = 0;
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
            if (a == 0x55 && (v & 1)) {
                rx.clear(); setRead(0x0c00); setWrite(0x0c00);
            }
            if (a == 0x74 || a == 0x75) regs[0x70] = memory[readPointer() & 0x3fff];
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
        if (a == 0x25 && !write_high_samples.empty()) {
            uint8_t sample = write_high_samples.front(); write_high_samples.pop_front();
            return sample;
        }
        if (a == 0x72) {
            ++memory_reads;
            uint16_t p = readPointer();
            uint8_t result = memory[p & 0x3fff];
            if (!rx.empty()) rx.pop_front();
            setRead(advance(p, 1));
            return result;
        }
        return regs[a];
    }
    static uint16_t advance(uint16_t p, unsigned n) {
        return 0x0c00 + ((p - 0x0c00 + n) % 0x3400);
    }
    uint16_t readPointer() const { return (regs[0x75] << 8) | regs[0x74]; }
    void setRead(uint16_t p) { regs[0x74] = p; regs[0x75] = p >> 8; regs[0x70] = memory[p & 0x3fff]; }
    void setWrite(uint16_t p) { regs[0x24] = p; regs[0x25] = p >> 8; }
    void frame(uint16_t length, uint8_t status = 0, uint8_t flag = 1, uint16_t start = 0x0c00) {
        rx = {flag, status, uint8_t(length), uint8_t(length >> 8)};
        for (unsigned i = 0; i < length && i < 1523; ++i) rx.push_back(uint8_t(i));
        uint16_t p = start;
        for (uint8_t b : rx) { memory[p] = b; p = advance(p, 1); }
        memory[p] = 0;
        setRead(start); setWrite(p);
        regs[0x70] = 1;
    }
};
extern SPIClass SPI;
