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
 * @file      driver_ads1115.h
 * @brief     driver ads1115 header file
 * @version   2.0.0
 * @author    Shifeng Li
 * @date      2021-02-13
 *
 * <h3>history</h3>
 * <table>
 * <tr><th>Date        <th>Version  <th>Author      <th>Description
 * <tr><td>2021/02/13  <td>2.0      <td>Shifeng Li  <td>format the code
 * <tr><td>2020/10/13  <td>1.0      <td>Shifeng Li  <td>first upload
 * </table>
 */

#ifndef DRIVER_ADS1115_H
#define DRIVER_ADS1115_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C"{
#endif

/**
 * @defgroup ads1115_driver ads1115 driver function
 * @brief    ads1115 driver modules
 * @{
 */

/**
 * @addtogroup ads1115_base_driver
 * @{
 */

/**
 * @brief ads1115 address enumeration definition
 */
typedef enum
{
    ADS1115_ADDR_GND = 0x00,        /**< ADDR pin connected to GND */
    ADS1115_ADDR_VCC = 0x01,        /**< ADDR pin connected to VCC */
    ADS1115_ADDR_SDA = 0x02,        /**< ADDR pin connected to SDA */
    ADS1115_ADDR_SCL = 0x03,        /**< ADDR pin connected to SCL */
} ads1115_address_t;

/**
 * @brief ads1115 bool enumeration definition
 */
typedef enum
{
    ADS1115_BOOL_FALSE = 0x00,        /**< disable function */
    ADS1115_BOOL_TRUE  = 0x01,        /**< enable function */
} ads1115_bool_t;

/**
 * @brief ads1115 range enumeration definition
 */
typedef enum
{
    ADS1115_RANGE_6P144V = 0x00,        /**< 6.144V range */
    ADS1115_RANGE_4P096V = 0x01,        /**< 4.096V range */
    ADS1115_RANGE_2P048V = 0x02,        /**< 2.048V range */
    ADS1115_RANGE_1P024V = 0x03,        /**< 1.024V range */
    ADS1115_RANGE_0P512V = 0x04,        /**< 0.512V range */
    ADS1115_RANGE_0P256V = 0x05,        /**< 0.256V range */
} ads1115_range_t;

/**
 * @brief ads1115 channel rate enumeration definition
 */
typedef enum
{
    ADS1115_RATE_8SPS   = 0x00,        /**< 8 sample per second */
    ADS1115_RATE_16SPS  = 0x01,        /**< 16 sample per second */
    ADS1115_RATE_32SPS  = 0x02,        /**< 32 sample per second */
    ADS1115_RATE_64SPS  = 0x03,        /**< 64 sample per second */
    ADS1115_RATE_128SPS = 0x04,        /**< 128 sample per second */
    ADS1115_RATE_250SPS = 0x05,        /**< 250 sample per second */
    ADS1115_RATE_475SPS = 0x06,        /**< 475 sample per second */
    ADS1115_RATE_860SPS = 0x07,        /**< 860 sample per second */
} ads1115_rate_t;

/**
 * @brief ads1115 channel enumeration definition
 */
typedef enum
{
    ADS1115_CHANNEL_AIN0_AIN1 = 0x00,        /**< AIN0 and AIN1 pins */
    ADS1115_CHANNEL_AIN0_AIN3 = 0x01,        /**< AIN0 and AIN3 pins */
    ADS1115_CHANNEL_AIN1_AIN3 = 0x02,        /**< AIN1 and AIN3 pins */
    ADS1115_CHANNEL_AIN2_AIN3 = 0x03,        /**< AIN2 and AIN3 pins */
    ADS1115_CHANNEL_AIN0_GND  = 0x04,        /**< AIN0 and GND pins */
    ADS1115_CHANNEL_AIN1_GND  = 0x05,        /**< AIN1 and GND pins */
    ADS1115_CHANNEL_AIN2_GND  = 0x06,        /**< AIN2 and GND pins */
    ADS1115_CHANNEL_AIN3_GND  = 0x07,        /**< AIN3 and GND pins */
} ads1115_channel_t;

