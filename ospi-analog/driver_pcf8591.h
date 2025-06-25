/**
 * Copyright (c) 2015 - present LibDriver All rights reserved
 * 
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. 
 *
 * @file      driver_pcf8591.h
 * @brief     driver pcf8591 header file
 * @version   2.0.0
 * @author    Shifeng Li
 * @date      2021-02-18
 *
 * <h3>history</h3>
 * <table>
 * <tr><th>Date        <th>Version  <th>Author      <th>Description
 * <tr><td>2021/02/18  <td>2.0      <td>Shifeng Li  <td>format the code
 * <tr><td>2020/10/17  <td>1.0      <td>Shifeng Li  <td>first upload
 * </table>
 */

#ifndef DRIVER_PCF8591_H
#define DRIVER_PCF8591_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C"{
#endif

/**
 * @defgroup pcf8591_driver pcf8591 driver function
 * @brief    pcf8591 driver modules
 * @{
 */

/**
 * @addtogroup pcf8591_base_driver
 * @{
 */

/**
 * @brief pcf8591 address enumeration definition
 */
typedef enum
{
    PCF8591_ADDRESS_A000 = 0,        /**< A2A1A0 000 */
    PCF8591_ADDRESS_A001 = 1,        /**< A2A1A0 001 */
    PCF8591_ADDRESS_A010 = 2,        /**< A2A1A0 010 */
    PCF8591_ADDRESS_A011 = 3,        /**< A2A1A0 011 */
    PCF8591_ADDRESS_A100 = 4,        /**< A2A1A0 100 */
    PCF8591_ADDRESS_A101 = 5,        /**< A2A1A0 101 */
    PCF8591_ADDRESS_A110 = 6,        /**< A2A1A0 110 */
    PCF8591_ADDRESS_A111 = 7,        /**< A2A1A0 111 */
} pcf8591_address_t;

/**
 * @brief pcf8591 bool enumeration definition
 */
typedef enum
{
    PCF8591_BOOL_FALSE = 0x00,        /**< disable function */
    PCF8591_BOOL_TRUE  = 0x01,        /**< enable function */
} pcf8591_bool_t;

/**
 * @brief pcf8591 channel definition
 */
typedef enum  
{
    PCF8591_CHANNEL_0 = 0x00,        /**< channel 0 */
    PCF8591_CHANNEL_1 = 0x01,        /**< channel 1 */
    PCF8591_CHANNEL_2 = 0x02,        /**< channel 2 */
    PCF8591_CHANNEL_3 = 0x03,        /**< channel 3 */
} pcf8591_channel_t;

/**
 * @brief pcf8591 mode definition
 */
typedef enum  
{
    PCF8591_MODE_AIN0123_GND                         = 0x00,        /**< AIN0-GND AIN1-GND AIN2-GND AIN3-GND */
    PCF8591_MODE_AIN012_AIN3                         = 0x01,        /**< AIN0-AIN3 AIN1-AIN3 AIN2-AIN3 */
    PCF8591_MODE_AIN0_GND_AND_AIN1_GND_AND_AIN2_AIN3 = 0x02,        /**< AIN0-GND AIN1-GND AIN2-AIN3 */
    PCF8591_MODE_AIN0_AIN1_AND_ANI2_AIN3             = 0x03,        /**< AIN0-AIN1 AIN2-AIN3 */
} pcf8591_mode_t;

/**
 * @brief pcf8591 handle structure definition
 */
typedef struct pcf8591_handle_s
{
    uint8_t iic_addr;                                                          /**< iic device address */
    uint8_t (*iic_init)(void);                                                 /**< point to an iic_init function address */
    uint8_t (*iic_deinit)(void);                                               /**< point to an iic_deinit function address */
    uint8_t (*iic_read_cmd)(uint8_t addr, uint8_t *buf, uint16_t len);         /**< point to an iic_read_cmd function address */
    uint8_t (*iic_write_cmd)(uint8_t addr, uint8_t *buf, uint16_t len);        /**< point to an iic_write_cmd function address */
    void (*delay_ms)(uint32_t ms);                                             /**< point to a delay_ms function address */
    void (*debug_print)(const char *const fmt, ...);                           /**< point to a debug_print function address */
    uint8_t inited;                                                            /**< inited flag */
    uint8_t conf;                                                              /**< chip conf */
    float ref_voltage;                                                         /**< chip reference voltage */
} pcf8591_handle_t;

/**
 * @brief pcf8591 information structure definition
 */
typedef struct pcf8591_info_s
{
    char chip_name[32];                /**< chip name */
    char manufacturer_name[32];        /**< manufacturer name */
    char interface[8];                 /**< chip interface name */
    float supply_voltage_min_v;        /**< chip min supply voltage */
    float supply_voltage_max_v;        /**< chip max supply voltage */
    float max_current_ma;              /**< chip max current */
    float temperature_min;             /**< chip min operating temperature */
    float temperature_max;             /**< chip max operating temperature */
    uint32_t driver_version;           /**< driver version */
} pcf8591_info_t;

