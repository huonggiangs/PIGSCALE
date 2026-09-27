#pragma once
/**
 * @file gt9271.h
 * @brief GT9271 touch driver — wrapper trên espressif/esp_lcd_touch_gt911
 *
 * GT9271 tương thích hoàn toàn với GT911 (cùng giao thức I2C, cùng register map).
 * Thực tế sử dụng esp_lcd_touch_new_i2c_gt911() nên struct nội bộ không bị expose.
 */

#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Cấu hình init ──────────────────────────────────────────────────────── */
typedef struct {
    i2c_port_t  i2c_num;
    gpio_num_t  sda_gpio;
    gpio_num_t  scl_gpio;
    gpio_num_t  int_gpio;
    gpio_num_t  rst_gpio;
    uint8_t     i2c_addr;   /*!< 0x5D (INT high during reset) hoặc 0x14 */
    uint32_t    i2c_freq;   /*!< Hz — thường 400000 */
    uint16_t    h_res;      /*!< Độ phân giải X */
    uint16_t    v_res;      /*!< Độ phân giải Y */
} gt9271_config_t;

/**
 * @brief Khởi tạo GT9271 (chuỗi reset GPIO + GT911 driver)
 */
esp_err_t gt9271_init(const gt9271_config_t *cfg);

/**
 * @brief Lấy esp_lcd_touch_handle_t để truyền vào lvgl_port_add_touch()
 * @return Con trỏ cast từ esp_lcd_touch_handle_t, hoặc NULL nếu chưa init
 */
void *gt9271_get_handle(void);

/**
 * @brief Deinit và giải phóng tài nguyên
 */
esp_err_t gt9271_deinit(void);

#ifdef __cplusplus
}
#endif
