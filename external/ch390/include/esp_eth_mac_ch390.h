/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2024 Sergey Kharenko
 * SPDX-FileContributor: 2024 Espressif Systems (Shanghai) CO LTD
 */

#pragma once

#include "esp_eth_com.h"
#include "esp_eth_mac.h"
#include "ch390_health.h"

#include "esp_idf_version.h"
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
#include "esp_eth_mac_spi.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief CH390 specific configuration
 *
 */
typedef struct {
    int int_gpio_num;                                   /*!< Interrupt GPIO number */
    uint32_t poll_period_ms;                            /*!< Period in ms to poll rx status when interrupt mode is not used */
    spi_host_device_t spi_host_id;                      /*!< SPI peripheral (this field is invalid when custom SPI driver is defined) */
    spi_device_interface_config_t *spi_devcfg;          /*!< SPI device configuration (this field is invalid when custom SPI driver is defined) */
    eth_spi_custom_driver_config_t custom_spi_driver;   /*!< Custom SPI driver definitions */
} eth_ch390_config_t;

/**
 * @brief Default CH390 specific configuration
 *
 */
#define ETH_CH390_DEFAULT_CONFIG(spi_host, spi_devcfg_p) \
    {                                           \
        .int_gpio_num = 4,                      \
        .spi_host_id = spi_host,                \
        .spi_devcfg = spi_devcfg_p,             \
        .custom_spi_driver = ETH_DEFAULT_SPI,   \
    }

/**
* @brief Create CH390 Ethernet MAC instance
*
* @param ch390_config: CH390 specific configuration
* @param mac_config: Ethernet MAC configuration
*
* @return
*      - instance: create MAC instance successfully
*      - NULL: create MAC instance failed because some error occurred
*/
esp_eth_mac_t *esp_eth_mac_new_ch390(const eth_ch390_config_t *ch390_config, const eth_mac_config_t *mac_config);

// OpenSprinkler local extension; snapshots are serialized with MAC operations.
esp_err_t esp_eth_mac_ch390_diagnostics(esp_eth_mac_t *mac, CH390Diagnostics *out);

#if defined(OS_ETH_DIAGNOSTICS)
// Bench-only traffic counters and register access; never in production builds.
#define CH390_DIAG_PROBE_PORT 40001
typedef struct {
    uint32_t rx_frames, rx_bcast, rx_probe, rx_probe_seq, rx_last_ms;
    uint32_t rx_probe_session, rx_probe_backwards;
    uint32_t tx_ok, tx_rejected, tx_last_ms, lock_timeouts;
    uint8_t tsr_or, last_tsra, last_tsrb;
    bool active, initialized;
} CH390TrafficDiag;
esp_err_t esp_eth_mac_ch390_traffic(esp_eth_mac_t *mac, CH390TrafficDiag *out);
esp_err_t esp_eth_mac_ch390_read_regs(esp_eth_mac_t *mac, const uint8_t *addrs, uint8_t *values, size_t count);
// MAC-only receive restart (stop, settle, start); no PHY or netif action.
esp_err_t esp_eth_mac_ch390_restart_rx(esp_eth_mac_t *mac);
#endif

#ifdef __cplusplus
}
#endif
