#if defined(OSPI)
#include "Request.h"
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

using namespace OTF;

// Find the pointers of substrings within the HTTP request and turns them into null-terminated C strings.
Request::Request(char *str, size_t length, bool cloudRequest) {
  this->cloudRequest = cloudRequest;
  size_t index = 0;

  // Parse the HTTP method.
  DEBUG(Serial.println(F("Parsing HTTP method"));)
  for (; index < length; index++) {
    if (str[index] == '\0') {
      // Reject any requests that contain null characters outside of the body to prevent null byte poisoning attacks.
      requestType = INVALID;
      return;
    } else if (str[index] == ' ') {
      // Null terminate the HTTP method.
      str[index] = '\0';

      // Move to the first character in the path.
      index++;

      break;
    }
  }
  if (index >= length) {
    // Error if the HTTP method extends to the end of the request.
    requestType = INVALID;
    return;
  }

  // Map the string to an enum value.
  if (strcmp("GET", &str[0]) == 0) {
    this->httpMethod = HTTP_GET;
  } else if (strcmp("POST", &str[0]) == 0) {
    this->httpMethod = HTTP_POST;
  } else if (strcmp("PUT", &str[0]) == 0) {
    this->httpMethod = HTTP_PUT;
  } else if (strcmp("DELETE", &str[0]) == 0) {
    this->httpMethod = HTTP_DELETE;
  } else if (strcmp("OPTIONS", &str[0]) == 0) {
    this->httpMethod = HTTP_OPTIONS;
  } else if (strcmp("PATCH", &str[0]) == 0) {
    this->httpMethod = HTTP_PATCH;
  } else {
    DEBUG(Serial.println(F("Could not match HTTP method"));)
    // Error if the method isn't a standard method.
    requestType = INVALID;
    return;
  }


  char character = str[index];
  // TODO handle cases where target isn't always a root path? (https://tools.ietf.org/html/rfc7230#section-5.3.1)
  DEBUG(Serial.println(F("Parsing the request target."));)
  // Parse the target.
  this->path = &str[index];
  // Skip over the path.
  while (true) {
    if (++index >= length) {
      // Error if the target extends to the end of the request.
      requestType = INVALID;
      return;
    }

    character = str[index];
    if (character == '\0') {
      // Reject any requests that contain null characters outside of the body to prevent null byte poisoning attacks.
      requestType = INVALID;
      return;
    } else if (character == '?' || character == '#' || character == ' ') {
      // Null terminate the path.
      str[index] = '\0';
      break;
    }
  }

  if (character == '?') {
    // Parse the query.
    character = parseQuery(str, length, ++index);

    // Exit if an error occurred while parsing the query.
    if (character == '\0') {
      requestType = INVALID;
      return;
    }
  }

  if (character == '#') {
    // Skip over the fragment.
    while (index < length && (character = str[++index]) != ' ') {
      if (character == '\0') {
        // Reject any requests that contain null characters outside of the body to prevent null byte poisoning attacks.
        requestType = INVALID;
        return;
      }
    }
  }

  DEBUG(Serial.println(F("Finished parsing request target."));)
  // Move to the first character in the HTTP version.
  index++;

  // Find the HTTP version.
  this->httpVersion = &str[index];
  for (; index < length; index++) {
    if (str[index] == '\0') {
      // Reject any requests that contain null characters outside of the body to prevent null byte poisoning attacks.
      requestType = INVALID;
      return;
    } else if (str[index] == '\r') {
      // Replace the carriage return with a null terminator.
      str[index] = '\0';
      // Move past the carriage return and assumed line feed.
      index += 2;
      break;
    }
  }

  // Parse headers until "\r\n\r\n" is encountered (indicates the end of headers) or the end of the string is reached (caused b"y invalid requests).
  while (index < length && str[index] != '\r') {
    if (!parseHeader(str, length, index, headers)) {
      // Reject the request if an error occurs while parsing headers.
      requestType = INVALID;
      return;
    }
  }

  // Move past the 2nd consecutive carriage return and assumed line feed.
  index += 2;

  if (index == length) {
    /* If the cursor is exactly 1 character after the end of the request, it means there is no body. Point the `body` pointer
     * to the last character in the request (since a value of NULL would indicate that the request was invalid), but set
     * `bodyLength` to 0 so it never gets read.
     */
    body = &str[index - 1];
    requestType = NORMAL;
  } else if (index > length) {
    // If the cursor is more than 1 character after the end of the request, it means the request was somehow illegally formatted.
    requestType = INVALID;
    return;
  } else {
    body = &str[index];
    requestType = NORMAL;
  }

  bodyLength = length - index;
}

