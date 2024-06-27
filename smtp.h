/**
 * @file
 * @brief SMTP client library.
 * @author James Humphrey (mail@somnisoft.com)
 * @version 1.00
 *
 * This SMTP client library allows the user to send emails to an SMTP server.
 * The user can include custom headers and MIME attachments.
 *
 * This software has been placed into the public domain using CC0.
 */
#ifndef SMTP_H
#define SMTP_H

#include <sys/types.h>
#include <stddef.h>
#include <stdio.h>

#ifndef SIZE_MAX
/**
 * Maximum value of size_t type.
 */
# define SIZE_MAX ((size_t)(-1))
#endif /* SIZE_MAX */

/**
 * Status codes indicating success or failure from calling any of the
 * SMTP library functions.
 *
 * This code gets returned by all functions in this header.
 */
enum smtp_status_code{
  /**
   * Successful operation completed.
   */
  SMTP_STATUS_OK              = 0,

  /**
   * Memory allocation failed.
   */
  SMTP_STATUS_NOMEM           = 1,

  /**
   * Failed to connect to the mail server.
   */
  SMTP_STATUS_CONNECT         = 2,

  /**
   * Failed to handshake or negotiate a TLS connection with the server.
   */
  SMTP_STATUS_HANDSHAKE       = 3,

  /**
   * Failed to authenticate with the given credentials.
   */
  SMTP_STATUS_AUTH            = 4,

  /**
   * Failed to send bytes to the server.
   */
  SMTP_STATUS_SEND            = 5,

  /**
   * Failed to receive bytes from the server.
   */
  SMTP_STATUS_RECV            = 6,

  /**
   * Failed to properly close a connection.
   */
  SMTP_STATUS_CLOSE           = 7,

  /**
   * SMTP server sent back an unexpected status code.
   */
  SMTP_STATUS_SERVER_RESPONSE = 8,

  /**
   * Invalid parameter.
   */
  SMTP_STATUS_PARAM           = 9,

  /**
   * Failed to open or read a local file.
   */
  SMTP_STATUS_FILE            = 10,

  /**
   * Failed to get the local date and time.
   */
  SMTP_STATUS_DATE            = 11,

  /**
   * Indicates the last status code in the enumeration, useful for
   * bounds checking.
   *
   * Not a valid status code.
   */
  SMTP_STATUS__LAST
};

/**
 * Address source and destination types.
 */
enum smtp_address_type{
  /**
   * From address.
   */
  SMTP_ADDRESS_FROM = 0,

  /**
   * To address.
   */
  SMTP_ADDRESS_TO   = 1,

  /**
   * Copy address.
   */
  SMTP_ADDRESS_CC   = 2,

  /**
   * Blind copy address.
   *
   * Recipients should not see any of the BCC addresses when they receive
   * their email. However, some SMTP server implementations may copy this
   * information into the mail header, so do not assume that this will
   * always get hidden. If the BCC addresses must not get shown to the
   * receivers, then send one separate email to each BCC party and add
   * the TO and CC addresses manually as a header property using
   * @ref smtp_header_add instead of as an address using
   * @ref smtp_address_add.
   */
  SMTP_ADDRESS_BCC  = 3
};

/**
 * Connect to the SMTP server using either an unencrypted socket or
 * TLS encryption.
 */
enum smtp_connection_security{
#ifdef SMTP_OPENSSL
  /**
   * First connect without encryption, then negotiate an encrypted connection
   * by issuing a STARTTLS command.
   *
   * Typically used when connecting to a mail server on port 25 or 587.
   */
  SMTP_SECURITY_STARTTLS = 0,

  /**
   * Use TLS when initially connecting to server.
   *
   * Typically used when connecting to a mail server on port 465.
   */
  SMTP_SECURITY_TLS      = 1,
#endif /* SMTP_OPENSSL */

  /**
   * Do not use TLS encryption.
   *
   * Not recommended unless connecting to the SMTP server locally.
   */
  SMTP_SECURITY_NONE     = 2
};

/**
 * List of supported methods for authenticating a mail user account on
 * the server.
 */
enum smtp_authentication_method{
#ifdef SMTP_OPENSSL
  /**
   * Use HMAC-MD5.
   */
  SMTP_AUTH_CRAM_MD5 = 0,
#endif /* SMTP_OPENSSL */
  /**
   * No authentication required.
   *
   * Some servers support this option if connecting locally.
   */
  SMTP_AUTH_NONE     = 1,

