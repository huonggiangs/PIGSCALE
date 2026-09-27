/**
 * @file display_driver.c
 * @brief MIPI-DSI display driver cho ESP32-P4-WIFI6-POE-ETH
 *
 * Panel: JD9365 (Jadard Technology) — 10.1" 800×1280 portrait, 2-lane MIPI-DSI
 *
 * Pipeline:
 *   LDO (MIPI PHY 2.5V)
 *   → esp_lcd_dsi_bus
 *   → panel_io (DBI/DCS, lệnh init panel)
 *   → esp_lcd_new_panel_jd9365()  [bao gồm DPI video panel nội bộ]
 *   → esp_lvgl_port
 *
 * Lưu ý thread-safety:
 *   Mọi lv_* call phải bọc lvgl_port_lock() / lvgl_port_unlock().
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_ldo_regulator.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_jd9365_10_1.h"     /* waveshare/esp_lcd_jd9365_10_1 managed component —
                                        KHÔNG dùng espressif/esp_lcd_jd9365: đọc ID panel
                                        lúc init bị treo cứng trên panel 10.1" này khi
                                        lane rate không đúng 1500 Mbps (đã xác nhận qua
                                        E:\Project\ESP32P4YOLO26n). Component này còn tự
                                        bật đèn nền qua IC I2C 0x45 trong lúc init. */
#include "esp_lvgl_port.h"

#include "board_config.h"
#include "display_driver.h"
#include "gt9271.h"

static const char *TAG = "display";

/* ── LDO channel cho MIPI DSI PHY (2.5 V) ─────────────────────────────────── */
#define MIPI_PHY_LDO_CHAN        3
#define MIPI_PHY_LDO_MV         2500

/* ── Handles (module-level statics) ──────────────────────────────────────── */
static esp_ldo_channel_handle_t  s_ldo_mipi    = NULL;
static esp_lcd_dsi_bus_handle_t  s_dsi_bus     = NULL;
static esp_lcd_panel_io_handle_t s_panel_io    = NULL;
static esp_lcd_panel_handle_t    s_panel       = NULL;   /* JD9365 + DPI tích hợp */
static lv_display_t             *s_lv_disp     = NULL;
static lv_indev_t               *s_lv_touch    = NULL;
static display_config_t          s_cfg;

/* ── Backlight (LEDC PWM, 10-bit, 5 kHz) ─────────────────────────────────── */
static void backlight_init(gpio_num_t gpio, uint8_t ledc_ch)
{
    ledc_timer_config_t timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .duty_resolution  = LEDC_TIMER_10_BIT,
        .timer_num        = LEDC_TIMER_0,
        .freq_hz          = 5000,
        .clk_cfg          = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t ch = {
        .gpio_num   = gpio,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = ledc_ch,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 0,
        .hpoint     = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch));
}

void display_set_brightness(uint8_t percent)
{
    if (percent > 100) percent = 100;
    uint32_t duty = (percent * 1023u) / 100u;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, s_cfg.bl_ledc_ch, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, s_cfg.bl_ledc_ch);
}

void display_release_backlight_i2c(void)
{
    /* waveshare/esp_lcd_jd9365_10_1 tạo bus I2C_NUM_1 trên đúng 2 chân
       BOARD_TOUCH_I2C_SDA/SCL (GPIO7/8) để bật đèn nền qua IC 0x45, rồi
       KHÔNG xoá bus đó (i2c_bus_delete bị comment trong mã nguồn của họ) —
       2 chân này vẫn còn nối tới bộ điều khiển I2C số 1. gpio_reset_pin()
       đưa 2 chân về trạng thái IOMUX/GPIO mặc định, gỡ hết định tuyến GPIO
       matrix cũ, để i2c_new_master_bus() gọi sau đó (trong gt9271_init(),
       trên I2C_NUM_0) chiếm lại 2 chân một cách sạch sẽ — tránh cảnh báo
       "GPIO 7/8 is not usable, maybe conflict with others" và tình trạng
       cảm ứng đọc chập chờn/không phản hồi do 2 controller cùng tranh 1 bus. */
    gpio_reset_pin(BOARD_TOUCH_I2C_SDA);
    gpio_reset_pin(BOARD_TOUCH_I2C_SCL);
    ESP_LOGI(TAG, "Da giai phong I2C GPIO%d/GPIO%d khoi bus den nen noi bo",
             BOARD_TOUCH_I2C_SDA, BOARD_TOUCH_I2C_SCL);
}

