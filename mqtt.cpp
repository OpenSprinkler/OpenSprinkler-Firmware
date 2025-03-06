/* OpenSprinkler Unified Firmware
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
	#else
		#include <Ethernet.h>
	#endif
	#define MQTT_SOCKET_TIMEOUT 5
	#include <PubSubClient.h>

	struct PubSubClient *mqtt_client = NULL;

#else
	#include <time.h>
	#include <stdio.h>
	#include <mosquitto.h>

	struct mosquitto *mqtt_client = NULL;
#endif

#include "OpenSprinkler.h"
#include "opensprinkler_server.h"
#include "main.h"
#include "program.h"
#include "types.h"
#include "mqtt.h"
#include "ArduinoJson.hpp"

// Debug routines to help identify any blocking of the event loop for an extended period

#if defined(ENABLE_DEBUG)
	#if defined(ARDUINO)
		#include "TimeLib.h"
		#define DEBUG_TIMESTAMP(msg, ...) {time_os_t t = os.now_tz(); Serial.printf("%02d-%02d-%02d %02d:%02d:%02d - ", year(t), month(t), day(t), hour(t), minute(t), second(t));}
	#else
		#include <sys/time.h>
		#define DEBUG_TIMESTAMP()         {char tstr[21]; time_os_t t = time(NULL); struct tm *tm = localtime(&t); strftime(tstr, 21, "%y-%m-%d %H:%M:%S - ", tm);printf("%s", tstr);}
	#endif
	#define DEBUG_LOGF(msg, ...)        {DEBUG_TIMESTAMP(); DEBUG_PRINTF(msg, ##__VA_ARGS__);}

	static unsigned long _lastMillis = 0; // Holds the timestamp associated with the last call to DEBUG_DURATION()
	inline unsigned long DEBUG_DURATION() {unsigned long dur = millis() - _lastMillis; _lastMillis = millis(); return dur;}
#else
	#define DEBUG_LOGF(msg, ...)    {}
	#define DEBUG_DURATION()        {}
#endif

extern OpenSprinkler os;
extern ProgramData pd;
extern char tmp_buffer[];

#define OS_MQTT_KEEPALIVE      60
#define MQTT_DEFAULT_PORT    1883  // Default port for MQTT. Can be overwritten through App config
#define MQTT_MAX_HOST_LEN      50  // Maximum broker/host name length
#define MQTT_MAX_USERNAME_LEN  50  // Maximum username length
#define MQTT_MAX_PASSWORD_LEN 100  // Maximum password length
#define MQTT_MAX_TOPIC_LEN	   24  // Maximum topic length
#define MQTT_MAX_ID_LEN        16  // MQTT Client Id to uniquely reference this unit
#define MQTT_RECONNECT_DELAY  120  // Minumum of 60 seconds between reconnect attempts

#define MQTT_AVAILABILITY_TOPIC	"availability"
#define MQTT_ONLINE_PAYLOAD  "online"
#define MQTT_OFFLINE_PAYLOAD "offline"

#define MQTT_SUCCESS    0  // Returned when function operated successfully
#define MQTT_ERROR      1  // Returned whan function failed

char OSMqtt::_id[MQTT_MAX_ID_LEN + 1] = {0};     // Id to identify the client to the broker
char OSMqtt::_host[MQTT_MAX_HOST_LEN + 1] = {0}; // IP or host name of the broker
char OSMqtt::_username[MQTT_MAX_USERNAME_LEN + 1] = {0};  // username to connect to the broker
char OSMqtt::_password[MQTT_MAX_PASSWORD_LEN + 1] = {0};  // password to connect to the broker
int OSMqtt::_port = MQTT_DEFAULT_PORT;  // Port of the broker (default 1883)
bool OSMqtt::_enabled = false;          // Flag indicating whether MQTT is enabled
char OSMqtt::_pub_topic[MQTT_MAX_TOPIC_LEN + 1] = {0}; // topic for publishing data
char OSMqtt::_sub_topic[MQTT_MAX_TOPIC_LEN + 1] = {0}; // topic for subscribing
bool OSMqtt::_done_subscribed = false;		//Flag indicating if command topic has been subscribed to

//******************************** HELPER FUNCTIONS ********************************// 

