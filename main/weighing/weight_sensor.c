/**
 * @file weight_sensor.c
 * @brief HX711 24-bit ADC driver cho load cell
 *
 * GPIO mặc định (thay đổi qua menuconfig hoặc define):
 *   DOUT = GPIO 4
 *   SCK  = GPIO 5
 */

#include "weight_sensor.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "WEIGHT_SENSOR";

/* Mặc định GPIO */
#define HX711_DOUT_DEFAULT  4
#define HX711_SCK_DEFAULT   5

/* Calibration lưu trong NVS — mặc định ban đầu */
#define DEFAULT_SCALE_FACTOR   450.0f   /* raw/kg — calibrate thực tế */
#define DEFAULT_OFFSET         0L

static int s_dout = HX711_DOUT_DEFAULT;
static int s_sck  = HX711_SCK_DEFAULT;
static int32_t s_tare_offset = DEFAULT_OFFSET;
static float   s_scale_factor = DEFAULT_SCALE_FACTOR;
static bool    s_initialized = false;

/* ---------------------------------------------------------- */
static int32_t hx711_read_once(void)
{
    /* Chờ DOUT = LOW (ready) */
    int timeout = 500;  /* ms */
    while (gpio_get_level(s_dout) == 1 && timeout > 0) {
        vTaskDelay(pdMS_TO_TICKS(1));
        timeout--;
    }
    if (timeout == 0) {
        ESP_LOGW(TAG, "HX711 timeout");
        return 0;
    }

    int32_t data = 0;

    /* Đọc 24 bit */
    for (int i = 0; i < 24; i++) {
        gpio_set_level(s_sck, 1);
        esp_rom_delay_us(1);
        data = (data << 1) | gpio_get_level(s_dout);
        gpio_set_level(s_sck, 0);
        esp_rom_delay_us(1);
    }

    /* 1 xung thêm = Channel A Gain 128 */
    gpio_set_level(s_sck, 1);
    esp_rom_delay_us(1);
    gpio_set_level(s_sck, 0);
    esp_rom_delay_us(1);

    /* Sign extend 24->32 bit */
    if (data & 0x800000) {
        data |= 0xFF000000;
    }

    return data;
}

/* ---------------------------------------------------------- */
void weight_sensor_init(int dout_pin, int sck_pin)
{
    s_dout = (dout_pin >= 0) ? dout_pin : HX711_DOUT_DEFAULT;
    s_sck  = (sck_pin  >= 0) ? sck_pin  : HX711_SCK_DEFAULT;

    gpio_config_t io = {
        .pin_bit_mask = (1ULL << s_dout),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io);

    io.pin_bit_mask = (1ULL << s_sck);
    io.mode         = GPIO_MODE_OUTPUT;
    io.pull_up_en   = GPIO_PULLUP_DISABLE;
    gpio_config(&io);
    gpio_set_level(s_sck, 0);

    s_initialized = true;
    ESP_LOGI(TAG, "HX711 init: DOUT=%d SCK=%d", s_dout, s_sck);
}

bool weight_sensor_is_ready(void)
{
    return s_initialized && (gpio_get_level(s_dout) == 0);
}

int32_t weight_sensor_read_raw(void)
{
    if (!s_initialized) return 0;
    return hx711_read_once();
}

float weight_sensor_read_kg(void)
{
    if (!s_initialized) {
        /* MOCK: giả lập tăng dần khi đang chạy demo */
        return 0.0f;
    }
    int32_t raw = hx711_read_once();
    float kg = (float)(raw - s_tare_offset) / s_scale_factor;
    if (kg < 0) kg = 0;
    return kg;
}

void weight_sensor_tare(void)
{
    if (!s_initialized) return;
    /* Trung bình 10 mẫu */
    int64_t sum = 0;
    for (int i = 0; i < 10; i++) {
        sum += hx711_read_once();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    s_tare_offset = (int32_t)(sum / 10);
    ESP_LOGI(TAG, "Tare offset = %ld", (long)s_tare_offset);
}

void weight_sensor_calibrate(float known_kg)
{
    if (!s_initialized || known_kg <= 0) return;
    int32_t raw = hx711_read_once();
    s_scale_factor = (float)(raw - s_tare_offset) / known_kg;
    ESP_LOGI(TAG, "Scale factor = %.2f", s_scale_factor);
}
