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
 * @file      driver_ads1115.c
 * @brief     driver ads1115 source file
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

#include "driver_ads1115.h"

/**
 * @brief chip information definition
 */
#define CHIP_NAME                 "Texas Instruments ADS1115"        /**< chip name */
#define MANUFACTURER_NAME         "Texas Instruments"                /**< manufacturer name */
#define SUPPLY_VOLTAGE_MIN        2.0f                               /**< chip min supply voltage */
#define SUPPLY_VOLTAGE_MAX        5.5f                               /**< chip max supply voltage */
#define MAX_CURRENT               0.2f                               /**< chip max current */
#define TEMPERATURE_MIN           -40.0f                             /**< chip min operating temperature */
#define TEMPERATURE_MAX           125.0f                             /**< chip max operating temperature */
#define DRIVER_VERSION            2000                               /**< driver version */

/**
 * @brief chip register definition
 */
#define ADS1115_REG_CONVERT         0x00        /**< adc result register */
#define ADS1115_REG_CONFIG          0x01        /**< chip config register */
#define ADS1115_REG_LOWRESH         0x02        /**< interrupt low threshold register */
#define ADS1115_REG_HIGHRESH        0x03        /**< interrupt high threshold register */

/**
 * @brief iic address definition
 */
#define ADS1115_ADDRESS1        (0x48 << 1)        /**< iic address 1 */
#define ADS1115_ADDRESS2        (0x49 << 1)        /**< iic address 2 */
#define ADS1115_ADDRESS3        (0x4A << 1)        /**< iic address 3 */
#define ADS1115_ADDRESS4        (0x4B << 1)        /**< iic address 4 */

/**
 * @brief      read multiple bytes
 * @param[in]  *handle points to an ads1115 handle structure
 * @param[in]  reg is the iic register address
 * @param[out] *data points to a data buffer
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 * @note       none
 */
static uint8_t a_ads1115_iic_multiple_read(ads1115_handle_t *handle, uint8_t reg, int16_t *data)
{
    uint8_t buf[2];
    
    memset(buf, 0, sizeof(uint8_t) * 2);                                        /* clear the buffer */
    if (handle->iic_read(handle->iic_addr, reg, (uint8_t *)buf, 2) == 0)        /* read data */
    {
        *data = (uint16_t)(((uint16_t)buf[0] << 8) | buf[1]);                   /* set data */
       
        return 0;                                                               /* success return 0 */
    }
    else
    {
        return 1;                                                               /* return error */
    }
}

/**
 * @brief     write multiple bytes
 * @param[in] *handle points to an ads1115 handle structure
 * @param[in] reg is the iic register address
 * @param[in] data is the sent data
 * @return    status code
 *            - 0 success
 *            - 1 write failed
 * @note      none
 */
