#if defined(OSPI)
#ifndef OTF_RESPONSE_H
#define OTF_RESPONSE_H

#include "StringBuilder.h"
#include "LocalServer.h"
#include "WebSocketsClient.h"

// The maximum possible size of response messages.
#define RESPONSE_BUFFER_SIZE 10*1024

namespace OTF {

  class Response : public StringBuilder {
    friend class OpenThingsFramework;

  private:
    enum ResponseStatus {
      CREATED,
      STATUS_WRITTEN,
      HEADERS_WRITTEN,
      BODY_WRITTEN
    };
    ResponseStatus responseStatus = CREATED;
    LocalClient *localClient = nullptr;
    WebSocketsClient *webSocket = nullptr;

    Response(WebSocketsClient *pwebSocket) : StringBuilder(RESPONSE_BUFFER_SIZE) {webSocket = pwebSocket;}

    Response(LocalClient *plocalClient) : StringBuilder(RESPONSE_BUFFER_SIZE) {localClient = plocalClient;}

  public:
    static const size_t MAX_RESPONSE_LENGTH = RESPONSE_BUFFER_SIZE;

    /** Writes the status code/message to the response. This must be called before writing the headers or body. */
    void writeStatus(uint16_t statusCode, const String &statusMessage);

    /** Writes the status code/message to the response. This must be called before writing the headers or body. */
    //void writeStatus(uint16_t statusCode, const __FlashStringHelper *const statusMessage);

    /** Writes the status code to the response. This must be called before writing the headers or body. */
    void writeStatus(uint16_t statusCode);

    /** Sets a response header. If called multiple times with the same header name, the header will be specified
     * multiple times to create a list. This function must not be called before the status has been written or after
     * the body has been written.
     */
    void writeHeader(const char *name, const char *value);

    void writeHeader(const char *name, int value);

    /**
     * Calls sprintf to write a chunk of data to the response body. This method may only be called after any desired
     * headers have been set.
     * @param format The format string to pass to sprintf.
     * @param ... The format arguments to pass to sprintf.
     */
    void writeBodyChunk(const char *format, ...);

    void flush();
  };
}// namespace OTF
#endif
#endif