extern uint16_t parse_listdata(char **p);
extern unsigned char findKeyVal (const char *str,char *strbuf, uint16_t maxlen,const char *key,bool key_in_pgm=false,uint8_t *keyfound=NULL);

//****************************** COMMAND ACTIONS ******************************//

//ensure command incudes correct password
boolean checkPassword(char* pw) {
	if (os.iopts[IOPT_IGNORE_PASSWORD])  return true;

	if(findKeyVal(pw, tmp_buffer, TMP_BUFFER_SIZE, PSTR("pw"), true)){
		if (os.password_verify(tmp_buffer)) return true;
	}else{
		DEBUG_LOGF("Device password not found.\r\n");
		return false;
	}

	DEBUG_LOGF("Device password verification Failed.\r\n");
	return false;
}

//handles /cv command
void changeValues(char *message){
	DEBUG_LOGF("Changing Values\r\n");
	#if defined(ESP8266)
		extern uint32_t reboot_timer;
	#endif

	if(findKeyVal(message, tmp_buffer, TMP_BUFFER_SIZE, PSTR("rsn"), true)){
		DEBUG_LOGF("Resetting all stations\r\n");
		reset_all_stations();
	}

	if(findKeyVal(message, tmp_buffer, TMP_BUFFER_SIZE, PSTR("rbt"), true)){
		DEBUG_LOGF("Rebooting\r\n");
		#if defined(ESP8266)
			os.status.safe_reboot = 0;
			reboot_timer = os.now_tz() + 1;
		#else
			os.reboot_dev(REBOOT_CAUSE_WEB);
		#endif
	}

	if(findKeyVal(message, tmp_buffer, TMP_BUFFER_SIZE, PSTR("en"), true)){
		if (tmp_buffer[0]=='1' && !os.status.enabled) os.enable();
		else if (tmp_buffer[0]=='0' && os.status.enabled) os.disable();
	}

	if(findKeyVal(message, tmp_buffer, TMP_BUFFER_SIZE, PSTR("rd"), true)){
		int rd = atoi(tmp_buffer);
		if(rd>0){
			os.nvdata.rd_stop_time = os.now_tz() + (unsigned long) rd * 3600;
			os.raindelay_start();
		}else if (rd==0){
			os.raindelay_stop();
		}
	}
}

//handles /cm command
void manualRun(char *message){
	int sid = -1;
	if(findKeyVal(message, tmp_buffer, TMP_BUFFER_SIZE, PSTR("sid"), true)){
		sid = atoi(tmp_buffer);
		if(sid < 0 || sid >= os.nstations){
			DEBUG_LOGF("Invalid station ID.\r\n");
			return;
		}
	}else{
		DEBUG_LOGF("No station ID found.\r\n");
		return;
	}

	unsigned char en = 0;
	if(findKeyVal(message, tmp_buffer, TMP_BUFFER_SIZE, PSTR("en"), true)){
		en = atoi(tmp_buffer);
	}else{
		DEBUG_LOGF("No enable bit found.\r\n");
		return;
	}

	uint16_t timer = 0;
	unsigned long curr_time = os.now_tz();
	if(en){
		if(findKeyVal(message, tmp_buffer, TMP_BUFFER_SIZE, PSTR("t"), true)){
			timer = (uint16_t)atol(tmp_buffer);
			if(timer==0 || timer>64800){
				DEBUG_LOGF("Time out of bounds.\r\n");
				return;
			}
			if((os.status.mas==sid+1) || (os.status.mas2==sid+1)){
				DEBUG_LOGF("Cannot independently schedule master.\r\n");
				return;
			}
			RuntimeQueueStruct *q = NULL;
			unsigned char sqi = pd.station_qid[sid];
			//check if station has schedule
			if(sqi!=0xFF){
				q = pd.queue+sqi;
			}else{
				q = pd.enqueue();
			}
			
			if(q){
				q->st = 0;
				q->dur = timer;
				q->sid = sid;
				q->pid = 99;
				schedule_all_stations(curr_time);
			}else{
				DEBUG_LOGF("Queue is full.\r\n");
				return;
			}
		}else{
			DEBUG_LOGF("No time value found.\r\n")
			return;
		}
	}else{
		unsigned char ssta = 0;
		if(findKeyVal(message, tmp_buffer, TMP_BUFFER_SIZE, PSTR("ssta"), true)){
			ssta = atoi(tmp_buffer);
		}
		RuntimeQueueStruct *q = pd.queue + pd.station_qid[sid];
		q->deque_time = curr_time;
		turn_off_station(sid, curr_time, ssta);
	}
	return;
}

