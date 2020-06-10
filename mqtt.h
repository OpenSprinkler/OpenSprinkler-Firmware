/* OpenSprinkler Unified (AVR/RPI/BBB/LINUX/ESP8266) Firmware
 * Copyright (C) 2015 by Ray Wang (ray@opensprinkler.com)
 *
 * OpenSprinkler library header file
 * Feb 2015 @ OpenSprinkler.com
 *
 * This file is part of the OpenSprinkler library
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

#ifndef _MQTT_H
#define _MQTT_H

class OSMqtt {
private:
    static char _id[];
    static char _host[];
    static int _port;
    static char _username[];
    static char _password[];
    static bool _enabled;

    // Following routines are platform specific versions of the public interface
    static int _init(void);
    static int _connect(void);
    static int _disconnect(void);
    static bool _connected(void);
    static int _publish(const char *topic, const char *payload);
    static int _loop(void);
    static const char * _state_string(int state);
public:
    static void init(void);
    static void init(const char * id);
    static void begin(void);
    static void begin(const char * host, int port, const char * username, const char * password, bool enable);
    static bool enabled(void) { return _enabled; };
    static void publish(const char *topic, const char *payload);
    static void loop(void);
};

#endif	// _MQTT_H