/**
 * @}
 */

/**
 * @defgroup pcf8591_link_driver pcf8591 link driver function
 * @brief    pcf8591 link driver modules
 * @ingroup  pcf8591_driver
 * @{
 */

/**
 * @brief     initialize pcf8591_handle_t structure
 * @param[in] HANDLE points to a pcf8591 handle structure
 * @param[in] STRUCTURE is pcf8591_handle_t
 * @note      none
 */
#define DRIVER_PCF8591_LINK_INIT(HANDLE, STRUCTURE)           memset(HANDLE, 0, sizeof(STRUCTURE))

/**
 * @brief     link iic_init function
 * @param[in] HANDLE points to a pcf8591 handle structure
 * @param[in] FUC points to an iic_init function address
 * @note      none
 */
#define DRIVER_PCF8591_LINK_IIC_INIT(HANDLE, FUC)            (HANDLE)->iic_init = FUC

/**
 * @brief     link iic_deinit function
 * @param[in] HANDLE points to a pcf8591 handle structure
 * @param[in] FUC points to an iic_deinit function address
 * @note      none
 */
#define DRIVER_PCF8591_LINK_IIC_DEINIT(HANDLE, FUC)          (HANDLE)->iic_deinit = FUC

/**
 * @brief     link iic_read_cmd function
 * @param[in] HANDLE points to a pcf8591 handle structure
 * @param[in] FUC points to an iic_read_cmd function address
 * @note      none
 */
#define DRIVER_PCF8591_LINK_IIC_READ_COMMAND(HANDLE, FUC)    (HANDLE)->iic_read_cmd = FUC

/**
 * @brief     link iic_write_cmd function
 * @param[in] HANDLE points to a pcf8591 handle structure
 * @param[in] FUC points to an iic_write_cmd function address
 * @note      none
 */
#define DRIVER_PCF8591_LINK_IIC_WRITE_COMMAND(HANDLE, FUC)   (HANDLE)->iic_write_cmd = FUC

/**
 * @brief     link delay_ms function
 * @param[in] HANDLE points to a pcf8591 handle structure
 * @param[in] FUC points to a delay_ms function address
 * @note      none
 */
#define DRIVER_PCF8591_LINK_DELAY_MS(HANDLE, FUC)            (HANDLE)->delay_ms = FUC

/**
 * @brief     link debug_print function
 * @param[in] HANDLE points to a pcf8591 handle structure
 * @param[in] FUC points to a debug_print function address
 * @note      none
 */
#define DRIVER_PCF8591_LINK_DEBUG_PRINT(HANDLE, FUC)         (HANDLE)->debug_print = FUC

/**
 * @}
 */

/**
 * @defgroup pcf8591_base_driver pcf8591 base driver function
 * @brief    pcf8591 base driver modules
 * @ingroup  pcf8591_driver
 * @{
 */

/**
 * @brief      get chip's information
 * @param[out] *info points to a pcf8591 info structure
 * @return     status code
 *             - 0 success
 *             - 2 handle is NULL
 * @note       none
 */
uint8_t pcf8591_info(pcf8591_info_t *info);

/**
 * @brief     set the address pin
 * @param[in] *handle points to a pcf8591 handle structure
 * @param[in] addr_pin is the chip address pins
 * @return    status code
 *            - 0 success
 *            - 2 handle is NULL
 * @note      none
 */
uint8_t pcf8591_set_addr_pin(pcf8591_handle_t *handle, pcf8591_address_t addr_pin);

/**
 * @brief      get the address pin
 * @param[in]  *handle points to a pcf8591 handle structure
 * @param[out] *addr_pin points to a chip address pins buffer
 * @return     status code
 *             - 0 success
 *             - 2 handle is NULL
 * @note       none
 */
uint8_t pcf8591_get_addr_pin(pcf8591_handle_t *handle, pcf8591_address_t *addr_pin);

/**
 * @brief     initialize the chip
 * @param[in] *handle points to a pcf8591 handle structure
 * @return    status code
 *            - 0 success
 *            - 1 iic initialization failed
 *            - 2 handle is NULL
 *            - 3 linked functions is NULL
 * @note      none
 */
uint8_t pcf8591_init(pcf8591_handle_t *handle);

/**
 * @brief     close the chip
 * @param[in] *handle points to a pcf8591 handle structure
 * @return    status code
 *            - 0 success
 *            - 1 iic deinit failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
uint8_t pcf8591_deinit(pcf8591_handle_t *handle);

/**
 * @brief      read data from the chip
 * @param[in]  *handle points to a pcf8591 handle structure
 * @param[out] *raw points to a raw adc buffer
 * @param[out] *adc points to a converted adc buffer
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 * @note       none
 */
uint8_t pcf8591_read(pcf8591_handle_t *handle, int16_t *raw, float *adc);