  /**
   * Authenticate using base64 user and password.
   */
  SMTP_AUTH_PLAIN    = 2,

  /**
   * Another base64 authentication method, similar to SMTP_AUTH_PLAIN.
   */
  SMTP_AUTH_LOGIN    = 3
};

/**
 * Special flags defining certain behaviors for the SMTP client context.
 */
enum smtp_flag{
  /**
   * Print client and server communication on stderr.
   */
  SMTP_DEBUG          = 1 << 0,

  /**
   * Do not verify TLS certificate.
   *
   * By default, the TLS handshake function will check if a certificate
   * has expired or if using a self-signed certificate. Either of those
   * conditions will cause the connection to fail. This option allows the
   * connection to proceed even if those checks fail.
   */
  SMTP_NO_CERT_VERIFY = 1 << 1
};

struct smtp;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Open a connection to an SMTP server and return the context.
 *
 * After successfully connecting and performing a handshake with the
 * SMTP server, this will return a valid SMTP client context that
 * the application can use in the other library functions.
 *
 * This always returns a valid SMTP client context even if
 * the server connection or memory allocation fails. In this scenario, the
 * error status will continue to propagate to future library calls for
 * the SMTP context while in this failure mode.
 *
 * This function will ignore the SIGPIPE signal. Applications that require a
 * handler for that signal should set it up after calling this function.
 *
 * @param[in]  server              Server name or IP address.
 * @param[in]  port                Server port number.
 * @param[in]  connection_security See @ref smtp_connection_security.
 * @param[in]  flags               See @ref smtp_flag.
 * @param[in]  cafile              Path to certificate file, or NULL to use
 *                                 certificates in the default path.
 * @param[out] smtp                Pointer to a new SMTP context. When
 *                                 finished, the caller must free this
 *                                 context using @ref smtp_close.
 * @return See @ref smtp_status_code.
 */
enum smtp_status_code
smtp_open(const char *const server,
          const char *const port,
          enum smtp_connection_security connection_security,
          enum smtp_flag flags,
          const char *const cafile,
          struct smtp **smtp);

/**
 * Authenticate the user using one of the methods listed in
 * @ref smtp_authentication_method.
 *
 * @param[in] smtp        SMTP client context.
 * @param[in] auth_method See @ref smtp_authentication_method.
 * @param[in] user        SMTP user name.
 * @param[in] pass        SMTP password.
 * @return See @ref smtp_status_code.
 */
enum smtp_status_code
smtp_auth(struct smtp *const smtp,
          enum smtp_authentication_method auth_method,
          const char *const user,
          const char *const pass);

/**
 * Sends an email using the addresses, attachments, and headers defined
 * in the current SMTP context.
 *
 * The caller must call the @ref smtp_open function prior to this.
 *
 * The 'Date' header will automatically get generated here if it hasn't
 * already been set using @ref smtp_header_add.
 *
 * If the application overrides the default 'Content-Type' header, then
 * this function will output the @p body as raw data just below the email
 * headers, and it will not output the attachments added using the
 * smtp_attachment_add_* functions. In other words, the application must
 * create its own MIME sections (if needed) when overriding the
 * 'Content-Type' header.
 *
 * @param[in] smtp SMTP client context.
 * @param[in] body Null-terminated string to send in the email body.
 * @return See @ref smtp_status_code.
 */
enum smtp_status_code
smtp_mail(struct smtp *const smtp,
          const char *const body);

/**
 * Close the SMTP connection and frees all resources held by the
 * SMTP context.
 *
 * @param[in] smtp SMTP client context.
 * @return See @ref smtp_status_code.
 */
enum smtp_status_code
smtp_close(struct smtp *smtp);

/**
 * Get the current status/error code.
 *
 * @param[in] smtp SMTP client context.
 * @return See @ref smtp_status_code.
 */
enum smtp_status_code
smtp_status_code_get(const struct smtp *const smtp);

/**
 * Clear the current error code set in the SMTP client context.
 *
 * @param[in,out] smtp SMTP client context.
 * @return             Previous error code before clearing.
 */
