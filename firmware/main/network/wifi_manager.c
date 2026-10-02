/**
 * @file wifi_manager.c
 * @brief Wi-Fi STA qua ESP32-C6 (esp_hosted + esp_wifi_remote, SDIO).
 *
 * Luồng:
 *   wifi_manager_init() -> spawn task nền -> esp_netif STA + esp_wifi_init
 *   (kích hoạt transport SDIO tới C6) -> esp_wifi_start.
 *   Quét: esp_wifi_scan_start(async) -> WIFI_EVENT_SCAN_DONE -> lưu vào s_aps.
 *   Kết nối: lưu SSID/pass vào NVS + esp_wifi_connect, auto-reconnect khi rớt.
 *
 * Lưu ý: esp_wifi_init có thể chặn vài giây khi bắt tay với C6 → chạy trong
 * task riêng, mọi lỗi đều non-fatal (UI vẫn chạy nếu C6 không phản hồi).
 */
#include "wifi_manager.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "nvs.h"

static const char *TAG = "WIFI";

#define WIFI_NVS_NS   "wificfg"
#define SCAN_MAX      20
#define RECONNECT_MAX 8

static esp_netif_t      *s_netif      = NULL;
static volatile bool     s_ready      = false;
static volatile bool     s_scanning   = false;
static volatile bool     s_connected  = false;
static char              s_ssid[33]   = {0};   /* SSID mục tiêu / đang kết nối */

static SemaphoreHandle_t s_lock       = NULL;
static wifi_ap_info_t    s_aps[SCAN_MAX];
static int               s_ap_count   = 0;
static uint32_t          s_gen        = 0;

static volatile bool     s_want_connect = false;
static volatile bool     s_connecting   = false;   /* đang kết nối (chưa có IP) */
static volatile bool     s_enabled      = true;    /* công tắc WiFi (UI bật/tắt) */
static volatile int8_t   s_rssi         = -100;    /* RSSI cache (cập nhật khi có IP) */
static int               s_retry        = 0;
static bool              s_sntp_init    = false;   /* SNTP đã khởi tạo chưa */
static volatile bool     s_time_synced  = false;   /* đã nhận >=1 lần đồng bộ NTP */

/* Giờ hệ thống = UTC; localtime() áp TZ="ICT-7" (set cố định ở main.c lúc
 * boot — thiết bị chỉ dùng ở Việt Nam) → đồng hồ hiển thị đúng GMT+7. */
static void time_sync_notify_cb(struct timeval *tv)
{
    (void)tv;
    s_time_synced = true;
    ESP_LOGI(TAG, "Da dong bo gio qua NTP");
}

static void start_sntp(void)
{
    if (s_sntp_init) { esp_netif_sntp_start(); return; }
    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    cfg.sync_cb = time_sync_notify_cb;
    if (esp_netif_sntp_init(&cfg) == ESP_OK) {
        s_sntp_init = true;
        ESP_LOGI(TAG, "SNTP bat dau (pool.ntp.org) — dong bo ngay gio");
    }
}

/* ── Helpers ──────────────────────────────────────────────────────────────── */
/* Copy có giới hạn + đảm bảo NUL. Dùng memcpy (không phải strncpy/snprintf) để
 * tránh cảnh báo -Wstringop-truncation/-Wformat-truncation của GCC — cả hai
 * không nhận ra đoạn cắt bớt ở đây là AN TOÀN và có chủ đích (SSID/mật khẩu
 * nhập từ UI luôn được null-terminate, cắt bớt nếu vượt buffer wifi_config_t). */