//handles /mp command
void manual_start_program(unsigned char, unsigned char);
void programStart(char *message){
	if(!findKeyVal(message, tmp_buffer, TMP_BUFFER_SIZE, PSTR("pid"), true)){
		DEBUG_LOGF("Program ID missing.\r\n")
		return;
	}
	int pid = atoi(tmp_buffer);
	if(pid < 0 || pid >= pd.nprograms){
		DEBUG_LOGF("Program ID out of bounds.\r\n");
		return;
	}

	unsigned char uwt = 0;
	if(findKeyVal(message, tmp_buffer, TMP_BUFFER_SIZE, PSTR("uwt"), true)){
		if(tmp_buffer[0]=='1') uwt = 1;
	}

	reset_all_stations_immediate();

	manual_start_program(pid+1, uwt);
	return;
}

//handles /cr command
void runOnceProgram(char *message){
	char *pv;
	bool found = false;
	for(pv = message; (*pv) != 0 && pv<message+100; pv++){
		if(strncmp(pv, "t=[", 3)==0){
			found = true;
			break;
		}
	}
	if(!found){
		DEBUG_LOGF("No program definition found.\r\n");
		return;
	}
	pv+=3;

	reset_all_stations_immediate();

	unsigned char sid, bid, s;
	uint16_t dur;
	boolean match_found = false;
	for(sid = 0; sid < os.nstations; sid++){
		dur = parse_listdata(&pv);
		bid = sid >> 3;
		s = sid&0x07;

		if(dur > 0 && !(os.attrib_dis[bid]&(1<<s))){
			RuntimeQueueStruct *q = pd.enqueue();
			if(q){
				q->st = 0;
				q->dur = water_time_resolve(dur);
				q->pid = 254;
				q->sid = sid;
				match_found = true;
			}
		}
	}
	if(match_found){
		schedule_all_stations(os.now_tz());
		return;
	}
	return;
}

//****************************** MQTT FUNCTIONS ******************************//