/**
 * @}
 */

/**
 * @addtogroup ads1115_interrupt_driver
 * @{
 */

/**
 * @brief ads1115 pin enumeration definition
 */
typedef enum
{
    ADS1115_PIN_LOW  = 0x00,        /**< set pin low */
    ADS1115_PIN_HIGH = 0x01,        /**< set pin high */
} ads1115_pin_t;

/**
 * @brief ads1115 compare mode enumeration definition
 */
typedef enum
{
    ADS1115_COMPARE_THRESHOLD = 0x00,        /**< threshold compare interrupt mode */
    ADS1115_COMPARE_WINDOW    = 0x01,        /**< window compare interrupt mode */
} ads1115_compare_t;

/**
 * @brief ads1115 comparator queue enumeration definition
 */
typedef enum
{
    ADS1115_COMPARATOR_QUEUE_1_CONV    = 0x00,        /**< comparator queue has 1 conv */
    ADS1115_COMPARATOR_QUEUE_2_CONV    = 0x01,        /**< comparator queue has 2 conv */
    ADS1115_COMPARATOR_QUEUE_4_CONV    = 0x02,        /**< comparator queue has 3 conv */
    ADS1115_COMPARATOR_QUEUE_NONE_CONV = 0x03,        /**< comparator queue has no conv */
} ads1115_comparator_queue_t;

/**
 * @}
 */

/**
 * @addtogroup ads1115_base_driver
 * @{
 */

/**
 * @brief ads1115 handle structure definition
 */
typedef struct ads1115_handle_s
{
    uint8_t iic_addr;                                                                   /**< iic device address */
    uint8_t (*iic_init)(void);                                                          /**< point to an iic_init function address */
    uint8_t (*iic_deinit)(void);                                                        /**< point to an iic_deinit function address */
    uint8_t (*iic_read)(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len);         /**< point to an iic_read function address */
    uint8_t (*iic_write)(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len);        /**< point to an iic_write function address */
    void (*delay_ms)(uint32_t ms);                                                      /**< point to a delay_ms function address */
    void (*debug_print)(const char *const fmt, ...);                                    /**< point to a debug_print function address */
    uint8_t inited;                                                                     /**< inited flag */
} ads1115_handle_t;

/**
 * @brief ads1115 information structure definition
 */
typedef struct ads1115_info_s
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
} ads1115_info_t;

/**
 * @}
 */
 
/**
 * @defgroup ads1115_link_driver ads1115 link driver function
 * @brief    ads1115 link driver modules
 * @ingroup  ads1115_driver
 * @{
 */

/**
 * @brief     initialize ads1115_handle_t structure
 * @param[in] HANDLE points to an ads1115 handle structure
 * @param[in] STRUCTURE is ads1115_handle_t
 * @note      none
 */
#define DRIVER_ADS1115_LINK_INIT(HANDLE, STRUCTURE)   memset(HANDLE, 0, sizeof(STRUCTURE))

/**
 * @brief     link iic_init function
 * @param[in] HANDLE points to an ads1115 handle structure
 * @param[in] FUC points to an iic_init function address
 * @note      none
 */
#define DRIVER_ADS1115_LINK_IIC_INIT(HANDLE, FUC)    (HANDLE)->iic_init = FUC

/**
 * @brief     link iic_deinit function
 * @param[in] HANDLE points to an ads1115 handle structure
 * @param[in] FUC points to an iic_deinit function address
 * @note      none
 */
#define DRIVER_ADS1115_LINK_IIC_DEINIT(HANDLE, FUC)  (HANDLE)->iic_deinit = FUC

/**
 * @brief     link iic_read function
 * @param[in] HANDLE points to an ads1115 handle structure
 * @param[in] FUC points to an iic_read function address
 * @note      none
 */
#define DRIVER_ADS1115_LINK_IIC_READ(HANDLE, FUC)    (HANDLE)->iic_read = FUC

/**
 * @brief     link iic_write function
 * @param[in] HANDLE points to an ads1115 handle structure
 * @param[in] FUC points to an iic_write function address
 * @note      none
 */
