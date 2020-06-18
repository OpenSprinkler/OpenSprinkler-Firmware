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
	#if defined(ESP8266)
		#include <ESP8266WiFi.h>
	#endif
	#include <UIPEthernet.h>
	#include <PubSubClient.h>

	struct PubSubClient *mqtt_client = NULL;

#else
	#include <time.h>
	#include <stdio.h>
	#include <mosquitto.h>

	struct mosquitto *mqtt_client = NULL;
#endif

#include "OpenSprinkler.h"
#include "mqtt.h"

// Debug routines to help identify any blocking of the event loop for an extended period

#if defined(ENABLE_DEBUG)
	#if defined(ARDUINO)
		#include "TimeLib.h"
		#define DEBUG_PRINTF(msg, ...)		{Serial.printf(msg, ##__VA_ARGS__);}
		#define DEBUG_TIMESTAMP(msg, ...)	{time_t t = os.now_tz(); Serial.printf(buffer, "%02d-%02d-%02d %02d:%02d:%02d - ", year(t), month(t), day(t), hour(t), minute(t), second(t));}
	#else
		#include <sys/time.h>
		#define DEBUG_PRINTF(msg, ...)		{printf(msg, ##__VA_ARGS__);}
		#define DEBUG_TIMESTAMP()			{char tstr[21]; time_t t = time(NULL); struct tm *tm = localtime(&t); strftime(tstr, 21, "%y-%m-%d %H:%M:%S - ", tm);printf("%s", tstr);}
	#endif
	#define DEBUG_LOGF(msg, ...)			{DEBUG_TIMESTAMP(); DEBUG_PRINTF(msg, ##__VA_ARGS__);}

	static unsigned long _lastMillis = 0;	// Holds the timestamp associated with the last call to DEBUG_DURATION() 
	inline unsigned long DEBUG_DURATION()	{unsigned long dur = millis() - _lastMillis; _lastMillis = millis(); return dur;}
#else
	#define DEBUG_PRINTF(msg, ...)			{}
	#define DEBUG_LOGF(msg, ...)			{}
	#define DEBUG_DURATION()				{}
#endif

#define str(s) #s
#define xstr(s) str(s)

extern OpenSprinkler os;
extern char tmp_buffer[];

#define MQTT_KEEPALIVE			60
#define MQTT_DEFAULT_PORT		1883	// Default port for MQTT. Can be overwritten through App config
#define MQTT_MAX_HOST_LEN		50		// Note: App is set to max 50 chars for broker name
#define MQTT_MAX_USERNAME_LEN	32		// Note: App is set to max 32 chars for username
#define MQTT_MAX_PASSWORD_LEN	32		// Note: App is set to max 32 chars for password
#define MQTT_MAX_ID_LEN			16		// MQTT Client Id to uniquely reference this unit
#define MQTT_RECONNECT_DELAY	120		// Minumum of 60 seconds between reconnect attempts

#define MQTT_ROOT_TOPIC			"opensprinkler"
#define MQTT_AVAILABILITY_TOPIC	MQTT_ROOT_TOPIC "/availability"
#define MQTT_ONLINE_PAYLOAD		"online"
#define MQTT_OFFLINE_PAYLOAD	"offline"

#define MQTT_SUCCESS			0					// Returned when function operated successfully
#define MQTT_ERROR				1					// Returned whan function failed

char OSMqtt::_id[MQTT_MAX_ID_LEN + 1] = {0};		// Id to identify the client to the broker
char OSMqtt::_host[MQTT_MAX_HOST_LEN + 1] = {0};	// IP or host name of the broker
char OSMqtt::_username[MQTT_MAX_USERNAME_LEN + 1] = {0};	// username to connect to the broker
char OSMqtt::_password[MQTT_MAX_PASSWORD_LEN + 1] = {0};	// password to connect to the broker
int OSMqtt::_port = MQTT_DEFAULT_PORT;				// Port of the broker (default 1883)
bool OSMqtt::_enabled = false;						// Flag indicating whether MQTT is enabled