static void safe_copy(char *dst, size_t dst_size, const char *src)
{
    size_t len = strlen(src);
    if (len >= dst_size) len = dst_size - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

static uint8_t rssi_to_bars(int8_t rssi)
{
    if (rssi >= -55) return 4;
    if (rssi >= -65) return 3;
    if (rssi >= -75) return 2;
    return 1;
}

/* ── Event handler (chạy trong task event loop hệ thống) ───────────────────── */
static void wifi_evt(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT) {
        switch (id) {
        case WIFI_EVENT_STA_START:
            s_ready = true;
            ESP_LOGI(TAG, "STA start (C6 hosted ready)");
            if (s_want_connect && s_ssid[0]) {
                s_retry = 0;
                esp_wifi_connect();
            }
            break;

        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "Associated, chờ IP...");
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
            s_connected = false;
            if (s_enabled && s_want_connect && s_retry < RECONNECT_MAX) {
                s_retry++;
                ESP_LOGW(TAG, "Rớt — thử lại %d/%d", s_retry, RECONNECT_MAX);
                esp_wifi_connect();
            } else if (s_want_connect) {
                s_connecting = false;   /* hết retry → thất bại */
                ESP_LOGW(TAG, "Kết nối thất bại (sai mật khẩu / ngoài vùng?)");
            }
            break;

        case WIFI_EVENT_SCAN_DONE: {
            uint16_t num = 0;
            esp_wifi_scan_get_ap_num(&num);
            wifi_ap_record_t *recs = num ? calloc(num, sizeof(*recs)) : NULL;
            if (recs) {
                uint16_t got = num;
                esp_wifi_scan_get_ap_records(&got, recs);
                if (s_lock) xSemaphoreTake(s_lock, portMAX_DELAY);
                s_ap_count = 0;
                for (int i = 0; i < got && s_ap_count < SCAN_MAX; i++) {
                    if (recs[i].ssid[0] == 0) continue;            /* bỏ SSID ẩn */
                    /* loại trùng SSID (giữ tín hiệu mạnh nhất, recs đã sort theo rssi) */
                    bool dup = false;
                    for (int j = 0; j < s_ap_count; j++)
                        if (strcmp(s_aps[j].ssid, (char *)recs[i].ssid) == 0) { dup = true; break; }
                    if (dup) continue;
                    wifi_ap_info_t *o = &s_aps[s_ap_count++];
                    strncpy(o->ssid, (char *)recs[i].ssid, sizeof(o->ssid) - 1);
                    o->ssid[sizeof(o->ssid) - 1] = 0;
                    o->rssi = recs[i].rssi;
                    o->bars = rssi_to_bars(recs[i].rssi);
                    o->open = (recs[i].authmode == WIFI_AUTH_OPEN);
                }
                s_gen++;
                if (s_lock) xSemaphoreGive(s_lock);
                free(recs);
            }
            s_scanning = false;
            ESP_LOGI(TAG, "Quét xong: %d mạng", s_ap_count);
            break;
        }
        default: break;
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        s_connected = true;
        s_connecting = false;
        s_retry = 0;
        /* Cache RSSI ngay (trong event task — KHÔNG phải luồng LVGL) để get_bars()
         * không phải gọi RPC tới C6 mỗi tick timer (tránh treo UI khi RPC timeout). */
        wifi_ap_record_t ap;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) s_rssi = ap.rssi;
        ESP_LOGI(TAG, "Đã có IP — kết nối '%s' OK (RSSI %d)", s_ssid, s_rssi);
        start_sntp();   /* đồng bộ ngày giờ qua NTP */
    }
}

/* ── Task khởi tạo nền ─────────────────────────────────────────────────────── */
static void wifi_init_task(void *arg)
{
    /* 1. Nạp credentials đã lưu */
    char pass[65] = {0};
    nvs_handle_t h;
    if (nvs_open(WIFI_NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        size_t sl = sizeof(s_ssid);
        if (nvs_get_str(h, "ssid", s_ssid, &sl) == ESP_OK && s_ssid[0]) {
            size_t pl = sizeof(pass);
            if (nvs_get_str(h, "pass", pass, &pl) != ESP_OK) pass[0] = 0;
            s_want_connect = true;
        }
        nvs_close(h);
    }

    /* 2. netif + esp_wifi (qua esp_wifi_remote → SDIO → C6) */
    s_netif = esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t e = esp_wifi_init(&cfg);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init that bai: %s — C6 hosted khong phan hoi?",
                 esp_err_to_name(e));
        vTaskDelete(NULL);
        return;
    }

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_evt, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_evt, NULL, NULL);

    esp_wifi_set_mode(WIFI_MODE_STA);
    if (s_want_connect && s_ssid[0]) {
        wifi_config_t wc = {0};
        safe_copy((char *)wc.sta.ssid, sizeof(wc.sta.ssid), s_ssid);
        safe_copy((char *)wc.sta.password, sizeof(wc.sta.password), pass);
        esp_wifi_set_config(WIFI_IF_STA, &wc);
    }
    esp_wifi_start();   /* → WIFI_EVENT_STA_START → s_ready + auto-connect */
    ESP_LOGI(TAG, "Wi-Fi STA started qua C6 (SDIO)");
    vTaskDelete(NULL);
}

