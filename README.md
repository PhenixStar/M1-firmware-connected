<!-- See COPYING.txt for license details. -->

# M1 Firmware Connected

Feature-rich firmware for the **Monstatek M1** (STM32H573, Cortex-M33) with an
**external app loader** and a new **HTTP client**, so SD-card `.m1app` apps can
talk to your LAN/cloud device APIs (Home Assistant, Synology, webhooks, …).

Pair it with the app SDK: **[M1-app-sdk](https://github.com/PhenixStar/M1-app-sdk)**.

## Highlights

| Capability | Notes |
|------------|-------|
| **External app loader** | Runs relocatable `.m1app` ELF apps from `0:/apps/` as FreeRTOS tasks, with a 199-symbol app API resolved at load time. **Main Menu → Apps**. |
| **HTTP client (new)** | `m1_http.{c,h}` formats `AT+HTTPCLIENT` requests over the existing ESP-AT SPI link — the ESP32-C6 does TCP/TLS, no host IP stack. GET/POST/PUT/DELETE + bearer-auth headers. Exposed to apps as `m1_http_get` / `m1_http_post_json` / `m1_http_request`. |
| **Dual Boot** | Swap between the two STM32H573 flash banks from the menu. |
| **RPC bridge, Games, BadUSB** | USB-CDC RPC (desktop companion), on-device games, and BadUSB. |

Base: NFC, LF RFID, Sub-GHz, IR, BLE/WiFi (ESP32-C6), GPIO, SD tools.

## Requirements for HTTP apps

The HTTP client needs the ESP32-C6 running **ESP-AT firmware built with
`CONFIG_AT_HTTP_COMMAND_SUPPORT`** (SPI transport), and WiFi connected on the
device (**Main Menu → WiFi → Config**).

## Build

```bash
sudo apt install gcc-arm-none-eabi cmake ninja-build srecord
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
# CRC/metadata for the SD updater:
python tools/append_crc32.py build/M1_v0800_C3.12.bin --output build/M1_v0800_C3.12_wCRC.bin --c3-revision 12
```

Flash `build/M1_v0800_C3.12.hex` via ST-Link, or `..._wCRC.bin` via the on-device
**Settings → Firmware Update**. Pre-built binaries are on the
[Releases](../../releases) page.

## HTTP client API (for firmware/app authors)

```c
int m1_http_get(const char *url, char *resp, int resp_size);
int m1_http_post_json(const char *url, const char *json_body, char *resp, int resp_size);
int m1_http_request(int method, int content_type, const char *url,
                    const char *headers /* "Authorization: Bearer …" */,
                    const char *body, char *resp, int resp_size);
```

Returns response-body bytes (≥0) or a negative error code. See
[`m1_csrc/m1_http.h`](m1_csrc/m1_http.h).

## License

GPL (see [`COPYING.txt`](COPYING.txt) / [`LICENSE`](LICENSE)). Derivative of the
open-source Monstatek M1 firmware; the license is preserved.
