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
 * @file      iic.c
 * @brief     iic source file
 * @version   1.0.0
 * @author    Shifeng Li
 * @date      2022-11-11
 *
 * <h3>history</h3>
 * <table>
 * <tr><th>Date        <th>Version  <th>Author      <th>Description
 * <tr><td>2022/11/11  <td>1.0      <td>Shifeng Li  <td>first upload
 * </table>
 */

#ifndef ARDUINO
#include "iic.h"
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <fcntl.h>

/**
 * @brief      iic bus init
 * @param[in]  *name points to an iic device name buffer
 * @param[out] *fd points to an iic device handle buffer
 * @return     status code
 *             - 0 success
 *             - 1 init failed
 * @note       none
 */
uint8_t iic_init(const char *name, int *fd)
{
    /* open the device */
    *fd = open(name, O_RDWR);
    
    /* check the fd */
    if ((*fd) < 0)
    {
        perror("iic: open failed.\n");
     
        return 1;
    }
    else
    {
        return 0;
    }
}

/**
 * @brief     iic bus deinit
 * @param[in] fd is the iic handle
 * @return    status code
 *            - 0 success
 *            - 1 deinit failed
 * @note      none
 */
uint8_t iic_deinit(int fd)
{
    /* close the device */
    if (close(fd) < 0)
    {
        perror("iic: close failed.\n");
        
        return 1;
    }
    else
    {
        return 0;
    }
}

/**
 * @brief      iic bus read command
 * @param[in]  fd is the iic handle
 * @param[in]  addr is the iic device write address
 * @param[out] *buf points to a data buffer
 * @param[in]  len is the length of the data buffer
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 * @note       addr = device_address_7bits << 1
 */
uint8_t iic_read_cmd(int fd, uint8_t addr, uint8_t *buf, uint16_t len)
{
    struct i2c_rdwr_ioctl_data i2c_rdwr_data;
    struct i2c_msg msgs[1];
    
    /* clear ioctl data */
    memset(&i2c_rdwr_data, 0, sizeof(struct i2c_rdwr_ioctl_data));

    /* clear msgs data */
    memset(msgs, 0, sizeof(struct i2c_msg) * 1);
    
    /* set the param */
    msgs[0].addr = addr >> 1;
    msgs[0].flags = I2C_M_RD;
    msgs[0].buf = buf;
    msgs[0].len = len;
    i2c_rdwr_data.msgs = msgs;
    i2c_rdwr_data.nmsgs = 1;
    
    /* transmit */
    if (ioctl(fd, I2C_RDWR, &i2c_rdwr_data) < 0)
    {
        perror("iic: read failed.\n");
        
        return 1;
    }
     
    return 0;
}

/**
 * @brief      iic bus read
 * @param[in]  fd is the iic handle
 * @param[in]  addr is the iic device write address
 * @param[in]  reg is the iic register address
 * @param[out] *buf points to a data buffer
 * @param[in]  len is the length of the data buffer
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 * @note       addr = device_address_7bits << 1
 */
uint8_t iic_read(int fd, uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len)
{
    struct i2c_rdwr_ioctl_data i2c_rdwr_data;
    struct i2c_msg msgs[2];
    
    /* clear ioctl data */
    memset(&i2c_rdwr_data, 0, sizeof(struct i2c_rdwr_ioctl_data));
    
    /* clear msgs data */
    memset(msgs, 0, sizeof(struct i2c_msg) * 2);
    
    /* set the param */
    msgs[0].addr = addr >> 1;
    msgs[0].flags = 0;
    msgs[0].buf = &reg;
    msgs[0].len = 1;
    msgs[1].addr = addr >> 1;
    msgs[1].flags = I2C_M_RD;
    msgs[1].buf = buf;
    msgs[1].len = len;
    i2c_rdwr_data.msgs = msgs;
    i2c_rdwr_data.nmsgs = 2;
    
    /* transmit */
    if (ioctl(fd, I2C_RDWR, &i2c_rdwr_data) < 0)
    {
        perror("iic: read failed.\n");
        
        return 1;
    }
     
    return 0;
}

/**
 * @brief      iic bus read with 16 bits register address
 * @param[in]  fd is the iic handle
 * @param[in]  addr is the iic device write address
 * @param[in]  reg is the iic register address
 * @param[out] *buf points to a data buffer
 * @param[in]  len is the length of the data buffer
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 * @note       addr = device_address_7bits << 1
 */
uint8_t iic_read_address16(int fd, uint8_t addr, uint16_t reg, uint8_t *buf, uint16_t len)
{
    struct i2c_rdwr_ioctl_data i2c_rdwr_data;
    struct i2c_msg msgs[2];
    uint8_t addr_buf[2];
    
    /* clear ioctl data */
    memset(&i2c_rdwr_data, 0, sizeof(struct i2c_rdwr_ioctl_data));
    
    /* clear msgs data */
    memset(msgs, 0, sizeof(struct i2c_msg) * 2);
    
    /* set the param */
    msgs[0].addr = addr >> 1;
    msgs[0].flags = 0;
    addr_buf[0] = (reg >> 8) & 0xFF;
    addr_buf[1] = (reg >> 0) & 0xFF;
    msgs[0].buf = addr_buf;
    msgs[0].len = 2;
    msgs[1].addr = addr >> 1;
    msgs[1].flags = I2C_M_RD;
    msgs[1].buf = buf;
    msgs[1].len = len;
    i2c_rdwr_data.msgs = msgs;
    i2c_rdwr_data.nmsgs = 2;
    
    /* transmit */
    if (ioctl(fd, I2C_RDWR, &i2c_rdwr_data) < 0)
    {
        perror("iic: read failed.\n");
        
        return 1;
    }
     
    return 0;
}

