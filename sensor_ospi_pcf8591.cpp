/* OpenSprinkler Unified (AVR/RPI/BBB/LINUX) Firmware Copyright (C) 2015 by
 * Ray Wang (ray@opensprinkler.com)
 *
 * Sensor-API
 * 2023/2024 @ OpenSprinklerShop
 * Stefan Schmaltz (info@opensprinklershop.de)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifdef PCF8591

#include "ospi-analog/driver_pcf8591.h"
#include "ospi-analog/driver_pcf8591_interface.h"
#include "sensor_ospi_pcf8591.h"
#include "sensors.h"

#define DEFAULT_ADDR PCF8591_ADDRESS_A000
#define DEFAULT_MODE PCF8591_MODE_AIN0123_GND
#define DEFAULT_REF_VOLTAGE 3.3
/**
* Read the OSPi onboard PCF8591 A2D
**/
int read_sensor_ospi(Sensor_t *sensor, ulong time) {
        if (!sensor || !sensor->flags.enable) return HTTP_RQT_NOT_RECEIVED;

        static pcf8591_handle_t gs_handle;        /**< pcf8591 handle */
        uint8_t res;

        DRIVER_PCF8591_LINK_INIT(&gs_handle, pcf8591_handle_t);
        DRIVER_PCF8591_LINK_IIC_INIT(&gs_handle, pcf8591_interface_iic_init);
        DRIVER_PCF8591_LINK_IIC_DEINIT(&gs_handle, pcf8591_interface_iic_deinit);
        DRIVER_PCF8591_LINK_IIC_READ_COMMAND(&gs_handle, pcf8591_interface_iic_read_cmd);
        DRIVER_PCF8591_LINK_IIC_WRITE_COMMAND(&gs_handle, pcf8591_interface_iic_write_cmd);
        DRIVER_PCF8591_LINK_DELAY_MS(&gs_handle, pcf8591_interface_delay_ms);
        DRIVER_PCF8591_LINK_DEBUG_PRINT(&gs_handle, pcf8591_interface_debug_print);

        /* set addr pin */
        res = pcf8591_set_addr_pin(&gs_handle, DEFAULT_ADDR);
        if (res != 0)
                return HTTP_RQT_NOT_RECEIVED;

        /* pcf8591 init */
        res = pcf8591_init(&gs_handle);
        if (res != 0)
                return HTTP_RQT_NOT_RECEIVED;
    
        /* set mode */
        res = pcf8591_set_mode(&gs_handle, DEFAULT_MODE);
        if (res != 0) {
                pcf8591_deinit(&gs_handle);
                return HTTP_RQT_NOT_RECEIVED;
        }

        /* disable auto increment */
        res = pcf8591_set_auto_increment(&gs_handle, PCF8591_BOOL_FALSE);
        if (res != 0) {
                pcf8591_deinit(&gs_handle);
                return HTTP_RQT_NOT_RECEIVED;
        }

        /* set default reference voltage */
        res = pcf8591_set_reference_voltage(&gs_handle, DEFAULT_REF_VOLTAGE);
        if (res != 0) {
                pcf8591_deinit(&gs_handle);
                return HTTP_RQT_NOT_RECEIVED;
        }
                
        pcf8591_channel_t channel = (pcf8591_channel_t)sensor->id;
        res = pcf8591_set_channel(&gs_handle, channel);
        if (res != 0) {
                pcf8591_deinit(&gs_handle);
                return HTTP_RQT_NOT_RECEIVED;
        }

        int16_t raw;
        float v;
        res = pcf8591_read(&gs_handle, &raw, &v);
        if (res != 0) {
                pcf8591_deinit(&gs_handle);
                return HTTP_RQT_NOT_RECEIVED;
        }

        pcf8591_deinit(&gs_handle);

        sensor->repeat_native += raw;
        sensor->repeat_data += v;
        if (++sensor->repeat_read < MAX_SENSOR_REPEAT_READ && time < sensor->last_read + sensor->read_interval)
                return HTTP_RQT_NOT_RECEIVED;

        raw = sensor->repeat_native/sensor->repeat_read;
        v = sensor->repeat_data/sensor->repeat_read;

        sensor->repeat_native = raw;
        sensor->repeat_data = v;
        sensor->repeat_read = 1;
        
        sensor->last_native_data = raw;
        sensor->flags.data_ok = true;
        sensor->last_read = time;

        //convert values:
        switch(sensor->type) {
                case SENSOR_OSPI_ANALOG:
                        sensor->last_data = (double)v;
                        return HTTP_RQT_SUCCESS;
                case SENSOR_OSPI_ANALOG_P:
                        sensor->last_data = (double)v / DEFAULT_REF_VOLTAGE * 100;
                        return HTTP_RQT_SUCCESS;
                case SENSOR_OSPI_ANALOG_SMT50_MOIS:
                        sensor->last_data = (double)v * 50 / 3;
                        return HTTP_RQT_SUCCESS;
                case SENSOR_OSPI_ANALOG_SMT50_TEMP:
                        sensor->last_data = ((double)v - 0.5) * 100;
                        return HTTP_RQT_SUCCESS;
        }
        return HTTP_RQT_NOT_RECEIVED;
}




#endif