static uint8_t a_ads1115_iic_multiple_write(ads1115_handle_t *handle, uint8_t reg, uint16_t data)
{
    uint8_t buf[2];
  
    buf[0] = (data >> 8) & 0xFF;                                            /* set MSB */
    buf[1] = data & 0xFF;                                                   /* set LSB */
    if (handle->iic_write(handle->iic_addr, reg, (uint8_t *)buf, 2) != 0)   /* write data */
    {
        return 1;                                                           /* return error */
    }
    else
    {
        return 0;                                                           /* success return 0 */
    }
}

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
uint8_t ads1115_init(ads1115_handle_t *handle)
{
    if (handle == NULL)                                              /* check handle */
    {
        return 2;                                                    /* return error */
    }
    if (handle->debug_print == NULL)                                 /* check debug_print */
    {
        return 3;                                                    /* return error */
    }
    if (handle->iic_init == NULL)                                    /* check iic_init */
    {
        handle->debug_print("ads1115: iic_init is null.\n");         /* iic_init is null */
        
        return 3;                                                    /* return error */
    }
    if (handle->iic_deinit == NULL)                                  /* check iic_deinit */
    {
        handle->debug_print("ads1115: iic_deinit is null.\n");       /* iic_deinit is null */
        
        return 3;                                                    /* return error */
    }
    if (handle->iic_read == NULL)                                    /* check iic_read */
    {
        handle->debug_print("ads1115: iic_read is null.\n");         /* iic_read is null */
        
        return 3;                                                    /* return error */
    }
    if (handle->iic_write == NULL)                                   /* check iic_write */
    {
        handle->debug_print("ads1115: iic_write is null.\n");        /* iic_write is null */
        
        return 3;                                                    /* return error */
    }
    if (handle->delay_ms == NULL)                                    /* check delay_ms */
    {
        handle->debug_print("ads1115: delay_ms is null.\n");         /* delay_ms is null */
        
        return 3;                                                    /* return error */
    }
    
    if (handle->iic_init() != 0)                                     /* iic init */
    {
        handle->debug_print("ads1115: iic init failed.\n");          /* iic init failed */
        
        return 1;                                                    /* return error */
    }
    handle->inited = 1;                                              /* flag inited */
    
    return 0;                                                        /* success return 0 */
}

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
uint8_t ads1115_deinit(ads1115_handle_t *handle)
{
    uint8_t res;
    uint16_t conf;
    
    if (handle == NULL)                                                                    /* check handle */
    {
        return 2;                                                                          /* return error */
    }
    if (handle->inited != 1)                                                               /* check handle initialization */
    {
        return 3;                                                                          /* return error */
    }
    
    res = a_ads1115_iic_multiple_read(handle, ADS1115_REG_CONFIG, (int16_t *)&conf);       /* read config */
    if (res != 0)                                                                          /* check error */
    {
        handle->debug_print("ads1115: read config failed.\n");                             /* read config failed */
        
        return 4;                                                                          /* return error */
    }
    conf &= ~(0x01 << 8);                                                                  /* clear bit */
    conf |= 1 << 8;                                                                        /* set stop continues read */
    res = a_ads1115_iic_multiple_write(handle, ADS1115_REG_CONFIG, conf);                  /* write config */
    if (res != 0)                                                                          /* check error */
    {
        handle->debug_print("ads1115: write config failed.\n");                            /* write config failed */
        
        return 4;                                                                          /* return error */
    }
    res = handle->iic_deinit();                                                            /* close iic */
    if (res != 0)                                                                          /* check the result */
    {
        handle->debug_print("ads1115: iic deinit failed.\n");                              /* iic deinit failed */
        
        return 1;                                                                          /* return error */
    }
    handle->inited = 0;                                                                    /* flag close */
    
    return 0;                                                                              /* success return 0 */
}

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
uint8_t ads1115_set_channel(ads1115_handle_t *handle, ads1115_channel_t channel)
{
    uint8_t res;
    uint16_t conf;
    
    if (handle == NULL)                                                                    /* check handle */
    {
        return 2;                                                                          /* return error */
    }
    if (handle->inited != 1)                                                               /* check handle initialization */
    {
        return 3;                                                                          /* return error */
    }
    
    res = a_ads1115_iic_multiple_read(handle, ADS1115_REG_CONFIG, (int16_t *)&conf);       /* read config */
    if (res != 0)                                                                          /* check error */
    {
        handle->debug_print("ads1115: read config failed.\n");                             /* read config failed */
        
        return 1;                                                                          /* return error */
    }
    conf &= ~(0x07 << 12);                                                                 /* clear channel */
    conf |= (channel & 0x07) << 12;                                                        /* set channel */
    res = a_ads1115_iic_multiple_write(handle, ADS1115_REG_CONFIG, conf);                  /* write config */
    if (res != 0)                                                                          /* check error */
    {
        handle->debug_print("ads1115: write config failed.\n");                            /* write config failed */
        
        return 1;                                                                          /* return error */
    }
    
    return 0;                                                                              /* success return 0 */
}

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
uint8_t ads1115_get_channel(ads1115_handle_t *handle, ads1115_channel_t *channel)
{
    uint8_t res;
    uint16_t conf;
    
    if (handle == NULL)                                                                    /* check handle */
    {
        return 2;                                                                          /* return error */
    }
    if (handle->inited != 1)                                                               /* check handle initialization */
    {
        return 3;                                                                          /* return error */
    }
    
    res = a_ads1115_iic_multiple_read(handle, ADS1115_REG_CONFIG, (int16_t *)&conf);       /* read config */
    if (res != 0)                                                                          /* check error */
    {
        handle->debug_print("ads1115: read config failed.\n");                             /* read config failed */
        
        return 1;                                                                          /* return error */
    }
    *channel = (ads1115_channel_t)((conf >> 12) & 0x07);                                   /* get channel */
    
    return 0;                                                                              /* success return 0 */
}

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
uint8_t ads1115_set_range(ads1115_handle_t *handle, ads1115_range_t range)
{
    uint8_t res;
    uint16_t conf;
    
    if (handle == NULL)                                                                    /* check handle */
    {
        return 2;                                                                          /* return error */
    }
    if (handle->inited != 1)                                                               /* check handle initialization */
    {
        return 3;                                                                          /* return error */
    }
    
    res = a_ads1115_iic_multiple_read(handle, ADS1115_REG_CONFIG, (int16_t *)&conf);       /* read config */
    if (res != 0)                                                                          /* check error */
    {
        handle->debug_print("ads1115: read config failed.\n");                             /* read config failed */
        
        return 1;                                                                          /* return error */
    }
    conf &= ~(0x07 << 9);                                                                  /* clear range */
    conf |= (range & 0x07) << 9;                                                           /* set range */
    res = a_ads1115_iic_multiple_write(handle, ADS1115_REG_CONFIG, conf);                  /* write config */
    if (res != 0)                                                                          /* check error */
    {
        handle->debug_print("ads1115: write config failed.\n");                            /* write config failed */
        
        return 1;                                                                          /* return error */
    }
    
    return 0;                                                                              /* success return 0 */
}

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
uint8_t ads1115_get_range(ads1115_handle_t *handle, ads1115_range_t *range)
{
    uint8_t res;
    uint16_t conf;
    
    if (handle == NULL)                                                                    /* check handle */
    {
        return 2;                                                                          /* return error */
    }
    if (handle->inited != 1)                                                               /* check handle initialization */
    {
        return 3;                                                                          /* return error */
    }
    
    res = a_ads1115_iic_multiple_read(handle, ADS1115_REG_CONFIG, (int16_t *)&conf);       /* read config */
    if (res != 0)                                                                          /* check error */
    {
        handle->debug_print("ads1115: read config failed.\n");                             /* read config failed */
        
        return 1;                                                                          /* return error */
    }
    *range = (ads1115_range_t)((conf >> 9) & 0x07);                                        /* get range */
    
    return 0;                                                                              /* success return 0 */
}

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
uint8_t ads1115_set_alert_pin(ads1115_handle_t *handle, ads1115_pin_t pin)
{
    uint8_t res;
    uint16_t conf;
    
    if (handle == NULL)                                                                    /* check handle */
    {
        return 2;                                                                          /* return error */
    }
    if (handle->inited != 1)                                                               /* check handle initialization */
    {
        return 3;                                                                          /* return error */
    }
    
    res = a_ads1115_iic_multiple_read(handle, ADS1115_REG_CONFIG, (int16_t *)&conf);       /* read config */
    if (res != 0)                                                                          /* check error */
    {
        handle->debug_print("ads1115: read config failed.\n");                             /* read config failed */
        
        return 1;                                                                          /* return error */
    }
    conf &= ~(1 << 3);                                                                     /* clear alert pin */
    conf |= (pin & 0x01) << 3;                                                             /* set alert pin */
    res = a_ads1115_iic_multiple_write(handle, ADS1115_REG_CONFIG, conf);                  /* write config */
    if (res != 0)                                                                          /* check error */
    {
        handle->debug_print("ads1115: write config failed.\n");                            /* write config failed */
        
        return 1;                                                                          /* return error */
    }
    
    return 0;                                                                              /* success return 0 */
}

