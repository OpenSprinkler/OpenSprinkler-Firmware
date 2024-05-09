#if defined(OSPI)
/**
 * @file WebSockets.h
 * @date 20.05.2015
 * @author Markus Sattler
 *
 * Copyright (c) 2015 Markus Sattler. All rights reserved.
 * This file is part of the WebSockets for Arduino.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef WEBSOCKETS_H_
#define WEBSOCKETS_H_

#include <functional>
#include <cstdint>
#include <defines.h>
#include "WebSocketsVersion.h"
#include "etherport.h"
#include "wscompat.h"
#include "libsha1.h"
#include "cencode_inc.h"

#ifndef DEBUG_WEBSOCKETS
#define DEBUG_WEBSOCKETS(msg, ...)    {printf(msg, ##__VA_ARGS__);}
//#ifndef NODEBUG_WEBSOCKETS
//#define NODEBUG_WEBSOCKETS
//#endif
#endif

#define WEBSOCKETS_MAX_DATA_SIZE (15 * 1024)
#define WEBSOCKETS_USE_BIG_MEM
#define GET_FREE_HEAP not for linux
#define WEBSOCKETS_YIELD() sched_yield() 
#define WEBSOCKETS_YIELD_MORE() delay(1)

#ifndef WEBSOCKETS_TCP_TIMEOUT
#define WEBSOCKETS_TCP_TIMEOUT (5000)
#endif

#define NETWORK_ESP8266_ASYNC (0)
#define NETWORK_ESP8266 (1)
#define NETWORK_W5100 (2)
#define NETWORK_ENC28J60 (3)
#define NETWORK_ESP32 (4)
#define NETWORK_ESP32_ETH (5)
#define NETWORK_RP2040 (6)
#define NETWORK_LINUX (7)

#define WEBSOCKETS_NETWORK_TYPE NETWORK_LINUX

// Network Class
#define WEBSOCKETS_NETWORK_CLASS EthernetClient
//#define WEBSOCKETS_NETWORK_SSL_CLASS EthernetClientSsl
#define WEBSOCKETS_NETWORK_SERVER_CLASS EthernetServer

// max size of the WS Message Header
#define WEBSOCKETS_MAX_HEADER_SIZE (14)

#ifdef WEBSOCKETS_NETWORK_SSL_CLASS
#define HAS_SSL
#endif

#define WEBSOCKETS_STRING(var) var

#define bit(b) (1UL << (b))    // Taken directly from Arduino.h

typedef enum {
    WSC_NOT_CONNECTED,
    WSC_HEADER,
    WSC_BODY,
    WSC_CONNECTED
} WSclientsStatus_t;

typedef enum {
    WStype_ERROR,
    WStype_DISCONNECTED,
    WStype_CONNECTED,
    WStype_TEXT,
    WStype_BIN,
    WStype_FRAGMENT_TEXT_START,
    WStype_FRAGMENT_BIN_START,
    WStype_FRAGMENT,
    WStype_FRAGMENT_FIN,
    WStype_PING,
    WStype_PONG,
} WStype_t;

typedef enum {
    WSop_continuation = 0x00,    ///< %x0 denotes a continuation frame
    WSop_text         = 0x01,    ///< %x1 denotes a text frame
    WSop_binary       = 0x02,    ///< %x2 denotes a binary frame
                                 ///< %x3-7 are reserved for further non-control frames
    WSop_close = 0x08,           ///< %x8 denotes a connection close
    WSop_ping  = 0x09,           ///< %x9 denotes a ping
    WSop_pong  = 0x0A            ///< %xA denotes a pong
                                 ///< %xB-F are reserved for further control frames
} WSopcode_t;

typedef struct {
    bool fin;
    bool rsv1;
    bool rsv2;
    bool rsv3;

    WSopcode_t opCode;
    bool mask;

    size_t payloadLen;

    uint8_t * maskKey;
} WSMessageHeader_t;

typedef struct {
    void init(uint8_t num,
        uint32_t pingInterval,
        uint32_t pongTimeout,
        uint8_t disconnectTimeoutCount) {
        this->num                    = num;
        this->pingInterval           = pingInterval;
        this->pongTimeout            = pongTimeout;
        this->disconnectTimeoutCount = disconnectTimeoutCount;
    }

    uint8_t num = 0;    ///< connection number

    WSclientsStatus_t status = WSC_NOT_CONNECTED;

    WEBSOCKETS_NETWORK_CLASS * tcp = nullptr;

    bool isSocketIO = false;    ///< client for socket.io server

#if defined(HAS_SSL)
    bool isSSL = false;    ///< run in ssl mode
    WEBSOCKETS_NETWORK_SSL_CLASS * ssl;
#endif

    String cUrl;           ///< http url
    uint16_t cCode = 0;    ///< http code

    bool cIsClient    = false;    ///< will be used for masking
    bool cIsUpgrade   = false;    ///< Connection == Upgrade
    bool cIsWebsocket = false;    ///< Upgrade == websocket

    String cSessionId;        ///< client Set-Cookie (session id)
    String cKey;              ///< client Sec-WebSocket-Key
    String cAccept;           ///< client Sec-WebSocket-Accept
    String cProtocol;         ///< client Sec-WebSocket-Protocol
    String cExtensions;       ///< client Sec-WebSocket-Extensions
    uint16_t cVersion = 0;    ///< client Sec-WebSocket-Version

    uint8_t cWsRXsize = 0;                            ///< State of the RX
    uint8_t cWsHeader[WEBSOCKETS_MAX_HEADER_SIZE];    ///< RX WS Message buffer
    WSMessageHeader_t cWsHeaderDecode;

    String base64Authorization;    ///< Base64 encoded Auth request
    String plainAuthorization;     ///< Base64 encoded Auth request

    String extraHeaders;

    bool cHttpHeadersValid = false;    ///< non-websocket http header validity indicator
    size_t cMandatoryHeadersCount;     ///< non-websocket mandatory http headers present count

    bool pongReceived              = false;
    uint32_t pingInterval          = 0;    // how often ping will be sent, 0 means "heartbeat is not active"
    uint32_t lastPing              = 0;    // millis when last pong has been received
    uint32_t pongTimeout           = 0;    // interval in millis after which pong is considered to timeout
    uint8_t disconnectTimeoutCount = 0;    // after how many subsequent pong timeouts discconnect will happen, 0 means "do not disconnect"
    uint8_t pongTimeoutCount       = 0;    // current pong timeout count

#if(WEBSOCKETS_NETWORK_TYPE == NETWORK_ESP8266_ASYNC)
    String cHttpLine;    ///< HTTP header lines
#endif

} WSclient_t;

class WebSockets {
  protected:
#ifdef __AVR__
    typedef void (*WSreadWaitCb)(WSclient_t * client, bool ok);
#else
    typedef std::function<void(WSclient_t * client, bool ok)> WSreadWaitCb;
#endif

    virtual void clientDisconnect(WSclient_t * client)  = 0;
    virtual bool clientIsConnected(WSclient_t * client) = 0;

    void clientDisconnect(WSclient_t * client, uint16_t code, char * reason = NULL, size_t reasonLen = 0);

    virtual void messageReceived(WSclient_t * client, WSopcode_t opcode, uint8_t * payload, size_t length, bool fin) = 0;

    uint8_t createHeader(uint8_t * buf, WSopcode_t opcode, size_t length, bool mask, uint8_t maskKey[4], bool fin);
    bool sendFrameHeader(WSclient_t * client, WSopcode_t opcode, size_t length = 0, bool fin = true);
    bool sendFrame(WSclient_t * client, WSopcode_t opcode, uint8_t * payload = NULL, size_t length = 0, bool fin = true, bool headerToPayload = false);

    void headerDone(WSclient_t * client);

    void handleWebsocket(WSclient_t * client);

    bool handleWebsocketWaitFor(WSclient_t * client, size_t size);
    void handleWebsocketCb(WSclient_t * client);
    void handleWebsocketPayloadCb(WSclient_t * client, bool ok, uint8_t * payload);

    String acceptKey(String & clientKey);
    String base64_encode(uint8_t * data, size_t length);

    bool readCb(WSclient_t * client, uint8_t * out, size_t n, WSreadWaitCb cb);
    virtual size_t write(WSclient_t * client, uint8_t * out, size_t n);
    size_t write(WSclient_t * client, const char * out);

    void enableHeartbeat(WSclient_t * client, uint32_t pingInterval, uint32_t pongTimeout, uint8_t disconnectTimeoutCount);
    void handleHBTimeout(WSclient_t * client);
};

#ifndef UNUSED
#define UNUSED(var) (void)(var)
#endif
#endif /* WEBSOCKETS_H_ */
#endif