# Prebuilt apps

Ready-to-run `.m1app` apps for this firmware. To install:

1. Copy an app's `*.m1app` to `0:/apps/` on the SD card.
2. Copy its `*.conf.example` to `0:/apps/<name>.conf` and edit it (URLs, tokens).
3. On the M1: **Main Menu → Apps** → pick the app.

All four need WiFi connected (**Main Menu → WiFi → Config**) and the ESP32-C6
running ESP-AT with HTTP support.

| App | Does | Config |
|-----|------|--------|
| `ha_remote` | Toggle Home Assistant entities | `ha.conf` |
| `fleet_status` | LAN host up/down board | `fleet.conf` |
| `webhook_buttons` | Fire GET/POST webhooks | `hooks.conf` |
| `device_status` | Poll authenticated JSON endpoints | `status.conf` |

Source (`main.c`) is included. To rebuild or write your own, use the
**[M1-app-sdk](https://github.com/PhenixStar/M1-app-sdk)**.
