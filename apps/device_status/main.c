/*
 * Device Status -- poll authenticated JSON endpoints, show a value each.
 *
 * Reads 0:/apps/status.conf. Each item is "Label|url|header|json_key":
 *   item1: Solar W|http://10.2.1.73:8123/api/states/sensor.solar|Authorization: Bearer TKN|state
 *   item2: NAS Temp|http://10.10.100.99:5000/webapi/entry.cgi?...|X-SYNO-TOKEN: xxx|temperature
 *   ...(up to 12). Leave the header field empty for no auth: Label|url||key
 *
 * Extracts the string/number after "json_key": from the response. OK refreshes,
 * BACK exits. Requires WiFi connected.
 *
 * NOTE: Yealink VC800/VC500 control is a raw TCP socket API, not HTTP, so it is
 * out of scope for this HTTP client. Add an m1_tcp_* SDK module (ESP-AT
 * AT+CIPSTART/AT+CIPSEND) to drive the Yealink endpoints.
 */

#include "m1app.h"
#include "m1app_conf.h"

M1_APP_MANIFEST("Device Status", 1024);

#define MAX_ITEM   12
#define LABEL_LEN  16
#define URL_LEN    160
#define HDR_LEN    160
#define KEY_LEN    24
#define VAL_LEN    24

static char cfg[2048];
static char labels[MAX_ITEM][LABEL_LEN];
static char urls[MAX_ITEM][URL_LEN];
static char hdrs[MAX_ITEM][HDR_LEN];
static char keys[MAX_ITEM][KEY_LEN];
static char vals[MAX_ITEM][VAL_LEN];
static int  n_item;

static char resp[512];

static int load_config(void)
{
    if (m1conf_load("0:/apps/status.conf", cfg, sizeof(cfg)) < 0)
        return 0;

    n_item = 0;
    for (int i = 1; i <= MAX_ITEM; i++)
    {
        char line[LABEL_LEN + URL_LEN + HDR_LEN + KEY_LEN + 8];
        if (!m1conf_get_n(cfg, "item", i, line, sizeof(line)))
            break;
        char *f[4];
        int nf = m1conf_split(line, '|', f, 4);
        if (nf < 4) continue;
        snprintf(labels[n_item], LABEL_LEN, "%s", f[0]);
        snprintf(urls[n_item],   URL_LEN,   "%s", f[1]);
        snprintf(hdrs[n_item],    HDR_LEN,   "%s", f[2]);
        snprintf(keys[n_item],    KEY_LEN,   "%s", f[3]);
        snprintf(vals[n_item],    VAL_LEN,   "%s", "?");
        n_item++;
    }
    return n_item > 0;
}

/* Extract the value after "key": from a JSON string. Returns 1 if found. */
static int json_extract(const char *json, const char *key, char *out, int outsize)
{
    char pat[KEY_LEN + 4];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(json, pat);
    if (!p) return 0;
    p += strlen(pat);
    while (*p == ' ' || *p == ':' || *p == '\t') p++;

    int i = 0;
    if (*p == '"')
    {
        p++;
        while (*p && *p != '"' && i < outsize - 1) out[i++] = *p++;
    }
    else
    {
        while (*p && *p != ',' && *p != '}' && *p != '\r' && *p != '\n'
               && i < outsize - 1)
            out[i++] = *p++;
    }
    out[i] = '\0';
    while (i > 0 && out[i - 1] == ' ') out[--i] = '\0';
    return i > 0;
}

static void refresh_all(u8g2_t *u8g2)
{
    for (int i = 0; i < n_item; i++)
    {
        m1app_display_begin();
        do {
            u8g2_SetFont(u8g2, u8g2_font_6x10_tr);
            u8g2_DrawStr(u8g2, 6, 30, "Polling:");
            u8g2_DrawStr(u8g2, 6, 44, labels[i]);
        } while (m1app_display_flush());

        int r = m1_http_request(M1APP_HTTP_GET, 0, urls[i],
                                hdrs[i][0] ? hdrs[i] : NULL, NULL,
                                resp, sizeof(resp));
        if (r < 0)
            snprintf(vals[i], VAL_LEN, "ERR%d", r);
        else if (!json_extract(resp, keys[i], vals[i], VAL_LEN))
            snprintf(vals[i], VAL_LEN, "n/a");
    }
}

int32_t app_main(void *context)
{
    (void)context;
    u8g2_t *u8g2 = m1app_get_u8g2();

    if (!load_config())
    {
        m1app_display_begin();
        do {
            u8g2_SetFont(u8g2, u8g2_font_6x10_tr);
            u8g2_DrawStr(u8g2, 4, 28, "No/bad config:");
            u8g2_DrawStr(u8g2, 4, 42, "0:/apps/status.conf");
        } while (m1app_display_flush());
        m1app_delay(1500);
        return 0;
    }

    refresh_all(u8g2);
    int top = 0;
    const int rows = 5;

    while (1)
    {
        m1app_display_begin();
        do {
            u8g2_SetFont(u8g2, u8g2_font_6x10_tr);
            u8g2_SetDrawColor(u8g2, 1);
            u8g2_DrawBox(u8g2, 0, 0, 128, 12);
            u8g2_SetDrawColor(u8g2, 0);
            u8g2_DrawStr(u8g2, 2, 10, "Device Status");
            u8g2_SetDrawColor(u8g2, 1);

            u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
            for (int i = 0; i < rows; i++)
            {
                int e = top + i;
                if (e >= n_item) break;
                int y = 14 + i * 10;
                u8g2_DrawStr(u8g2, 3, y + 8, labels[e]);
                int vw = u8g2_GetStrWidth(u8g2, vals[e]);
                u8g2_DrawStr(u8g2, 125 - vw, y + 8, vals[e]);
            }
            if (top > 0) u8g2_DrawStr(u8g2, 122, 20, "^");
            if (top + rows < n_item) u8g2_DrawStr(u8g2, 122, 62, "v");
        } while (m1app_display_flush());

        m1app_button_t btn = game_poll_button(150);
        if (btn == M1APP_BTN_BACK) break;
        if (btn == M1APP_BTN_DOWN && top + rows < n_item) top++;
        if (btn == M1APP_BTN_UP && top > 0) top--;
        if (btn == M1APP_BTN_OK) refresh_all(u8g2);
    }
    return 0;
}
