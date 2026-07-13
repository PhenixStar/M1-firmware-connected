/*
 * Fleet Status -- reachability board for your LAN hosts.
 *
 * Reads 0:/apps/fleet.conf, one host per numbered key, "Label|url":
 *     host1: NAS-SAF|http://10.10.100.99:5000
 *     host2: Home Asst|http://10.2.1.73:8123
 *     host3: Pi-hole|http://10.2.1.9/admin
 *     ...(up to 16)
 *
 * OK/refresh re-checks all hosts (HTTP GET; any response = UP). BACK exits.
 * Requires WiFi connected.
 */

#include "m1app.h"
#include "m1app_conf.h"

M1_APP_MANIFEST("Fleet Status", 1024);

#define MAX_HOST   16
#define LABEL_LEN  16
#define URL_LEN    96

static char cfg[1024];
static char labels[MAX_HOST][LABEL_LEN];
static char urls[MAX_HOST][URL_LEN];
static uint8_t up[MAX_HOST];   /* 0=unknown/down, 1=up */
static int n_host;

static char resp[128];

static int load_config(void)
{
    if (m1conf_load("0:/apps/fleet.conf", cfg, sizeof(cfg)) < 0)
        return 0;

    n_host = 0;
    for (int i = 1; i <= MAX_HOST; i++)
    {
        char line[LABEL_LEN + URL_LEN + 4];
        if (!m1conf_get_n(cfg, "host", i, line, sizeof(line)))
            break;
        char *f[2];
        int nf = m1conf_split(line, '|', f, 2);
        if (nf < 2) continue;
        snprintf(labels[n_host], LABEL_LEN, "%s", f[0]);
        snprintf(urls[n_host], URL_LEN, "%s", f[1]);
        up[n_host] = 0;
        n_host++;
    }
    return n_host > 0;
}

static void check_all(u8g2_t *u8g2)
{
    for (int i = 0; i < n_host; i++)
    {
        m1app_display_begin();
        do {
            u8g2_SetFont(u8g2, u8g2_font_6x10_tr);
            u8g2_SetDrawColor(u8g2, 1);
            u8g2_DrawStr(u8g2, 6, 30, "Checking:");
            u8g2_DrawStr(u8g2, 6, 44, labels[i]);
        } while (m1app_display_flush());

        int r = m1_http_get(urls[i], resp, sizeof(resp));
        up[i] = (r >= 0) ? 1 : 0;   /* any HTTP response = reachable */
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
            u8g2_DrawStr(u8g2, 4, 42, "0:/apps/fleet.conf");
        } while (m1app_display_flush());
        m1app_delay(1500);
        return 0;
    }

    check_all(u8g2);
    int top = 0;
    const int rows = 6;

    while (1)
    {
        m1app_display_begin();
        do {
            u8g2_SetFont(u8g2, u8g2_font_6x10_tr);
            u8g2_SetDrawColor(u8g2, 1);
            u8g2_DrawBox(u8g2, 0, 0, 128, 12);
            u8g2_SetDrawColor(u8g2, 0);
            u8g2_DrawStr(u8g2, 2, 10, "Fleet Status");
            u8g2_SetDrawColor(u8g2, 1);

            u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
            for (int i = 0; i < rows; i++)
            {
                int h = top + i;
                if (h >= n_host) break;
                int y = 13 + i * 8;
                /* status dot: filled = up, hollow = down */
                if (up[h]) u8g2_DrawDisc(u8g2, 4, y + 3, 2, U8G2_DRAW_ALL);
                else       u8g2_DrawCircle(u8g2, 4, y + 3, 2, U8G2_DRAW_ALL);
                u8g2_DrawStr(u8g2, 10, y + 6, labels[h]);
                u8g2_DrawStr(u8g2, 104, y + 6, up[h] ? "UP" : "DN");
            }
        } while (m1app_display_flush());

        m1app_button_t btn = game_poll_button(150);
        if (btn == M1APP_BTN_BACK) break;
        if (btn == M1APP_BTN_DOWN && top + rows < n_host) top++;
        if (btn == M1APP_BTN_UP && top > 0) top--;
        if (btn == M1APP_BTN_OK) check_all(u8g2);
    }
    return 0;
}
