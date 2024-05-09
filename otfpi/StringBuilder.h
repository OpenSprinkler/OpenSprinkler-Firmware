#ifndef OTF_STRINGBUILDER_H
#define OTF_STRINGBUILDER_H

#include <stddef.h>
#include <cstdarg>

namespace OTF {
  /**
   * Wraps a buffer to build a string with repeated calls to sprintf. If any of calls to sprintf cause an error (such
   * as exceeding the size of the internal buffer), the error will be silently swallowed and the StringBuilder will be
   * marked as invalid. This means that any error checking can occur after the entire string has been built instead of
   * a check being required after each individual call to sprintf.
   */
  class StringBuilder {
  private:
    size_t maxLength;
    char *buffer;
    size_t length = 0;

  protected:
    bool valid = true;

  public:
    explicit StringBuilder(size_t maxLength);

    ~StringBuilder();
    /**
     * Inserts a string into the buffer at the current position using the same formatting rules as printf. If the operation
     * would cause the buffer length to be exceeded or some other error occurs, the StringBuilder will be marked as invalid.
     * @param format The format string to pass to sprintf.
     * @param ... The format arguments to pass to sprintf.
     */
    void bprintf(const char *format, va_list args);

    void bprintf(const char *format, ...);

    void append(const char *txt, size_t size);

    void append(const char *txt);

    /**
     * Returns the null-terminated represented string stored in the underlying buffer.
     * @return The null-terminated represented string stored in the underlying buffer.
     */
    char *toString() const;

    size_t getLength() const;

    /**
     * Returns a boolean indicating if the string was built successfully without any errors. If false, the behavior
     * of toString() is undefined, and the string it returns (which may or may not be null terminated) should NOT
     * be used.
     */
    bool isValid();

    bool willFit(size_t size);

    void reset();
  };
}// namespace OTF

#endif
