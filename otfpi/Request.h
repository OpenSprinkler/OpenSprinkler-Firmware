#if defined(OSPI)
#ifndef OTF_REQUEST_H
#define OTF_REQUEST_H

//#define SERIAL_DEBUG
#ifdef SERIAL_DEBUG
#define DEBUG(x) x
#else
#define DEBUG(x) 
#endif

#include "LinkedMap.h"
#include <stddef.h>

namespace OTF {

  enum HTTPMethod {
    HTTP_ANY,
    HTTP_GET,
    HTTP_POST,
    HTTP_PUT,
    HTTP_PATCH,
    HTTP_DELETE,
    HTTP_OPTIONS
  };

  enum RequestType {
    INVALID,
    NORMAL
  };

  class Request {
    friend class OpenThingsFramework;

  private:
    enum HTTPMethod httpMethod;
    char *httpVersion = nullptr;
    char *path = nullptr;
    LinkedMap<char *> queryParams;
    LinkedMap<char *> headers;
    char *body = nullptr;
    size_t bodyLength = 0;
    RequestType requestType = INVALID;
    bool cloudRequest;

    /**
     * Parses an HTTP request. The parser makes some assumptions about the message format that may not hold if the
     * message is improperly formatted, so the behavior of this constructor is undefined if it is passed an improperly
     * formatted request.
     */
    Request(char *str, size_t length, bool cloudRequest);

    /**
     * Parses the query of the request.
     * @param str The full request string.
     * @param length The length of the full request.
     * @param index The index of the first character after the '?' character that marks the start of the request.
     * When the function terminates successfully, this will be updated to the index of the character that marked the
     * end of the query. The character at this index may be changed, but its original value will be returned.
     * @return The character that was originally at `index` in the string. Possible values are:
     * '\0', indicating an error occurred.
     * '#', indicating that the new value of `index` marks the first character in the URI fragment.
     * ' ', indicating that the new value of `index` marks the first character in the HTTP version.
     */
    char parseQuery(char *str, size_t length, size_t &index);

    /**
     * Parses a single header and adds it to `headers`.
     * @param str The full request string.
     * @param length The length of the full request.
     * @param index The index of the first character in the header line. When the function terminates successfully,
     * this will be updated to the index of the first character after the CRLF sequence at the end of the header.
     * @param headers The set of headers to add the parsed header to.
     * @return A boolean indicating if the header could be parsed successfully.
     */
    bool parseHeader(char *str, size_t length, size_t &index, LinkedMap<char *> &headers);

    /**
     * Decodes the specified query string, updating and null-terminating the string in-place.
     * @param value a null-terminated query value.
     */
    static void decodeQueryParameter(char *value);

  public:
    /** Returns the path of the request (not including the query) as a null-terminated string. */
    char *getPath() const;

    /** Returns the decoded value of the specified query parameter as a null-terminated string, or NULL if the parameter was not set in the request. */
    char *getQueryParameter(const char *key) const;

    /**
     * Returns the value of the specified header as a null-terminated string, or NULL if the header was not set in the request.
     * @param key The lowercase header name.
     */
    char *getHeader(const char *key) const;

    /**
     * Returns the body of the request. THIS STRING IS NOT NULL TERMINATED, AND IT MAY CONTAIN NULL CHARACTERS. If
     * the request could not be parsed (as indicated by the getType() method), this method has undefined behavior.
     */
    char *getBody() const;

    /** Returns the length of the request body. */
    size_t getBodyLength() const;

    /** Indicates if this request is a multipart request, a non-multipart request, or an illegally formatted request. */
    RequestType getType() const;

    /**
     * Indicates if the request came through the cloud rather than through the local server. If this returns `true`,
     * it means that the request has already been authenticated by an OpenThings Cloud token.
     */

    bool isCloudRequest() const;
  };
}// namespace OTF
#endif
#endif