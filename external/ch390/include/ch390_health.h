#pragma once

#include <stdbool.h>
#include <stdint.h>

// OpenSprinkler local policy, shared by the polling adapter and ESP32 MAC (C).
typedef struct {
    uint32_t bad_ready;
    uint32_t bad_header;
    uint32_t rx_errors;
    uint32_t rx_resets;
    uint32_t rx_deferred;
    uint32_t empty_ready;
    uint32_t rx_incomplete;
    uint32_t tx_timeouts;
    uint32_t health_failures;
    uint32_t recoveries;
    uint32_t recovery_failures;
    uint32_t last_fault_ms;
    uint8_t last_fault; // 1 ready, 2 header, 3 TX timeout, 4 register check, 5 RX ring
    bool pending;
} CH390Diagnostics;

typedef struct {
    CH390Diagnostics diag;
    uint32_t burst_start;
    uint32_t last_attempt;
    uint32_t retry_ms;
    uint32_t last_rx_reset;
    uint32_t tx_busy_start;
    uint8_t burst_count;
    uint8_t health_streak;
    bool attempted;
    bool rx_reset_done;
    bool tx_busy;
    uint32_t rx_wait_since;
    uint16_t rx_wait_read, rx_wait_write;
    uint8_t rx_wait_ready, rx_wait_polls;
} CH390Health;

static inline bool ch390_rx_pointer_valid(uint16_t p) {
    return p >= 0x0c00 && p < 0x4000;
}

static inline uint16_t ch390_rx_available(uint16_t read, uint16_t write) {
    return write >= read ? write - read : 0x3400 - (read - write);
}

static inline bool ch390_rx_frame_fits(uint16_t available, uint32_t wire_length) {
    return available >= 4 && wire_length <= (uint32_t)(available - 4);
}

static inline void ch390_rx_progress(CH390Health* h) {
    h->rx_wait_polls = 0;
}

// A changing pointer or ready byte restarts the observation; slow arrivals are
// not faults. Time subtraction deliberately supports millis() rollover.
static inline bool ch390_rx_defer(CH390Health* h, uint16_t read, uint16_t write,
                                  uint8_t ready, uint32_t now) {
    ++h->diag.rx_deferred;
    if (!h->rx_wait_polls || h->rx_wait_read != read ||
            h->rx_wait_write != write || h->rx_wait_ready != ready) {
        h->rx_wait_since = now;
        h->rx_wait_read = read;
        h->rx_wait_write = write;
        h->rx_wait_ready = ready;
        h->rx_wait_polls = 1;
        return false;
    }
    if (h->rx_wait_polls < 3) ++h->rx_wait_polls;
    return h->rx_wait_polls >= 3 && (uint32_t)(now - h->rx_wait_since) >= 2000;
}

static inline bool ch390_valid_wire_length(uint32_t length, uint32_t maximum) {
    // Four CRC bytes alone are not a frame. In LwipIntfDev, zero means no frame.
    return length > 4 && length <= maximum;
}

static inline void ch390_fault(CH390Health* h, uint8_t fault, uint32_t now) {
    h->diag.last_fault = fault;
    h->diag.last_fault_ms = now;
    if (!h->burst_count || (uint32_t)(now - h->burst_start) > 10000) {
        h->burst_start = now;
        h->burst_count = 0;
    }
    if (h->burst_count < 3) ++h->burst_count;
    if (h->burst_count >= 3) h->diag.pending = true;
}

static inline void ch390_health_sample(CH390Health* h, bool good, uint32_t now) {
    if (good) {
        h->health_streak = 0;
    } else {
        ++h->diag.health_failures;
        h->diag.last_fault = 4;
        h->diag.last_fault_ms = now;
        if (h->health_streak < 3) ++h->health_streak;
        if (h->health_streak >= 3) h->diag.pending = true;
    }
}

static inline void ch390_tx_timeout(CH390Health* h, bool linked, uint32_t now) {
    ++h->diag.tx_timeouts;
    if (!linked) { h->tx_busy = false; return; }
    if (!h->tx_busy) {
        h->tx_busy = true;
        h->tx_busy_start = now;
    }
    // Ordinary congestion/pause frames can delay TX. Escalate only if observed
    // busy across at least five seconds, with no successful TX-slot acquisition.
    if ((uint32_t)(now - h->tx_busy_start) >= 5000) ch390_fault(h, 3, now);
}

static inline bool ch390_allow_rx_reset(CH390Health* h, uint32_t now) {
    if (h->rx_reset_done && (uint32_t)(now - h->last_rx_reset) < 1000) return false;
    h->rx_reset_done = true;
    h->last_rx_reset = now;
    ++h->diag.rx_resets;
    return true;
}

static inline bool ch390_recovery_due(CH390Health* h, uint32_t now) {
    if (!h->diag.pending) return false;
    if (h->attempted && (uint32_t)(now - h->last_attempt) < h->retry_ms) return false;
    h->attempted = true;
    h->last_attempt = now;
    return true;
}

static inline void ch390_recovery_result(CH390Health* h, bool success) {
    if (success) {
        ++h->diag.recoveries;
        h->diag.pending = false;
        h->burst_count = h->health_streak = 0;
        h->tx_busy = false;
        h->retry_ms = 30000;
    } else {
        ++h->diag.recovery_failures;
        h->diag.pending = true;
        h->retry_ms = h->retry_ms ? h->retry_ms * 2 : 30000;
        if (h->retry_ms > 300000) h->retry_ms = 300000;
    }
}
