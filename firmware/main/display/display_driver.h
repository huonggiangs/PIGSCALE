#pragma once
/**
 * @file display_driver.h
 * @brief MIPI-DSI display driver + esp_lvgl_port integration
 *        Board: ESP32-P4-WIFI6-POE-ETH | Panel: JD9365 10.1" 800×1280
 */

#include "esp_err.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_lvgl_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Cấu hình display ───────────────────────────────────────────────────── */
typedef struct {
    uint32_t h_res;          /*!< Độ rộng (pixels) */
    uint32_t v_res;          /*!< Độ cao  (pixels) */
    uint8_t  dsi_lanes;      /*!< Số data lanes (thường = 2) */
    uint32_t lane_mbps;      /*!< Tốc độ mỗi lane (Mbps) */
    uint32_t dpi_clk_mhz;   /*!< Pixel clock DPI (MHz) */
    gpio_num_t rst_gpio;     /*!< GPIO reset panel */
    gpio_num_t bl_gpio;      /*!< GPIO backlight PWM */
    uint8_t    bl_ledc_ch;   /*!< LEDC channel cho backlight */
} display_config_t;

/**
 * @brief Khởi tạo MIPI-DSI bus, panel driver và LVGL port
 * @param cfg  Con trỏ tới display_config_t
 * @return ESP_OK nếu thành công
 */
esp_err_t display_init(const display_config_t *cfg);

/**
 * @brief Đăng ký touch GT9271 vào LVGL (gọi SAU gt9271_init)
 * @return ESP_OK nếu thành công
 */
esp_err_t display_register_touch(void);

/**
 * @brief Giải phóng bus I2C nội bộ (I2C_NUM_1, GPIO7/8) mà component
 *        waveshare/esp_lcd_jd9365_10_1 tạo ra để bật đèn nền qua IC 0x45,
 *        rồi bỏ quên không xoá (dòng i2c_bus_delete bị comment trong mã
 *        nguồn của họ). Nếu không gọi hàm này TRƯỚC khi khởi tạo I2C cho
 *        cảm ứng/audio/camera trên I2C_NUM_0 (cùng vật lý GPIO7/8), driver
 *        sẽ cảnh báo "GPIO 7/8 is not usable, maybe conflict with others"
 *        và việc đọc cảm ứng có thể chập chờn/không phản hồi do 2 bộ điều
 *        khiển I2C cùng tranh nhau 2 chân này.
 *        Gọi NGAY SAU display_init() thành công, TRƯỚC gt9271_init().
 */
void display_release_backlight_i2c(void);

/**
 * @brief Đặt độ sáng màn hình (0–100)
 */
void display_set_brightness(uint8_t percent);

/**
 * @brief Lấy handle lv_display_t (dùng khi cần cấu hình thêm)
 */
lv_display_t *display_get_handle(void);

#ifdef __cplusplus
}
#endif
