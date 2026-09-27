#pragma once
/**
 * @file ads131m08.h
 * @brief ADS131M08 — 24-bit 8-channel simultaneous-sampling ADC
 *        Giao tiếp SPI, đọc 6 kênh áp suất 4-20mA cho cân máy xúc
 *
 * Kết nối phần cứng (chỉnh theo schematic):
 *   SCLK → GPIO_ADS_SCLK
 *   MOSI → GPIO_ADS_MOSI (DIN)
 *   MISO → GPIO_ADS_MISO (DOUT)
 *   CS   → GPIO_ADS_CS
 *   DRDY → GPIO_ADS_DRDY  (data ready interrupt)
 *
 * Shunt resistor: 250Ω → 4mA = 1V, 20mA = 5V → sau voltage divider vào ADC input
 */

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* ============================================================
 * GPIO mặc định — chỉnh theo board
 * ============================================================ */
#define ADS_SCLK_PIN   12
#define ADS_MOSI_PIN   13
#define ADS_MISO_PIN   11
#define ADS_CS_PIN     10
#define ADS_DRDY_PIN    9

/* ============================================================
 * Thông số sensor áp suất 4-20mA
 * ============================================================ */
#define SENSOR_I_MIN_MA    4.0f   /* mA — zero pressure */
#define SENSOR_I_MAX_MA   20.0f   /* mA — full scale pressure */
#define SENSOR_I_WIRE_MA   3.5f   /* mA threshold — dưới này = ĐỨT DÂY */

/* Pressure range từng cảm biến (bar) — điều chỉnh theo datasheet cảm biến */
#define SENSOR_P_MAX_BOOM  400.0f  /* bar — P1, P2 */
#define SENSOR_P_MAX_ARM   350.0f  /* bar — P3, P4 */
#define SENSOR_P_MAX_BUCKET 300.0f /* bar — P5, P6 */

/* Shunt resistor (Ω) — để tính I từ V đo được */
#define ADS_SHUNT_OHM    250.0f
/* ADS131M08 Vref = 1.2V, gain = 1→128 — mặc định dùng PGA=1, Vref=1.2V */
#define ADS_VREF          1.2f
#define ADS_PGA           1
#define ADS_FULL_SCALE   ((float)(1 << 23))  /* 24-bit signed */

/* ============================================================
 * Kênh mapping
 * ============================================================ */
typedef enum {
    ADS_CH_P1_BOOM_BORE   = 0,
    ADS_CH_P2_BOOM_ROD    = 1,
    ADS_CH_P3_ARM_BORE    = 2,
    ADS_CH_P4_ARM_ROD     = 3,
    ADS_CH_P5_BUCKET_BORE = 4,
    ADS_CH_P6_BUCKET_ROD  = 5,
    ADS_NUM_CHANNELS      = 6,
} ads_channel_t;

/* ============================================================
 * Dữ liệu một lần đọc
 * ============================================================ */
typedef struct {
    float    current_ma[ADS_NUM_CHANNELS];  /* dòng đo (mA) */
    float    pressure_bar[ADS_NUM_CHANNELS];/* áp suất đã quy đổi (bar) */
    bool     wire_break[ADS_NUM_CHANNELS];  /* true = ĐỨT DÂY */
    bool     valid;                          /* true = đọc thành công */
} ads_reading_t;

/* ============================================================
 * API
 * ============================================================ */

/** Khởi tạo SPI bus + chip. Gọi một lần trong app_main. */
esp_err_t ads131m08_init(void);

/**
 * @brief Đọc toàn bộ 6 kênh (blocking, chờ DRDY)
 * @param out  Pointer tới struct nhận kết quả
 * @return ESP_OK hoặc lỗi
 */
esp_err_t ads131m08_read(ads_reading_t *out);

/** Đọc raw 24-bit một kênh (chưa quy đổi) */
int32_t ads131m08_read_raw(ads_channel_t ch);

/** Chuyển đổi raw → mA dựa trên shunt + Vref */
float ads131m08_raw_to_ma(int32_t raw);

/** Chuyển đổi mA → bar */
float ads131m08_ma_to_bar(float ma, float p_max_bar);