char Request::parseQuery(char *str, size_t length, size_t &index) {
  DEBUG(Serial.println(F("Starting to parse query."));)
  while (index < length) {
    char *key = &str[index];
    char *value = nullptr;

    for (; index < length; index++) {
      char character = str[index];

      if (character == '\0') {
        // Reject any requests that contain null characters outside of the body to prevent null byte poisoning attacks.
        return '\0';
      }

      // If the end of the parameter has been reached, store it in the map.
      if (character == ' ' || character == '#' || character == '&') {
        // Null terminate the parameter.
        str[index] = '\0';

        // Handle parameters without a value.
        if (value == nullptr) {
          // Replace the null pointer with a pointer to an empty string to differentiate between empty parameters and unspecified parameters.
          value = &str[index];
        }

        decodeQueryParameter(value);
        DEBUG(Serial.printf((char *) F("Found query parameter '%s' with value '%s'.\n"), key, value);)

        queryParams.add(key, value);

        if (character == ' ' || character == '#') {
          // If the end of the query has been reached, return.
          return character;
        } else {
          // Move to the start of the next parameter key.
          index++;
          break;
        }
      } else if (character == '=') {
        // Make sure that the character is separating the key and the value (it isn't part of the value).
        if (value == nullptr) {
          // Null terminate the key.
          str[index] = '\0';
          value = &str[index + 1];
        }
      }
    }
  }

  // Error if the query extends to the end of the request.
  return '\0';
}

bool Request::parseHeader(char *str, size_t length, size_t &index, LinkedMap<char *> &headers) {
  char *lineStart = &str[index];
  char *colon = nullptr;
  char *value = nullptr;

  for (; index < length; index++) {
    if (str[index] == '\0') {
      // Reject any requests that contain null characters outside of the body to prevent null byte poisoning attacks.
      return false;
    } else if (str[index] == ':' && colon == nullptr) {
      colon = &str[index];
      str[index] = '\0';
    } else if (str[index] == '\r') {
      if (colon == nullptr) {
        // Error if there is an illegal header line that doesn't contain a colon.
        return false;
      }

      // Terminate the header value.
      str[index] = '\0';

      // Handle headers without a value.
      if (value == nullptr) {
        // Replace the null pointer with a pointer to an empty string to differentiate between empty header and unspecified headers.
        value = &str[index];
      }

      // Trim trailing whitespace from the header value (https://tools.ietf.org/html/rfc7230#section-3.2.4).
      for (size_t i = index; i > 0; i--) {
        // Replace whitespace with null terminators so the value string will terminate at the first space after it.
        if (isspace(str[index])) {
          str[index] = '\0';
        } else {
          break;
        }
      }

      DEBUG(Serial.printf((char *) F("Found header '%s' with value '%s'.\n"), lineStart, value);)
      // TODO handle duplicate header fields by concatenating them with a comma.
      headers.add(lineStart, value);

      // Move past the carriage return and assumed line feed.
      index += 2;
      break;
    } else if (colon != nullptr && value == nullptr && !isspace(str[index])) {
      // Mark the location of the header value after skipping whitespace.
      value = &str[index];
    } else if (value == nullptr) {
      // Make the header name lowercase.
      str[index] = tolower(str[index]);
    }
  }
  return true;
}

void Request::decodeQueryParameter(char *value) {
  unsigned int offset = 0;
  unsigned int index = 0;
  while (value[index + offset] != '\0') {
    DEBUG(Serial.printf((char *) F("Index is %d and offset is %d\n"), index, offset);)
    char character = value[index + offset];
    if (character == '+') {
      character = ' ';
    } else if (character == '%') {
      char highDigit = value[index + ++offset];
      char lowDigit = value[index + ++offset];
      if (highDigit == '\0' || lowDigit == '\0') {
        // Abort decoding because the query string is illegally formatted.
        return;
      }

      char hex[3] = {highDigit, lowDigit, '\0'};
      character = strtol(hex, nullptr, 16);
    }

    value[index] = character;

    index++;
  }
  value[index] = '\0';
}

char *Request::getPath() const { return path; }

char *Request::getQueryParameter(const char *key) const { return queryParams.find(key); }

char *Request::getHeader(const char *key) const { return headers.find(key); }

char *Request::getBody() const { return body; }

size_t Request::getBodyLength() const { return bodyLength; }

RequestType Request::getType() const { return requestType; }

bool Request::isCloudRequest() const { return cloudRequest; }
#endif