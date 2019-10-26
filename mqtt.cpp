/* OpenSprinkler Unified (AVR/RPI/BBB/LINUX/ESP8266) Firmware
 * Copyright (C) 2015 by Ray Wang (ray@opensprinkler.com)
 *
 * OpenSprinkler library
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

#if defined(ARDUINO)
	#include <Arduino.h>
	#include <UIPEthernet.h>
	#include <PubSubClient.h>
	#if MQTT_KEEPALIVE != 60
		#error Set MQTT_KEEPALIVE to 60 in PubSubClient.h
	#endif
#elif defined(OSPI)
	#include <mosquitto.h>
	#include <time.h>
#endif

#include "OpenSprinkler.h"
#include "mqtt.h"

#define MQTT_MAX_HOST_LEN		50 // Note: App is set to max 50 chars for broker name
#define MQTT_RECONNECT_DELAY	60 // Minumum of 60 seconds between reconnect attempts

#if defined(ARDUINO)
	#if defined(ESP8266)
		WiFiClient wifiClient;
	#endif
	EthernetClient ethClient;
    struct PubSubClient *mqtt_client = NULL;
#else
	#if defined(OSPI)
		#define MQTT_KEEPALIVE 60
		struct mosquitto *mqtt_client = NULL;

		static void mqtt_connection_cb(struct mosquitto *mqtt_client, void *obj, int rc);
		static void mqtt_disconnection_cb(struct mosquitto *mqtt_client, void *obj, int rc);
		static void mqtt_log_cb(struct mosquitto *mqtt_client, void *obj, int level, const char *message);
	#else	// Do nothing os OSBO and DEMO
		void * mqtt_client = NULL;
	#endif
#endif

extern OpenSprinkler os;

char OSMqtt::_host[MQTT_MAX_HOST_LEN + 1];	// IP or host name of the broker
int OSMqtt::_port = 1883;					// Port of the broker (default 1883)
char OSMqtt::_id[16];						// Id to identify the client to the broker
bool OSMqtt::_enabled = false;				// Flag indicating whether MQTT is enabled

// Create an MQTT instance. Note: on OSPi this also starts the loop thread.
void OSMqtt::start(void) {
	DEBUG_LOGF("MQTT Start: ");
#if defined(ARDUINO)
	Client * client = NULL;
	bool wired_mac = false;
	byte mac[6];

    if (mqtt_client) { delete mqtt_client; mqtt_client = 0; }

	#if defined(ESP8266)
		if (m_server) client = &ethClient;
		else client = &wifiClient;
		wired_mac = (m_server != NULL);
	#else
		client = &ethClient;
		wired_mac = true;
	#endif
	mqtt_client = new PubSubClient(*client);
	os.load_hardware_mac(mac, wired_mac);

	sprintf(_id, "OS-%02X%02X%02X%02X%02X%02X",mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	DEBUG_PRINTF("Client %s ", _id);

	if (mqtt_client == NULL) {
		DEBUG_PRINTF("Failed to initialise client\n");
		return;
	}
#elif defined(OSPI)
	int rc;

	mosquitto_lib_init();

	if (mqtt_client) { mosquitto_destroy(mqtt_client); mqtt_client = NULL; };

	mqtt_client = mosquitto_new(NULL, true, NULL);
	if (mqtt_client == NULL) {
		DEBUG_PRINTF("Failed to initialise client\n");
		return;
	}

	mosquitto_will_set(mqtt_client, "opensprinkler/availability", strlen("offline"), "offline", 0, true);
	mosquitto_reconnect_delay_set(mqtt_client, 2, MQTT_RECONNECT_DELAY, true);
	mosquitto_connect_callback_set(mqtt_client, mqtt_connection_cb);
	mosquitto_disconnect_callback_set(mqtt_client, mqtt_disconnection_cb);
	mosquitto_log_callback_set(mqtt_client, mqtt_log_cb);
#endif
	DEBUG_PRINTF("Success\n");
}

#if defined(OSPI)
// Callback called following a connect attempt. If the connection was successful then an "availability" message is sent.
static void mqtt_connection_cb(struct mosquitto *mqtt_client, void *obj, int rc) {
	DEBUG_LOGF("MQTT Connnection: %s\n", mosquitto_strerror(rc));
	if (rc == MOSQ_ERR_SUCCESS)
		mosquitto_publish(mqtt_client, NULL, "opensprinkler/availability", strlen("online"), "online", 0, true);
}

static void mqtt_disconnection_cb(struct mosquitto *mqtt_client, void *obj, int rc) {
	DEBUG_LOGF("MQTT Disconnection: %s\n", mosquitto_strerror(rc));
}

static void mqtt_log_cb(struct mosquitto *mqtt_client, void *obj, int level, const char *message){
	if (level != MOSQ_LOG_DEBUG )
	DEBUG_LOGF("MQTT Log: %s (%d)\n", message, level);
}

#endif

bool OSMqtt::enabled(void) { return _enabled; }

// Configure the broker host and port fields and enabled/disable the interface
// param config pointer to a string containing the JSON configuration settings in the form of "{server:"host_name|IP address", port: 1883, enabled:"0|1"}"
void OSMqtt::setup(void) {
	String config = os.sopt_load(SOPT_MQTT_OPTS);
	if (config.length() == 0) {
		_enabled = false;
		DEBUG_LOGF("MQTT Setup: Config (None) Disabled\n");
	} else {
		int enable = false;
		sscanf(config.c_str(), "\"server\":\"%[^\"]\",\"port\":\%d,\"enable\":\%d", _host, &_port, &enable);
		_enabled = (bool)enable;
		DEBUG_LOGF("MQTT Setup: Config (%s:%d) %s\n", _host, _port, _enabled ? "Enabled" : "Disabled");
	}

	if (mqtt_client == NULL) return;

#if defined(ARDUINO)
	if (mqtt_client->connected()) {
		mqtt_client->disconnect();
	}

	if (_enabled) {
		mqtt_client->setServer(_host, _port);
		if (mqtt_client->connect(_id, NULL, NULL, "opensprinkler/availability", 0, true, "offline")) {
			mqtt_client->publish("opensprinkler/availability", "online", true);
		} else {
			DEBUG_LOGF("MQTT Setup: Connect Failed (%d)\n", mqtt_client->state());
		}
	}
#elif defined(OSPI)
	mosquitto_disconnect(mqtt_client);

	if (_enabled) {
		int rc = mosquitto_connect(mqtt_client, _host, _port, MQTT_KEEPALIVE);
		if (rc != MOSQ_ERR_SUCCESS) DEBUG_LOGF("MQTT Setup: Connect Failed (%s)\n", mosquitto_strerror(rc));
	}
#endif
}

void OSMqtt::publish(const char *topic, const char *payload) {
	DEBUG_LOGF("MQTT Publish: %s %s\n", topic, payload);
	if (mqtt_client == NULL || !_enabled || os.status.network_fails > 0) return;

#if defined (ARDUINO)
	if (!mqtt_client->publish(topic, payload)) {
		DEBUG_LOGF("MQTT Publish: Failed (%d)\n", mqtt_client->state());
	}
#elif defined(OSPI)
	int rc = mosquitto_publish(mqtt_client, NULL, topic, strlen(payload), payload, 0, false);
	if (rc != MOSQ_ERR_SUCCESS) {
		DEBUG_LOGF("MQTT Publish: Failed (%s)\n", mosquitto_strerror(rc));
	}
#endif
}

// Need to regularly call the loop function to ensure "keep alive" messages are sent to the broker and to reconnect if needed.
void OSMqtt::loop(void) {
	static unsigned long last_reconnect_attempt = 0;
	unsigned long now = millis();

	if (mqtt_client == NULL || !_enabled || os.status.network_fails > 0) return;

#if defined (ARDUINO)
	static int last_state = 999;
	if (!mqtt_client->connected() && (now - last_reconnect_attempt >= (unsigned long)MQTT_RECONNECT_DELAY*1000)) {
		if (mqtt_client->connect(_id, NULL, NULL, "opensprinkler/availability", 0, true, "offline"))
			mqtt_client->publish("opensprinkler/availability", "online", true);
		last_reconnect_attempt = millis();
	}

	mqtt_client->loop();

	if (last_state != mqtt_client->state()) {
		DEBUG_LOGF("MQTT Loop: Connected %d, State %d\n", mqtt_client->connected(), mqtt_client->state());
		last_state = mqtt_client->state();
	}

#elif defined(OSPI)
	static int last_rc = 999;

	int rc = mosquitto_loop(mqtt_client, 0 , 1);
	if ((rc == MOSQ_ERR_NO_CONN || rc == MOSQ_ERR_CONN_LOST) && (now - last_reconnect_attempt >= (unsigned long)MQTT_RECONNECT_DELAY*1000)) {
		rc = mosquitto_reconnect(mqtt_client);
		DEBUG_LOGF("MQTT Loop: Reconnect Status %s\n", mosquitto_strerror(rc));
		last_reconnect_attempt = millis();
	}

	if (last_rc != rc) {
		DEBUG_LOGF("MQTT Loop: Staus %s\n", mosquitto_strerror(rc));
		last_rc = rc;
	}
#endif
}
