#include "drivers/ch390_raw.h"
#include <cassert>
#include <cstdio>

uint32_t fake_us = 0;
void (*delay_hook)() = nullptr;
SPIClass SPI;
class TestDriver : public CH390Raw {
public:
    using CH390Raw::readFrameSize;
    using CH390Raw::readFrameData;
    explicit TestDriver(SPIClass& spi): CH390Raw(16, spi) {}
};
static TestDriver* during_reset;
static void reentry_check() {
    uint8_t frame[60] = {};
    assert(during_reset->readFrameSize() == 0);
    assert(during_reset->sendFrame(frame, sizeof(frame)) == 0);
}

int main() {
    SPIClass bus;
    bus.absent = true;
    assert(!ch390_probe(bus, 16, 1000000));
    assert(bus.writes.empty() && fake_us == 0); // No reset or 250 ms wait on an empty bus.
    bus.absent = false;
    assert(ch390_probe(bus, 16, 1000000));
    assert(bus.writes.empty() && bus.clock == 1000000);

    TestDriver driver(bus);
    const uint8_t mac[] = {2, 3, 4, 5, 6, 7};
    assert(driver.begin(mac));
    for (uint16_t length : {0, 1, 3, 4, 1523, 65535}) {
        fake_us += 1000001;
        bus.frame(length, 0xff); // Bad status must not bypass length validation.
        assert(driver.readFrameSize() == 0);
        // Complete recovery if the injected incidents have escalated.
        driver.service();
        fake_us += 31000000;
        driver.service();
        bus.frame(64);
        assert(driver.readFrameSize() == 60);
        uint8_t data[60];
        assert(driver.readFrameData(data, 60) == 60);
        assert(driver.readFrameSize() == 0);
    }
    assert(driver.diagnostics().bad_header == 6);
    fake_us += 1000001;
    bus.frame(64, 0, 0xff);
    assert(driver.readFrameSize() == 0);
    assert(driver.diagnostics().bad_header == 7);
    bus.frame(64, 0x02);
    assert(driver.readFrameSize() == 0);
    assert(bus.rx.empty());
    assert(driver.diagnostics().rx_errors == 1);

    fake_us += 11000000;
    bus.frame(64);
    bus.regs[0x70] = 0xff;
    assert(driver.readFrameSize() == 0);
    assert(driver.diagnostics().bad_ready == 1);
    bus.frame(1522);
    assert(driver.readFrameSize() == 1518);
    uint8_t maximum_frame[1518];
    assert(driver.readFrameData(maximum_frame, sizeof(maximum_frame)) == sizeof(maximum_frame));

    // Pointer reset leaves a valid old frame in SRAM. Equal pointers forbid
    // consuming it, even when MRCMDX still advertises ready=1.
    bus.frame(64); bus.setWrite(bus.readPointer());
    auto reads = bus.memory_reads;
    auto empty = driver.diagnostics().empty_ready;
    assert(driver.readFrameSize() == 0 && bus.memory_reads == reads);
    assert(driver.diagnostics().empty_ready == empty + 1);
    bus.regs[0x70] = 0;
    assert(driver.readFrameSize() == 0);
    assert(driver.diagnostics().empty_ready == empty + 1);

    // Header and payload wrap; an incomplete snapshot restores the header.
    for (uint16_t start : {uint16_t(0x3ffe), uint16_t(0x3fe0)}) {
        bus.frame(64, 0, 1, start);
        bus.setWrite(SPIClass::advance(start, 2));
        reads = bus.memory_reads;
        assert(driver.readFrameSize() == 0 && bus.memory_reads == reads);
        bus.setWrite(SPIClass::advance(start, 10));
        assert(driver.readFrameSize() == 0 && bus.readPointer() == start);
        bus.setWrite(SPIClass::advance(start, 68));
        assert(driver.readFrameSize() == 60);
        uint8_t payload[60];
        assert(driver.readFrameData(payload, 60) == 60);
        for (unsigned i = 0; i < 60; ++i) assert(payload[i] == i);
        assert(driver.readFrameSize() == 0);
    }
    bus.frame(64, 0x02); bus.setWrite(0x0c08);
    auto errors = driver.diagnostics().rx_errors;
    assert(driver.readFrameSize() == 0 && bus.readPointer() == 0x0c00);
    assert(driver.diagnostics().rx_errors == errors); // Not discarded prematurely.
    bus.setWrite(0x0c44);
    assert(driver.readFrameSize() == 0 && driver.diagnostics().rx_errors == errors + 1);

    // Unstable hardware pointer snapshots defer without consuming or resetting.
    bus.frame(64);
    bus.write_high_samples = {0x0c, 0x0d, 0x0d, 0x0e, 0x0e, 0x0f};
    reads = bus.memory_reads;
    auto resets = driver.diagnostics().rx_resets;
    assert(driver.readFrameSize() == 0 && bus.memory_reads == reads);
    assert(driver.diagnostics().rx_resets == resets);

    // Persistent data-with-ready-zero, unlike a changing arrival, recovers.
    bus.regs[0x70] = 0;
    assert(driver.readFrameSize() == 0);
    fake_us += 1000000; assert(driver.readFrameSize() == 0);
    fake_us += 1000000; assert(driver.readFrameSize() == 0);
    assert(driver.diagnostics().rx_resets == resets + 1);
    reads = bus.memory_reads;
    assert(driver.readFrameSize() == 0 && bus.memory_reads == reads);

    fake_us += 11000000;
    bus.frame(64); bus.setWrite(0x0c08);
    resets = driver.diagnostics().rx_resets;
    assert(driver.readFrameSize() == 0 && bus.readPointer() == 0x0c00);
    fake_us += 1000000; assert(driver.readFrameSize() == 0);
    fake_us += 1000000; assert(driver.readFrameSize() == 0);
    assert(driver.diagnostics().rx_resets == resets + 1);
    assert(driver.diagnostics().rx_incomplete >= 3);

    fake_us += 11000000;
    bus.setRead(0x0100);
    reads = bus.memory_reads;
    assert(driver.readFrameSize() == 0 && bus.memory_reads == reads);
    assert(bus.readPointer() == 0x0c00);

    // Link down alone never requests recovery.
    auto count = driver.diagnostics().recoveries;
    bus.regs[1] = 0;
    for (int i = 0; i < 4; ++i) { fake_us += 1000000; driver.service(); }
    assert(!driver.isLinked() && driver.diagnostics().recoveries == count);
    uint8_t data[60] = {};
    bus.regs[2] = 1;
    for (int i = 0; i < 4; ++i) assert(driver.sendFrame(data, 60) == 0);
    assert(!driver.diagnostics().pending && driver.diagnostics().tx_timeouts == 0);

    // Brief TX congestion is not a chip fault. Persistent busy requests a raw reset.
    fake_us += 11000000;
    bus.regs[1] = 0x40;
    bus.regs[2] = 1;
    for (int i = 0; i < 3; ++i) assert(driver.sendFrame(data, 60) == 0);
    assert(!driver.diagnostics().pending);
    fake_us += 5000000;
    for (int i = 0; i < 3; ++i) assert(driver.sendFrame(data, 60) == 0);
    assert(driver.diagnostics().tx_timeouts == 6 && driver.diagnostics().pending);
    during_reset = &driver; delay_hook = reentry_check;
    fake_us += 31000000;
    assert(driver.service());
    delay_hook = nullptr;
    assert(driver.diagnostics().recoveries == count + 1);
    assert(!driver.diagnostics().pending);
    for (int i = 0; i < 6; ++i) assert(bus.regs[0x10 + i] == mac[i]);

    // Lost SPI produces failed recovery with bounded attempts, not a reboot loop.
    bus.absent = true;
    for (int i = 0; i < 3; ++i) { fake_us += 1000000; driver.service(); }
    fake_us += 31000000; driver.service();
    auto failures = driver.diagnostics().recovery_failures;
    assert(failures > 0);
    for (int i = 0; i < 10; ++i) { fake_us += 1000000; driver.service(); }
    assert(driver.diagnostics().recovery_failures == failures);
    bus.absent = false;
    fake_us += 301000000;
    assert(driver.service());
    assert(!driver.diagnostics().pending);

    // A stuck reset is bounded, remains pending, and can later recover.
    bus.regs[0x10] = 0;
    bus.stuck_reset = true;
    fake_us += 31000000;
    for (int i = 0; i < 2; ++i) { fake_us += 1000000; assert(!driver.service()); }
    fake_us += 1000000;
    uint32_t before = fake_us;
    assert(!driver.service());
    assert(fake_us - before == 250000);
    assert(driver.diagnostics().pending);
    bus.stuck_reset = false;
    fake_us += 301000000;
    assert(driver.service());
    puts("CH390 RAW DRIVER TEST PASS");
}