#define DRIVER_ADS1115_LINK_IIC_WRITE(HANDLE, FUC)   (HANDLE)->iic_write = FUC

/**
 * @brief     link delay_ms function
 * @param[in] HANDLE points to an ads1115 handle structure
 * @param[in] FUC points to a delay_ms function address
 * @note      none
 */
#define DRIVER_ADS1115_LINK_DELAY_MS(HANDLE, FUC)    (HANDLE)->delay_ms = FUC

/**
 * @brief     link debug_print function
 * @param[in] HANDLE points to an ads1115 handle structure
 * @param[in] FUC points to a debug_print function address
 * @note      none
 */
#define DRIVER_ADS1115_LINK_DEBUG_PRINT(HANDLE, FUC) (HANDLE)->debug_print = FUC

/**
 * @}
 */
 
/**
 * @defgroup ads1115_base_driver ads1115 base driver function
 * @brief    ads1115 base driver modules
 * @ingroup  ads1115_driver
 * @{
 */

/**
 * @brief      get chip's information
 * @param[out] *info points to an ads1115 info structure
 * @return     status code
 *             - 0 success
 *             - 2 handle is NULL
 * @note       none
 */
uint8_t ads1115_info(ads1115_info_t *info);

/**
 * @brief     set the iic address pin
 * @param[in] *handle points to an ads1115 handle structure
 * @param[in] addr_pin is the chip iic address pin
 * @return    status code
 *            - 0 success
 *            - 1 set addr pin failed
 *            - 2 handle is NULL
 * @note      none
 */
uint8_t ads1115_set_addr_pin(ads1115_handle_t *handle, ads1115_address_t addr_pin);

/**
 * @brief      get the iic address pin
 * @param[in]  *handle points to an ads1115 handle structure
 * @param[out] *addr_pin points to a chip iic address pin buffer
 * @return      status code
 *              - 0 success
 *              - 1 get addr pin failed
 *              - 2 handle is NULL
 * @note        none
 */
uint8_t ads1115_get_addr_pin(ads1115_handle_t *handle, ads1115_address_t *addr_pin);

/**
 * @brief     initialize the chip
 * @param[in] *handle points to an ads1115 handle structure
 * @return    status code
 *            - 0 success
 *            - 1 iic initialization failed
 *            - 2 handle is NULL
 *            - 3 linked functions is NULL
 * @note      none
 */
uint8_t ads1115_init(ads1115_handle_t *handle);

/**
 * @brief     close the chip
 * @param[in] *handle points to an ads1115 handle structure
 * @return    status code
 *            - 0 success
 *            - 1 iic deinit failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 *            - 4 power down failed
 * @note      none
 */
uint8_t ads1115_deinit(ads1115_handle_t *handle);

/**
 * @brief      read data from the chip once
 * @param[in]  *handle points to an ads1115 handle structure
 * @param[out] *raw points to a raw adc buffer
 * @param[out] *v points to a converted adc buffer
 * @return     status code
 *             - 0 success
 *             - 1 single read failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 * @note       none
 */
uint8_t ads1115_single_read(ads1115_handle_t *handle, int16_t *raw, float *v);

/**
 * @brief     start the chip reading
 * @param[in] *handle points to an ads1115 handle structure
 * @return    status code
 *            - 0 success
 *            - 1 start continuous read failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
uint8_t ads1115_start_continuous_read(ads1115_handle_t *handle);

/**
 * @brief     stop the chip reading
 * @param[in] *handle points to an ads1115 handle structure
 * @return    status code
 *            - 0 success
 *            - 1 stop continuous read failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
uint8_t ads1115_stop_continuous_read(ads1115_handle_t *handle);

/**
 * @brief      read data from the chip continuously
 * @param[in]  *handle points to an ads1115 handle structure
 * @param[out] *raw points to a raw adc buffer
 * @param[out] *v points to a converted adc buffer
 * @return     status code
 *             - 0 success
 *             - 1 continuous read failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 * @note       this function can be used only after run ads1115_start_continuous_read
 *             and can be stopped by ads1115_stop_continuous_read
 */
