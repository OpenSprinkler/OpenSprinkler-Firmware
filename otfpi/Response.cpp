#if defined(OSPI)
#include "Response.h"

using namespace OTF;

void Response::writeStatus(uint16_t statusCode, const String &statusMessage) {
  if (responseStatus > CREATED) {
    valid = false;
    DEBUG_PRINTLN("VALID=FALSE writeStatus1");
    return;
  }
  responseStatus = STATUS_WRITTEN;

  bprintf(F("HTTP/1.1 %d %s\r\n"), statusCode, statusMessage.c_str());
}

void Response::writeStatus(uint16_t statusCode) {
  if (responseStatus > CREATED) {
    valid = false;
    DEBUG_PRINTLN("VALID=FALSE writeStatus2");
    return;
  }
  responseStatus = STATUS_WRITTEN;

  writeStatus(statusCode, F("No message"));
}

void Response::writeHeader(const char *name, const char *value) {
  if (responseStatus < STATUS_WRITTEN || responseStatus > HEADERS_WRITTEN) {
    valid = false;
    DEBUG_PRINTLN("VALID=FALSE WRITEHEADER1");
    return;
  }
  responseStatus = HEADERS_WRITTEN;

  bprintf(F("%s: %s\r\n"), name, value);
}

void Response::writeHeader(const char *name, int value) {
  if (responseStatus < STATUS_WRITTEN || responseStatus > HEADERS_WRITTEN) {
    valid = false;
    DEBUG_PRINTLN("VALID=FALSE WRITEHEADER2");
    return;
  }
  responseStatus = HEADERS_WRITTEN;

  bprintf(F("%s: %d\r\n"), name, value);
}

void Response::writeBodyChunk(const char *format, ...) {
  if (responseStatus < STATUS_WRITTEN) {
    valid = false;
    DEBUG_PRINTLN("VALID=FALSE writeBodyChunk");
    return;
  }
  if (responseStatus != BODY_WRITTEN) {
    append("\r\n");
    responseStatus = BODY_WRITTEN;
  }

  va_list args;
  va_start(args, format);
  bprintf(format, args);
  va_end(args);
}

void Response::flush() {
  char *responseString = toString();
  if (localClient)
    localClient->print(responseString);
  else if (webSocket)
    webSocket->sendTXT(responseString, getLength(), false, false);

  reset();
}
#endif