/**
 * @brief      get the alert pin active status
 * @param[in]  *handle points to an ads1115 handle structure
 * @param[out] *pin points to a pin alert active status buffer
 * @return     status code
 *             - 0 success
 *             - 1 get alert pin failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 * @note       none
 */
uint8_t ads1115_get_alert_pin(ads1115_handle_t *handle, ads1115_pin_t *pin)
{
    uint8_t res;
    uint16_t conf;
    
    if (handle == NULL)                                                                    /* check handle */
    {
        return 2;                                                                          /* return error */
    }
    if (handle->inited != 1)                                                               /* check handle initialization */
    {
        return 3;                                                                          /* return error */
    }
    
    res = a_ads1115_iic_multiple_read(handle, ADS1115_REG_CONFIG, (int16_t *)&conf);       /* read config */
    if (res != 0)                                                                          /* check error */
    {
        handle->debug_print("ads1115: read config failed.\n");                             /* read config failed */
        
        return 1;                                                                          /* return error */
    }
    *pin = (ads1115_pin_t)((conf >> 3) & 0x01);                                            /* get alert pin */
    
    return 0;                                                                              /* success return 0 */
}

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
uint8_t ads1115_set_compare_mode(ads1115_handle_t *handle, ads1115_compare_t compare)
{
    uint8_t res;
    uint16_t conf;
    
    if (handle == NULL)                                                                    /* check handle */
    {
        return 2;                                                                          /* return error */
    }
    if (handle->inited != 1)                                                               /* check handle initialization */
    {
        return 3;                                                                          /* return error */
    }
    
    res = a_ads1115_iic_multiple_read(handle, ADS1115_REG_CONFIG, (int16_t *)&conf);       /* read config */
    if (res != 0)                                                                          /* check error */
    {
        handle->debug_print("ads1115: read config failed.\n");                             /* read config failed */
        
        return 1;                                                                          /* return error */
    }
    conf &= ~(1 << 4);                                                                     /* clear compare mode */
    conf |= (compare & 0x01) << 4;                                                         /* set compare mode */
    res = a_ads1115_iic_multiple_write(handle, ADS1115_REG_CONFIG, conf);                  /* write config */
    if (res != 0)                                                                          /* check error */
    {
        handle->debug_print("ads1115: write config failed.\n");                            /* write config failed */
        
        return 1;                                                                          /* return error */
    }
    
    return 0;                                                                              /* success return 0 */
}

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
uint8_t ads1115_get_compare_mode(ads1115_handle_t *handle, ads1115_compare_t *compare)
{
    uint8_t res;
    uint16_t conf;
    
    if (handle == NULL)                                                                    /* check handle */
    {
        return 2;                                                                          /* return error */
    }
    if (handle->inited != 1)                                                               /* check handle initialization */
    {
        return 3;                                                                          /* return error */
    }
    
    res = a_ads1115_iic_multiple_read(handle, ADS1115_REG_CONFIG, (int16_t *)&conf);       /* read config */
    if (res != 0)                                                                          /* check error */
    {
        handle->debug_print("ads1115: read config failed.\n");                             /* read config failed */
        
        return 1;                                                                          /* return error */
    }
    *compare = (ads1115_compare_t)((conf >> 4) & 0x01);                                    /* get compare mode */
    
    return 0;                                                                              /* success return 0 */
}

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
uint8_t ads1115_set_rate(ads1115_handle_t *handle, ads1115_rate_t rate)
{
    uint8_t res;
    uint16_t conf;
    
    if (handle == NULL)                                                                    /* check handle */
    {
        return 2;                                                                          /* return error */
    }
    if (handle->inited != 1)                                                               /* check handle initialization */
    {
        return 3;                                                                          /* return error */
    }
    
    res = a_ads1115_iic_multiple_read(handle, ADS1115_REG_CONFIG, (int16_t *)&conf);       /* read config */
    if (res != 0)                                                                          /* check error */
    {
        handle->debug_print("ads1115: read config failed.\n");                             /* read config failed */
        
        return 1;                                                                          /* return error */
    }
    conf &= ~(0x07 << 5);                                                                  /* clear rate */
    conf |= (rate & 0x07) << 5;                                                            /* set rate */
    res = a_ads1115_iic_multiple_write(handle, ADS1115_REG_CONFIG, conf);                  /* write config */
    if (res != 0)                                                                          /* check error */
    {
        handle->debug_print("ads1115: write config failed.\n");                            /* write config failed */
        
        return 1;                                                                          /* return error */
    }
    
    return 0;                                                                              /* success return */
}

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
uint8_t ads1115_get_rate(ads1115_handle_t *handle, ads1115_rate_t *rate)
{
    uint8_t res;
    uint16_t conf;
    
    if (handle == NULL)                                                                    /* check handle */
    {
        return 2;                                                                          /* return error */
    }
    if (handle->inited != 1)                                                               /* check handle initialization */
    {
        return 3;                                                                          /* return error */
    }
    
    res = a_ads1115_iic_multiple_read(handle, ADS1115_REG_CONFIG, (int16_t *)&conf);       /* read config */
    if (res != 0)                                                                          /* check error */
    {
        handle->debug_print("ads1115: read config failed.\n");                             /* read config failed */
        
        return 1;                                                                          /* return error */
    }
    *rate = (ads1115_rate_t)((conf >> 5) & 0x07);                                          /* get rate */
    
    return 0;                                                                              /* success return 0 */
}

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
uint8_t ads1115_set_comparator_queue(ads1115_handle_t *handle, ads1115_comparator_queue_t comparator_queue)
{
    uint8_t res;
    uint16_t conf;
    
    if (handle == NULL)                                                                    /* check handle */
    {
        return 2;                                                                          /* return error */
    }
    if (handle->inited != 1)                                                               /* check handle initialization */
    {
        return 3;                                                                          /* return error */
    }
    
    res = a_ads1115_iic_multiple_read(handle, ADS1115_REG_CONFIG, (int16_t *)&conf);       /* read config */
    if (res != 0)                                                                          /* check error */
    {
        handle->debug_print("ads1115: read config failed.\n");                             /* read config failed */
        
        return 1;                                                                          /* return error */
    }
    conf &= ~(0x03 << 0);                                                                  /* clear comparator queue */
    conf |= (comparator_queue & 0x03) << 0;                                                /* set comparator queue */
    res = a_ads1115_iic_multiple_write(handle, ADS1115_REG_CONFIG, conf);                  /* write config */
    if (res != 0)                                                                          /* check error */
    {
        handle->debug_print("ads1115: write config failed.\n");                            /* write config failed */
        
        return 1;                                                                          /* return error */
    }
    
    return 0;                                                                              /* success return 0 */
}

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
uint8_t ads1115_get_comparator_queue(ads1115_handle_t *handle, ads1115_comparator_queue_t *comparator_queue)
{
    uint8_t res;
    uint16_t conf;
    
    if (handle == NULL)                                                                    /* check handle */
    {
        return 2;                                                                          /* return error */
    }
    if (handle->inited != 1)                                                               /* check handle initialization */
    {
        return 3;                                                                          /* return error */
    }
    
    res = a_ads1115_iic_multiple_read(handle, ADS1115_REG_CONFIG, (int16_t *)&conf);       /* read config */
    if (res != 0)                                                                          /* check error */
    {
        handle->debug_print("ads1115: read config failed.\n");                             /* read config failed */
        
        return 1;                                                                          /* return error */
    }
    *comparator_queue = (ads1115_comparator_queue_t)((conf >> 0) & 0x03);                  /* get comparator queue */
    
    return 0;                                                                              /* success return 0 */
}

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
uint8_t ads1115_set_addr_pin(ads1115_handle_t *handle, ads1115_address_t addr_pin)
{
    if (handle == NULL)                                                /* check handle */
    {
        return 2;                                                      /* return error */
    }
    
    if (addr_pin == ADS1115_ADDR_GND)                                  /* gnd */
    {
        handle->iic_addr = ADS1115_ADDRESS1;                           /* set address 1 */
    }
    else if (addr_pin == ADS1115_ADDR_VCC)                             /* vcc */
    {
        handle->iic_addr = ADS1115_ADDRESS2;                           /* set address 2 */
    }
    else if (addr_pin == ADS1115_ADDR_SDA)                             /* sda */
    {
        handle->iic_addr = ADS1115_ADDRESS3;                           /* set address 3 */
    }
    else if (addr_pin == ADS1115_ADDR_SCL)                             /* scl */
    {
        handle->iic_addr = ADS1115_ADDRESS4;                           /* set address 4 */
    }
    else
    {
        handle->debug_print("ads1115: addr_pin is invalid.\n");        /* addr_pin is invalid */
        
        return 1;                                                      /* return error */
    }

    return 0;                                                          /* success return 0 */
}

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
uint8_t ads1115_get_addr_pin(ads1115_handle_t *handle, ads1115_address_t *addr_pin)
{
    if (handle == NULL)                                                /* check handle */
    {
        return 2;                                                      /* return error */
    }
    
    if (handle->iic_addr == ADS1115_ADDRESS1)                          /* if address 1 */
    {
        *addr_pin = ADS1115_ADDR_GND;                                  /* set gnd */
    }
    else if (handle->iic_addr == ADS1115_ADDRESS2)                     /* if address 2 */
    {
        *addr_pin = ADS1115_ADDR_VCC;                                  /* set vcc */
    }
    else if (handle->iic_addr == ADS1115_ADDRESS3)                     /* if address 3 */
    {
        *addr_pin = ADS1115_ADDR_SDA;                                  /* set sda */
    }
    else if (handle->iic_addr == ADS1115_ADDRESS4)                     /* set address 4 */
    {
        *addr_pin = ADS1115_ADDR_SCL;                                  /* set scl */
    }
    else
    {
        handle->debug_print("ads1115: addr_pin is invalid.\n");        /* addr_pin is invalid */
        
        return 1;                                                      /* return error */
    }

    return 0;                                                          /* success return 0 */
}

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
uint8_t ads1115_single_read(ads1115_handle_t *handle, int16_t *raw, float *v)
{
    uint8_t res;
    uint8_t range;
    uint16_t conf;
    uint32_t timeout = 500;
    
    if (handle == NULL)                                                                        /* check handle */
    {
        return 2;                                                                              /* return error */
    }
    if (handle->inited != 1)                                                                   /* check handle initialization */
    {
        return 3;                                                                              /* return error */
    }
    
    res = a_ads1115_iic_multiple_read(handle, ADS1115_REG_CONFIG, (int16_t *)&conf);           /* read config */
    if (res != 0)                                                                              /* check error */
    {
        handle->debug_print("ads1115: read config failed.\n");                                 /* read config failed */
        
        return 1;                                                                              /* return error */
    }
    range = (ads1115_range_t)((conf >> 9) & 0x07);                                             /* get range conf */
    conf &= ~(1 << 8);                                                                         /* clear bit */
    conf |= 1 << 8;                                                                            /* set single read */
    conf |= 1 << 15;                                                                           /* start single read */
    res = a_ads1115_iic_multiple_write(handle, ADS1115_REG_CONFIG, conf);                      /* write config */
    if (res != 0)                                                                              /* check error */
    {
        handle->debug_print("ads1115: write config failed.\n");                                /* write config failed */
        
        return 1;                                                                              /* return error */
    }
    while (timeout != 0)                                                                       /* check timeout */
    {
        handle->delay_ms(8);                                                                   /* wait 8 ms */
        res = a_ads1115_iic_multiple_read(handle, ADS1115_REG_CONFIG, (int16_t *)&conf);       /* read config */
        if (res != 0)                                                                          /* check error */
        {
            handle->debug_print("ads1115: read config failed.\n");                             /* read config failed */
            
            return 1;                                                                          /* return error */
        }
        if ((conf & (1 << 15)) == (1 << 15))                                                   /* check finished */
        {
            break;                                                                             /* break */
        }
        timeout--;                                                                             /* timeout-- */
    }
    if (timeout == 0)                                                                          /* check timeout */
    {
        handle->debug_print("ads1115: read timeout.\n");                                       /* timeout */
        
        return 1;                                                                              /* return error */
    }
    res = a_ads1115_iic_multiple_read(handle, ADS1115_REG_CONVERT, raw);                       /* read data */
    if (res != 0)                                                                              /* check the result */
    {
        handle->debug_print("ads1115: continues read failed.\n");                              /* continues read failed */
        
        return 1;                                                                              /* return error */
    }
    if (range == ADS1115_RANGE_6P144V)                                                         /* if 6.144V */
    {
        *v = (float)(*raw) * 6.144f / 32768.0f;                                                /* get convert adc */
    }
    else if (range == ADS1115_RANGE_4P096V)                                                    /* if 4.096V */
    {
        *v = (float)(*raw) * 4.096f / 32768.0f;                                                /* get convert adc */
    }
    else if (range == ADS1115_RANGE_2P048V)                                                    /* if 2.048V */
    {
        *v = (float)(*raw) * 2.048f / 32768.0f;                                                /* get convert adc */
    }
    else if (range == ADS1115_RANGE_1P024V)                                                    /* if 1.024V */
    {
        *v = (float)(*raw) * 1.024f / 32768.0f;                                                /* get convert adc */
    }
    else if (range == ADS1115_RANGE_0P512V)                                                    /* if 0.512V */
    {
        *v = (float)(*raw) * 0.512f / 32768.0f;                                                /* get convert adc */
    }
    else if (range == ADS1115_RANGE_0P256V)                                                    /* if 0.256V */
    {
        *v = (float)(*raw) * 0.256f / 32768.0f;                                                /* get convert adc */
    }
    else
    {
        handle->debug_print("ads1115: range is invalid.\n");                                   /* range is invalid */
        
        return 1;                                                                              /* return error */
    }
    
    return 0;                                                                                  /* success return 0 */
}

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
uint8_t ads1115_continuous_read(ads1115_handle_t *handle,int16_t *raw, float *v)
{
    uint8_t res;
    uint8_t range;
    uint16_t conf;
    
    if (handle == NULL)                                                                    /* check handle */
    {
        return 2;                                                                          /* return error */
    }
    if (handle->inited != 1)                                                               /* check handle initialization */
    {
        return 3;                                                                          /* return error */
    }
    
    res = a_ads1115_iic_multiple_read(handle, ADS1115_REG_CONFIG, (int16_t *)&conf);       /* read config */
    if (res != 0)                                                                          /* check error */
    {
        handle->debug_print("ads1115: read config failed.\n");                             /* read config failed */
        
        return 1;                                                                          /* return error */
    }
    range = (ads1115_range_t)((conf >> 9) & 0x07);                                         /* get range conf */
    res = a_ads1115_iic_multiple_read(handle, ADS1115_REG_CONVERT, raw);                   /* read data */
    if (res != 0)                                                                          /* check error */
    {
        handle->debug_print("ads1115: continuous read failed.\n");                         /* continuous read failed */
        
        return 1;                                                                          /* return error */
    }
    if (range == ADS1115_RANGE_6P144V)                                                     /* if 6.144V */
    {
        *v = (float)(*raw) * 6.144f / 32768.0f;                                            /* get convert adc */
    }
    else if (range == ADS1115_RANGE_4P096V)                                                /* if 4.096V */
    {
        *v = (float)(*raw) * 4.096f / 32768.0f;                                            /* get convert adc */
    }
    else if (range == ADS1115_RANGE_2P048V)                                                /* if 2.048V */
    {
        *v = (float)(*raw) * 2.048f / 32768.0f;                                            /* get convert adc */
    }
    else if (range == ADS1115_RANGE_1P024V)                                                /* if 1.024V */
    {
        *v = (float)(*raw) * 1.024f / 32768.0f;                                            /* get convert adc */
    }
    else if (range == ADS1115_RANGE_0P512V)                                                /* if 0.512V */
    {
        *v = (float)(*raw) * 0.512f / 32768.0f;                                            /* get convert adc */
    }
    else if (range == ADS1115_RANGE_0P256V)                                                /* if 0.256V */
    {
        *v = (float)(*raw) * 0.256f / 32768.0f;                                            /* get convert adc */
    }
    else
    {
        handle->debug_print("ads1115: range is invalid.\n");                               /* range is invalid */
        
        return 1;                                                                          /* return error */
    }
    
    return 0;                                                                              /* success return 0 */
}

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
uint8_t ads1115_start_continuous_read(ads1115_handle_t *handle)
{
    uint8_t res;
    uint16_t conf;
    
    if (handle == NULL)                                                                    /* check handle */
    {
        return 2;                                                                          /* return error */
    }
    if (handle->inited != 1)                                                               /* check handle initialization */
    {
        return 3;                                                                          /* return error */
    }
    
    res = a_ads1115_iic_multiple_read(handle, ADS1115_REG_CONFIG, (int16_t *)&conf);       /* read config */
    if (res != 0)                                                                          /* check error */
    {
        handle->debug_print("ads1115: read config failed.\n");                             /* read config failed */
        
        return 1;                                                                          /* return error */
    }
    conf &= ~(0x01 << 8);                                                                  /* set start continuous read */
    res = a_ads1115_iic_multiple_write(handle, ADS1115_REG_CONFIG, conf);                  /* write config */
    if (res != 0)                                                                          /* check error */
    {
        handle->debug_print("ads1115: write config failed.\n");                            /* write config failed */
        
        return 1;                                                                          /* return error */
    }
    
    return 0;                                                                              /* success return 0 */
}

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
uint8_t ads1115_stop_continuous_read(ads1115_handle_t *handle)
{
    uint8_t res;
    uint16_t conf;
    
    if (handle == NULL)                                                                    /* check handle */
    {
        return 2;                                                                          /* return error */
    }
    if (handle->inited != 1)                                                               /* check handle initialization */
    {
        return 3;                                                                          /* return error */
    }
    
    res = a_ads1115_iic_multiple_read(handle, ADS1115_REG_CONFIG, (int16_t *)&conf);       /* read config */
    if (res != 0)                                                                          /* check error */
    {
        handle->debug_print("ads1115: read config failed.\n");                             /* read config failed */
        
        return 1;                                                                          /* return error */
    }
    conf &= ~(0x01 << 8);                                                                  /* clear bit */
    conf |= 1 << 8;                                                                        /* set stop continues read */
    res = a_ads1115_iic_multiple_write(handle, ADS1115_REG_CONFIG, conf);                  /* write config */
    if (res != 0)                                                                          /* check error */
    {
        handle->debug_print("ads1115: write config failed.\n");                            /* write config failed */
        
        return 1;                                                                          /* return error */
    }
    
    return 0;                                                                              /* success return 0 */
}

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
uint8_t ads1115_set_compare(ads1115_handle_t *handle, ads1115_bool_t enable)
{
    uint8_t res;
    uint16_t conf;
    
    if (handle == NULL)                                                                    /* check handle */
    {
        return 2;                                                                          /* return error */
    }
    if (handle->inited != 1)                                                               /* check handle initialization */
    {
        return 3;                                                                          /* return error */
    }
    
    res = a_ads1115_iic_multiple_read(handle, ADS1115_REG_CONFIG, (int16_t *)&conf);       /* read config */
    if (res != 0)                                                                          /* check error */
    {
        handle->debug_print("ads1115: read config failed.\n");                             /* read config failed */
        
        return 1;                                                                          /* return error */
    }
    conf &= ~(0x01 << 2);                                                                  /* clear compare */
    conf |= enable << 2;                                                                   /* set compare */
    res = a_ads1115_iic_multiple_write(handle, ADS1115_REG_CONFIG, conf);                  /* write config */
    if (res != 0)                                                                          /* check error */
    {
        handle->debug_print("ads1115: write config failed.\n");                            /* write config failed */
        
        return 1;                                                                          /* return error */
    }
    
    return 0;                                                                              /* success return 0 */
}

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
uint8_t ads1115_get_compare(ads1115_handle_t *handle, ads1115_bool_t *enable)
{
    uint8_t res;
    uint16_t conf;
    
    if (handle == NULL)                                                                    /* check handle */
    {
        return 2;                                                                          /* return error */
    }
    if (handle->inited != 1)                                                               /* check handle initialization */
    {
        return 3;                                                                          /* return error */
    }
    
    res = a_ads1115_iic_multiple_read(handle, ADS1115_REG_CONFIG, (int16_t *)&conf);       /* read config */
    if (res != 0)                                                                          /* check error */
    {
        handle->debug_print("ads1115: read config failed.\n");                             /* read config failed */
        
        return 1;                                                                          /* return error */
    }
    *enable = (ads1115_bool_t)((conf >> 2) & 0x01);                                        /* get compare */
    
    return 0;                                                                              /* success return 0 */
}

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
uint8_t ads1115_set_compare_threshold(ads1115_handle_t *handle, int16_t high_threshold, int16_t low_threshold)
{
    if (handle == NULL)                                                                   /* check handle */
    {
        return 2;                                                                         /* return error */
    }
    if (handle->inited != 1)                                                              /* check handle initialization */
    {
        return 3;                                                                         /* return error */
    }
    
    if (a_ads1115_iic_multiple_write(handle, ADS1115_REG_HIGHRESH, high_threshold) != 0)  /* write high threshold */
    {
        handle->debug_print("ads1115: write high threshold failed.\n");                   /* write high threshold failed */
        
        return 1;                                                                         /* return error */
    }
    if (a_ads1115_iic_multiple_write(handle, ADS1115_REG_LOWRESH, low_threshold) != 0)    /* write low threshold */
    {
        handle->debug_print("ads1115: write low threshold failed.\n");                    /* write low threshold failed */
        
        return 1;                                                                         /* return error */
    }
  
    return 0;                                                                             /* success return 0 */
}

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
uint8_t ads1115_get_compare_threshold(ads1115_handle_t *handle, int16_t *high_threshold, int16_t *low_threshold)
{
    if (handle == NULL)                                                                  /* check handle */
    {
        return 2;                                                                        /* return error */
    }
    if (handle->inited != 1)                                                             /* check handle initialization */
    {
        return 3;                                                                        /* return error */
    }

    if (a_ads1115_iic_multiple_read(handle, ADS1115_REG_HIGHRESH, high_threshold) != 0)  /* read high threshold */
    {
        handle->debug_print("ads1115: read high threshold failed.\n");                   /* read high threshold failed */
        
        return 1;                                                                        /* return error */
    }
    if (a_ads1115_iic_multiple_read(handle, ADS1115_REG_LOWRESH, low_threshold) != 0)    /* read low threshold */
    {
        handle->debug_print("ads1115: read low threshold failed.\n");                    /* read low threshold failed */
        
        return 1;                                                                        /* return error */
    }
    
    return 0;                                                                            /* success return 0 */
}

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
uint8_t ads1115_convert_to_register(ads1115_handle_t *handle, float s, int16_t *reg)
{  
    uint8_t res;
    uint8_t range;
    uint16_t conf;
    
    if (handle == NULL)                                                                    /* check handle */
    {
        return 2;                                                                          /* return error */
    }
    if (handle->inited != 1)                                                               /* check handle initialization */
    {
        return 3;                                                                          /* return error */
    }
    
    res = a_ads1115_iic_multiple_read(handle, ADS1115_REG_CONFIG, (int16_t *)&conf);       /* read config */
    if (res != 0)                                                                          /* check error */
    {
        handle->debug_print("ads1115: read config failed.\n");                             /* read config failed */
        
        return 1;                                                                          /* return error */
    }
    range = (ads1115_range_t)((conf >> 9) & 0x07);                                         /* get range conf */
    if (range == ADS1115_RANGE_6P144V)                                                     /* if 6.144V */
    {
        *reg = (int16_t)(s * 32768.0f / 6.144f);                                           /* convert to reg */
    }
    else if (range == ADS1115_RANGE_4P096V)                                                /* if 4.096V */
    {
        *reg = (int16_t)(s * 32768.0f / 4.096f);                                           /* convert to reg */
    }
    else if (range == ADS1115_RANGE_2P048V)                                                /* if 2.048V */
    {
        *reg = (int16_t)(s * 32768.0f / 2.048f);                                           /* convert to reg */
    }
    else if (range == ADS1115_RANGE_1P024V)                                                /* if 1.024V */
    {
        *reg = (int16_t)(s * 32768.0f / 1.024f);                                           /* convert to reg */
    }
    else if (range == ADS1115_RANGE_0P512V)                                                /* if 0.512V */
    {
        *reg = (int16_t)(s * 32768.0f / 0.512f);                                           /* convert to reg */
    }
    else if (range == ADS1115_RANGE_0P256V)                                                /* if 0.256V */
    {
        *reg = (int16_t)(s * 32768.0f / 0.256f);                                           /* convert to reg */
    }
    else
    {
        handle->debug_print("ads1115: range is invalid.\n");                               /* range is invalid */
        
        return 1;                                                                          /* return error */
    }
    
    return 0;                                                                              /* success return 0 */
}

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
uint8_t ads1115_convert_to_data(ads1115_handle_t *handle, int16_t reg, float *s)
{
    uint8_t res;
    uint8_t range;
    uint16_t conf;
    
    if (handle == NULL)                                                                    /* check handle */
    {
        return 2;                                                                          /* return error */
    }
    if (handle->inited != 1)                                                               /* check handle initialization */
    {
        return 3;                                                                          /* return error */
    }
    
    res = a_ads1115_iic_multiple_read(handle, ADS1115_REG_CONFIG, (int16_t *)&conf);       /* read config */
    if (res != 0)                                                                          /* check error */
    {
        handle->debug_print("ads1115: read config failed.\n");                             /* read config failed */
        
        return 1;                                                                          /* return error */
    }
    range = (ads1115_range_t)((conf >> 9) & 0x07);                                         /* get range conf */
    if (range == ADS1115_RANGE_6P144V)                                                     /* if 6.144V */
    {
        *s = (float)(reg) * 6.144f / 32768.0f;                                             /* convert to data */
    }
    else if (range == ADS1115_RANGE_4P096V)                                                /* if 4.096V */
    {
        *s = (float)(reg) * 4.096f / 32768.0f;                                             /* convert to data */
    }
    else if (range == ADS1115_RANGE_2P048V)                                                /* if 2.048V */
    {
        *s = (float)(reg) * 2.048f / 32768.0f;                                             /* convert to data */
    }
    else if (range == ADS1115_RANGE_1P024V)                                                /* if 1.024V */
    {
        *s = (float)(reg) * 1.024f / 32768.0f;                                             /* convert to data */
    }
    else if (range == ADS1115_RANGE_0P512V)                                                /* if 0.512V */
    {
        *s = (float)(reg) * 0.512f / 32768.0f;                                             /* convert to data */
    }
    else if (range == ADS1115_RANGE_0P256V)                                                /* if 0.256V */
    {
        *s = (float)(reg) * 0.256f / 32768.0f;                                             /* convert to data */
    }
    else
    {
        handle->debug_print("ads1115: range is invalid.\n");                               /* range is invalid */
        
        return 1;                                                                          /* return error */
    }
    
    return 0;                                                                              /* success return 0 */
}

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
uint8_t ads1115_set_reg(ads1115_handle_t *handle, uint8_t reg, int16_t value)
{
    if (handle == NULL)                                            /* check handle */
    {
        return 2;                                                  /* return error */
    }
    if (handle->inited != 1)                                       /* check handle initialization */
    {
        return 3;                                                  /* return error */
    }
    
    return a_ads1115_iic_multiple_write(handle, reg, value);       /* write reg */
}

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
uint8_t ads1115_get_reg(ads1115_handle_t *handle, uint8_t reg, int16_t *value)
{
    if (handle == NULL)                                           /* check handle */
    {
        return 2;                                                 /* return error */
    }
    if (handle->inited != 1)                                      /* check handle initialization */
    {
        return 3;                                                 /* return error */
    }
    
    return a_ads1115_iic_multiple_read(handle, reg, value);       /* read reg */
}

