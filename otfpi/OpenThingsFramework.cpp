#if defined(OSPI)
#include "OpenThingsFramework.h"
#include "StringBuilder.h"
#include "defines.h"

// The timeout for reading and parsing incoming requests.
#define WIFI_CONNECTION_TIMEOUT 1500
/* How often to try to reconnect to the websocket if the connection is lost. Each reconnect attempt is blocking and has
 * a 5 second timeout.
 */
#define WEBSOCKET_RECONNECT_INTERVAL 30000
#define strncmp_P strncmp
using namespace OTF;

OpenThingsFramework::OpenThingsFramework(uint16_t webServerPort, char *hdBuffer, int hdBufferSize) : localServer(webServerPort) {
  DEBUG_PRINTLN("Instantiating OTF...");
  if(hdBuffer != NULL) { // if header buffer is externally provided, use it directly
    headerBuffer = hdBuffer;
    headerBufferSize = (hdBufferSize > 0) ? hdBufferSize : HEADERS_BUFFER_SIZE;
  } else { // otherwise allocate one
    headerBuffer = new char[HEADERS_BUFFER_SIZE];
    headerBufferSize = HEADERS_BUFFER_SIZE;
  }
  missingPageCallback = defaultMissingPageCallback;
  localServer.begin();
}

OpenThingsFramework::OpenThingsFramework(uint16_t webServerPort, const String &webSocketHost, uint16_t webSocketPort,
                                         const String &deviceKey, bool useSsl, char *hdBuffer, int hdBufferSize) : OpenThingsFramework(webServerPort, hdBuffer, hdBufferSize) {
  setCloudStatus(UNABLE_TO_CONNECT);
  DEBUG_PRINTLN(F("Initializing websocket..."));
  webSocket = new WebSocketsClient();
  if (useSsl) {
    DEBUG_PRINTLN(F("Connecting to websocket with SSL"));
    //webSocket->beginSSL(webSocketHost, webSocketPort, "/socket/v1?deviceKey=" + deviceKey);
  } else {
    DEBUG_PRINTLN(F("Connecting to websocket without SSL"));
    webSocket->begin(webSocketHost, webSocketPort, "/socket/v1?deviceKey=" + deviceKey);
  }
  DEBUG_PRINTLN(F("Initialized websocket"));

  // Wrap the member function in a static function.
  webSocket->onEvent([this](WStype_t type, uint8_t *payload, size_t length) -> void {
    webSocketCallback(type, payload, length);
  });

  webSocket->setReconnectInterval(WEBSOCKET_RECONNECT_INTERVAL);
  // Ping the server every 15 seconds with a timeout of 5 seconds, and treat 1 missed ping as a lost connection.
  webSocket->enableHeartbeat(15000, 5000, 1);
}

char *makeMapKey(StringBuilder *sb, HTTPMethod method, const char *path) {
  sb->bprintf(F("%d%s"), method, path);
  return sb->toString();
}

void OpenThingsFramework::on(const char *path, callback_t callback, HTTPMethod method) {
  callbacks.add(makeMapKey(new StringBuilder(KEY_MAX_LENGTH), method, path), callback);
}

//void OpenThingsFramework::on(const __FlashStringHelper *path, callback_t callback, HTTPMethod method) {
//  callbacks.add(makeMapKey(new StringBuilder(KEY_MAX_LENGTH), method, (char *) path), callback);
//}

void OpenThingsFramework::onMissingPage(callback_t callback) {
  missingPageCallback = callback;
}

