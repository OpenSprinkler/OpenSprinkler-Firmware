#include "external/ch390/include/ch390_health.h"
#include <cassert>
#include <cstdio>

int main() {
    assert(ch390_rx_available(0x0c00, 0x0c00) == 0);
    assert(ch390_rx_available(0x0f30, 0x0c00) == 0x30d0);
    assert(ch390_rx_available(0x3ffe, 0x0c42) == 68);
    assert(ch390_rx_frame_fits(68, 64));
    assert(!ch390_rx_frame_fits(67, 64));
    assert(!ch390_rx_frame_fits(3, 0));
    assert(!ch390_rx_pointer_valid(0x0bff) && !ch390_rx_pointer_valid(0x4000));
    CH390Health wait = {};
    assert(!ch390_rx_defer(&wait, 0xc00, 0xc44, 0, 0xffffff00U));
    assert(!ch390_rx_defer(&wait, 0xc00, 0xc44, 0, 500));
    assert(ch390_rx_defer(&wait, 0xc00, 0xc44, 0, 1744));
    assert(!ch390_rx_defer(&wait, 0xc00, 0xc45, 0, 2000));
    assert(!ch390_rx_defer(&wait, 0xc00, 0xc45, 1, 5000));
    ch390_rx_progress(&wait);
    assert(!ch390_rx_defer(&wait, 0xc00, 0xc45, 1, 8000));
    for (unsigned n = 0; n <= 4; ++n) assert(!ch390_valid_wire_length(n, 1522));
    assert(ch390_valid_wire_length(5, 1522));
    assert(ch390_valid_wire_length(64, 1522));
    assert(ch390_valid_wire_length(1522, 1522));
    assert(!ch390_valid_wire_length(1523, 1522));
    assert(!ch390_valid_wire_length(65535, 1522));

    CH390Health h = {};
    assert(!ch390_recovery_due(&h, 0));
    // Healthy register checks, regardless of link/DHCP/OTC availability.
    for (unsigned t = 0; t < 100000; t += 1000) ch390_health_sample(&h, true, t);
    assert(!h.diag.pending);
    ch390_health_sample(&h, false, 100000);
    ch390_health_sample(&h, false, 101000);
    assert(!h.diag.pending);
    ch390_health_sample(&h, true, 102000);
    ch390_health_sample(&h, false, 103000);
    assert(!h.diag.pending);
    ch390_health_sample(&h, false, 104000);
    ch390_health_sample(&h, false, 105000);
    assert(h.diag.pending && h.diag.last_fault == 4);
    assert(ch390_recovery_due(&h, 105000));
    ch390_recovery_result(&h, false);
    assert(!ch390_recovery_due(&h, 134999));
    assert(ch390_recovery_due(&h, 135000));
    ch390_recovery_result(&h, false);
    assert(h.retry_ms == 60000);
    assert(!ch390_recovery_due(&h, 194999));
    assert(ch390_recovery_due(&h, 195000));
    ch390_recovery_result(&h, true);
    assert(!h.diag.pending && h.diag.recoveries == 1 && h.diag.recovery_failures == 2);
    for (int i = 0; i < 3; ++i) ch390_fault(&h, 3, 196000 + i);
    assert(!ch390_recovery_due(&h, 224999));
    assert(ch390_recovery_due(&h, 225000));
    for (int i = 0; i < 10; ++i) ch390_recovery_result(&h, false);
    assert(h.retry_ms == 300000);

    h = {};
    ch390_fault(&h, 1, 1);
    ch390_fault(&h, 2, 10002);
    ch390_fault(&h, 1, 20003);
    assert(!h.diag.pending); // Isolated incidents do not escalate.
    assert(ch390_allow_rx_reset(&h, 0xfffffff0U));
    assert(!ch390_allow_rx_reset(&h, 100));
    assert(ch390_allow_rx_reset(&h, 984));
    assert(h.diag.rx_resets == 2);
    h = {};
    ch390_fault(&h, 1, 0xffffff00U);
    ch390_fault(&h, 1, 0xfffffff0U);
    ch390_fault(&h, 1, 0x10);
    assert(ch390_recovery_due(&h, 0xfffffff0U));
    ch390_recovery_result(&h, false);
    assert(!ch390_recovery_due(&h, 29983));
    assert(ch390_recovery_due(&h, 29984));

    h = {};
    for (unsigned t = 0; t < 5000; t += 100) ch390_tx_timeout(&h, true, t);
    assert(!h.diag.pending);
    ch390_tx_timeout(&h, false, 5000); // A cable loss abandons the observation.
    ch390_tx_timeout(&h, true, 6000);
    assert(!h.diag.pending);
    h.tx_busy = false; // Successful acquisition of the transmit slot.
    ch390_tx_timeout(&h, true, 12000);
    ch390_tx_timeout(&h, true, 17000);
    ch390_tx_timeout(&h, true, 18000);
    assert(!h.diag.pending);
    ch390_tx_timeout(&h, true, 19000);
    assert(h.diag.pending);
    puts("CH390 HEALTH TEST PASS");
}