uint8_t ads1115_continuous_read(ads1115_handle_t *handle,int16_t *raw, float *v);

/**
 * @brief     set the adc channel
 * @param[in] *handle points to an ads1115 handle structure
 * @param[in] channel is the adc channel
 * @return    status code
 *            - 0 success
 *            - 1 set channel failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
uint8_t ads1115_set_channel(ads1115_handle_t *handle, ads1115_channel_t channel);

/**
 * @brief      get the adc channel
 * @param[in]  *handle points to an ads1115 handle structure
 * @param[out] *channel points to a channel buffer
 * @return     status code
 *             - 0 success
 *             - 1 get channel failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 * @note       none
 */
uint8_t ads1115_get_channel(ads1115_handle_t *handle, ads1115_channel_t *channel);

/**
 * @brief     set the adc range
 * @param[in] *handle points to an ads1115 handle structure
 * @param[in] range is the adc max voltage range
 * @return    status code
 *            - 0 success
 *            - 1 set range failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
uint8_t ads1115_set_range(ads1115_handle_t *handle, ads1115_range_t range);

/**
 * @brief      get the adc range
 * @param[in]  *handle points to an ads1115 handle structure
 * @param[out] *range points to a voltage range buffer
 * @return     status code
 *             - 0 success
 *             - 1 get range failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 * @note       none
 */
uint8_t ads1115_get_range(ads1115_handle_t *handle, ads1115_range_t *range);

/**
 * @brief     set the sample rate
 * @param[in] *handle points to an ads1115 handle structure
 * @param[in] rate is the adc sample rate
 * @return    status code
 *            - 0 success
 *            - 1 set rate failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
uint8_t ads1115_set_rate(ads1115_handle_t *handle, ads1115_rate_t rate);

/**
 * @brief      get the sample rate
 * @param[in]  *handle points to an ads1115 handle structure
 * @param[out] *rate points to an adc sample rate buffer
 * @return     status code
 *             - 0 success
 *             - 1 get rate failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 * @note       none
 */
uint8_t ads1115_get_rate(ads1115_handle_t *handle, ads1115_rate_t *rate);

/**
 * @}
 */

/**
 * @defgroup ads1115_interrupt_driver ads1115 interrupt driver function
 * @brief    ads1115 interrupt driver modules
 * @ingroup  ads1115_driver
 * @{
 */

/**
 * @brief     set the alert pin active status
 * @param[in] *handle points to an ads1115 handle structure
 * @param[in] pin is the alert active status
 * @return    status code
 *            - 0 success
 *            - 1 set alert pin failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
uint8_t ads1115_set_alert_pin(ads1115_handle_t *handle, ads1115_pin_t pin);

/**
 * @brief      get the alert pin active status
 * @param[in]  *handle points to an ads1115 handle structure
 * @param[out] *pin points to a pin alert active status buffer
 * @return     status code
 *             - 0 success
 *             - 1 get alert pin failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 * @note      none
 */
uint8_t ads1115_get_alert_pin(ads1115_handle_t *handle, ads1115_pin_t *pin);

/**
 * @brief     set the interrupt compare mode
 * @param[in] *handle points to an ads1115 handle structure
 * @param[in] compare is the interrupt compare mode
 * @return    status code
 *            - 0 success
 *            - 1 set compare mode failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
uint8_t ads1115_set_compare_mode(ads1115_handle_t *handle, ads1115_compare_t compare);

/**
 * @brief      get the interrupt compare mode
 * @param[in]  *handle points to an ads1115 handle structure
 * @param[out] *compare points to an interrupt compare mode buffer
 * @return     status code
 *             - 0 success
 *             - 1 get compare mode failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 * @note       none
 */
uint8_t ads1115_get_compare_mode(ads1115_handle_t *handle, ads1115_compare_t *compare);

