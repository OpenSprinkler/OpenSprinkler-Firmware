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

#ifdef ADS1115

#include "ospi-analog/driver_ads1115.h"
#include "ospi-analog/driver_ads1115_interface.h"
#include "sensor_ospi_ads1115.h"
#include "sensors.h"

#define ADS1115_BASIC_DEFAULT_RANGE        ADS1115_RANGE_6P144V        /**< set range 6.144V */
#define ADS1115_BASIC_DEFAULT_RATE         ADS1115_RATE_128SPS         /**< set 128 SPS */

#define DEFAULT_RANGE 6.144

/**
* Read the OSPi 1.6 onboard ADS1115 A2D
**/
int read_sensor_ospi(Sensor_t *sensor) {
        DEBUG_PRINTLN(F("read_sensor_ospi"));
        if (!sensor || !sensor->flags.enable) return HTTP_RQT_NOT_RECEIVED;

        sensor->flags.data_ok = false;

        static ads1115_handle_t gs_handle; /**< ads1115 handle */
        uint8_t res;


        DRIVER_ADS1115_LINK_INIT(&gs_handle, ads1115_handle_t);
        DRIVER_ADS1115_LINK_IIC_INIT(&gs_handle, ads1115_interface_iic_init);
        DRIVER_ADS1115_LINK_IIC_DEINIT(&gs_handle, ads1115_interface_iic_deinit);
        DRIVER_ADS1115_LINK_IIC_READ(&gs_handle, ads1115_interface_iic_read);
        DRIVER_ADS1115_LINK_IIC_WRITE(&gs_handle, ads1115_interface_iic_write);
        DRIVER_ADS1115_LINK_DELAY_MS(&gs_handle, ads1115_interface_delay_ms);
        DRIVER_ADS1115_LINK_DEBUG_PRINT(&gs_handle, ads1115_interface_debug_print);

        ads1115_address_t addr = ADS1115_ADDR_GND;
        ads1115_channel_t channel = ADS1115_CHANNEL_AIN0_GND;
        /* set channel */
        switch(sensor->id) {
                case 0: channel = ADS1115_CHANNEL_AIN0_GND; break;
                case 1: channel = ADS1115_CHANNEL_AIN1_GND; break;
                case 2: channel = ADS1115_CHANNEL_AIN2_GND; break;
                case 3: channel = ADS1115_CHANNEL_AIN3_GND; break;
                case 4: addr = ADS1115_ADDR_VCC; channel = ADS1115_CHANNEL_AIN0_GND; break;
                case 5: addr = ADS1115_ADDR_VCC; channel = ADS1115_CHANNEL_AIN1_GND; break;
                case 6: addr = ADS1115_ADDR_VCC; channel = ADS1115_CHANNEL_AIN2_GND; break;
                case 7: addr = ADS1115_ADDR_VCC; channel = ADS1115_CHANNEL_AIN3_GND; break;
                default: return HTTP_RQT_NOT_RECEIVED;
        }

        /* set addr pin */
        res = ads1115_set_addr_pin(&gs_handle, addr);
        if (res != 0)
                return HTTP_RQT_NOT_RECEIVED;

        /* ads1115 init */
        res = ads1115_init(&gs_handle);
        if (res != 0)
            return HTTP_RQT_NOT_RECEIVED;

        res = ads1115_set_channel(&gs_handle, channel);
        if (res != 0) {
                ads1115_deinit(&gs_handle);
                return HTTP_RQT_NOT_RECEIVED;
        }

        /* set default range */
        res = ads1115_set_range(&gs_handle, ADS1115_BASIC_DEFAULT_RANGE);
        if (res != 0) {
                ads1115_deinit(&gs_handle);
                return HTTP_RQT_NOT_RECEIVED;
        }

        /* set default rate */
        res = ads1115_set_rate(&gs_handle, ADS1115_BASIC_DEFAULT_RATE);
        if (res != 0) {
                ads1115_deinit(&gs_handle);
                return HTTP_RQT_NOT_RECEIVED;
        }

        /* disable compare */
        res = ads1115_set_compare(&gs_handle, ADS1115_BOOL_FALSE);
        if (res != 0) {
                ads1115_deinit(&gs_handle);
                return HTTP_RQT_NOT_RECEIVED;
        }

        int16_t raw;
        float   v;
        res = ads1115_single_read(&gs_handle, &raw, &v);
        if (res != 0) {
                ads1115_deinit(&gs_handle);
                return HTTP_RQT_NOT_RECEIVED;
        }

        ads1115_deinit(&gs_handle);

        sensor->last_native_data = raw;
        sensor->flags.data_ok = true;

        //convert values:
        switch(sensor->type) {
                case SENSOR_OSPI_ANALOG:
                        sensor->last_data = (double)v;
                        return HTTP_RQT_SUCCESS;
                case SENSOR_OSPI_ANALOG_P:
                        sensor->last_data = (double)v / DEFAULT_RANGE * 3.3 * 100;
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
