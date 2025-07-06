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
 * @file      driver_pcf8591.c
 * @brief     driver pcf8591 source file
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

#include "driver_pcf8591.h"

/**
 * @brief chip information definition
 */
#define CHIP_NAME                 "NXP PCF8591"        /**< chip name */
#define MANUFACTURER_NAME         "NXP"                /**< manufacturer name */
#define SUPPLY_VOLTAGE_MIN        2.5f                 /**< chip min supply voltage */
#define SUPPLY_VOLTAGE_MAX        6.0f                 /**< chip max supply voltage */
#define MAX_CURRENT               50.0f                /**< chip max current */
#define TEMPERATURE_MIN           -40.0f               /**< chip min operating temperature */
#define TEMPERATURE_MAX           85.0f                /**< chip max operating temperature */
#define DRIVER_VERSION            2000                 /**< driver version */

/**
 * @brief     set the address pin
 * @param[in] *handle points to a pcf8591 handle structure
 * @param[in] addr_pin is the chip address pins
 * @return    status code
 *            - 0 success
 *            - 2 handle is NULL
 * @note      none
 */
uint8_t pcf8591_set_addr_pin(pcf8591_handle_t *handle, pcf8591_address_t addr_pin)
{
    if (handle == NULL)                       /* check handle */
    {
        return 2;                             /* return error */
    }

    handle->iic_addr = 0x90;                  /* set iic addr */
    handle->iic_addr |= addr_pin << 1;        /* set iic address */
    
    return 0;                                 /* success return 0 */
}

/**
 * @brief      get the address pin
 * @param[in]  *handle points to a pcf8591 handle structure
 * @param[out] *addr_pin points to a chip address pins buffer
 * @return     status code
 *             - 0 success
 *             - 2 handle is NULL
 * @note       none
 */