enum smtp_status_code
smtp_status_code_clear(struct smtp *const smtp);

/**
 * Set the error status of the SMTP client context and return the same code.
 *
 * This allows the caller to clear an error status to SMTP_STATUS_OK
 * so that previous errors will stop propagating. However, this will only
 * work correctly for clearing the SMTP_STATUS_PARAM and SMTP_STATUS_FILE
 * errors. Do not use this to clear any other error codes.
 *
 * @deprecated Use @ref smtp_status_code_clear instead.
 *
 * @param[in] smtp            SMTP client context.
 * @param[in] new_status_code See @ref smtp_status_code.
 * @return See @ref smtp_status_code.
 */
enum smtp_status_code
smtp_status_code_set(struct smtp *const smtp,
                     enum smtp_status_code new_status_code);

/**
 * Convert a standard SMTP client status code to a descriptive string.
 *
 * @param[in] status_code Status code returned from one of the other
 *                        library functions.
 * @return String containing a description of the @p status_code. The caller
 *         must not free or modify this string.
 */
const char *
smtp_status_code_errstr(enum smtp_status_code status_code);

/**
 * Add a key/value header to the header list in the SMTP context.
 *
 * If adding a header with an existing key, this will insert instead of
 * replacing the existing header. See @ref smtp_header_clear_all.
 *
 * See @ref smtp_mail when overriding the default 'Content-Type' header.
 *
 * @param[in] smtp  SMTP client context.
 * @param[in] key   Key name for new header. It must consist only of
 *                  printable US-ASCII characters except colon.
 * @param[in] value Value for new header. It must consist only of printable
 *                  US-ASCII, space, or horizontal tab. If set to NULL,
 *                  this will prevent the header from printing out.
 * @return See @ref smtp_status_code.
 */
enum smtp_status_code
smtp_header_add(struct smtp *const smtp,
                const char *const key,
                const char *const value);

/**
 * Free all memory related to email headers.
 *
 * @param[in] smtp SMTP client context.
 */
void
smtp_header_clear_all(struct smtp *const smtp);

/**
 * Add a FROM, TO, CC, or BCC address destination to this SMTP context.
 *
 * @note Some SMTP servers may reject over 100 recipients.
 *
 * @param[in] smtp  SMTP client context.
 * @param[in] type  See @ref smtp_address_type.
 * @param[in] email The email address of the party. Must consist only of
 *                  printable characters excluding the angle brackets
 *                  (<) and (>).
 * @param[in] name  Name or description of the party. Must consist only of
 *                  printable characters, excluding the quote characters. If
 *                  set to NULL or empty string, no name will get associated
 *                  with this email.
 * @return See @ref smtp_status_code.
 */
enum smtp_status_code
smtp_address_add(struct smtp *const smtp,
                 enum smtp_address_type type,
                 const char *const email,
                 const char *const name);

/**
 * Free all memory related to the address list.
 *
 * @param[in] smtp SMTP client context.
 */
void
smtp_address_clear_all(struct smtp *const smtp);

/**
 * Add a file attachment from a path.
 *
 * See @ref smtp_attachment_add_mem for more details.
 *
 * @param[in] smtp SMTP client context.
 * @param[in] name Filename of the attachment shown to recipients. Must
 *                 consist only of printable ASCII characters, excluding
 *                 the quote characters (') and (").
 * @param[in] path Path to file.
 * @return See @ref smtp_status_code.
 */
enum smtp_status_code
smtp_attachment_add_path(struct smtp *const smtp,
                         const char *const name,
                         const char *const path);

/**
 * Add an attachment using a file pointer.
 *
 * See @ref smtp_attachment_add_mem for more details.
 *
 * @param[in] smtp SMTP client context.
 * @param[in] name Filename of the attachment shown to recipients. Must
 *                 consist only of printable ASCII characters, excluding
 *                 the quote characters (') and (").
 * @param[in] fp   File pointer already opened by the caller.
 * @return See @ref smtp_status_code.
 */
enum smtp_status_code
smtp_attachment_add_fp(struct smtp *const smtp,
                       const char *const name,
                       FILE *fp);

