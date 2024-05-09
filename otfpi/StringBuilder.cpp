#include "StringBuilder.h"
#include <string.h>
#include <cstdio>

using namespace OTF;

StringBuilder::StringBuilder(size_t maxLength) {
  this->maxLength = maxLength;
  buffer = new char[maxLength];
}

StringBuilder::~StringBuilder() {
  delete buffer;
}

void StringBuilder::bprintf(const char *format, va_list args) {
  // Don't do anything if the buffer already contains invalid data.
  if (!valid) {
    return;
  }

  length += vsnprintf(&buffer[length], maxLength - length, format, args);

  // The builder is invalid if the string fits perfectly in the buffer since there wouldn't be room for the null terminator.
  if (length >= maxLength) {
    // snprintf will not allow more than the specified number of characters to be written to the buffer, so the length will be the buffer size.
    length = maxLength;
    valid = false;
  }
}

void StringBuilder::bprintf(const char *const format, ...) {
  va_list args;
  va_start(args, format);
  bprintf(format, args);
  va_end(args);
}

char *StringBuilder::toString() const {
  return &buffer[0];
}

size_t StringBuilder::getLength() const {
  return length;
}

bool StringBuilder::isValid() {
  return valid;
}

bool StringBuilder::willFit(size_t size) {
  return size + length + 1 < maxLength;
}

void StringBuilder::reset() {
  length = 0;
  buffer[0] = 0;
  valid = true;
}

void StringBuilder::append(const char *txt, size_t size) {
  if (!valid || !willFit(size)) {
    valid = false;
    return;
  }
  strncpy(&buffer[length], txt, size);
  length += size;
  buffer[length] = 0;
}

void StringBuilder::append(const char *txt) {
  append(txt, strlen(txt));
}