uint8_t pcf8591_get_addr_pin(pcf8591_handle_t *handle, pcf8591_address_t *addr_pin)
{
    if (handle == NULL)                                                        /* check handle */
    {
        return 2;                                                              /* return error */
    }

    *addr_pin = (pcf8591_address_t)((handle->iic_addr & (~0x90)) >> 1);        /*get iic address */
    
    return 0;                                                                  /* success return 0 */
}

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
uint8_t pcf8591_set_channel(pcf8591_handle_t *handle, pcf8591_channel_t channel)
{
    uint8_t res;
    uint8_t prev_conf;
    
    if (handle == NULL)                                                                /* check handle */
    {
        return 2;                                                                      /* return error */
    }
    if (handle->inited != 1)                                                           /* check handle initialization */
    {
        return 3;                                                                      /* return error */
    }
    
    prev_conf = handle->conf;                                                          /* save conf */
    handle->conf &= ~(3 << 0);                                                         /* clear channel bits */
    handle->conf |= channel << 0;                                                      /* set channel */
    res = handle->iic_write_cmd(handle->iic_addr, (uint8_t *)&handle->conf, 1);        /* write command */
    if (res != 0)                                                                      /* check error */
    {
        handle->debug_print("pcf8591: write command failed.\n");                       /* write command failed */
        handle->conf = prev_conf;                                                      /* recover conf */
        
        return 1;                                                                      /* return error */
    }
    
    return 0;                                                                          /* success return 0 */
}

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
uint8_t pcf8591_get_channel(pcf8591_handle_t *handle, pcf8591_channel_t *channel)
{
    if (handle == NULL)                                             /* check handle */
    {
        return 2;                                                   /* return error */
    }
    if (handle->inited != 1)                                        /* check handle initialization */
    {
        return 3;                                                   /* return error */
    }
    
    *channel = (pcf8591_channel_t)(handle->conf & (3 << 0));        /* get channel */
    
    return 0;                                                       /* success return 0 */
}

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
uint8_t pcf8591_set_mode(pcf8591_handle_t *handle, pcf8591_mode_t mode)
{
    uint8_t res;
    uint8_t prev_conf;
    
    if (handle == NULL)                                                                /* check handle */
    {
        return 2;                                                                      /* return error */
    }
    if (handle->inited != 1)                                                           /* check handle initialization */
    {
        return 3;                                                                      /* return error */
    }
    
    prev_conf = handle->conf;                                                          /* save conf */
    handle->conf &= ~(3 << 4);                                                         /* clear mode bits */
    handle->conf |= mode << 4;                                                         /* set mode */
    res = handle->iic_write_cmd(handle->iic_addr, (uint8_t *)&handle->conf, 1);        /* write command */
    if (res != 0)                                                                      /* check error */
    {
        handle->debug_print("pcf8591: write command failed.\n");                       /* write command failed */
        handle->conf = prev_conf;                                                      /* recover conf */
        
        return 1;                                                                      /* return error */
    }
    
    return 0;                                                                          /* success return 0 */
}

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
uint8_t pcf8591_get_mode(pcf8591_handle_t *handle, pcf8591_mode_t *mode)
{
    if (handle == NULL)                                           /* check handle */
    {
        return 2;                                                 /* return error */
    }
    if (handle->inited != 1)                                      /* check handle initialization */
    {
        return 3;                                                 /* return error */
    }
    
    *mode =  (pcf8591_mode_t)((handle->conf >> 4) & 0x03);        /* get mode */
    
    return 0;                                                     /* success return 0 */
}

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
uint8_t pcf8591_set_auto_increment(pcf8591_handle_t *handle, pcf8591_bool_t enable)
{
    uint8_t res;
    uint8_t prev_conf;
    
    if (handle == NULL)                                                                /* check handle */
    {
        return 2;                                                                      /* return error */
    }
    if (handle->inited != 1)                                                           /* check handle initialization */
    {
        return 3;                                                                      /* return error */
    }
    
    prev_conf = handle->conf;                                                          /* save conf */
    handle->conf &= ~(1 << 2);                                                         /* clear auto bit */
    handle->conf |= enable << 2;                                                       /* set enable */
    if (enable != 0)                                                                   /* if enable auto increment */
    {
        handle->conf |= 0x40;                                                          /* set output bit */
    }
    else
    {
        handle->conf &= ~0x40;                                                         /* clear output bit */
    }
    res = handle->iic_write_cmd(handle->iic_addr, (uint8_t *)&handle->conf, 1);        /* write command */
    if (res != 0)                                                                      /* check error */
    {
        handle->debug_print("pcf8591: write command failed.\n");                       /* write command failed */
        handle->conf = prev_conf;                                                      /* recover conf */
        
        return 1;                                                                      /* return error */
    }
    
    return 0;                                                                          /* success return 0 */
}

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
uint8_t pcf8591_get_auto_increment(pcf8591_handle_t *handle, pcf8591_bool_t *enable)
{
    if (handle == NULL)                                            /* check handle */
    {
        return 2;                                                  /* return error */
    }
    if (handle->inited != 1)                                       /* check handle initialization */
    {
        return 3;                                                  /* return error */
    }
    
    *enable = (pcf8591_bool_t)((handle->conf >> 2) & 0x01);        /* get auto */
    
    return 0;                                                      /* success return 0 */
}

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
uint8_t pcf8591_set_reference_voltage(pcf8591_handle_t *handle, float ref_voltage)
{
    if (handle == NULL)                       /* check handle */
    {
        return 2;                             /* return error */
    }
    if (handle->inited != 1)                  /* check handle initialization */
    {
        return 3;                             /* return error */
    }
    
    handle->ref_voltage = ref_voltage;        /* set reference voltage */
    
    return 0;                                 /* success return 0 */
}

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
uint8_t pcf8591_get_reference_voltage(pcf8591_handle_t *handle, float *ref_voltage)
{
    if (handle == NULL)                                 /* check handle */
    {
        return 2;                                       /* return error */
    }
    if (handle->inited != 1)                            /* check handle initialization */
    {
        return 3;                                       /* return error */
    }
    
    *ref_voltage = (float)(handle->ref_voltage);        /* get ref voltage */
    
    return 0;                                           /* success return 0 */
}

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
uint8_t pcf8591_init(pcf8591_handle_t *handle)
{
    if (handle == NULL)                                                  /* check handle */
    {
        return 2;                                                        /* return error */
    }
    if (handle->debug_print == NULL)                                     /* check debug_print */
    {
        return 3;                                                        /* return error */
    }
    if (handle->iic_init == NULL)                                        /* check iic_init */
    {
        handle->debug_print("pcf8591: iic_init is null.\n");             /* iic_init is null */
       
        return 3;                                                        /* return error */
    }
    if (handle->iic_deinit == NULL)                                      /* check iic_deinit */
    {
        handle->debug_print("pcf8591: iic_deinit is null.\n");           /* iic_deinit is null */
       
        return 3;                                                        /* return error */
    }
    if (handle->iic_read_cmd == NULL)                                    /* check iic_read_cmd */
    {
        handle->debug_print("pcf8591: iic_read_cmd is null.\n");         /* iic_read_cmd is null */
       
        return 3;                                                        /* return error */
    }
    if (handle->iic_write_cmd == NULL)                                   /* check iic_write_cmd */
    {
        handle->debug_print("pcf8591: iic_write_cmd is null.\n");        /* iic_write_cmd is null */
       
        return 3;                                                        /* return error */
    }
    if (handle->delay_ms == NULL)                                        /* check delay_ms */
    {
        handle->debug_print("pcf8591: delay_ms is null.\n");             /* delay_ms is null */
       
        return 3;                                                        /* return error */
    }
    
    if (handle->iic_init() != 0)                                         /* iic init */
    {
        handle->debug_print("pcf8591: iic init failed.\n");              /* iic init failed */
       
        return 1;                                                        /* return error */
    }
    handle->inited = 1;                                                  /* flag finish initialization */
    
    return 0;                                                            /* success return 0 */
}

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
uint8_t pcf8591_deinit(pcf8591_handle_t *handle)
{
    uint8_t res;
    uint8_t buf[2];
    
    if (handle == NULL)                                                      /* check handle */
    {
        return 2;                                                            /* return error */
    }
    if (handle->inited != 1)                                                 /* check handle initialization */
    {
        return 3;                                                            /* return error */
    }
    
    buf[0] = 0x00;                                                           /* set conf */
    buf[1] = 0x00;                                                           /* set 0x00 */
    res = handle->iic_write_cmd(handle->iic_addr, (uint8_t *)buf, 2);        /* write data */
    if (res != 0)                                                            /* check error */
    {
        handle->debug_print("pcf8591: write command failed.\n");             /* write failed */
       
        return 1;                                                            /* return error */
    }
    res = handle->iic_deinit();                                              /* iic deinit */
    if (res != 0)                                                            /* check error */
    {
        handle->debug_print("pcf8591: iic deinit failed.\n");                /* iic deinit failed */
       
        return 1;                                                            /* return error */
    }
    handle->inited = 0;                                                      /* flag closed */
    
    return 0;                                                                /* success return 0 */
}

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
uint8_t pcf8591_write(pcf8591_handle_t *handle, uint8_t data)
{
    uint8_t res;
    uint8_t buf[2];
    uint8_t conf;
    
    if (handle == NULL)                                                      /* check handle */
    {
        return 2;                                                            /* return error */
    }
    if (handle->inited != 1)                                                 /* check handle initialization */
    {
        return 3;                                                            /* return error */
    }
    
    conf = handle->conf | 0x40;                                              /* set output */
    buf[0] = conf;                                                           /* set conf */
    buf[1] = data;                                                           /* set data */
    res = handle->iic_write_cmd(handle->iic_addr, (uint8_t *)buf, 2);        /* write data */
    if (res != 0)                                                            /* check error */
    {
        handle->debug_print("pcf8591: write command failed.\n");             /* write failed */
       
        return 1;                                                            /* return error */
    }
    
    return 0;                                                                /* success return 0 */
}

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
uint8_t pcf8591_dac_convert_to_register(pcf8591_handle_t *handle, float dac, uint8_t *reg)
{
    if (handle == NULL)                                          /* check handle */
    {
        return 2;                                                /* return error */
    }
    if (handle->inited != 1)                                     /* check handle initialization */
    {
        return 3;                                                /* return error */
    }
    
    *reg = (uint8_t)(dac / handle->ref_voltage * 256.0f);        /* convert to register */
    
    return 0;                                                    /* success return 0 */
}

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
uint8_t pcf8591_read(pcf8591_handle_t *handle, int16_t *raw, float *adc)
{
    uint8_t res;
    uint8_t mode;
    uint8_t channel;
    uint8_t u_data;
    int8_t  s_data;
    uint8_t buf[1];
    
    if (handle == NULL)                                                       /* check handle */
    {
        return 2;                                                             /* return error */
    }
    if (handle->inited != 1)                                                  /* check handle initialization */
    {
        return 3;                                                             /* return error */
    }
    
    if ((handle->conf & (1 << 2)) != 0)                                       /* check mode */
    {
        handle->debug_print("pcf8591: can't use this function.\n");           /* channel is invalid */
        
        return 1;
    }
    mode = (handle->conf >> 4) & 0x03;                                        /* check mode */
    channel = handle->conf & 0x03;                                            /* check channel */
    if ((mode == 1) || (mode == 2))                                           /* if mode 1 mode 2 */
    {
        if (channel > 2)                                                      /* check channel */
        {
            handle->debug_print("pcf8591: channel is invalid.\n");            /* channel is invalid */
            
            return 1;                                                         /* return error */
        }
    }
    if (mode == 3)                                                            /* if mode 3 */
    {
        if (channel > 1)                                                      /* check channel */
        {
            handle->debug_print("pcf8591: channel is invalid.\n");            /* channel is invalid */
            
            return 1;                                                         /* return error */
        }
    }
    res = handle->iic_read_cmd(handle->iic_addr, (uint8_t *)buf, 1);          /* read data */
    if (res != 0)                                                             /* check error */
    {
        handle->debug_print("pcf8591: read command failed.\n");               /* read failed */
       
        return 1;                                                             /* return error */
    }
    switch (mode)                                                             /* mode param */
    {
        case 0 :                                                              /* mode 0 */
        {
            if (channel == 0)                                                 /* channel 0 */
            {
                u_data = buf[0];                                              /* get data */
                *raw = (int16_t)(u_data);                                     /* get raw */
                *adc = (float)(*raw) / 256.0f * handle->ref_voltage;          /* convert to read data */
                res = 0;                                                      /* successful */
            }
            else if (channel == 1)                                            /* channel 1 */
            {
                u_data = buf[0];                                              /* get data */
                *raw = (int16_t)(u_data);                                     /* get raw */
                *adc = (float)(*raw) / 256.0f * handle->ref_voltage;          /* convert to read data */
                res = 0;                                                      /* successful */
            }
            else if (channel == 2)                                            /* channel 2 */
            {
                u_data = buf[0];                                              /* get data */
                *raw = (int16_t)(u_data);                                     /* get raw */
                *adc = (float)(*raw) / 256.0f * handle->ref_voltage;          /* convert to read data */
                res = 0;                                                      /* successful */
            }
            else                                                              /* channel 3 */
            {
                u_data = buf[0];                                              /* get data */
                *raw = (int16_t)(u_data);                                     /* get raw */
                *adc = (float)(*raw) / 256.0f * handle->ref_voltage;          /* convert to read data */
                res = 0;                                                      /* successful */
            }
            
            break;                                                            /* break */
        }
        case 1 :                                                              /* mode 1 */
        {
            if (channel == 0)                                                 /* channel 0 */
            {
                s_data = (int8_t)(buf[0]);                                    /* get data */
                *raw = (int16_t)(s_data);                                     /* get raw */
                *adc = (float)(*raw) / 256.0f * handle->ref_voltage;          /* convert to read data */
                res = 0;                                                      /* successful */
            }
            else if (channel == 1)                                            /* channel 1 */
            {
                s_data = (int8_t)(buf[0]);                                    /* get data */
                *raw = (int16_t)(s_data);                                     /* get raw */
                *adc = (float)(*raw) / 256.0f * handle->ref_voltage;          /* convert to read data */
                res = 0;                                                      /* successful */
            }
            else if (channel == 2)                                            /* channel */
            {
                s_data = (int8_t)(buf[0]);                                    /* get data */
                *raw = (int16_t)(s_data);                                     /* get raw */
                *adc = (float)(*raw) / 256.0f * handle->ref_voltage;          /* convert to read data */
                res = 0;                                                      /* successful */
            }
            else
            {
                handle->debug_print("pcf8591: channel is invalid.\n");        /* channel is invalid */
                res = 1;                                                      /* failed */
            }
            
            break;                                                            /* break */
        }
        case 2 :                                                              /* mode 2 */
        {
            if (channel == 0)                                                 /* channel 0 */
            {
                u_data = buf[0];                                              /* get data */
                *raw = (int16_t)(u_data);                                     /* get raw */
                *adc = (float)(*raw) / 256.0f * handle->ref_voltage;          /* convert to read data */
                res = 0;                                                      /* successful */
            }
            else if (channel == 1)                                            /* channel 1 */
            {
                u_data = buf[0];                                              /* get data */
                *raw = (int16_t)(u_data);                                     /* get raw */
                *adc = (float)(*raw) / 256.0f * handle->ref_voltage;          /* convert to read data */
                res = 0;                                                      /* successful */
            }
            else if (channel == 2)                                            /* channel 2 */
            {
                s_data = (int8_t)(buf[0]);                                    /* get data  */
                *raw = (int16_t)(s_data);                                     /* get raw */
                *adc = (float)(*raw) / 256.0f * handle->ref_voltage;          /* convert to read data */
                res = 0;                                                      /* successful */
            }
            else
            {
                handle->debug_print("pcf8591: channel is invalid.\n");        /* channel is invalid */
                res = 1;                                                      /* failed */
            }
            
            break;                                                            /* break */
        }
        case 3 :                                                              /* mode 3 */
        {
            if (channel == 0)                                                 /* channel 0 */
            {
                s_data = (int8_t)(buf[0]);                                    /* get data */
                *raw = (int16_t)(s_data);                                     /* get raw */
                *adc = (float)(*raw) / 256.0f * handle->ref_voltage;          /* convert to read data */
                res = 0;                                                      /* successful */
            }
            else if (channel == 1)                                            /* channel 1 */
            {
                s_data = (int8_t)(buf[0]);                                    /* get data */
                *raw = (int16_t)(s_data);                                     /* get raw */
                *adc = (float)(*raw) / 256.0f * handle->ref_voltage;          /* convert to read data */
                res = 0;                                                      /* successful */
            }
            else
            {
                handle->debug_print("pcf8591: channel is invalid.\n");        /* channel is invalid */
                res = 1;                                                      /* failed */
            }
            
            break;                                                            /* break */
        }
        default :
        {
            handle->debug_print("pcf8591: mode is invalid.\n");               /* mode is invalid */
            res = 1;                                                          /* failed */
            
            break;                                                            /* break */
        }
    }
    
    return res;                                                               /* return the result */
}

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
uint8_t pcf8591_multiple_read(pcf8591_handle_t *handle, int16_t *raw, float *adc, uint8_t *len)
{
    uint8_t res;
    uint8_t mode;
    uint8_t i, j;
    uint8_t u_data;
    int8_t  s_data;
    uint8_t buf[4];
    
    if (handle == NULL)                                                       /* check handle */
    {
        return 2;                                                             /* return error */
    }
    if (handle->inited != 1)                                                  /* check handle initialization */
    {
        return 3;                                                             /* return error */
    }
    
    if ((handle->conf & (1 << 2)) == 0)                                       /* check mode */
    {
        handle->debug_print("pcf8591: can't use this function.\n");           /* channel is invalid */
        
        return 1;                                                             /* return error */
    }
    mode = (handle->conf >> 4) & 0x03;                                        /* check mode */
    if (mode == 0)                                                            /* mode 0 */
    {
        i = 4;                                                                /* 4 */
        i = i<(*len) ? i : (*len);                                            /* get min length */
    }
    else if (mode == 1)                                                       /* mode 1 */
    {
        i = 3;                                                                /* 3 */
        i = i<(*len) ? i : (*len);                                            /* get min length */
    }
    else if (mode == 2)                                                       /* mode 2 */
    {
        i = 3;                                                                /* 3 */
        i = i<(*len) ? i : (*len);
    }                                                                         /* get min length */
    else                                                                      /* mode 3 */
    {
        i = 2;                                                                /* 2 */
        i = i<(*len) ? i : (*len);                                            /* get min length */
    }
    
    memset(buf, 0, sizeof(uint8_t) * 4);                                      /* clear the buffer */
    for (j = 0; j < i; j++)
    {
        res = handle->iic_read_cmd(handle->iic_addr, (uint8_t *)&buf[j], 1);  /* read data */
        if (res != 0)                                                         /* check error */
        {
            handle->debug_print("pcf8591: read command failed.\n");           /* read failed */
           
            return 1;                                                         /* return error */
        }
        handle->delay_ms(5);                                                  /* delay 5 ms */
    }
    switch (mode)                                                             /* mode param */
    {
        case 0 :                                                              /* mode 0 */
        {
            u_data = buf[1];                                                  /* get data */
            *raw = (int16_t)(u_data);                                         /* get raw */
            *adc = (float)(*raw) / 256.0f * handle->ref_voltage;              /* convert to read data */
            raw++;                                                            /* raw address add */
            adc++;                                                            /* adc address add */
            u_data = buf[2];                                                  /* get data */
            *raw = (int16_t)(u_data);                                         /* get raw */
            *adc = (float)(*raw) / 256.0f * handle->ref_voltage;              /* convert to read data */
            raw++;                                                            /* raw address add */
            adc++;                                                            /* adc address add */
            u_data = buf[3];                                                  /* get data */
            *raw = (int16_t)(u_data);                                         /* get raw */
            *adc = (float)(*raw) / 256.0f * handle->ref_voltage;              /* convert to read data */
            raw++;                                                            /* raw address add */
            adc++;                                                            /* adc address add */
            u_data = buf[0];                                                  /* get data */
            *raw = (int16_t)(u_data);                                         /* get raw */
            *adc = (float)(*raw) / 256.0f * handle->ref_voltage;              /* convert to read data */
            *len = i;                                                         /* set length */
            res = 0;                                                          /* successful */
            
            break;                                                            /* break */
        }
        case 1 :                                                              /* mode 1 */
        {
            s_data = (int8_t)(buf[1]);                                        /* get data */
            *raw = (int16_t)(s_data);                                         /* get raw */
            *adc = (float)(*raw) / 256.0f * handle->ref_voltage;              /* convert to read data */
            raw++;                                                            /* raw address add */
            adc++;                                                            /* adc address add */
            s_data = (int8_t)(buf[2]);                                        /* get data */
            *raw = (int16_t)(s_data);                                         /* get raw */
            *adc = (float)(*raw) / 256.0f * handle->ref_voltage;              /* convert to read data */
            raw++;                                                            /* raw address add */
            adc++;                                                            /* adc address add */
            s_data = (int8_t)(buf[0]);                                        /* get data */
            *raw = (int16_t)(s_data);                                         /* get raw */
            *adc = (float)(*raw) / 256.0f * handle->ref_voltage;              /* convert to read data */
            *len = i;                                                         /* set length */
            res = 0;                                                          /* successful */
            
            break;                                                            /* break */
        }
        case 2 :                                                              /* mode 2 */
        {
            u_data = buf[1];                                                  /* get data */
            *raw = (int16_t)(u_data);                                         /* get raw */
            *adc = (float)(*raw) / 256.0f * handle->ref_voltage;              /* convert to read data */
            raw++;                                                            /* raw address add */
            adc++;                                                            /* adc address add */
            u_data = buf[2];                                                  /* get data */
            *raw = (int16_t)(u_data);                                         /* get raw */
            *adc = (float)(*raw) / 256.0f * handle->ref_voltage;              /* convert to read data */
            raw++;                                                            /* raw address add */
            adc++;                                                            /* adc address add */
            s_data = (int8_t)(buf[0]);                                        /* get data */
            *raw = (int16_t)(s_data);                                         /* get raw */
            *adc = (float)(*raw) / 256.0f * handle->ref_voltage;              /* convert to read data */
            *len = i;                                                         /* set length */
            res = 0;                                                          /* successful */
            
            break;                                                            /* break */
        }
        case 3 :                                                              /* mode 3 */
        {
            s_data = (int8_t)(buf[1]);                                        /* get data */
            *raw = (int16_t)(s_data);                                         /* get raw */
            *adc = (float)(*raw) / 256.0f * handle->ref_voltage;              /* convert to read data */
            raw++;                                                            /* raw address add */
            adc++;                                                            /* adc address add */
            s_data = (int8_t)(buf[0]);                                        /* get data */
            *raw = (int16_t)(s_data);                                         /* get raw */
            *adc = (float)(*raw) / 256.0f * handle->ref_voltage;              /* convert to read data */
            *len = i;                                                         /* set length */
            res = 0;                                                          /* successful */
            
            break;                                                            /* break */
        }
        default :
        {
            handle->debug_print("pcf8591: mode is invalid.\n");               /* mode is invalid */
            res = 1;                                                          /* failed */
            
            break;                                                            /* break */
        }
    }
    
    
    return res;                                                               /* return the result */
}

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
uint8_t pcf8591_set_reg(pcf8591_handle_t *handle, uint8_t *buf, uint16_t len)
{
    if (handle == NULL)                                              /* check handle */
    {
        return 2;                                                    /* return error */
    }
    if (handle->inited != 1)                                         /* check handle initialization */
    {
        return 3;                                                    /* return error */
    }
    
    return handle->iic_write_cmd(handle->iic_addr, buf, len);        /* write command */
}

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
uint8_t pcf8591_get_reg(pcf8591_handle_t *handle, uint8_t *buf, uint16_t len)
{
    if (handle == NULL)                                             /* check handle */
    {
        return 2;                                                   /* return error */
    }
    if (handle->inited != 1)                                        /* check handle initialization */
    {
        return 3;                                                   /* return error */
    }
    
    return handle->iic_read_cmd(handle->iic_addr, buf, len);        /* read command */
}

/**
 * @brief      get chip's information
 * @param[out] *info points to a pcf8591 info structure
 * @return     status code
 *             - 0 success
 *             - 2 handle is NULL
 * @note       none
 */
uint8_t pcf8591_info(pcf8591_info_t *info)
{
    if (info == NULL)                                               /* check handle */
    {
        return 2;                                                   /* return error */
    }
    
    memset(info, 0, sizeof(pcf8591_info_t));                        /* initialize pcf8591 info structure */
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
