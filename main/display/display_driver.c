/**
 * @file display_driver.c
 * @brief Display init cho ESP32-P4 + LVGL v9 via esp_lvgl_port
 *
 * Hỗ trợ hai chế độ tuỳ cấu hình:
 *   - SPI panel (ILI9488, ST7796, ...) — dùng khi dùng module SPI TFT
 *   - MIPI DSI panel                   — dùng khi dùng Function-EV-Board chính thức
 *
 * Chọn bằng #define DISP_USE_MIPI_DSI / DISP_USE_SPI trong menuconfig hoặc ở đây.
 */

#include "display_driver.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/* ---- Cấu hình panel — thay đổi theo hardware thực tế ---- */
#define DISP_USE_SPI    1     /* 0 = MIPI DSI, 1 = SPI */

#if DISP_USE_SPI
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_st7789.h"   /* thay bằng header panel thực tế */
#include "driver/spi_master.h"
#include "driver/gpio.h"

/* GPIO mapping — chỉnh theo schematic board */
#define LCD_HOST         SPI2_HOST
#define LCD_SCLK_PIN     12
#define LCD_MOSI_PIN     13
#define LCD_CS_PIN       10
#define LCD_DC_PIN       9
#define LCD_RST_PIN      11
#define LCD_BL_PIN       46

#define LCD_CMD_BITS     8
#define LCD_PARAM_BITS   8
#define LCD_SPI_FREQ_HZ  (40 * 1000 * 1000)  /* 40 MHz */
#endif /* DISP_USE_SPI */

static const char *TAG = "DISPLAY";
static SemaphoreHandle_t s_lvgl_mutex = NULL;

/* ---------------------------------------------------------- */
/*  Backlight                                                   */
/* ---------------------------------------------------------- */
static void backlight_init(void)
{
#if DISP_USE_SPI
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LCD_BL_PIN),
        .mode         = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);
    gpio_set_level(LCD_BL_PIN, 1);  /* BL ON */
#endif
}

/* ---------------------------------------------------------- */
/*  display_driver_init                                         */
/* ---------------------------------------------------------- */
void display_driver_init(void)
{
    ESP_LOGI(TAG, "Init display %dx%d", DISP_HOR_RES, DISP_VER_RES);

#if DISP_USE_SPI
    /* --- SPI bus --- */
    spi_bus_config_t buscfg = {
        .mosi_io_num   = LCD_MOSI_PIN,
        .miso_io_num   = -1,
        .sclk_io_num   = LCD_SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = DISP_HOR_RES * 80 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    /* --- Panel IO (SPI) --- */
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num       = LCD_DC_PIN,
        .cs_gpio_num       = LCD_CS_PIN,
        .pclk_hz           = LCD_SPI_FREQ_HZ,
        .lcd_cmd_bits      = LCD_CMD_BITS,
        .lcd_param_bits    = LCD_PARAM_BITS,
        .spi_mode          = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST,
                                              &io_config, &io_handle));

    /* --- Panel (ST7796 / ILI9488 — đổi hàm nếu panel khác) --- */
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_RST_PIN,
        .rgb_endian     = LCD_RGB_ENDIAN_BGR,
        .bits_per_pixel = DISP_COLOR_DEPTH,
    };
    /*
     * Chọn hàm tạo panel phù hợp với màn hình thực tế:
     *   esp_lcd_new_panel_st7789  → ST7789 (phổ biến)
     *   esp_lcd_new_panel_ili9341 → ILI9341
     *   esp_lcd_new_panel_ssd1306 → SSD1306 (mono OLED)
     * Include header tương ứng ở đầu file.
     */
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, false));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    /* --- Backlight --- */
    backlight_init();

    /* --- LVGL port --- */
    const lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&port_cfg));

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle      = io_handle,
        .panel_handle   = panel_handle,
        .buffer_size    = DISP_HOR_RES * 40,  /* 40 dòng */
        .double_buffer  = true,
        .hres           = DISP_HOR_RES,
        .vres           = DISP_VER_RES,
        .monochrome     = false,
        .color_format   = LV_COLOR_FORMAT_RGB565,
        .rotation = {
            .swap_xy  = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_spiram = true,  /* Frame buffer trong PSRAM */
        },
    };
    lvgl_port_add_disp(&disp_cfg);

    /* Mutex cho thread safety */
    s_lvgl_mutex = xSemaphoreCreateMutex();

#else
    /* MIPI DSI path — implement theo esp_lcd_mipi_dsi driver của ESP-IDF 5.3+ */
    ESP_LOGE(TAG, "MIPI DSI not implemented yet — set DISP_USE_SPI=1");
    ESP_ERROR_CHECK(ESP_ERR_NOT_SUPPORTED);
#endif

    ESP_LOGI(TAG, "Display init OK");
}

/* ---------------------------------------------------------- */
/*  LVGL mutex helpers                                          */
/* ---------------------------------------------------------- */
void lvgl_lock(void)
{
    if (s_lvgl_mutex) xSemaphoreTake(s_lvgl_mutex, portMAX_DELAY);
}

void lvgl_unlock(void)
{
    if (s_lvgl_mutex) xSemaphoreGive(s_lvgl_mutex);
}
