/* See COPYING.txt for license details. */

/*
 *  m1_http.h
 *
 *  Minimal HTTP client for the M1, layered on the ESP-AT SPI link.
 *  The ESP32 (running esp-at firmware with CONFIG_AT_HTTP_COMMAND_SUPPORT)
 *  performs the actual TCP/TLS + HTTP work via AT+HTTPCLIENT; the STM32 host
 *  only formats the command and parses the response. No host TCP/IP stack
 *  is required.
 *
 *  Exposed to external .m1app apps via the app symbol table so apps can talk
 *  to LAN/cloud device APIs (Home Assistant, Synology DSM, webhooks, etc.).
 *
 * M1 Project
 */

#ifndef M1_HTTP_H_
#define M1_HTTP_H_

#include <stdint.h>

/* HTTP methods -> AT+HTTPCLIENT <opt> field */
#define M1_HTTP_HEAD    1
#define M1_HTTP_GET     2
#define M1_HTTP_POST    3
#define M1_HTTP_PUT     4
#define M1_HTTP_DELETE  5

/* Content types -> AT+HTTPCLIENT <content-type> field (POST/PUT only) */
#define M1_HTTP_CT_URLENCODED  0
#define M1_HTTP_CT_JSON        1
#define M1_HTTP_CT_MULTIPART   2
#define M1_HTTP_CT_XML         3

/* Error returns (all negative; >=0 means N response-body bytes written). */
#define M1_HTTP_ERR_ARG      (-1)  /* bad argument                         */
#define M1_HTTP_ERR_CMDLEN   (-2)  /* URL/body too long for command buffer */
#define M1_HTTP_ERR_LINK     (-3)  /* ESP link timeout / send failure      */
#define M1_HTTP_ERR_HTTP     (-4)  /* ESP returned ERROR (DNS/TLS/HTTP)    */

/*
 * Perform an HTTP request through the ESP-AT link.
 *   method       : M1_HTTP_GET / _POST / _PUT / _DELETE / _HEAD
 *   content_type : M1_HTTP_CT_* (used for POST/PUT bodies, else pass 0)
 *   url          : full "http://..." or "https://..." URL (TLS auto-selected)
 *   headers      : one HTTP request header line, e.g.
 *                  "Authorization: Bearer <token>" (NULL for none)
 *   body         : request body for POST/PUT (NULL for none)
 *   resp_out     : caller buffer for the response body (NUL-terminated)
 *   resp_size    : size of resp_out in bytes
 * Returns number of response-body bytes written (>=0), or an M1_HTTP_ERR_* code.
 *
 * Not reentrant (uses shared static scratch buffers). The M1 runs one app at
 * a time, so apps may call it freely; firmware callers must not overlap.
 */
int m1_http_request(int method, int content_type, const char *url,
                    const char *headers, const char *body,
                    char *resp_out, int resp_size);

/* Convenience: HTTP GET. Returns body bytes (>=0) or M1_HTTP_ERR_*. */
int m1_http_get(const char *url, char *resp_out, int resp_size);

/* Convenience: HTTP POST with a JSON body. Returns body bytes or error. */
int m1_http_post_json(const char *url, const char *json_body,
                      char *resp_out, int resp_size);

#endif /* M1_HTTP_H_ */
