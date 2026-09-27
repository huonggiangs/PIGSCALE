/**
 * @file ads131m08.c
 * @brief Driver ADS131M08 — 24-bit 8-channel ADC qua SPI
 *
 * Datasheet: https://www.ti.com/product/ADS131M08
 * Protocol:  SPI Mode 1 (CPOL=0, CPHA=1), MSB first
 *            Mỗi frame = 3 bytes/channel × 8 channels + 3 byte status + 3 byte CRC
 *            = 27 bytes tổng (24 words × 3 bytes = 72 bytes khi đọc đủ)
 *
 * Luồng đọc:
 *   1. Chờ DRDY low (data ready)
 *   2. CS low
 *   3. Gửi command RDATA (0x12) hoặc NULL + đọc 27 bytes
 *   4. CS high
 *   5. Parse raw[i] từ bytes 3+i*3 .. 5+i*3 (24-bit signed)
 */

#include "ads131m08.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <math.h>

static const char *TAG = "ADS131M08";

/* ---- SPI ---- */
#define ADS_SPI_HOST     SPI3_HOST   /* dùng SPI3 (VSPI) — không dùng chung với display */
#define ADS_SPI_FREQ_HZ  (8 * 1000 * 1000)  /* 8 MHz max theo datasheet */

/* ---- Registers ---- */
#define ADS_CMD_NULL     0x0000
#define ADS_CMD_RESET    0x0011
#define ADS_CMD_STANDBY  0x0022
#define ADS_CMD_WAKEUP   0x0033
#define ADS_CMD_LOCK     0x0555
#define ADS_CMD_UNLOCK   0x0655
#define ADS_CMD_RREG(addr, n)  (0xA000 | ((addr)<<7) | ((n)-1))
#define ADS_CMD_WREG(addr, n)  (0x6000 | ((addr)<<7) | ((n)-1))

/* ---- Frame size ---- */
/* STATUS(3) + CH0..CH7(3 each) + CRC(3) = 30 bytes */
#define ADS_FRAME_BYTES  30
#define ADS_STATUS_OFF    0
#define ADS_CH_OFFSET(ch) (3 + (ch)*3)

static spi_device_handle_t s_spi = NULL;
static bool s_initialized = false;

/* p_max_bar per channel */
static const float k_p_max[ADS_NUM_CHANNELS] = {
    SENSOR_P_MAX_BOOM,   /* P1 */
    SENSOR_P_MAX_BOOM,   /* P2 */
    SENSOR_P_MAX_ARM,    /* P3 */
    SENSOR_P_MAX_ARM,    /* P4 */
    SENSOR_P_MAX_BUCKET, /* P5 */
    SENSOR_P_MAX_BUCKET, /* P6 */
};

/* ---------------------------------------------------------- */
/*  Low-level SPI                                              */
/* ---------------------------------------------------------- */
static esp_err_t spi_transfer(const uint8_t *tx, uint8_t *rx, size_t len)
{
    spi_transaction_t t = {
        .length    = len * 8,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    return spi_device_transmit(s_spi, &t);
}

/* ---------------------------------------------------------- */
/*  Init                                                        */
/* ---------------------------------------------------------- */
esp_err_t ads131m08_init(void)
{
    /* GPIO DRDY */
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << ADS_DRDY_PIN),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));

    /* SPI bus */
    spi_bus_config_t bus = {
        .mosi_io_num   = ADS_MOSI_PIN,
        .miso_io_num   = ADS_MISO_PIN,
        .sclk_io_num   = ADS_SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = ADS_FRAME_BYTES,
    };
    esp_err_t ret = spi_bus_initialize(ADS_SPI_HOST, &bus, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Device config */
    spi_device_interface_config_t dev = {
        .command_bits     = 0,
        .address_bits     = 0,
        .clock_speed_hz   = ADS_SPI_FREQ_HZ,
        .mode             = 1,          /* CPOL=0, CPHA=1 */
        .spics_io_num     = ADS_CS_PIN,
        .queue_size       = 1,
        .cs_ena_pretrans  = 2,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(ADS_SPI_HOST, &dev, &s_spi));

    /* Reset chip */
    uint8_t cmd_reset[3] = {
        (ADS_CMD_RESET >> 8) & 0xFF,
         ADS_CMD_RESET & 0xFF,
        0x00
    };
    uint8_t rx[3];
    spi_transfer(cmd_reset, rx, 3);
    vTaskDelay(pdMS_TO_TICKS(5));  /* wait reset complete */

    /*
     * Config registers:
     * MODE reg (0x02): word_length=24bit, format=signed, drdy_sel=most_lagging
     * CLOCK reg (0x03): en_ch0..ch5 = 1, ch6..ch7 = 0 (chỉ dùng 6 kênh)
     */
    /* CLOCK reg: bits[7:0] = channel enable (bit0=CH0..bit7=CH7) */
    uint8_t cfg_clock[6] = {
        (ADS_CMD_WREG(0x03, 1) >> 8) & 0xFF,
         ADS_CMD_WREG(0x03, 1) & 0xFF,
        0x3F,   /* CH0..CH5 enabled, CH6 CH7 disabled */
        0x00, 0x00, 0x00
    };
    spi_transfer(cfg_clock, NULL, 6);
    vTaskDelay(pdMS_TO_TICKS(2));

    s_initialized = true;
    ESP_LOGI(TAG, "ADS131M08 init OK — 6 channels enabled");
    return ESP_OK;
}