/* ── display_init ─────────────────────────────────────────────────────────── */
esp_err_t display_init(const display_config_t *cfg)
{
    ESP_RETURN_ON_FALSE(cfg, ESP_ERR_INVALID_ARG, TAG, "cfg is NULL");
    memcpy(&s_cfg, cfg, sizeof(s_cfg));

    /* ── 1. LVGL port init ──────────────────────────────────────────────── */
    const lvgl_port_cfg_t port_cfg = {
        .task_priority     = LVGL_TASK_PRIORITY,
        .task_stack        = LVGL_TASK_STACK_KB * 1024,
        .task_affinity     = LVGL_TASK_CORE,
        .task_max_sleep_ms = 500,
        .timer_period_ms   = LVGL_TICK_PERIOD_MS,
    };
    ESP_RETURN_ON_ERROR(lvgl_port_init(&port_cfg), TAG, "lvgl_port_init failed");

    /* ── 2. LDO cho MIPI DSI PHY ────────────────────────────────────────── */
    ESP_LOGI(TAG, "Power up MIPI DSI PHY LDO: chan=%d, %d mV",
             MIPI_PHY_LDO_CHAN, MIPI_PHY_LDO_MV);
    esp_ldo_channel_config_t ldo_cfg = {
        .chan_id    = MIPI_PHY_LDO_CHAN,
        .voltage_mv = MIPI_PHY_LDO_MV,
    };
    ESP_RETURN_ON_ERROR(
        esp_ldo_acquire_channel(&ldo_cfg, &s_ldo_mipi),
        TAG, "esp_ldo_acquire_channel failed"
    );

    /* ── 3. MIPI-DSI bus (2-lane, 1000 Mbps/lane) ───────────────────────── */
    esp_lcd_dsi_bus_config_t dsi_bus_cfg = JD9365_PANEL_BUS_DSI_2CH_CONFIG();
    /* Cho phép override lane rate từ board_config nếu cần */
    dsi_bus_cfg.lane_bit_rate_mbps = cfg->lane_mbps;
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_dsi_bus(&dsi_bus_cfg, &s_dsi_bus),
        TAG, "esp_lcd_new_dsi_bus failed"
    );

    /* ── 4. Panel IO (DBI — giao tiếp DCS để gửi lệnh init) ─────────────── */
    esp_lcd_dbi_io_config_t dbi_cfg = JD9365_PANEL_IO_DBI_CONFIG();
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_dbi(s_dsi_bus, &dbi_cfg, &s_panel_io),
        TAG, "esp_lcd_new_panel_io_dbi failed"
    );

    /* ── 5. JD9365 panel driver (bao gồm DPI video panel) ──────────────── */
    /*
     * JD9365_800_1280_PANEL_60HZ_DPI_CONFIG: timing 800×1280 @60 Hz
     * use_mipi_interface = 1: driver tạo DPI panel nội bộ
     * panel_handle trả về = DPI panel handle (dùng trực tiếp cho LVGL)
     */
    esp_lcd_dpi_panel_config_t dpi_cfg =
        JD9365_800_1280_PANEL_60HZ_DPI_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB565);
    /* num_fbs=1 (mặc định của macro): LVGL vẽ vào buffer riêng trong PSRAM,
       esp_lcd_panel_draw_bitmap() dùng DMA2D chép sang frame buffer panel —
       xem lý do KHÔNG dùng direct_mode/avoid_tearing ở lv_disp_cfg bên dưới. */

    jd9365_vendor_config_t vendor_cfg = {
        .init_cmds      = NULL,
        .init_cmds_size = 0,
        .mipi_config = {
            .dsi_bus    = s_dsi_bus,
            .dpi_config = &dpi_cfg,
            .lane_num   = cfg->dsi_lanes,
        },
    };

    const esp_lcd_panel_dev_config_t panel_dev_cfg = {
        .reset_gpio_num = cfg->rst_gpio,
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,           /* RGB565 */
        .vendor_config  = &vendor_cfg,
    };
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_jd9365(s_panel_io, &panel_dev_cfg, &s_panel),
        TAG, "esp_lcd_new_panel_jd9365 failed"
    );

    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel),    TAG, "panel_reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel),     TAG, "panel_init");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, true), TAG, "disp_on");

    /* ── 6. Đăng ký display vào LVGL ───────────────────────────────────── */
    /* Panel là MIPI-DSI (DPI) — BẮT BUỘC dùng lvgl_port_add_disp_dsi(), không
       phải lvgl_port_add_disp() (dành cho I2C/SPI/I8080). Dùng sai hàm khiến
       esp_lvgl_port không đăng ký esp_lcd_dpi_panel_register_event_callbacks
       và gọi nhầm draw_bitmap của panel DPI (không tồn tại) → crash ngay
       (Instruction access fault, MEPC=0).

       KHÔNG dùng direct_mode + avoid_tearing=true: đã xác nhận qua dự án
       tham chiếu E:\Project\ESP32P4YOLO26n rằng trên IDF 5.5, avoid_tearing
       khiến esp_lvgl_port chờ trên trans_sem được nhả bởi callback
       on_refresh_done — nhưng callback này đã bị đổi tên thành
       on_frame_buf_complete, không bao giờ được gọi nữa → task LVGL treo
       VĨNH VIỄN ngay lần vẽ đầu tiên (vẫn giữ khóa lvgl_port_lock, kéo theo
       toàn bộ app_main bị treo theo ở bước gọi tiếp theo).

       Dùng lại đúng cấu hình đã chạy ổn định trên cùng board/panel: buffer
       vẽ 1/4 màn hình trong PSRAM, KHÔNG direct_mode, esp_lcd_panel_draw_bitmap
       tự DMA2D chép sang frame buffer panel (num_fbs=1 ở dpi_cfg phía trên). */
    const lvgl_port_display_cfg_t lv_disp_cfg = {
        .io_handle     = s_panel_io,
        .panel_handle  = s_panel,
        .buffer_size   = (cfg->h_res * cfg->v_res) / 4,
        .double_buffer = true,
        .hres          = cfg->h_res,
        .vres          = cfg->v_res,
        .monochrome    = false,
        .color_format  = LV_COLOR_FORMAT_RGB565,
        .rotation = {
            .swap_xy  = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_spiram  = true,   /* Buffer vẽ trong PSRAM */
            .direct_mode  = false,
            .full_refresh = false,
        },
    };
    const lvgl_port_display_dsi_cfg_t dsi_cfg = {
        .flags = {
            .avoid_tearing = false,
        },
    };
    s_lv_disp = lvgl_port_add_disp_dsi(&lv_disp_cfg, &dsi_cfg);
    ESP_RETURN_ON_FALSE(s_lv_disp, ESP_FAIL, TAG, "lvgl_port_add_disp_dsi failed");

    /* ── 7. Backlight ───────────────────────────────────────────────────── */
    /* Đèn nền thật đã được waveshare/esp_lcd_jd9365_10_1 bật lên mức tối đa
       qua IC I2C 0x45 ngay trong esp_lcd_panel_init() ở trên. Lệnh LEDC/PWM
       dưới đây chỉ đi ra GPIO26 — không nối tới đèn nền trên board này —
       giữ lại vô hại để tương thích chữ ký display_config_t, không phải
       đường điều khiển độ sáng thật. */
    backlight_init(cfg->bl_gpio, cfg->bl_ledc_ch);
    display_set_brightness(85);

    ESP_LOGI(TAG, "Display ready: %"PRIu32"x%"PRIu32" JD9365 MIPI-DSI",
             cfg->h_res, cfg->v_res);
    return ESP_OK;
}

/* ── display_register_touch ──────────────────────────────────────────────── */
esp_err_t display_register_touch(void)
{
    ESP_RETURN_ON_FALSE(s_lv_disp, ESP_ERR_INVALID_STATE,
                        TAG, "Display chua duoc init");

    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp   = s_lv_disp,
        .handle = (esp_lcd_touch_handle_t)gt9271_get_handle(),
    };
    s_lv_touch = lvgl_port_add_touch(&touch_cfg);
    ESP_RETURN_ON_FALSE(s_lv_touch, ESP_FAIL, TAG, "lvgl_port_add_touch failed");

    ESP_LOGI(TAG, "Touch GT911 registered with LVGL");
    return ESP_OK;
}

/* ── display_get_handle ──────────────────────────────────────────────────── */
lv_display_t *display_get_handle(void)
{
    return s_lv_disp;
}
