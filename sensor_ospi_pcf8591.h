/* OpenSprinkler Unified (AVR/RPI/BBB/LINUX) Firmware
 * Copyright (C) 2015 by Ray Wang (ray@opensprinkler.com)
 *
 * sensors header file - OSPI
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

#ifndef _SENSOR_OSPI_PCF8591_H
#define _SENSOR_OSPI_PCF8591_H

#ifdef PCF8591

#include "sensors.h"

/*
 * @brief     Read sensor function
 * @param[in] Sensor
 * @note      
 */
int read_sensor_ospi(Sensor_t *sensor, ulong time);

#endif // PCF8591

#endif // _SENSORS_H