// Initialise the client libraries and event handlers.
void OSMqtt::init(void) {
	DEBUG_LOGF("MQTT Init\n");
	char id[MQTT_MAX_ID_LEN + 1] = {0};

#if defined(ARDUINO)
	uint8_t mac[6] = {0};
	os.load_hardware_mac(mac, m_server!=NULL);
	snprintf(id, MQTT_MAX_ID_LEN, "OS-%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
#endif

	init(id);
};

// Initialise the client libraries and event handlers.
void OSMqtt::init(const char * clientId) {
	DEBUG_LOGF("MQTT Init: ClientId %s\n", clientId);

	strncpy(_id, clientId, MQTT_MAX_ID_LEN);
	_id[MQTT_MAX_ID_LEN] = 0;
	_init();
};

// Start the MQTT service and connect to the MQTT broker using the stored configuration.
void OSMqtt::begin(void) {
	DEBUG_LOGF("MQTT Begin\n");
	char host[MQTT_MAX_HOST_LEN + 1] = {0};
	char username[MQTT_MAX_USERNAME_LEN + 1] = {0};
	char password[MQTT_MAX_PASSWORD_LEN + 1] = {0};
	int port = MQTT_DEFAULT_PORT;
	int enabled = 0;

	// JSON configuration settings in the form of {"en":0|1,"host":"server_name|IP address","port":1883,user:"",pass:""}
	char *config = tmp_buffer;
	os.sopt_load(SOPT_MQTT_OPTS, config);
	if (*config != 0) {
		sscanf(
			config,
			"\"en\":%d,\"host\":\"%" xstr(MQTT_MAX_HOST_LEN) "[^\"]\",\"port\":%d,\"user\":\"%" xstr(MQTT_MAX_USERNAME_LEN) "[^\"]\",\"pass\":\"%" xstr(MQTT_MAX_PASSWORD_LEN) "[^\"]\"",
			&enabled, host, &port, username, password
			);
	}

	begin(host, port, username, password, (bool)enabled);
}

// Start the MQTT service and connect to the MQTT broker.
void OSMqtt::begin( const char * host, int port, const char * username, const char * password, bool enabled ) {
	DEBUG_LOGF("MQTT Begin: Config (%s:%d %s) %s\n", host, port, username, enabled ? "Enabled" : "Disabled");

	strncpy(_host, host, MQTT_MAX_HOST_LEN);
	_host[MQTT_MAX_HOST_LEN] = 0;
	_port = port;
	strncpy(_username, username, MQTT_MAX_USERNAME_LEN);
	_username[MQTT_MAX_USERNAME_LEN] = 0;
	strncpy(_password, password, MQTT_MAX_PASSWORD_LEN);
	_username[MQTT_MAX_PASSWORD_LEN] = 0;
	_enabled = enabled;

	if (mqtt_client == NULL || os.status.network_fails > 0) return;

	if (_connected()) {
		_disconnect();
	}

	if (_enabled) {
		_connect();
	}
}

// Publish an MQTT message to a specific topic
void OSMqtt::publish(const char *topic, const char *payload) {
	DEBUG_LOGF("MQTT Publish: %s %s\n", topic, payload);

	if (mqtt_client == NULL || !_enabled || os.status.network_fails > 0) return;

	if (!_connected()) {
		DEBUG_LOGF("MQTT Publish: Not connected\n");
		return;
	}

	_publish(topic, payload);
}

// Regularly call the loop function to ensure "keep alive" messages are sent to the broker and to reconnect if needed.
void OSMqtt::loop(void) {
	static unsigned long last_reconnect_attempt = 0;

	if (mqtt_client == NULL || !_enabled || os.status.network_fails > 0) return;

	// Only attemp to reconnect every MQTT_RECONNECT_DELAY seconds to avoid blocking the main loop
	if (!_connected() && (millis() - last_reconnect_attempt >= MQTT_RECONNECT_DELAY * 1000UL)) {
		DEBUG_LOGF("MQTT Loop: Reconnecting\n");
		_connect();
		last_reconnect_attempt = millis();
	}

	int state = _loop();

#if defined(ENABLE_DEBUG)
	// Print a diagnostic message whenever the MQTT state changes
	bool network = os.network_connected(), mqtt = _connected();
	static bool last_network = 0, last_mqtt = 0;
	static int last_state = 999;

	if (last_state != state || last_network != network || last_mqtt != mqtt) {
		DEBUG_LOGF("MQTT Loop: Network %s, MQTT %s, State - %s\n",
					network ? "UP" : "DOWN",
					mqtt ? "UP" : "DOWN",
					_state_string(state));
		last_state = state; last_network = network; last_mqtt = mqtt;
	}
#endif
}

/**************************** ARDUINO ********************************************/
#if defined(ARDUINO)

	#if defined(ESP8266)
		WiFiClient wifiClient;
	#endif
	EthernetClient ethClient;

int OSMqtt::_init(void) {
	Client * client = NULL;

    if (mqtt_client) { delete mqtt_client; mqtt_client = 0; }

	#if defined(ESP8266)
		if (m_server) client = &ethClient;
		else client = &wifiClient;
	#else
		client = &ethClient;
	#endif

	mqtt_client = new PubSubClient(*client);
	mqtt_client->setKeepAlive(MQTT_KEEPALIVE);

	if (mqtt_client == NULL) {
		DEBUG_LOGF("MQTT Init: Failed to initialise client\n");
		return MQTT_ERROR;
	}

	return MQTT_SUCCESS;
}

int OSMqtt::_connect(void) {
	mqtt_client->setServer(_host, _port);
	boolean state;
	if (_username[0])
		state = mqtt_client->connect(_id, _username, _password, MQTT_AVAILABILITY_TOPIC, 0, true, MQTT_OFFLINE_PAYLOAD);
	else
		state = mqtt_client->connect(_id, NULL, NULL, MQTT_AVAILABILITY_TOPIC, 0, true, MQTT_OFFLINE_PAYLOAD);
	if (state) {
		mqtt_client->publish(MQTT_AVAILABILITY_TOPIC, MQTT_ONLINE_PAYLOAD, true);
	} else {
		DEBUG_LOGF("MQTT Connect: Failed (%d)\n", mqtt_client->state());
		return MQTT_ERROR;
	}
	return MQTT_SUCCESS;
}

int OSMqtt::_disconnect(void) {
	mqtt_client->disconnect();
	return MQTT_SUCCESS;
}

bool OSMqtt::_connected(void) { return mqtt_client->connected(); }

int OSMqtt::_publish(const char *topic, const char *payload) {
	if (!mqtt_client->publish(topic, payload)) {
		DEBUG_LOGF("MQTT Publish: Failed (%d)\n", mqtt_client->state());
		return MQTT_ERROR;
	}
	return MQTT_SUCCESS;
}

int OSMqtt::_loop(void) {
	mqtt_client->loop();
	return mqtt_client->state();
}

const char * OSMqtt::_state_string(int rc) {
	switch (rc) {
		case MQTT_CONNECTION_TIMEOUT:		return "The server didn't respond within the keepalive time";
		case MQTT_CONNECTION_LOST:			return "The network connection was lost";
		case MQTT_CONNECT_FAILED:			return "The network connection failed";
		case MQTT_DISCONNECTED:				return "The client has cleanly disconnected";
		case MQTT_CONNECTED:				return "The client is connected";
		case MQTT_CONNECT_BAD_PROTOCOL:		return "The server doesn't support the requested version of MQTT";
		case MQTT_CONNECT_BAD_CLIENT_ID:	return "The server rejected the client identifier";
		case MQTT_CONNECT_UNAVAILABLE:		return "The server was unavailable to accept the connection";
		case MQTT_CONNECT_BAD_CREDENTIALS:	return "The username/password were rejected";
		case MQTT_CONNECT_UNAUTHORIZED:		return "The client was not authorized to connect";
		default:							return "Unrecognised state";
	}
}
#else

/************************** RASPBERRY PI / BBB / DEMO ****************************************/

static bool _connected = false;

static void _mqtt_connection_cb(struct mosquitto *mqtt_client, void *obj, int reason) {
	DEBUG_LOGF("MQTT Connnection Callback: %s (%d)\n", mosquitto_strerror(reason), reason);

	::_connected = true;

	if (reason == 0) {
		int rc = mosquitto_publish(mqtt_client, NULL, MQTT_AVAILABILITY_TOPIC, strlen(MQTT_ONLINE_PAYLOAD), MQTT_ONLINE_PAYLOAD, 0, true);
		if (rc != MOSQ_ERR_SUCCESS) {
			DEBUG_LOGF("MQTT Publish: Failed (%s)\n", mosquitto_strerror(rc));
		}
	}
}

static void _mqtt_disconnection_cb(struct mosquitto *mqtt_client, void *obj, int reason) {
	DEBUG_LOGF("MQTT Disconnnection Callback: %s (%d)\n", mosquitto_strerror(reason), reason);

	::_connected = false;
}

static void _mqtt_log_cb(struct mosquitto *mqtt_client, void *obj, int level, const char *message){
	if (level != MOSQ_LOG_DEBUG )
		DEBUG_LOGF("MQTT Log Callback: %s (%d)\n", message, level);
}

int OSMqtt::_init(void) {
	int major, minor, revision;

	mosquitto_lib_init();
	mosquitto_lib_version(&major, &minor, &revision);
	DEBUG_LOGF("MQTT Init: Mosquitto Library v%d.%d.%d\n", major, minor, revision);

	if (mqtt_client) { mosquitto_destroy(mqtt_client); mqtt_client = NULL; };

	mqtt_client = mosquitto_new("OS", true, NULL);
	if (mqtt_client == NULL) {
		DEBUG_PRINTF("MQTT Init: Failed to initialise client\n");
		return MQTT_ERROR;
	}

	mosquitto_connect_callback_set(mqtt_client, _mqtt_connection_cb);
	mosquitto_disconnect_callback_set(mqtt_client, _mqtt_disconnection_cb);
	mosquitto_log_callback_set(mqtt_client, _mqtt_log_cb);
	mosquitto_will_set(mqtt_client, MQTT_AVAILABILITY_TOPIC, strlen(MQTT_OFFLINE_PAYLOAD), MQTT_OFFLINE_PAYLOAD, 0, true);

	return MQTT_SUCCESS;
}

int OSMqtt::_connect(void) {
	int rc;
	if (_username[0]) {
		rc = mosquitto_username_pw_set(mqtt_client, _username, _password);
		if (rc != MOSQ_ERR_SUCCESS) {
			DEBUG_LOGF("MQTT Connect: Connection Failed (%s)\n", mosquitto_strerror(rc));
			return MQTT_ERROR;
		}
	}
	rc = mosquitto_connect(mqtt_client, _host, _port, MQTT_KEEPALIVE);
	if (rc != MOSQ_ERR_SUCCESS) {
		DEBUG_LOGF("MQTT Connect: Connection Failed (%s)\n", mosquitto_strerror(rc));
		return MQTT_ERROR;
	}

	// Allow 10ms for the Broker's ack to be received. We need this on start-up so that the
	// connection is registered before we attempt to send our first NOTIFY_REBOOT notification. 
	usleep(10000);

	return MQTT_SUCCESS;
}

int OSMqtt::_disconnect(void) {
	int rc = mosquitto_disconnect(mqtt_client);
	return rc == MOSQ_ERR_SUCCESS ? MQTT_SUCCESS : MQTT_ERROR;
}

bool OSMqtt::_connected(void) { return ::_connected; }

int OSMqtt::_publish(const char *topic, const char *payload) {
	int rc = mosquitto_publish(mqtt_client, NULL, topic, strlen(payload), payload, 0, false);
	if (rc != MOSQ_ERR_SUCCESS) {
		DEBUG_LOGF("MQTT Publish: Failed (%s)\n", mosquitto_strerror(rc));
		return MQTT_ERROR;
	}
	return MQTT_SUCCESS;
}

int OSMqtt::_loop(void) {
	return mosquitto_loop(mqtt_client, 0 , 1);
}

const char * OSMqtt::_state_string(int error) {
	return mosquitto_strerror(error);
}
#endif
