/**
 * @file main.c
 * @brief PIG WEIGH — Trạm cân heo — app_main entry point
 *
 * Thứ tự khởi động:
 *  1. NVS flash
 *  2. ESP-NETIF + event loop
 *  3. Display MIPI-DSI (esp_lvgl_port)
 *  4. Touch GT9271 (I2C)
 *  5. Ethernet PoE (LAN8720 RMII)
 *  6. WiFi qua ESP32-C6 (SDIO/esp_hosted)
 *  7. UI LVGL — app_state + ui_shell (chạy trong LVGL task, theo
 *     esp32p4-lvgl-handoff.md)
 *
 * Board: ESP32-P4-WIFI6-POE-ETH (Waveshare) — tái sử dụng nguyên phần bring-up
 * phần cứng (display/touch/ethernet) từ dự án CÂN MÁY XÚC V3; chỉ thay lớp UI.
 * IDF  : 5.5   |  LVGL: v9.x   |  Display: MIPI-DSI 800×1280
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>   /* setenv */
#include <time.h>     /* tzset */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"

#include "board_config.h"
#include "display_driver.h"
#include "gt9271.h"
#include "eth_manager.h"
#include "wifi_manager.h"   /* wifi_manager_init() — WiFi qua ESP32-C6 (SDIO) */
#include "app_state.h"
#include "ui_shell.h"
#include "heap_memory_layout.h"  /* SOC_RESERVE_MEMORY_REGION */

static const char *TAG = "app_main";

/* FIX boot-loop esp_hosted: giữ phần TCM heap (7KB) khỏi allocator → TCB/stack
 * task không rơi vào TCM (FreeRTOS P4 từ chối → assert app_startup.c:86).
 * Reserve TỪ _spm_data_end (sau vùng IDF đã reserve) để KHÔNG chồng start
 * (tránh assert s_prepare_reserved_regions:88). Đặt trong main.c để chắc
 * chắn được link. Xác nhận qua dự án tham chiếu
 * C:\Users\16flip\Claude\Projects\Mayxucv3 (cùng board, cùng chip rev v1.3). */
extern int _spm_data_end;
SOC_RESERVE_MEMORY_REGION((intptr_t)&_spm_data_end, 0x30102000, tcm_heap_keepout);

/* ── Prototype helper ──────────────────────────────────────────────────────── */
static esp_err_t nvs_init(void);