/**
 * @brief      get chip's information
 * @param[out] *info points to an ads1115 info structure
 * @return     status code
 *             - 0 success
 *             - 2 handle is NULL
 * @note       none
 */
uint8_t ads1115_info(ads1115_info_t *info)
{
    if (info == NULL)                                               /* check handle */
    {
        return 2;                                                   /* return error */
    }
    
    memset(info, 0, sizeof(ads1115_info_t));                        /* initialize ads1115 info structure */
    strncpy(info->chip_name, CHIP_NAME, 32);                        /* copy chip name */
    strncpy(info->manufacturer_name, MANUFACTURER_NAME, 32);        /* copy manufacturer name */
    strncpy(info->interface, "IIC", 8);                             /* copy interface name */
    info->supply_voltage_min_v = SUPPLY_VOLTAGE_MIN;                /* set minimal supply voltage */
    info->supply_voltage_max_v = SUPPLY_VOLTAGE_MAX;                /* set maximum supply voltage */
    info->max_current_ma = MAX_CURRENT;                             /* set maximum current */
    info->temperature_max = TEMPERATURE_MAX;                        /* set minimal temperature */
    info->temperature_min = TEMPERATURE_MIN;                        /* set maximum temperature */
    info->driver_version = DRIVER_VERSION;                          /* set driver version */
    
    return 0;                                                       /* success return 0 */
}
