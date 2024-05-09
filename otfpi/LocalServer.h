#if defined(OSPI)
#ifndef OTF_LOCALSERVER_H
#define OTF_LOCALSERVER_H

#include <stddef.h>

namespace OTF {
  class LocalClient {
  public:
    /** Returns a boolean indicating if data is currently available from this client. */
    virtual bool dataAvailable() = 0;

    /**
     * Reads up to `length` bytes from the request stream into `buffer`.
     * @return The number of bytes read.
     */
    virtual size_t readBytes(char *buffer, size_t length) = 0;

    /**
     * Reads up to `length` bytes from the request stream until `terminator` or the end of stream is reached.
     * @return The number of bytes read.
     */
    virtual size_t readBytesUntil(char terminator, char *buffer, size_t length) = 0;

    /** Prints a null-terminated string to the response stream. This method may be called multiple times before the stream is closed. */
    virtual void print(const char *data) = 0;

    /** Prints a null-terminated string to the response stream. This method may be called multiple times before the stream is closed. */
    //virtual void print(const __FlashStringHelper *data) = 0;

    /** Returns the next character in the request stream (without advancing the stream), or returns -1 if no character is available. */
    //virtual int peek() = 0;

    /** Sets the maximum number of milliseconds to wait for data to be available when readBytes() is called. */
    virtual void setTimeout(int timeout) = 0;
    
    virtual void flush() = 0;
    virtual void stop() = 0;
  };

  class LocalServer {
  public:
    /**
     * Closes the active client (if one is active) and accepts a new client (if one is available).
     * @return The newly accepted client, or `nullptr` if none was available.
     */
    virtual LocalClient *acceptClient() = 0;

    /** Starts listening for connections. */
    virtual void begin() = 0;
  };
}// namespace OTF

#endif
#endif