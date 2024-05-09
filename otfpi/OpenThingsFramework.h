#ifndef OTF_OPENTHINGSFRAMEWORK_H
#define OTF_OPENTHINGSFRAMEWORK_H

#include "Request.h"
#include "Response.h"
#include <utils.h>

#include "WebSocketsClient.h"
#include "LinuxLocalServer.h"

#define LOCAL_SERVER_CLASS LinuxLocalServer

// The size of the buffer to store the incoming request line and headers (does not include body). Larger requests will be discarded.
#define HEADERS_BUFFER_SIZE 1536

namespace OTF {
  typedef void (*callback_t)(const Request &request, Response &response);

  enum CLOUD_STATUS {
    /** Indicates that an OTC token was not specified on initialization. */
    NOT_ENABLED,
    /** Indicates that the device was never able to connect to the server. */
    UNABLE_TO_CONNECT,
    /** Indicates that the device was previously connected to the server, but got disconnected and has been unable to reconnect. */
    DISCONNECTED,
    /** Indicates that the device is currently connected to the server. */
    CONNECTED
  };

  class OpenThingsFramework {
  private:
    LOCAL_SERVER_CLASS localServer = LOCAL_SERVER_CLASS(80);
    LocalClient *localClient = nullptr;
    WebSocketsClient *webSocket = nullptr;
    LinkedMap<callback_t> callbacks;
    callback_t missingPageCallback;
    CLOUD_STATUS cloudStatus = NOT_ENABLED;
    unsigned long lastCloudStatusChangeTime = millis();
    char *headerBuffer = NULL;
    int headerBufferSize = 0;

    void webSocketCallback(WStype_t type, uint8_t *payload, size_t length);

    void fillResponse(const Request &req, Response &res);
    void localServerLoop();
    void setCloudStatus(CLOUD_STATUS status);

    static void defaultMissingPageCallback(const Request &req, Response &res);

  public:
    /**
     * Initializes the library to only listen on a local webserver.
     * @param webServerPort The local port to bind the webserver to.
     * @param hdBuffer externally provided header buffer (optional)
     * @param hdBufferSize size of the externally provided header buffer (optional)
     */
    OpenThingsFramework(uint16_t webServerPort, char *hdBuffer = NULL, int hdBufferSize = HEADERS_BUFFER_SIZE);

    /**
     * Initializes the library to listen on a local webserver and connect to a remote websocket.
     * @param webServerPort The local port to bind the webserver to.
     * @param webSocketHost The host of the remote websocket.
     * @param webSocketPort The port of the remote websocket.
     * @param deviceKey The unique device key that identifies this device.
     * @param useSsl Indicates if SSL should be used when connecting to the websocket.
     * @param hdBuffer externally provided header buffer (optional)
     * @param hdBufferSize size of the externally provided header buffer (optional)
     */
    OpenThingsFramework(uint16_t webServerPort, const String &webSocketHost, uint16_t webSocketPort,
                        const String &deviceKey, bool useSsl, char *hdBuffer = NULL, int hdBufferSize = HEADERS_BUFFER_SIZE);

    /**
     * Registers a callback function to run when a request is made to the specified path. The callback function will
     * be passed an OpenThingsRequest, and must return an OpenThingsResponse.
     * @param path
     * @param callback
     */
    void on(const char *path, callback_t callback, HTTPMethod method = HTTP_ANY);

    /**
     * Registers a callback function to run when a request is made to the specified path. The callback function will
     * be passed an OpenThingsRequest, and must return an OpenThingsResponse.
     * @param path
     * @param callback
     */
    //void on(const __FlashStringHelper *path, callback_t callback, HTTPMethod method = HTTP_ANY);

    /** Registers a callback function to run when a request is received but its path does not match a registered callback. */
    void onMissingPage(callback_t callback);

    void loop();

    /** Returns the current status of the connection to the OpenThings Cloud server. */
    CLOUD_STATUS getCloudStatus();

    /** Returns the number of milliseconds since there was last a change in the cloud status. */
    unsigned long getTimeSinceLastCloudStatusChange();
  };
}// namespace OTF

#endif