/**
 * @brief     iic bus write command
 * @param[in] fd is the iic handle
 * @param[in] addr is the iic device write address
 * @param[in] *buf points to a data buffer
 * @param[in] len is the length of the data buffer
 * @return    status code
 *            - 0 success
 *            - 1 write failed
 * @note      addr = device_address_7bits << 1
 */
uint8_t iic_write_cmd(int fd, uint8_t addr, uint8_t *buf, uint16_t len)
{
    struct i2c_rdwr_ioctl_data i2c_rdwr_data;
    struct i2c_msg msgs[1];
    
    /* clear ioctl data */
    memset(&i2c_rdwr_data, 0, sizeof(struct i2c_rdwr_ioctl_data));
    
    /* clear msgs data */
    memset(msgs, 0, sizeof(struct i2c_msg) * 1);
    
    /* set the param */
    msgs[0].addr = addr >> 1;
    msgs[0].flags = 0;
    msgs[0].buf = buf;
    msgs[0].len = len;
    i2c_rdwr_data.msgs = msgs;
    i2c_rdwr_data.nmsgs = 1;
    
    /* transmit */
    if (ioctl(fd, I2C_RDWR, &i2c_rdwr_data) < 0)
    {
        perror("iic: write failed.\n");
        
        return 1;
    }
     
    return 0;
}

/**
 * @brief     iic bus write
 * @param[in] fd is the iic handle
 * @param[in] addr is the iic device write address
 * @param[in] reg is the iic register address
 * @param[in] *buf points to a data buffer
 * @param[in] len is the length of the data buffer
 * @return    status code
 *            - 0 success
 *            - 1 write failed
 * @note      addr = device_address_7bits << 1
 */
uint8_t iic_write(int fd, uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len)
{
    struct i2c_rdwr_ioctl_data i2c_rdwr_data;
    struct i2c_msg msgs[1];
    uint8_t buf_send[len + 1];
    
    /* clear ioctl data */
    memset(&i2c_rdwr_data, 0, sizeof(struct i2c_rdwr_ioctl_data));
    
    /* clear msgs data */
    memset(msgs, 0, sizeof(struct i2c_msg) * 1);
    
    /* clear sent buf */
    memset(buf_send, 0, sizeof(uint8_t) * (len + 1));
    
    /* set the param */
    msgs[0].addr = addr >> 1;
    msgs[0].flags = 0;
    buf_send[0] = reg;
    memcpy(&buf_send[1], buf, len);
    msgs[0].buf = buf_send;
    msgs[0].len = len + 1;
    i2c_rdwr_data.msgs = msgs;
    i2c_rdwr_data.nmsgs = 1;
    
    /* transmit */
    if (ioctl(fd, I2C_RDWR, &i2c_rdwr_data) < 0)
    {
        perror("iic: write failed.\n");
        
        return 1;
    }
     
    return 0;
}

/**
 * @brief     iic bus write with 16 bits register address
 * @param[in] fd is the iic handle
 * @param[in] addr is the iic device write address
 * @param[in] reg is the iic register address
 * @param[in] *buf points to a data buffer
 * @param[in] len is the length of the data buffer
 * @return    status code
 *            - 0 success
 *            - 1 write failed
 * @note      addr = device_address_7bits << 1
 */
uint8_t iic_write_address16(int fd, uint8_t addr, uint16_t reg, uint8_t *buf, uint16_t len)
{
    struct i2c_rdwr_ioctl_data i2c_rdwr_data;
    struct i2c_msg msgs[1];
    uint8_t buf_send[len + 2];
    
    /* clear ioctl data */
    memset(&i2c_rdwr_data, 0, sizeof(struct i2c_rdwr_ioctl_data));
    
    /* clear msgs data */
    memset(msgs, 0, sizeof(struct i2c_msg) * 1);
    
    /* clear sent buf */
    memset(buf_send, 0, sizeof(uint8_t) * (len + 2));
    
    /* set the param */
    msgs[0].addr = addr >> 1;
    msgs[0].flags = 0;
    buf_send[0] = (reg >> 8) & 0xFF;
    buf_send[1] = (reg >> 0) & 0xFF;
    memcpy(&buf_send[2], buf, len);
    msgs[0].buf = buf_send;
    msgs[0].len = len + 2;
    i2c_rdwr_data.msgs = msgs;
    i2c_rdwr_data.nmsgs = 1;
    
    /* transmit */
    if (ioctl(fd, I2C_RDWR, &i2c_rdwr_data) < 0)
    {
        perror("iic: write failed.\n");
        
        return 1;
    }
     
    return 0;
}

#endif