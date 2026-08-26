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

#ifdef __cplusplus
}
#endif
