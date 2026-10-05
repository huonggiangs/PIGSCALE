#pragma once
/**
 * @file wifi_manager.h
 * @brief Wi-Fi STA qua ESP32-C6 (esp_hosted + esp_wifi_remote, SDIO).
 *
 * ESP32-P4 không có radio WiFi: mọi lệnh esp_wifi_* được chuyển tiếp xuống C6.
 * Module này khởi tạo nền (không chặn boot), cung cấp quét bất đồng bộ + kết nối.
 */
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char    ssid[33];
    int8_t  rssi;     /* dBm */
    uint8_t bars;     /* 1..4 (suy ra từ rssi) */
    bool    open;     /* true = mạng mở (không cần mật khẩu) */
} wifi_ap_info_t;

/** Khởi tạo Wi-Fi qua C6 trong task nền — không chặn app_main. */
void wifi_manager_init(void);

/** True khi transport C6 + Wi-Fi STA đã start và sẵn sàng nhận lệnh. */
bool wifi_manager_is_ready(void);

/** Bắt đầu quét bất đồng bộ. No-op nếu chưa sẵn sàng hoặc đang quét. */
void wifi_manager_scan_start(void);

/** True khi đang có một phiên quét chạy. */
bool wifi_manager_is_scanning(void);

/**
 * Sao chép kết quả quét gần nhất ra @p out (tối đa @p max phần tử).
 * @param gen (out, optional) generation tăng mỗi lần có kết quả mới —
 *            UI so sánh để biết khi nào cần vẽ lại.
 * @return số AP đã sao chép.
 */
int wifi_manager_get_scan(wifi_ap_info_t *out, int max, uint32_t *gen);

bool        wifi_manager_is_connected(void);
const char *wifi_manager_get_ssid(void);   /* SSID đang/đã chọn ("" nếu chưa) */

/** True khi đang trong quá trình kết nối (đã yêu cầu, chưa có IP, chưa hết retry). */
bool wifi_manager_is_connecting(void);

/** Số vạch tín hiệu của mạng ĐANG kết nối: 0 = chưa kết nối, 1..4 = cường độ. */
int  wifi_manager_get_bars(void);

/** Kết nối tới @p ssid với @p pass (NULL/"" cho mạng mở). Lưu NVS để auto-connect. */
esp_err_t wifi_manager_connect(const char *ssid, const char *pass);

/** Công tắc WiFi: Off → ngắt + chặn auto-reconnect/scan; On → kết nối lại nếu có SSID lưu. */
void wifi_manager_set_enabled(bool enabled);

/** True khi WiFi đang được bật (mặc định bật lúc khởi động). */
bool wifi_manager_is_enabled(void);

/** True khi đã nhận được ít nhất 1 lần đồng bộ giờ qua NTP kể từ lúc khởi
 *  động (xem start_sntp() — tự chạy sau khi có IP). Đồng hồ hệ thống hiển
 *  thị đúng giờ Việt Nam (GMT+7) ngay cả khi false (TZ đã set từ main.c),
 *  nhưng false nghĩa là giờ gốc có thể lệch (chưa từng lấy mốc từ Internet). */
bool wifi_manager_is_time_synced(void);

/** Bắt buộc đồng bộ lại giờ qua NTP ngay. No-op nếu chưa từng kết nối WiFi
 *  (SNTP chưa init) hoặc đang mất mạng. */
void wifi_manager_force_ntp_sync(void);

/* ── Địa chỉ IP tĩnh ──────────────────────────────────────────────────────
 * Mặc định DHCP (enabled=false). Khi bật, tắt DHCP client trên netif và tự
 * gán ip/netmask/gw/dns — áp dụng ngay lúc WIFI_EVENT_STA_CONNECTED (trước
 * khi DHCP kịp chạy), theo đúng mẫu "static_ip" chính thức của ESP-IDF.
 * Lưu NVS nên giữ qua reboot. */
typedef struct {
    bool enabled;
    char ip[16];        /* "192.168.1.50" */
    char netmask[16];   /* "255.255.255.0" */
    char gateway[16];   /* "192.168.1.1" */
    char dns[16];       /* "8.8.8.8" */
} wifi_static_ip_cfg_t;

/** Lưu cấu hình IP tĩnh vào NVS + áp dụng ngay nếu đang/sắp kết nối.
 *  Chuỗi rỗng ("") ở netmask/gateway/dns dùng giá trị mặc định hợp lý
 *  (netmask 255.255.255.0, không set gateway/dns nếu rỗng). */
esp_err_t wifi_manager_set_static_ip(const wifi_static_ip_cfg_t *cfg);

/** Đọc cấu hình IP tĩnh hiện tại (đã nạp từ NVS lúc khởi động hoặc vừa set). */
void wifi_manager_get_static_ip(wifi_static_ip_cfg_t *out);

#ifdef __cplusplus
}
#endif