/**
 * Add a MIME attachment to this SMTP context with the data retrieved
 * from memory.
 *
 * The attachment data will get base64 encoded before sending to the server.
 *
 * @param[in] smtp   SMTP client context.
 * @param[in] name   Filename of the attachment shown to recipients. Must
 *                   consist only of printable ASCII characters, excluding
 *                   the quote characters (') and (").
 * @param[in] data   Raw attachment data stored in memory.
 * @param[in] datasz Number of bytes in @p data, or -1 if data
 *                   null-terminated.
 * @return See @ref smtp_status_code.
 */
enum smtp_status_code
smtp_attachment_add_mem(struct smtp *const smtp,
                        const char *const name,
                        const void *const data,
                        size_t datasz);

/**
 * Remove all attachments from the SMTP client context.
 *
 * @param[in] smtp SMTP client context.
 */
void
smtp_attachment_clear_all(struct smtp *const smtp);


/*
 * The SMTP_INTERNAL DEFINE section contains definitions that get used
 * internally by the SMTP client library.
 */
#ifdef SMTP_INTERNAL_DEFINE
/**
 * SMTP codes returned by the server and parsed by the client.
 */
enum smtp_result_code{
  /**
   * Client error code which does not get set by the server.
   */
  SMTP_INTERNAL_ERROR =  -1,

  /**
   * Returned when ready to begin processing next step.
   */
  SMTP_READY          = 220,

  /**
   * Returned in response to QUIT.
   */
  SMTP_CLOSE          = 221,

  /**
   * Returned if client successfully authenticates.
   */
  SMTP_AUTH_SUCCESS   = 235,

  /**
   * Returned when some commands successfully complete.
   */
  SMTP_DONE           = 250,

  /**
   * Returned for some multi-line authentication mechanisms where this code
   * indicates the next stage in the authentication step.
   */
  SMTP_AUTH_CONTINUE  = 334,

  /**
   * Returned in response to DATA command.
   */
  SMTP_BEGIN_MAIL     = 354
};

/**
 * Used for parsing out the responses from the SMTP server.
 *
 * For example, if the server sends back '250-STARTTLS', then code would
 * get set to 250, more would get set to 1, and text would get set to STARTTLS.
 */
struct smtp_command{
  /**
   * Result code converted to an integer.
   */
  enum smtp_result_code code;

  /**
   * Indicates if more server commands follow.
   *
   * This will get set to 1 if the fourth character in the response line
   * contains a '-', otherwise this will get set to 0.
   */
  int more;

  /**
   * The text shown after the status code.
   */
  const char *text;
};

/**
 * Return codes for the getdelim interface which allows the caller to check
 * if more delimited lines can get processed.
 */
enum str_getdelim_retcode{
  /**
   * An error occurred during the getdelim processing.
   */
  STRING_GETDELIMFD_ERROR = -1,

  /**
   * Found a new line and can process more lines in the next call.
   */
  STRING_GETDELIMFD_NEXT  =  0,

  /**
   * Found a new line and unable to read any more lines at this time.
   */
  STRING_GETDELIMFD_DONE  =  1
};

/**
 * Data structure for read buffer and line parsing.
 *
 * This assists with getting and parsing the server response lines.
 */
struct str_getdelimfd{
  /**
   * Read buffer which may include bytes past the delimiter.
   */
  char *_buf;

  /**
   * Number of allocated bytes in the read buffer.
   */
  size_t _bufsz;

  /**
   * Number of actual stored bytes in the read buffer.
   */
  size_t _buf_len;

  /**
   * Current line containing the text up to the delimiter.
   */
  char *line;

  /**
   * Number of stored bytes in the line buffer.
   */
  size_t line_len;

  /**
   * Function pointer to a custom read function for the
   * @ref smtp_str_getdelimfd interface.
   *
   * This function prototype has similar semantics to the read function.
   * The @p gdfd parameter allows the custom function to pull the user_data
   * info from the @ref str_getdelimfd struct which can contain file pointer,
   * socket connection, etc.
   */
  long (*getdelimfd_read)(struct str_getdelimfd *const gdfd,
                          void *buf,
                          size_t count);

  /**
   * User data which gets sent to the read handler function.
   */
  void *user_data;

  /**
   * Character delimiter used for determining line separation.
   */
  int delim;

  /**
   * Padding structure to align.
   */
  char pad[4];
};

#endif /* SMTP_INTERNAL_DEFINE */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SMTP_H */