/**
 * @brief     set the interrupt comparator queue
 * @param[in] *handle points to an ads1115 handle structure
 * @param[in] comparator_queue is the interrupt comparator queue
 * @return    status code
 *            - 0 success
 *            - 1 set comparator queue failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
uint8_t ads1115_set_comparator_queue(ads1115_handle_t *handle, ads1115_comparator_queue_t comparator_queue);

/**
 * @brief      get the interrupt comparator queue
 * @param[in]  *handle points to an ads1115 handle structure
 * @param[out] *comparator_queue points to an interrupt comparator queue
 * @return     status code
 *             - 0 success
 *             - 1 get comparator queue failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 * @note       none
 */
uint8_t ads1115_get_comparator_queue(ads1115_handle_t *handle, ads1115_comparator_queue_t *comparator_queue);

/**
 * @brief     enable or disable the interrupt compare
 * @param[in] *handle points to an ads1115 handle structure
 * @param[in] enable is a bool value
 * @return    status code
 *            - 0 success
 *            - 1 set compare failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
uint8_t ads1115_set_compare(ads1115_handle_t *handle, ads1115_bool_t enable);

/**
 * @brief      get the interrupt compare status
 * @param[in]  *handle points to an ads1115 handle structure
 * @param[out] *enable points to a bool value buffer
 * @return     status code
 *             - 0 success
 *             - 1 get compare failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 * @note       none
 */
uint8_t ads1115_get_compare(ads1115_handle_t *handle, ads1115_bool_t *enable);

/**
 * @brief     set the interrupt compare threshold
 * @param[in] *handle points to an ads1115 handle structure
 * @param[in] high_threshold is the interrupt high threshold
 * @param[in] low_threshold is the interrupt low threshold
 * @return    status code
 *            - 0 success
 *            - 1 set compare threshold failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
uint8_t ads1115_set_compare_threshold(ads1115_handle_t *handle, int16_t high_threshold, int16_t low_threshold);

/**
 * @brief      get the interrupt compare threshold
 * @param[in]  *handle points to an ads1115 handle structure
 * @param[out] *high_threshold points to an interrupt high threshold buffer
 * @param[out] *low_threshold points to an interrupt low threshold buffer
 * @return     status code
 *             - 0 success
 *             - 1 get compare threshold failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 * @note       none
 */
uint8_t ads1115_get_compare_threshold(ads1115_handle_t *handle, int16_t *high_threshold, int16_t *low_threshold);

/**
 * @brief      convert a adc value to a register raw data
 * @param[in]  *handle points to an ads1115 handle structure
 * @param[in]  s is a converted adc value
 * @param[out] *reg points to a register raw buffer
 * @return     status code
 *             - 0 success
 *             - 1 convert to register failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 * @note       none
 */
uint8_t ads1115_convert_to_register(ads1115_handle_t *handle, float s, int16_t *reg);

/**
 * @brief      convert a register raw data to a converted adc data
 * @param[in]  *handle points to an ads1115 handle structure
 * @param[in]  reg is the register raw data
 * @param[out] *s points to a converted adc value buffer
 * @return     status code
 *             - 0 success
 *             - 1 convert to data failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 * @note       none
 */
uint8_t ads1115_convert_to_data(ads1115_handle_t *handle, int16_t reg, float *s);

/**
 * @}
 */

/**
 * @defgroup ads1115_extend_driver ads1115 extend driver function
 * @brief    ads1115 extend driver modules
 * @ingroup  ads1115_driver
 * @{
 */

/**
 * @brief     set the chip register
 * @param[in] *handle points to an ads1115 handle structure
 * @param[in] reg is the iic register address
 * @param[in] value is the data written to the register
 * @return    status code
 *            - 0 success
 *            - 1 write failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
uint8_t ads1115_set_reg(ads1115_handle_t *handle, uint8_t reg, int16_t value);

/**
 * @brief      get the chip register
 * @param[in]  *handle points to an ads1115 handle structure
 * @param[in]  reg is the iic register address
 * @param[out] *value points to a read data buffer
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 * @note       none
 */
uint8_t ads1115_get_reg(ads1115_handle_t *handle, uint8_t reg, int16_t *value);

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
