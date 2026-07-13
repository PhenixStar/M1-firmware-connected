/* See COPYING.txt for license details. */

/*
 *  m1_http.c
 *
 *  HTTP client over the ESP-AT SPI link (AT+HTTPCLIENT). See m1_http.h.
 *
 * M1 Project
 */

/*************************** I N C L U D E S **********************************/

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "m1_http.h"
#include "esp_app_main.h"   /* spi_AT_send_recv() */

/*************************** D E F I N E S ************************************/

#define M1_HTTP_CMD_MAX     640    /* AT command scratch (URL + body inline)  */
#define M1_HTTP_RAW_MAX     2048   /* raw AT response scratch                 */
#define M1_HTTP_TIMEOUT_S   15     /* per-request timeout                     */

/***************************** V A R I A B L E S ******************************/

/* Shared scratch — module is documented non-reentrant (single app at a time). */
static char m1_http_cmd[M1_HTTP_CMD_MAX];
static char m1_http_raw[M1_HTTP_RAW_MAX];

/************ P R I V A T E   F U N C T I O N S *******************************/

/* AT+HTTPCLIENT transport field: 2 = HTTPS(SSL), 1 = plain HTTP. */
static int m1_http_transport(const char *url)
{
    return (strncmp(url, "https://", 8) == 0) ? 2 : 1;
}

/*
 * ESP-AT streams the response body as one or more lines:
 *     +HTTPCLIENT:<len>,<data>
 * where <len> is the exact byte count of <data> (which may itself contain
 * commas/newlines). Parse each chunk by length and concatenate into out.
 */
static int m1_http_extract_body(const char *raw, char *out, int out_size)
{
    const char *tag = "+HTTPCLIENT:";
    const char *p = raw;
    int total = 0;

    while ((p = strstr(p, tag)) != NULL)
    {
        p += strlen(tag);

        int len = atoi(p);                 /* byte count before the comma */
        const char *comma = strchr(p, ',');
        if (!comma || len <= 0)
            break;

        const char *data = comma + 1;
        for (int i = 0; i < len && total < out_size - 1; i++)
            out[total++] = data[i];

        p = data + len;                    /* advance past this chunk */
    }

    out[total] = '\0';
    return total;
}

/* Append `"<src>"` to dst at pos, escaping " and \ as ESP-AT requires
 * (e.g. a JSON body {"a":"b"} becomes "{\"a\":\"b\"}"). Returns the new
 * position, or -1 if it would overflow dstsize. */
static int m1_http_append_quoted(char *dst, int dstsize, int pos, const char *src)
{
    if (pos + 1 >= dstsize) return -1;
    dst[pos++] = '"';
    for (const char *s = src; *s; s++)
    {
        if (*s == '"' || *s == '\\')
        {
            if (pos + 2 >= dstsize) return -1;
            dst[pos++] = '\\';
            dst[pos++] = *s;
        }
        else
        {
            if (pos + 1 >= dstsize) return -1;
            dst[pos++] = *s;
        }
    }
    if (pos + 1 >= dstsize) return -1;
    dst[pos++] = '"';
    return pos;
}

/* Append a plain (unquoted) format snippet; returns new pos or -1 on overflow. */
static int m1_http_append_fmt(char *dst, int dstsize, int pos, const char *fmt, int v)
{
    int n = snprintf(dst + pos, dstsize - pos, fmt, v);
    if (n < 0 || pos + n >= dstsize) return -1;
    return pos + n;
}

/************ P U B L I C   F U N C T I O N S *********************************/

int m1_http_request(int method, int content_type, const char *url,
                    const char *headers, const char *body,
                    char *resp_out, int resp_size)
{
    const int cap = (int)sizeof(m1_http_cmd);
    int transport, pos, has_body, has_hdr;

    if (!url || !resp_out || resp_size < 2)
        return M1_HTTP_ERR_ARG;

    resp_out[0] = '\0';
    transport = m1_http_transport(url);
    has_body  = (body && (method == M1_HTTP_POST || method == M1_HTTP_PUT)) ? 1 : 0;
    has_hdr   = (headers && headers[0]) ? 1 : 0;

    /* AT+HTTPCLIENT=<opt>,<ct>,"<url>",,,<transport>[,"<data>"][,"<header>"]
     * URL/body/header are quoted with inner " and \ escaped. The <data> field
     * must be present (even empty) when a header follows. */
    pos = snprintf(m1_http_cmd, cap, "AT+HTTPCLIENT=%d,%d,", method, content_type);
    if (pos <= 0 || pos >= cap) return M1_HTTP_ERR_CMDLEN;

    pos = m1_http_append_quoted(m1_http_cmd, cap, pos, url);
    if (pos < 0) return M1_HTTP_ERR_CMDLEN;

    pos = m1_http_append_fmt(m1_http_cmd, cap, pos, ",,,%d", transport);
    if (pos < 0) return M1_HTTP_ERR_CMDLEN;

    if (has_body)
    {
        if (pos + 1 >= cap) return M1_HTTP_ERR_CMDLEN;
        m1_http_cmd[pos++] = ',';
        pos = m1_http_append_quoted(m1_http_cmd, cap, pos, body);
        if (pos < 0) return M1_HTTP_ERR_CMDLEN;
    }
    else if (has_hdr)
    {
        if (pos + 1 >= cap) return M1_HTTP_ERR_CMDLEN;
        m1_http_cmd[pos++] = ',';   /* empty data field placeholder */
    }

    if (has_hdr)
    {
        if (pos + 1 >= cap) return M1_HTTP_ERR_CMDLEN;
        m1_http_cmd[pos++] = ',';
        pos = m1_http_append_quoted(m1_http_cmd, cap, pos, headers);
        if (pos < 0) return M1_HTTP_ERR_CMDLEN;
    }

    if (pos + 3 >= cap) return M1_HTTP_ERR_CMDLEN;
    m1_http_cmd[pos++] = '\r';
    m1_http_cmd[pos++] = '\n';
    m1_http_cmd[pos]   = '\0';

    /* spi_AT_send_recv() returns SUCCESS even on timeout, writing a marker
     * string into the buffer, so classify by response content instead. */
    spi_AT_send_recv(m1_http_cmd, m1_http_raw, sizeof(m1_http_raw),
                     M1_HTTP_TIMEOUT_S);

    if (strstr(m1_http_raw, "TIMEOUT(") || strstr(m1_http_raw, "SEND_ERR="))
        return M1_HTTP_ERR_LINK;
    if (strstr(m1_http_raw, "\r\nERROR\r\n"))
        return M1_HTTP_ERR_HTTP;

    return m1_http_extract_body(m1_http_raw, resp_out, resp_size);
}

int m1_http_get(const char *url, char *resp_out, int resp_size)
{
    return m1_http_request(M1_HTTP_GET, 0, url, NULL, NULL, resp_out, resp_size);
}

int m1_http_post_json(const char *url, const char *json_body,
                      char *resp_out, int resp_size)
{
    return m1_http_request(M1_HTTP_POST, M1_HTTP_CT_JSON, url, NULL, json_body,
                           resp_out, resp_size);
}
