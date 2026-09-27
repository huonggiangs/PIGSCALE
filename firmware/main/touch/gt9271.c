/**
 * @file gt9271.c
 * @brief GT9271 touch driver — sử dụng espressif/esp_lcd_touch_gt911
 *
 * GT9271 và GT911 dùng cùng giao thức I2C / register map, chỉ khác tên chip.
 * esp_lcd_touch_new_i2c_gt911() hoạt động đúng với GT9271.
 *
 * Luồng init:
 *  1. Cấu hình GPIO RST + INT
 *  2. Chuỗi reset phần cứng để chọn I2C address (0x5D / 0x14)
 *  3. Tạo I2C master bus
 *  4. Tạo esp_lcd panel_io (I2C abstraction layer)
 *  5. Gọi esp_lcd_touch_new_i2c_gt911() → trả về esp_lcd_touch_handle_t
 *
 * Lý do dùng GT911 driver:
 *   struct esp_lcd_touch_t là opaque type — không cho phép sizeof / truy cập trực tiếp
 *   từ bên ngoài component espressif/esp_lcd_touch.
 */

#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_gt911.h"

#include "gt9271.h"

static const char *TAG = "gt9271";

/* ── Module state ──────────────────────────────────────────────────────────── */
static i2c_master_bus_handle_t   s_bus       = NULL;
static esp_lcd_panel_io_handle_t s_tp_io     = NULL;
static esp_lcd_touch_handle_t    s_tp_handle = NULL;

/* ── gt9271_init ────────────────────────────────────────────────────────────── */
esp_err_t gt9271_init(const gt9271_config_t *cfg)
{
    ESP_RETURN_ON_FALSE(cfg, ESP_ERR_INVALID_ARG, TAG, "cfg NULL");

    /* ── 1. GPIO reset + INT ────────────────────────────────────────────── */
    /* Trên board Waveshare ESP32-P4-WIFI6-POE-ETH, RST/INT của GT911/GT9271
     * KHÔNG nối tới ESP32-P4 (xác nhận qua schematic + dự án tham chiếu
     * E:\Project\ESP32P4YOLO26n — dùng rst_gpio_num=-1, int_gpio_num=-1,
     * đọc theo chu kỳ/polling). Cấu hình rst_gpio/int_gpio = GPIO_NUM_NC (-1)
     * trong board_config.h để bỏ qua chuỗi reset/chọn địa chỉ dưới đây —
     * chip dùng địa chỉ mặc định theo strap phần cứng (0x5D). Nếu bo mạch của
     * bạn thực sự có nối RST/INT, đặt lại 2 macro đó về GPIO thật để dùng
     * chuỗi reset này. */
    bool have_rst = (cfg->rst_gpio >= 0);
    bool have_int = (cfg->int_gpio >= 0);
    if (have_rst || have_int) {
        gpio_config_t io_conf = {
            .pin_bit_mask = (have_rst ? (1ULL << cfg->rst_gpio) : 0) |
                            (have_int ? (1ULL << cfg->int_gpio) : 0),
            .mode         = GPIO_MODE_OUTPUT,
            .pull_up_en   = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_DISABLE,
        };
        ESP_RETURN_ON_ERROR(gpio_config(&io_conf), TAG, "gpio_config failed");

        /*
         * Chuỗi reset để chọn I2C address:
         *   addr 0x5D → INT = HIGH trong khi RST = LOW
         *   addr 0x14 → INT = LOW  trong khi RST = LOW
         */
        if (have_rst) gpio_set_level(cfg->rst_gpio, 0);
        if (have_int) gpio_set_level(cfg->int_gpio, (cfg->i2c_addr == 0x5D) ? 1 : 0);
        vTaskDelay(pdMS_TO_TICKS(15));
        if (have_rst) gpio_set_level(cfg->rst_gpio, 1);
        vTaskDelay(pdMS_TO_TICKS(60));

        /* INT → input (GT9271 sẽ tự drive khi có touch event) */
        if (have_int) gpio_set_direction(cfg->int_gpio, GPIO_MODE_INPUT);
    }

    /* ── 2. I2C master bus ──────────────────────────────────────────────── */
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port              = cfg->i2c_num,
        .sda_io_num            = cfg->sda_gpio,
        .scl_io_num            = cfg->scl_gpio,
        .clk_source            = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt     = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(
        i2c_new_master_bus(&bus_cfg, &s_bus),
        TAG, "i2c_new_master_bus failed"
    );

    /* ── 3. LCD I2C panel IO (abstraction layer cho GT911 driver) ────────── */
    esp_lcd_panel_io_i2c_config_t tp_io_cfg = {
        .dev_addr              = cfg->i2c_addr,
        .control_phase_bytes   = 1,
        .dc_bit_offset         = 0,
        .lcd_cmd_bits          = 16,   /* GT9x register address = 16-bit */
        .lcd_param_bits        = 8,
        .scl_speed_hz          = cfg->i2c_freq,
        .flags.disable_control_phase = 1,
    };
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_i2c(s_bus, &tp_io_cfg, &s_tp_io),
        TAG, "esp_lcd_new_panel_io_i2c failed"
    );

    /* ── 4. GT911 touch driver ──────────────────────────────────────────── */
    /*
     * rst_gpio_num = GPIO_NUM_NC: bỏ qua reset nội bộ của GT911 driver
     * vì chúng ta đã thực hiện chuỗi reset (kèm address selection) ở bước 1.
     */
    esp_lcd_touch_config_t tp_cfg = {
        .x_max         = cfg->h_res,
        .y_max         = cfg->v_res,
        .rst_gpio_num  = GPIO_NUM_NC,       /* reset đã làm ở bước 1 */
        .int_gpio_num  = cfg->int_gpio,
        .levels = {
            .reset     = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy   = 0,
            .mirror_x  = 0,
            .mirror_y  = 0,
        },
    };
    ESP_RETURN_ON_ERROR(
        esp_lcd_touch_new_i2c_gt911(s_tp_io, &tp_cfg, &s_tp_handle),
        TAG, "esp_lcd_touch_new_i2c_gt911 failed"
    );

    ESP_LOGI(TAG, "GT9271 init OK (%"PRIu16"x%"PRIu16", addr=0x%02X)",
             cfg->h_res, cfg->v_res, cfg->i2c_addr);
    return ESP_OK;
}

/* ── gt9271_get_handle ──────────────────────────────────────────────────────── */
void *gt9271_get_handle(void)
{
    return (void *)s_tp_handle;
}

/* ── gt9271_deinit ──────────────────────────────────────────────────────────── */
esp_err_t gt9271_deinit(void)
{
    if (s_tp_handle) {
        esp_lcd_touch_del(s_tp_handle);
        s_tp_handle = NULL;
    }
    if (s_tp_io) {
        esp_lcd_panel_io_del(s_tp_io);
        s_tp_io = NULL;
    }
    if (s_bus) {
        i2c_del_master_bus(s_bus);
        s_bus = NULL;
    }
    return ESP_OK;
}