// Initialise the client libraries and event handlers.
void OSMqtt::init(void) {
	DEBUG_LOGF("MQTT Init\r\n");

	uint8_t mac[6] = {0};
	#if defined(ESP8266)
	os.load_hardware_mac(mac, useEth);
	#else
	os.load_hardware_mac(mac, true);
	#endif
	snprintf(_id, MQTT_MAX_ID_LEN, "OS-%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	_id[MQTT_MAX_ID_LEN] = 0;

	_init();
};

// Start the MQTT service and connect to the MQTT broker using the stored configuration.
void OSMqtt::begin(void) {
	DEBUG_LOGF("MQTT Begin\r\n");
	_port = MQTT_DEFAULT_PORT;
	_enabled = 0;
	_done_subscribed = false;
	_host[0] = 0;
	_username[0] = 0;
	_password[0] = 0;
	_pub_topic[0] = 0;
	_sub_topic[0] = 0;

	// JSON configuration settings in the form of {"en":0|1,"host":"server_name|IP address","port":1883,"user:"","pass":"","pubt":"","subt":""}
	char *config = tmp_buffer + 1;
	os.sopt_load(SOPT_MQTT_OPTS, config);

	if(*config != 0) {
		// Add the wrapping curly braces to the string
		config = tmp_buffer;
		config[0] = '{';
		int len = strlen(config);
		config[len] = '}';
		config[len+1] = 0;

		ArduinoJson::JsonDocument doc;
		ArduinoJson::DeserializationError error = ArduinoJson::deserializeJson(doc, config);

		// Test the parsing otherwise parse
		if (error) {
				DEBUG_PRINT(F("mqtt: deserializeJson() failed: "));
				DEBUG_PRINTLN(error.c_str());
		} else {
				_enabled = (bool)doc["en"];
				const char *host_val = doc["host"];
				if(host_val) strncpy(_host, host_val, MQTT_MAX_HOST_LEN);
				_port = doc["port"];
				const char *username_val = doc["user"];
				if(username_val) strncpy(_username, username_val, MQTT_MAX_USERNAME_LEN);
				const char *password_val = doc["pass"];
				if(password_val) strncpy(_password, password_val, MQTT_MAX_PASSWORD_LEN);
				const char *pubt_val = doc["pubt"];
				if(pubt_val) strncpy(_pub_topic, pubt_val, MQTT_MAX_TOPIC_LEN);
				const char *subt_val = doc["subt"];
				if(subt_val) strncpy(_sub_topic, subt_val, MQTT_MAX_TOPIC_LEN);
		}

		// properly end all strings to make sure 
		_host[MQTT_MAX_HOST_LEN] = 0;
		_username[MQTT_MAX_USERNAME_LEN] = 0;
		_password[MQTT_MAX_PASSWORD_LEN] = 0;
		_pub_topic[MQTT_MAX_TOPIC_LEN] = 0;
		_sub_topic[MQTT_MAX_TOPIC_LEN] = 0;
	}

	if(_pub_topic[0] == 0) { // publish topic is empty
		DEBUG_LOGF("No pub_topic found\r\n");
		strcpy_P(_pub_topic, PSTR("opensprinkler"));
	}

	if(_sub_topic[0] == 0) { // subscribe topic is empty
		DEBUG_LOGF("No sub_topic found\r\n");
	}

	DEBUG_LOGF("MQTT Begin: Config (%s:%d %s) %s\r\n", _host, _port, _username, _enabled ? "Enabled" : "Disabled");

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
	DEBUG_LOGF("MQTT Publish: %s %s\r\n", topic, payload);

	if (mqtt_client == NULL || !_enabled || os.status.network_fails > 0) return;

	if (!_connected()) {
		DEBUG_LOGF("MQTT Publish: Not connected\r\n");
		return;
	}

	_publish(topic, payload);
}

//Subscribe to a specific topic
void OSMqtt::subscribe(void){
	if(_sub_topic[0] == 0) { _done_subscribed = true; return; }

	if (mqtt_client == NULL || !_enabled || os.status.network_fails > 0) return;

	if (!_connected()) {
		return;
	}
	DEBUG_LOGF("MQTT Subscribe: %s\r\n", _sub_topic);
	_done_subscribed = true;
	_subscribe();
}

// Regularly call the loop function to ensure "keep alive" messages are sent to the broker and to reconnect if needed.
void OSMqtt::loop(void) {
	static unsigned long last_reconnect_attempt = 0;

	if (mqtt_client == NULL || !_enabled || os.status.network_fails > 0) return;

	// Only attemp to reconnect every MQTT_RECONNECT_DELAY seconds to avoid blocking the main loop
	if (!_connected() && (millis() - last_reconnect_attempt >= MQTT_RECONNECT_DELAY * 1000UL)) {
		DEBUG_LOGF("MQTT Loop: Reconnecting\r\n");
		_done_subscribed = false;
		_connect();
		last_reconnect_attempt = millis();
	}

	if(!_done_subscribed){
		subscribe();
	}

#if defined(ENABLE_DEBUG)
	int state = _loop();
#else
	(void) _loop();
#endif

#if defined(ENABLE_DEBUG)
	// Print a diagnostic message whenever the MQTT state changes
	bool network = os.network_connected(), mqtt = _connected();
	static bool last_network = 0, last_mqtt = 0;
	static int last_state = 999;

	if (last_state != state || last_network != network || last_mqtt != mqtt) {
		DEBUG_LOGF("MQTT Loop: Network %s, MQTT %s, State - %s\r\n",
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
	#else
		EthernetClient ethClient;
	#endif

int OSMqtt::_init(void) {
	Client * client = NULL;

	if (mqtt_client) { delete mqtt_client; mqtt_client = 0; }

	#if defined(ESP8266)
		client = &wifiClient;
	#else
		client = &ethClient;
	#endif

	mqtt_client = new PubSubClient(*client);
	mqtt_client->setKeepAlive(OS_MQTT_KEEPALIVE);

	if (mqtt_client == NULL) {
		DEBUG_LOGF("MQTT Init: Failed to initialise client\r\n");
		return MQTT_ERROR;
	}

	return MQTT_SUCCESS;
}

int OSMqtt::_connect(void) {
	mqtt_client->setServer(_host, _port);
	boolean state;
	#define MQTT_CONNECT_NTRIES 2
	unsigned char tries = 0;
	String avail_topic(_pub_topic);
	avail_topic += "/";
	avail_topic += MQTT_AVAILABILITY_TOPIC;
	do {
		DEBUG_PRINT(F("mqtt: "));
		DEBUG_PRINTLN(_host);
		if (_username[0])
			state = mqtt_client->connect(_id, _username, _password, avail_topic.c_str(), 0, true, MQTT_OFFLINE_PAYLOAD);
		else
			state = mqtt_client->connect(_id, NULL, NULL, avail_topic.c_str(), 0, true, MQTT_OFFLINE_PAYLOAD);
		if(state) break;
		tries++;
	} while(tries<MQTT_CONNECT_NTRIES);

	if(tries==MQTT_CONNECT_NTRIES) {
		DEBUG_LOGF("MQTT Connect: Failed (%d)\r\n", mqtt_client->state());
		return MQTT_ERROR;
	} else {
		mqtt_client->publish(avail_topic.c_str(), MQTT_ONLINE_PAYLOAD, true);
	}
	return MQTT_SUCCESS;
}

int OSMqtt::_disconnect(void) {
	mqtt_client->disconnect();
	return MQTT_SUCCESS;
}

bool OSMqtt::_connected(void) { return mqtt_client->connected(); }

int OSMqtt::_publish(const char *topic, const char *payload) {
	String total_topic(_pub_topic); // concatenate root topic with specific topic
	total_topic += "/";
	total_topic += topic;
	if (!mqtt_client->publish(total_topic.c_str(), payload)) {
		DEBUG_LOGF("MQTT Publish: Failed (%d)\r\n", mqtt_client->state());
		return MQTT_ERROR;
	}
	return MQTT_SUCCESS;
}

void subscribe_callback(const char *topic, unsigned char *payload, unsigned int length) {
	DEBUG_LOGF("Subscribe Callback\r\n");
	payload[length] = 0; // properly end the message
	char* message = (char*)payload;
	if(!checkPassword(message)){
		return;
	}

	if(message[0]=='c'){
		if(message[1]=='v'){
			changeValues(message);
		}else if(message[1]=='m'){
			manualRun(message);
		}else if(message[1]=='r'){
			runOnceProgram(message);
		}
	}else if(message[0]=='m' && message[1]=='p'){
		programStart(message);
	}else{
		DEBUG_LOGF("Unsupported mqtt subscribe request\r\n");
		return;
	}
}

int OSMqtt::_subscribe(void){
	mqtt_client->setCallback(subscribe_callback);
	if (!mqtt_client->subscribe(_sub_topic)) {
		DEBUG_LOGF("MQTT Subscribe: Failed (%d)\r\n", mqtt_client->state());
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
		case MQTT_CONNECTION_TIMEOUT:  return "The server didn't respond within the keepalive time";
		case MQTT_CONNECTION_LOST:     return "The network connection was lost";
		case MQTT_CONNECT_FAILED:      return "The network connection failed";
		case MQTT_DISCONNECTED:        return "The client has cleanly disconnected";
		case MQTT_CONNECTED:           return "The client is connected";
		case MQTT_CONNECT_BAD_PROTOCOL: return "The server doesn't support the requested version of MQTT";
		case MQTT_CONNECT_BAD_CLIENT_ID: return "The server rejected the client identifier";
		case MQTT_CONNECT_UNAVAILABLE:  return "The server was unavailable to accept the connection";
		case MQTT_CONNECT_BAD_CREDENTIALS: return "The username/password were rejected";
		case MQTT_CONNECT_UNAUTHORIZED: return "The client was not authorized to connect";
		default:  return "Unrecognised state";
	}
}
#else

/************************** RASPBERRY PI / Linux ****************************************/

static bool _connected = false;

static void _mqtt_connection_cb(struct mosquitto *mqtt_client, void *obj, int reason) {
	DEBUG_LOGF("MQTT Connnection Callback: %s (%d)\r\n", mosquitto_strerror(reason), reason);

	::_connected = true;
	
	String avail_topic(OSMqtt::get_pub_topic());
	avail_topic += "/";
	avail_topic += MQTT_AVAILABILITY_TOPIC;

	if (reason == 0) {
		int rc = mosquitto_publish(mqtt_client, NULL, avail_topic.c_str(), strlen(MQTT_ONLINE_PAYLOAD), MQTT_ONLINE_PAYLOAD, 0, true);
		if (rc != MOSQ_ERR_SUCCESS) {
			DEBUG_LOGF("MQTT Publish: Failed (%s)\r\n", mosquitto_strerror(rc));
		}
	}
}

static void _mqtt_disconnection_cb(struct mosquitto *mqtt_client, void *obj, int reason) {
	DEBUG_LOGF("MQTT Disconnnection Callback: %s (%d)\r\n", mosquitto_strerror(reason), reason);

	::_connected = false;
}

static void _mqtt_log_cb(struct mosquitto *mqtt_client, void *obj, int level, const char *message){
	if (level != MOSQ_LOG_DEBUG )
		DEBUG_LOGF("MQTT Log Callback: %s (%d)\r\n", message, level);
}

int OSMqtt::_init(void) {
	int major, minor, revision;

	mosquitto_lib_init();
	mosquitto_lib_version(&major, &minor, &revision);
	DEBUG_LOGF("MQTT Init: Mosquitto Library v%d.%d.%d\r\n", major, minor, revision);

	if (mqtt_client) { mosquitto_destroy(mqtt_client); mqtt_client = NULL; };

	mqtt_client = mosquitto_new("OS", true, NULL);
	if (mqtt_client == NULL) {
		DEBUG_PRINTF("MQTT Init: Failed to initialise client\r\n");
		return MQTT_ERROR;
	}

	mosquitto_connect_callback_set(mqtt_client, _mqtt_connection_cb);
	mosquitto_disconnect_callback_set(mqtt_client, _mqtt_disconnection_cb);
	mosquitto_log_callback_set(mqtt_client, _mqtt_log_cb);
	String avail_topic(_id);
	avail_topic += "/";
	avail_topic += MQTT_AVAILABILITY_TOPIC;
	DEBUG_LOGF("%s\n", avail_topic.c_str());
	mosquitto_will_set(mqtt_client, avail_topic.c_str(), strlen(MQTT_OFFLINE_PAYLOAD), MQTT_OFFLINE_PAYLOAD, 0, true);

	return MQTT_SUCCESS;
}

int OSMqtt::_connect(void) {
	int rc;
	if (_username[0]) {
		rc = mosquitto_username_pw_set(mqtt_client, _username, _password);
		if (rc != MOSQ_ERR_SUCCESS) {
			DEBUG_LOGF("MQTT Connect: Connection Failed (%s)\r\n", mosquitto_strerror(rc));
			return MQTT_ERROR;
		}
	}
	rc = mosquitto_connect(mqtt_client, _host, _port, OS_MQTT_KEEPALIVE);
	if (rc != MOSQ_ERR_SUCCESS) {
		DEBUG_LOGF("MQTT Connect: Connection Failed (%s)\r\n", mosquitto_strerror(rc));
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
	String total_topic(_pub_topic); // concatenate root topic with specific topic
	total_topic += "/";
	total_topic += topic;
	int rc = mosquitto_publish(mqtt_client, NULL, total_topic.c_str(), strlen(payload), payload, 0, false);
	if (rc != MOSQ_ERR_SUCCESS) {
		DEBUG_LOGF("MQTT Publish: Failed (%s)\r\n", mosquitto_strerror(rc));
		return MQTT_ERROR;
	}
	return MQTT_SUCCESS;
}

void subscribe_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message){
	DEBUG_LOGF("Callback\r\n");
	char *topic = message->topic;
	char *msg = (char*)(message->payload);

	if(!checkPassword(msg)){
		return;
	}

	if(msg[0]=='c'){
		if(msg[1]=='v'){
			changeValues(msg);
		}else if(msg[1]=='m'){
			manualRun(msg);
		}else if(msg[1]=='r'){
			runOnceProgram(msg);
		}
	}else if(msg[0]=='m' && msg[1]=='p'){
		programStart(msg);
	}else{
		DEBUG_LOGF("Invalid request\r\n");
		return;
	}
}

int OSMqtt::_subscribe(void) {
	mosquitto_message_callback_set(mqtt_client, subscribe_callback);
	int rc = mosquitto_subscribe(mqtt_client, NULL, _sub_topic, 0);
	if (rc != MOSQ_ERR_SUCCESS) {
		DEBUG_LOGF("MQTT Subscribe: Failed (%s)\r\n", mosquitto_strerror(rc));
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
