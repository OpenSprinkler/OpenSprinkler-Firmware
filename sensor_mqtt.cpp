/* OpenSprinkler Unified (AVR/RPI/BBB/LINUX) Firmware
 * Copyright (C) 2015 by Ray Wang (ray@opensprinkler.com)
 *
 * Utility functions
 * 2024 @ OpenSprinklerShop
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
#include <errno.h>
#include "sensor_mqtt.h"
#include "sensors.h"
#include "mqtt.h"
#include "OpenSprinkler.h"

extern OpenSprinkler os;

void sensor_mqtt_init() {
	os.mqtt.setCallback(sensor_mqtt_callback);
}
/**
 * @brief 
 * 
 * @param mtopic reported topic opensprinkler/analogsensor/name
 * @param pattern topic pattern  opensprinkler/ <star> or opensprinkler/# or opensprinkler/#/abc/#
 * @return true 
 * @return false 
 */
bool mqtt_filter_matches(char* mtopic, char* pattern) {
	
	while (pattern && mtopic) {
		char ch1 = *mtopic++;
		char ch2 = *pattern++;
		if (ch2 == '+') { //level ok up to "/"
			while (mtopic[0]) {
				if (ch1 == '/')
					break;
				ch1 = *mtopic++;
			}
		} else if (ch2 == '#') { //multilevel
			char *p = strpbrk(pattern, "#+");
			if (!p) return true;
			if (strncmp(pattern, mtopic, p-pattern) ==0) {
				mtopic = mtopic + (p-pattern);
				pattern = p;
			}
		} 
		if (ch1 != ch2) 
			return false;
		else if (ch1 == 0 && ch2 == 0)
			return true;
		else if (ch1 == 0 || ch2 == 0)
			return false;
	}
	return true;
}

char *strnlstr(const char *haystack, const char *needle, size_t needle_len, size_t len)
{
        int i;
        for (i=0; i<=(int)(len-needle_len); i++)
        {
		if (haystack[0] == 0)
			break;
                if ((haystack[0] == needle[0]) &&
                    (strncmp(haystack, needle, needle_len) == 0))
                        return (char *)haystack;
                haystack++;
        }
        return NULL;
}

/**
 * @brief mqtt callback
 * 
 */
#if defined(ARDUINO)
void sensor_mqtt_callback(char* mtopic, byte* payload, unsigned int length) {
#else
static void sensor_mqtt_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg) {
    DEBUG_PRINTLN("sensor_mqtt_callback0");
	char* mtopic = msg->topic;
	byte* payload = (byte*)msg->payload;
	unsigned int length = msg->payloadlen;
#endif

    DEBUG_PRINTLN("sensor_mqtt_callback1");

	if (!mtopic || !payload) return;
	time_t now = os.now_tz();
	Sensor_t *sensor = getSensors();
	while (sensor) {
		if (sensor->type == SENSOR_MQTT && sensor->last_read != now) {
			char *topic = SensorUrl_get(sensor->nr, SENSORURL_TYPE_TOPIC);
			DEBUG_PRINT("mtopic: "); DEBUG_PRINTLN(mtopic);
			DEBUG_PRINT("topic:  "); DEBUG_PRINTLN(topic);
			
			if (topic && mqtt_filter_matches(mtopic, topic)) {
				DEBUG_PRINTLN("topic match!");
				char *jsonFilter =  SensorUrl_get(sensor->nr, SENSORURL_TYPE_FILTER);
				char *p = (char*)payload;				
				char *f = jsonFilter;

				DEBUG_PRINT("payload: ");
				DEBUG_PRINTLN(p);

				DEBUG_PRINT("jsonfilter: ");
				DEBUG_PRINTLN(jsonFilter);

				while (f && p) {
					f = strstr(jsonFilter, "|");
					if (f) {
						p = strnlstr(p, jsonFilter, f-jsonFilter, (char*)payload+length-p);
						jsonFilter = f+1;
					} else {
						p = strstr(p, jsonFilter);
					}
				}
				if (p) {
					char buf[30];
					p = strpbrk(p, "0123456789.-+");
					uint i = 0;
					while (p && i < sizeof(buf)) {
						char ch = *p++;
						if ((ch >= '0' && ch <= '9') || ch == '.' || ch == '-' || ch == '+') {
							buf[i++] = ch;
						} else break;
					}
					buf[i] = 0;
					DEBUG_PRINT("result: ");
					DEBUG_PRINTLN(buf);	
					errno = 0;
					char *e;
					double value = strtod(buf, &e);
					if (*e == 0 && errno == 0 && (value != sensor->last_data || !sensor->flags.data_ok || now-sensor->last_read > 6000)) {
						sensor->last_data = value;
						sensor->flags.data_ok = true;
						sensor->last_read = now;	
                    	DEBUG_PRINTLN("sensor_mqtt_callback2");
    	
        				sensorlog_add(LOG_STD, sensor, sensor->last_read);
						sensor->mqtt_push = true;
					}
				}
			}
		}
		sensor = sensor->next;
	}

	//Now push the data out:
	sensor = getSensors();
	while (sensor) {
		if (sensor->type == SENSOR_MQTT && sensor->mqtt_push) {
			sensor->mqtt_push = false;
			push_message(sensor);
		}
		sensor = sensor->next;
	}

    DEBUG_PRINTLN("sensor_mqtt_callback3");
}

int read_sensor_mqtt(Sensor_t *sensor) {
	if (!os.mqtt.enabled() || !os.mqtt.connected()) {
		sensor->flags.data_ok = false;
		sensor->mqtt_init = false;
	} else {
        DEBUG_PRINT("read_sensor_mqtt1: ");
		DEBUG_PRINTLN(sensor->name);
		char *topic = SensorUrl_get(sensor->nr, SENSORURL_TYPE_TOPIC);
		if (topic && topic[0]) {
            DEBUG_PRINT("subscribe: ");
            DEBUG_PRINTLN(topic);
			os.mqtt.subscribe(topic);
			sensor->mqtt_init = true;
		}
	}
	return HTTP_RQT_NOT_RECEIVED;
}

void sensor_mqtt_subscribe(uint nr, uint type, const char *urlstr) {
    Sensor_t* sensor = sensor_by_nr(nr);
    if (urlstr && urlstr[0] && type == SENSORURL_TYPE_TOPIC && sensor && sensor->type == SENSOR_MQTT) {
	    DEBUG_PRINT("sensor_mqtt_subscribe1: ");
		DEBUG_PRINTLN(sensor->name);
        DEBUG_PRINT("subscribe: ");
        DEBUG_PRINTLN(urlstr);
		if (!os.mqtt.subscribe(urlstr))
			DEBUG_PRINTLN("error subscribe!!");
	    sensor->mqtt_init = true;
    }
}

void sensor_mqtt_unsubscribe(uint nr, uint type, const char *urlstr) {
    Sensor_t* sensor = sensor_by_nr(nr);
    if (urlstr && urlstr[0] && type == SENSORURL_TYPE_TOPIC && sensor && sensor->type == SENSOR_MQTT) {
	    DEBUG_PRINT("sensor_mqtt_unsubscribe1: ");
		DEBUG_PRINTLN(sensor->name);
        DEBUG_PRINT("unsubscribe: ");
        DEBUG_PRINTLN(urlstr);
		if (!os.mqtt.unsubscribe(urlstr))
			DEBUG_PRINTLN("error unsubscribe!!");
		sensor->mqtt_init = false;
    }
}