void OpenThingsFramework::localServerLoop() {

  static ulong wait_to = 0; // timeout to wait for client data
  if (!wait_to) {
    localClient = localServer.acceptClient();
    // If a client wasn't available from the server, exit the local server loop.
    if (!localClient) {
      return;
    }
    // set a timeout to wait for client data
    wait_to = millis()+WIFI_CONNECTION_TIMEOUT;
  }
  DEBUG_PRINT("wait_to=");
  DEBUG_PRINTLN(wait_to);
  if (!localClient->dataAvailable()) {
    // If data isn't available from the client yet, exit the local server loop and check again next iteration.
    // but if we reached timeout, then reset wait_to to 0 and flush localClient so we can accept new client
    if(millis()>wait_to) {
      wait_to=0;
      DEBUG_PRINTLN(F("client wait timeout"));
      localClient->flush();
      localClient->stop();
    }
    return;
  }

  // got new client data, reset wait_to to 0
  wait_to = 0;

  DEBUG_PRINTLN("incoming localServer request");

  // Update the timeout for each data read to ensure that the total timeout is WIFI_CONNECTION_TIMEOUT.
  ulong timeout = millis()+WIFI_CONNECTION_TIMEOUT;


  char *buffer = headerBuffer;
  size_t length = 0;
  while (localClient->dataAvailable() && millis() < timeout) {
    if (length >= (size_t)headerBufferSize) {
      localClient->print(F("HTTP/1.1 413 Request too large\r\n\r\nThe request was too large"));
      // Get a new client to indicate that the previous client is no longer needed.
      localClient = localServer.acceptClient();
      DEBUG_PRINTLN("incoming localServer request - 1");
      return;
    }

    size_t read = localClient->readBytesUntil('\n', &buffer[length], min((int) (headerBufferSize - length - 1), headerBufferSize));
    DEBUG_PRINTF("read=%d", read);
    char rc = buffer[length];
    length += read;
    buffer[length++] = '\n';
    if(read==1 && rc=='\r') { break; }
  }
    
  buffer[length] = 0;
  DEBUG_PRINTF((char *) F("Finished reading data from client. Request line + headers were %d bytes\n"), length);
  DEBUG_PRINTF((char *) F("Request: %s\n"), (char *) buffer);

  // Make sure that the headers were fully read into the buffer.
  if (strncmp_P(&buffer[length - 4], (char *) F("\r\n\r\n"), 4) != 0) {
    DEBUG_PRINTLN(F("The request headers were not fully read into the buffer."));
    localClient->print(F("HTTP/1.1 413 Request too large\r\n\r\nThe request was too large"));
    return;
  }

  
  DEBUG_PRINTLN(F("Parsing request"));
  Request request(buffer, length, false);

  char *bodyBuffer = NULL;
  // If the request was valid, read the body and add it to the Request object.
  if (request.getType() > INVALID) {
    char *contentLengthString = request.getHeader(F("content-length"));
    // If the header was not specified, the message has no body.
    if (contentLengthString != nullptr) {
      long contentLength = atol(contentLengthString);
      // If the header specifies a length of 0 or could not be parsed, the message has no body.
      if (contentLength > 0) {
        // Read the body from the client.
        bodyBuffer = new char[contentLength];
        size_t bodyLength = 0;
        timeout = millis()+WIFI_CONNECTION_TIMEOUT;
        while (localClient->dataAvailable() && millis()<timeout) {
          size_t read = localClient->readBytes(&bodyBuffer[bodyLength], min((int) (contentLength - bodyLength), 1024));
          bodyLength += read;
        }
        bodyBuffer[bodyLength] = 0;
        request.body = bodyBuffer;
        request.bodyLength = bodyLength;
      }
    }
  }

  DEBUG_PRINTLN(F("Filling response"));
  Response res = Response(localClient);
  fillResponse(request, res);

  if(bodyBuffer) delete[] bodyBuffer;
  DEBUG_PRINTLN(F("Sending response"));
  if (res.isValid()) {
    char *responseString = res.toString();
    DEBUG_PRINTF((char *) F("Response message is: %s\n"), responseString);
    localClient->print(responseString);
  } else {
    localClient->print(F("HTTP/1.1 500 OTF error\r\nResponse string could not be built\r\n"));
    DEBUG_PRINTLN(F("An error occurred while building the response string."));
  }
  localClient->flush();
  localClient->stop();

  DEBUG_PRINTLN(F("Finished handling request"));

  // Get a new client to indicate that the previous client is no longer needed.
  localClient = localServer.acceptClient();
  if (localClient)
    wait_to = millis()+WIFI_CONNECTION_TIMEOUT;
}

void OpenThingsFramework::loop() {
  localServerLoop();
  if (webSocket != nullptr) {
    webSocket->loop();
  }
}

