/*
 * HA Remote -- toggle Home Assistant entities from the M1.
 *
 * Reads 0:/apps/ha.conf:
 *     url:   http://10.2.1.73:8123
 *     token: <long-lived access token>
 *     name1: Living Room       entity1: light.living_room
 *     name2: Bedroom Fan        entity2: switch.bedroom_fan
 *     ...(up to 12)
 *
 * UP/DOWN to select, OK to toggle (POST /api/services/homeassistant/toggle),
 * BACK to exit. Requires WiFi connected (Main Menu -> WiFi -> Config).
 */

#include "m1app.h"
#include "m1app_conf.h"

M1_APP_MANIFEST("HA Remote", 1024);

#define MAX_ENT   12
#define NAME_LEN  20
#define EID_LEN   48

static char  cfg[768];
static char  base_url[96];
static char  token[320];
static char  names[MAX_ENT][NAME_LEN];
static char  eids[MAX_ENT][EID_LEN];
static int   n_ent;

static char  auth_hdr[352];
static char  url[160];
static char  body[80];
static char  resp[256];

static int load_config(void)
{
    if (m1conf_load("0:/apps/ha.conf", cfg, sizeof(cfg)) < 0)
        return 0;
    if (!m1conf_get(cfg, "url", base_url, sizeof(base_url)))
        return 0;
    if (!m1conf_get(cfg, "token", token, sizeof(token)))
        return 0;

    n_ent = 0;
    for (int i = 1; i <= MAX_ENT; i++)
    {
        if (!m1conf_get_n(cfg, "entity", i, eids[n_ent], EID_LEN))
            break;
        if (!m1conf_get_n(cfg, "name", i, names[n_ent], NAME_LEN))
            snprintf(names[n_ent], NAME_LEN, "%s", eids[n_ent]);
        n_ent++;
    }
    snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Bearer %s", token);
    return n_ent > 0;
}

/* Returns 1 on HTTP success. */
static int toggle_entity(int idx)
{
    snprintf(url, sizeof(url), "%s/api/services/homeassistant/toggle", base_url);
    snprintf(body, sizeof(body), "{\"entity_id\":\"%s\"}", eids[idx]);
    int r = m1_http_request(M1APP_HTTP_POST, M1APP_HTTP_CT_JSON, url,
                            auth_hdr, body, resp, sizeof(resp));
    return r >= 0;
}

static void flash_msg(u8g2_t *u8g2, const char *l1, const char *l2)
{
    m1app_display_begin();
    do {
        u8g2_SetFont(u8g2, u8g2_font_6x10_tr);
        u8g2_SetDrawColor(u8g2, 1);
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
        flash_msg(u8g2, "No/bad config:", "0:/apps/ha.conf");
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
            u8g2_DrawStr(u8g2, 2, 10, "HA Remote");
            u8g2_SetDrawColor(u8g2, 1);

            u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
            for (int i = 0; i < rows; i++)
            {
                int e = top + i;
                if (e >= n_ent) break;
                int y = 14 + i * 10;
                if (e == sel)
                {
                    u8g2_DrawBox(u8g2, 0, y, 128, 10);
                    u8g2_SetDrawColor(u8g2, 0);
                }
                u8g2_DrawStr(u8g2, 3, y + 8, names[e]);
                u8g2_SetDrawColor(u8g2, 1);
            }
            if (top > 0) u8g2_DrawStr(u8g2, 122, 20, "^");
            if (top + rows < n_ent) u8g2_DrawStr(u8g2, 122, 62, "v");
        } while (m1app_display_flush());

        m1app_button_t btn = game_poll_button(150);
        if (btn == M1APP_BTN_BACK) break;
        if (btn == M1APP_BTN_UP && sel > 0)
        {
            sel--;
            if (sel < top) top = sel;
        }
        if (btn == M1APP_BTN_DOWN && sel < n_ent - 1)
        {
            sel++;
            if (sel >= top + rows) top = sel - rows + 1;
        }
        if (btn == M1APP_BTN_OK)
        {
            flash_msg(u8g2, "Toggling...", names[sel]);
            if (toggle_entity(sel))
            {
                m1_buzzer_notification();
                flash_msg(u8g2, "OK", names[sel]);
            }
            else
            {
                m1_buzzer_notification2();
                flash_msg(u8g2, "Failed", names[sel]);
            }
        }
    }
    return 0;
}