/* ---------------------------------------------------------- */
/*  Chờ DRDY (max 50ms)                                        */
/* ---------------------------------------------------------- */
static bool wait_drdy(void)
{
    int timeout = 500;  /* 500 × 0.1ms = 50ms */
    while (gpio_get_level(ADS_DRDY_PIN) != 0 && timeout-- > 0) {
        esp_rom_delay_us(100);
    }
    return (timeout > 0);
}

/* ---------------------------------------------------------- */
/*  Parse 24-bit signed từ 3 bytes (MSB first)                 */
/* ---------------------------------------------------------- */
static int32_t parse24(const uint8_t *b)
{
    int32_t raw = ((int32_t)b[0] << 16) | ((int32_t)b[1] << 8) | b[2];
    /* Sign extend */
    if (raw & 0x800000) raw |= 0xFF000000;
    return raw;
}

/* ---------------------------------------------------------- */
/*  Public: đọc raw một kênh                                   */
/* ---------------------------------------------------------- */
int32_t ads131m08_read_raw(ads_channel_t ch)
{
    ads_reading_t r;
    if (ads131m08_read(&r) != ESP_OK) return 0;
    return (int32_t)(r.current_ma[ch] * ADS_SHUNT_OHM /
                     (ADS_VREF / ADS_FULL_SCALE * ADS_PGA));
}

/* ---------------------------------------------------------- */
/*  Conversion: raw → mA                                        */
/* ---------------------------------------------------------- */
float ads131m08_raw_to_ma(int32_t raw)
{
    /* V = raw / FULL_SCALE × Vref / PGA */
    float v = (float)raw / ADS_FULL_SCALE * ADS_VREF / ADS_PGA;
    /* I = V / R_shunt */
    float ma = (v / ADS_SHUNT_OHM) * 1000.0f;
    return ma;
}

/* ---------------------------------------------------------- */
/*  Conversion: mA → bar                                        */
/* ---------------------------------------------------------- */
float ads131m08_ma_to_bar(float ma, float p_max_bar)
{
    /* 4mA = 0 bar, 20mA = p_max_bar (linear) */
    float p = (ma - SENSOR_I_MIN_MA) / (SENSOR_I_MAX_MA - SENSOR_I_MIN_MA) * p_max_bar;
    if (p < 0.0f) p = 0.0f;
    return p;
}

/* ---------------------------------------------------------- */
/*  Public: đọc đầy đủ 6 kênh                                  */
/* ---------------------------------------------------------- */
esp_err_t ads131m08_read(ads_reading_t *out)
{
    memset(out, 0, sizeof(*out));
    out->valid = false;

    if (!s_initialized) {
        ESP_LOGW(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!wait_drdy()) {
        ESP_LOGW(TAG, "DRDY timeout");
        return ESP_ERR_TIMEOUT;
    }

    /* Gửi NULL command để latch + đọc frame */
    uint8_t tx[ADS_FRAME_BYTES];
    uint8_t rx[ADS_FRAME_BYTES];
    memset(tx, 0, sizeof(tx));

    /* NULL command trong 3 bytes đầu */
    tx[0] = (ADS_CMD_NULL >> 8) & 0xFF;
    tx[1] =  ADS_CMD_NULL & 0xFF;
    tx[2] = 0x00;

    esp_err_t ret = spi_transfer(tx, rx, ADS_FRAME_BYTES);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI transfer error: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Parse từng kênh */
    for (int ch = 0; ch < ADS_NUM_CHANNELS; ch++) {
        int32_t raw = parse24(&rx[ADS_CH_OFFSET(ch)]);
        float ma    = ads131m08_raw_to_ma(raw);

        /* Clamp: cảm biến 4-20mA không thể dưới 0 hoặc trên 21mA */
        if (ma < 0.0f) ma = 0.0f;
        if (ma > 21.0f) ma = 21.0f;

        out->current_ma[ch]   = ma;
        out->wire_break[ch]   = (ma < SENSOR_I_WIRE_MA);
        out->pressure_bar[ch] = out->wire_break[ch]
                                ? 0.0f
                                : ads131m08_ma_to_bar(ma, k_p_max[ch]);
    }

    out->valid = true;
    return ESP_OK;
}