void OpenThingsFramework::webSocketCallback(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      DEBUG_PRINTLN(F("Disconnected from websocket"));
      /* A failed attempt to connect will also fire a WStype_DISCONNECTED event, so this check is
       * needed to prevent the UNABLE_TO_CONNECT status to be overwritten. */
      if (cloudStatus == CONNECTED) {
        setCloudStatus(DISCONNECTED);
      }
      break;
    case WStype_CONNECTED:
      DEBUG_PRINTLN(F("Connected to websocket"));
      setCloudStatus(CONNECTED);
      break;
    case WStype_TEXT: {
      DEBUG_PRINTF((char *) F("Received a websocket message of length %d\n"), length);
      char *message = (char *) payload;

#define PREFIX_LENGTH 5
#define ID_LENGTH 4
// Length of the prefix, request ID, carriage return, and line feed.
#define HEADER_LENGTH PREFIX_LENGTH + ID_LENGTH + 2
      if (strncmp_P(message, (char *) F("FWD: "), PREFIX_LENGTH) == 0) {
        DEBUG_PRINTLN(F("Message is a forwarded request."));
        char *requestId = &message[PREFIX_LENGTH];
        // Replace the assumed carriage return with a null character to terminate the ID string.
        requestId[ID_LENGTH] = '\0';

        Request request(&message[HEADER_LENGTH], length - HEADER_LENGTH, true);
        Response res = Response(webSocket);
        res.bprintf(F("RES: %s\r\n"), requestId);
        fillResponse(request, res);

        if (res.isValid()) {
          webSocket->sendTXT(res.toString(), res.getLength(), true, false);
        } else {
          DEBUG_PRINTLN(F("An error occurred building response string"));
          StringBuilder builder(100);
          builder.bprintf(F("RES: %s\r\n%s"), requestId,
                          F("HTTP/1.1 500 Internal Error\r\n\r\nAn internal error occurred"));
          if (!builder.isValid()) {
            DEBUG_PRINTLN(F("Builder is not valid"));
            return;
          }
        }
      } else {
        DEBUG_PRINTLN(F("Websocket message does not start with the correct prefix."));
      }
      break;
    }
    case WStype_PING:
    case WStype_PONG:
      // Pings/pongs will be automatically handled by the websockets library.
      break;
    case WStype_ERROR:
      DEBUG(Serial.print(F("Websocket error: "));)
      DEBUG_PRINTLN((char *) payload);
      break;
    default:
      DEBUG_PRINTF((char *) F("Received unsupported websocket event of type %d\n"), type);
      break;
  }
}

void OpenThingsFramework::fillResponse(const Request &req, Response &res) {
  if (req.getType() == INVALID) {
    res.writeStatus(400, F("Invalid request"));
    res.writeHeader(F("content-type"), F("text/plain"));
    res.writeBodyChunk(F("Could not parse request"));
    return;
  }

  // TODO handle trailing slash in path?
  DEBUG_PRINTF((char *) F("Attempting to route request to path '%s'\n"), req.getPath());
  StringBuilder *sb = new StringBuilder(KEY_MAX_LENGTH);
  char *key = makeMapKey(sb, req.httpMethod, req.getPath());
  callback_t callback = callbacks.find(key);

  // If there isn't a callback for the specific method, check if there's one for any method.
  if (callback == nullptr) {
    delete sb;
    sb = new StringBuilder(KEY_MAX_LENGTH);

    callback = callbacks.find(makeMapKey(sb, HTTP_ANY, req.getPath()));
  }

  delete sb;

  if (callback != nullptr) {
    DEBUG_PRINTLN(F("Found callback"));
    callback(req, res);
  } else {
    // Run the missing page callback if none of the registered paths matched.
    missingPageCallback(req, res);
  }
}

void OpenThingsFramework::defaultMissingPageCallback(const Request &req, Response &res) {
  res.writeStatus(404, F("Not found"));
  res.writeHeader(F("content-type"), F("text/plain"));
  res.writeBodyChunk(F("The requested page does not exist"));
}

void OpenThingsFramework::setCloudStatus(CLOUD_STATUS status) {
  this->cloudStatus = status;
  lastCloudStatusChangeTime = millis();
}

CLOUD_STATUS OpenThingsFramework::getCloudStatus() {
  return cloudStatus;
}

unsigned long OpenThingsFramework::getTimeSinceLastCloudStatusChange() {
  return millis() - lastCloudStatusChangeTime;
}
#endif