/* ── API ──────────────────────────────────────────────────────────────────── */
void wifi_manager_init(void)
{
    if (!s_lock) s_lock = xSemaphoreCreateMutex();
    /* Pin core 0 (PRO) để không tranh core 1 (LVGL). Stack đủ cho init + nvs. */
    xTaskCreatePinnedToCore(wifi_init_task, "wifi_init", 5120, NULL, 5, NULL, 0);
}

bool wifi_manager_is_ready(void)    { return s_ready; }
bool wifi_manager_is_scanning(void) { return s_scanning; }
bool wifi_manager_is_connected(void){ return s_connected; }
bool wifi_manager_is_connecting(void){ return s_connecting; }
bool wifi_manager_is_enabled(void)  { return s_enabled; }
const char *wifi_manager_get_ssid(void) { return s_ssid; }
bool wifi_manager_is_time_synced(void) { return s_time_synced; }

void wifi_manager_force_ntp_sync(void)
{
    if (!s_connected || !s_sntp_init) return;
    s_time_synced = false;
    esp_netif_sntp_start();   /* "restart it if already started" — ép lấy mốc mới */
}

/* Trả số vạch từ RSSI cache (không gọi RPC → an toàn gọi trong LVGL timer). */
int wifi_manager_get_bars(void)
{
    if (!s_connected) return 0;
    return rssi_to_bars(s_rssi);
}

/* Công tắc WiFi (UI). Off: ngắt + chặn auto-reconnect/scan. On: kết nối lại nếu có
 * credentials đã lưu. Không gọi esp_wifi_stop để tránh rủi ro với esp_hosted. */
void wifi_manager_set_enabled(bool en)
{
    s_enabled = en;
    if (!en) {
        s_want_connect = false;
        s_connecting   = false;
        if (s_ready) esp_wifi_disconnect();
        s_connected    = false;
        ESP_LOGI(TAG, "WiFi TẮT (UI)");
    } else {
        ESP_LOGI(TAG, "WiFi BẬT (UI)");
        if (s_ready && s_ssid[0]) {
            s_want_connect = true;
            s_connecting   = true;
            s_retry        = 0;
            esp_wifi_connect();
        }
    }
}

void wifi_manager_scan_start(void)
{
    if (!s_enabled || !s_ready || s_scanning) return;
    wifi_scan_config_t sc = {
        .show_hidden = false,
        .scan_type   = WIFI_SCAN_TYPE_ACTIVE,
    };
    s_scanning = true;
    if (esp_wifi_scan_start(&sc, false) != ESP_OK) {
        s_scanning = false;
        ESP_LOGW(TAG, "scan_start that bai");
    }
}

int wifi_manager_get_scan(wifi_ap_info_t *out, int max, uint32_t *gen)
{
    if (!out || max <= 0) { if (gen) *gen = s_gen; return 0; }
    if (s_lock) xSemaphoreTake(s_lock, portMAX_DELAY);
    int n = (s_ap_count < max) ? s_ap_count : max;
    memcpy(out, s_aps, n * sizeof(wifi_ap_info_t));
    if (gen) *gen = s_gen;
    if (s_lock) xSemaphoreGive(s_lock);
    return n;
}

esp_err_t wifi_manager_connect(const char *ssid, const char *pass)
{
    if (!ssid || !ssid[0]) return ESP_ERR_INVALID_ARG;

    strncpy(s_ssid, ssid, sizeof(s_ssid) - 1);
    s_ssid[sizeof(s_ssid) - 1] = 0;
    s_want_connect = true;
    s_connecting = true;
    s_connected = false;
    s_retry = 0;

    /* Lưu NVS để auto-connect lần sau */
    nvs_handle_t h;
    if (nvs_open(WIFI_NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, "ssid", ssid);
        nvs_set_str(h, "pass", pass ? pass : "");
        nvs_commit(h);
        nvs_close(h);
    }

    if (!s_ready) {
        ESP_LOGW(TAG, "Chưa sẵn sàng — sẽ kết nối khi C6 lên");
        return ESP_ERR_INVALID_STATE;
    }

    wifi_config_t wc = {0};
    safe_copy((char *)wc.sta.ssid, sizeof(wc.sta.ssid), ssid);
    if (pass) safe_copy((char *)wc.sta.password, sizeof(wc.sta.password), pass);

    esp_wifi_disconnect();
    esp_err_t e = esp_wifi_set_config(WIFI_IF_STA, &wc);
    if (e != ESP_OK) return e;
    return esp_wifi_connect();
}