/**
 * @brief         read the multiple channel data from the chip
 * @param[in]     *handle points to a pcf8591 handle structure
 * @param[out]    *raw points to a raw adc buffer
 * @param[out]    *adc points to a converted adc buffer
 * @param[in,out] *len points to an adc length buffer
 * @return        status code
 *                - 0 success
 *                - 1 read failed
 *                - 2 handle is NULL
 *                - 3 handle is not initialized
 * @note          len means max length of raw and adc input buffer and return the read length of raw and adc
 */
uint8_t pcf8591_multiple_read(pcf8591_handle_t *handle, int16_t *raw, float *adc, uint8_t *len);

/**
 * @brief     write to the dac
 * @param[in] *handle points to a pcf8591 handle structure
 * @param[in] data is the dac value
 * @return    status code
 *            - 0 success
 *            - 1 write dac value failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
uint8_t pcf8591_write(pcf8591_handle_t *handle, uint8_t data);

/**
 * @brief      convert a dac value to a register raw data
 * @param[in]  *handle points to a pcf8591 handle structure
 * @param[in]  dac is a converted dac value
 * @param[out] *reg points to a register raw buffer
 * @return     status code
 *             - 0 success
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 * @note       none
 */
uint8_t pcf8591_dac_convert_to_register(pcf8591_handle_t *handle, float dac, uint8_t *reg);

/**
 * @brief     set the adc reference voltage
 * @param[in] *handle points to a pcf8591 handle structure
 * @param[in] ref_voltage is the adc reference voltage
 * @return    status code
 *            - 0 success
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
uint8_t pcf8591_set_reference_voltage(pcf8591_handle_t *handle, float ref_voltage);

/**
 * @brief      get the adc reference voltage
 * @param[in]  *handle points to a pcf8591 handle structure
 * @param[out] *ref_voltage points to an adc reference voltage buffer
 * @return     status code
 *             - 0 success
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 * @note       none
 */
uint8_t pcf8591_get_reference_voltage(pcf8591_handle_t *handle, float *ref_voltage);

/**
 * @brief     set the adc channel
 * @param[in] *handle points to a pcf8591 handle structure
 * @param[in] channel is the adc channel
 * @return    status code
 *            - 0 success
 *            - 1 set channel failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
uint8_t pcf8591_set_channel(pcf8591_handle_t *handle, pcf8591_channel_t channel);

/**
 * @brief      get the adc channel
 * @param[in]  *handle points to a pcf8591 handle structure
 * @param[out] *channel points to an adc channel buffer
 * @return     status code
 *             - 0 success
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 * @note       none
 */
uint8_t pcf8591_get_channel(pcf8591_handle_t *handle, pcf8591_channel_t *channel);

/**
 * @brief     set the adc mode
 * @param[in] *handle points to a pcf8591 handle structure
 * @param[in] mode is the adc mode
 * @return    status code
 *            - 0 success
 *            - 1 set mode failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
uint8_t pcf8591_set_mode(pcf8591_handle_t *handle, pcf8591_mode_t mode);

/**
 * @brief      get the adc mode
 * @param[in]  *handle points to a pcf8591 handle structure
 * @param[out] *mode points to an adc mode buffer
 * @return     status code
 *             - 0 success
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 * @note       none
 */
uint8_t pcf8591_get_mode(pcf8591_handle_t *handle, pcf8591_mode_t *mode);

/**
 * @brief     set the adc auto increment read mode
 * @param[in] *handle points to a pcf8591 handle structure
 * @param[in] enable is a bool value
 * @return    status code
 *            - 0 success
 *            - 1 set auto increment failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
uint8_t pcf8591_set_auto_increment(pcf8591_handle_t *handle, pcf8591_bool_t enable);

/**
 * @brief      get the adc auto increment read mode
 * @param[in]  *handle points to a pcf8591 handle structure
 * @param[out] *enable points to a bool value buffer
 * @return     status code
 *             - 0 success
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 * @note       none
 */
uint8_t pcf8591_get_auto_increment(pcf8591_handle_t *handle, pcf8591_bool_t *enable);

/**
 * @}
 */

/**
 * @defgroup pcf8591_extern_driver pcf8591 extern driver function
 * @brief    pcf8591 extern driver modules
 * @ingroup  pcf8591_driver
 * @{
 */

/**
 * @brief     set the chip register
 * @param[in] *handle points to a pcf8591 handle structure
 * @param[in] *buf points to a data buffer.
 * @param[in] len is the data buffer
 * @return    status code
 *            - 0 success
 *            - 1 write failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
uint8_t pcf8591_set_reg(pcf8591_handle_t *handle, uint8_t *buf, uint16_t len);

/**
 * @brief      get the chip register
 * @param[in]  *handle points to a pcf8591 handle structure
 * @param[out] *buf points to a data buffer.
 * @param[in]  len is the data buffer
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 * @note       none
 */
uint8_t pcf8591_get_reg(pcf8591_handle_t *handle, uint8_t *buf, uint16_t len);

/**
 * @}
 */

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