/* ── app_main ──────────────────────────────────────────────────────────────── */
void app_main(void)
{
    ESP_LOGI(TAG, "══ PIG WEIGH — TRẠM CÂN HEO — KHỞI ĐỘNG ══");
    ESP_LOGI(TAG, "ESP32-P4-WIFI6-POE-ETH | LVGL v9 | MIPI-DSI 800×1280");

    /* 1. NVS ---------------------------------------------------------------- */
    ESP_ERROR_CHECK(nvs_init());
    ESP_LOGI(TAG, "[1/6] NVS OK");

    /* Múi giờ cố định GMT+7 (Việt Nam) — thiết bị chỉ triển khai trong nước,
     * không cần màn chọn múi giờ. localtime()/strftime() dùng ở khắp nơi
     * (đồng hồ header, màn Cài đặt, màn chờ) sẽ tự hiển thị đúng giờ VN
     * ngay khi giờ hệ thống (UTC) được đặt — qua NTP (wifi_manager.c) hoặc
     * chỉnh tay. Trước đây KHÔNG có dòng này ở đâu trong dự án → localtime()
     * luôn trả về giờ UTC (lệch 7 tiếng so với giờ VN thực tế). POSIX TZ
     * "ICT-7" = Indochina Time, lệch UTC-(-7) = UTC+7 (dấu NGƯỢC theo chuẩn
     * POSIX). */
    setenv("TZ", "ICT-7", 1);
    tzset();

    /* 2. Network stack ------------------------------------------------------ */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_LOGI(TAG, "[2/6] NetIF + Event loop OK");

    /* 3. Display ------------------------------------------------------------ */
    display_config_t disp_cfg = {
        .h_res          = BOARD_LCD_H_RES,
        .v_res          = BOARD_LCD_V_RES,
        .dsi_lanes      = BOARD_LCD_DSI_LANES,
        .lane_mbps      = BOARD_LCD_DSI_LANE_MBPS,
        /* Không đặt dpi_clk_mhz: JD9365_800_1280_PANEL_60HZ_DPI_CONFIG() trong
         * display_driver.c đã cung cấp toàn bộ timing DPI (gồm pixel clock) —
         * đúng như ghi chú sẵn có ở board_config.h ("Không cần khai báo thủ
         * công ở đây"). BOARD_LCD_DPI_CLK_MHZ trước đây được tham chiếu ở đây
         * nhưng KHÔNG hề được khai báo (lỗi biên dịch) và cũng không được
         * display_driver.c đọc tới — đã bỏ dòng này thay vì thêm 1 macro chết. */
        .rst_gpio       = BOARD_LCD_RST_GPIO,
        .bl_gpio        = BOARD_LCD_BL_GPIO,
        .bl_ledc_ch     = BOARD_LCD_BL_LEDC_CH,
    };
    ESP_ERROR_CHECK(display_init(&disp_cfg));
    /* Panel component để lại bus I2C nội bộ (đèn nền) trên đúng 2 chân cảm
       ứng dùng chung — phải giải phóng TRƯỚC khi gt9271_init() bên dưới
       chiếm lại 2 chân đó, nếu không cảm ứng đọc chập chờn/không phản hồi. */
    display_release_backlight_i2c();
    ESP_LOGI(TAG, "[3/6] Display MIPI-DSI OK");

    /* 4. Touch -------------------------------------------------------------- */
    gt9271_config_t touch_cfg = {
        .i2c_num    = BOARD_TOUCH_I2C_NUM,
        .sda_gpio   = BOARD_TOUCH_I2C_SDA,
        .scl_gpio   = BOARD_TOUCH_I2C_SCL,
        .int_gpio   = BOARD_TOUCH_INT_GPIO,
        .rst_gpio   = BOARD_TOUCH_RST_GPIO,
        .i2c_addr   = BOARD_TOUCH_I2C_ADDR,
        .i2c_freq   = BOARD_TOUCH_I2C_FREQ,
        .h_res      = BOARD_LCD_H_RES,
        .v_res      = BOARD_LCD_V_RES,
    };
    /* Không dùng ESP_ERROR_CHECK ở đây: nếu GT911 lỗi I2C (địa chỉ/dây/reset
       sai — xem cảnh báo BOARD_TOUCH_I2C_ADDR trong board_config.h), hệ thống
       sẽ abort() và reboot-loop vô hạn, làm cả màn hình MIPI-DSI (đã lên hình
       tốt) không bao giờ hiển thị được. Cho phép chạy tiếp không cảm ứng và
       log rõ lỗi để debug phần cứng riêng, thay vì crash toàn bộ thiết bị. */
    esp_err_t touch_err = gt9271_init(&touch_cfg);
    if (touch_err == ESP_OK) {
        touch_err = display_register_touch();
    }
    if (touch_err == ESP_OK) {
        ESP_LOGI(TAG, "[4/6] Touch GT9271 OK");
    } else {
        ESP_LOGE(TAG, "[4/6] Touch GT9271 LOI (%s) — tiep tuc khong cam ung. "
                       "Kiem tra day I2C/dia chi GT911 trong board_config.h",
                 esp_err_to_name(touch_err));
    }

    /* 5. Ethernet PoE ------------------------------------------------------- */
    eth_manager_config_t eth_cfg = {
        .mdc_gpio       = BOARD_ETH_MDC_GPIO,
        .mdio_gpio      = BOARD_ETH_MDIO_GPIO,
        .phy_rst_gpio   = BOARD_ETH_PHY_RST_GPIO,
        .phy_addr       = BOARD_ETH_PHY_ADDR,
    };
    /* Cũng không dùng ESP_ERROR_CHECK: PHY LAN8720 báo sai OUI (MDC/MDIO/
       phy_addr/reset chưa đúng phần cứng) không được phép làm crash-loop
       toàn bộ thiết bị — máy vẫn phải lên màn hình để cân được, mạng có thể
       cấu hình/sửa sau. */
    esp_err_t eth_err = eth_manager_init(&eth_cfg);
    if (eth_err == ESP_OK) {
        ESP_LOGI(TAG, "[5/7] Ethernet PoE LAN8720 OK");
    } else {
        ESP_LOGE(TAG, "[5/7] Ethernet LOI (%s) — tiep tuc khong mang. "
                       "Kiem tra MDC/MDIO/phy_addr/reset trong board_config.h",
                 esp_err_to_name(eth_err));
    }

    /* 6. Wi-Fi qua ESP32-C6 (SDIO/esp_hosted) — init nền, non-fatal.
       Auto-connect nếu đã lưu SSID/pass; màn CÀI ĐẶT dùng để quét/đổi mạng. */
    wifi_manager_init();
    ESP_LOGI(TAG, "[6/7] WiFi (ESP32-C6) dang khoi tao nen");

    /* 7. UI LVGL — app_state (dữ liệu mô phỏng) + ui_shell (toàn bộ giao diện
     *    theo esp32p4-lvgl-handoff.md) — mọi lv_* call phải bọc
     *    lvgl_port_lock()/lvgl_port_unlock() (xem display_driver.c). --------- */
    app_state_init();
    if (lvgl_port_lock(0)) {
        ui_shell_build();
        lvgl_port_unlock();
    } else {
        ESP_LOGE(TAG, "lvgl_port_lock thất bại — không dựng được UI");
    }
    ESP_LOGI(TAG, "[7/7] UI LVGL (Pig Weigh) OK");

    ESP_LOGI(TAG, "══ KHỞI ĐỘNG HOÀN TẤT ══");

    /* app_main không cần loop — LVGL task quản lý render */
    vTaskDelete(NULL);
}

/* ── Helpers ───────────────────────────────────────────────────────────────── */
static esp_err_t nvs_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition bị hỏng, đang xóa...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}
