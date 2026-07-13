/*
 * Webhook Buttons -- fire configurable HTTP webhooks from the M1.
 *
 * Reads 0:/apps/hooks.conf. Each hook is "Label|METHOD|url", body optional:
 *     hook1: Open Gate|GET|http://10.2.1.73:8123/api/webhook/gate_abc
 *     hook2: Movie Mode|POST|http://10.2.1.50:5678/webhook/movie
 *     body2: {"scene":"movie"}
 *     ...(up to 16)
 *
 * UP/DOWN select, OK fires, BACK exits. Works with Home Assistant webhooks,
 * n8n, Node-RED, or any endpoint. Requires WiFi connected.
 */

#include "m1app.h"
#include "m1app_conf.h"

M1_APP_MANIFEST("Webhook Buttons", 1024);

#define MAX_HOOK   16
#define LABEL_LEN  20
#define URL_LEN    128
#define BODY_LEN   96

static char cfg[1536];
static char labels[MAX_HOOK][LABEL_LEN];
static char urls[MAX_HOOK][URL_LEN];
static char bodies[MAX_HOOK][BODY_LEN];
static uint8_t is_post[MAX_HOOK];
static int n_hook;

static char resp[192];

static int load_config(void)
{
    if (m1conf_load("0:/apps/hooks.conf", cfg, sizeof(cfg)) < 0)
        return 0;

    n_hook = 0;
    for (int i = 1; i <= MAX_HOOK; i++)
    {
        char line[LABEL_LEN + URL_LEN + 12];
        if (!m1conf_get_n(cfg, "hook", i, line, sizeof(line)))
            break;
        char *f[3];
        int nf = m1conf_split(line, '|', f, 3);
        if (nf < 3) continue;
        snprintf(labels[n_hook], LABEL_LEN, "%s", f[0]);
        is_post[n_hook] = (f[1][0] == 'P' || f[1][0] == 'p') ? 1 : 0;
        snprintf(urls[n_hook], URL_LEN, "%s", f[2]);
        if (!m1conf_get_n(cfg, "body", i, bodies[n_hook], BODY_LEN))
            bodies[n_hook][0] = '\0';
        n_hook++;
    }
    return n_hook > 0;
}

static int fire(int idx)
{
    int r;
    if (is_post[idx])
        r = m1_http_request(M1APP_HTTP_POST, M1APP_HTTP_CT_JSON, urls[idx],
                            NULL, bodies[idx][0] ? bodies[idx] : "{}",
                            resp, sizeof(resp));
    else
        r = m1_http_get(urls[idx], resp, sizeof(resp));
    return r >= 0;
}

static void flash_msg(u8g2_t *u8g2, const char *l1, const char *l2)
{
    m1app_display_begin();
    do {
        u8g2_SetFont(u8g2, u8g2_font_6x10_tr);
        u8g2_DrawStr(u8g2, 8, 28, l1);
        if (l2) u8g2_DrawStr(u8g2, 8, 42, l2);
    } while (m1app_display_flush());
    m1app_delay(900);
}

int32_t app_main(void *context)
{
    (void)context;
    u8g2_t *u8g2 = m1app_get_u8g2();

    if (!load_config())
    {
        flash_msg(u8g2, "No/bad config:", "0:/apps/hooks.conf");
        return 0;
    }

    int sel = 0, top = 0;
    const int rows = 5;

    while (1)
    {
        m1app_display_begin();
        do {
            u8g2_SetFont(u8g2, u8g2_font_6x10_tr);
            u8g2_SetDrawColor(u8g2, 1);
            u8g2_DrawBox(u8g2, 0, 0, 128, 12);
            u8g2_SetDrawColor(u8g2, 0);
            u8g2_DrawStr(u8g2, 2, 10, "Webhooks");
            u8g2_SetDrawColor(u8g2, 1);

            u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
            for (int i = 0; i < rows; i++)
            {
                int h = top + i;
                if (h >= n_hook) break;
                int y = 14 + i * 10;
                if (h == sel)
                {
                    u8g2_DrawBox(u8g2, 0, y, 128, 10);
                    u8g2_SetDrawColor(u8g2, 0);
                }
                u8g2_DrawStr(u8g2, 3, y + 8, labels[h]);
                u8g2_DrawStr(u8g2, 108, y + 8, is_post[h] ? "PST" : "GET");
                u8g2_SetDrawColor(u8g2, 1);
            }
            if (top > 0) u8g2_DrawStr(u8g2, 122, 20, "^");
            if (top + rows < n_hook) u8g2_DrawStr(u8g2, 122, 62, "v");
        } while (m1app_display_flush());

        m1app_button_t btn = game_poll_button(150);
        if (btn == M1APP_BTN_BACK) break;
        if (btn == M1APP_BTN_UP && sel > 0)
        {
            sel--;
            if (sel < top) top = sel;
        }
        if (btn == M1APP_BTN_DOWN && sel < n_hook - 1)
        {
            sel++;
            if (sel >= top + rows) top = sel - rows + 1;
        }
        if (btn == M1APP_BTN_OK)
        {
            flash_msg(u8g2, "Firing...", labels[sel]);
            if (fire(sel))
            {
                m1_buzzer_notification();
                flash_msg(u8g2, "Sent", labels[sel]);
            }
            else
            {
                m1_buzzer_notification2();
                flash_msg(u8g2, "Failed", labels[sel]);
            }
        }
    }
    return 0;
}
