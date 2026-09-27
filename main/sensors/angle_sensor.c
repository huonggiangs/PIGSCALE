/**
 * @file angle_sensor.c
 * @brief RS485 Modbus RTU driver cho HWT9053-485
 *
 * Modbus RTU frame (read holding registers):
 *   [addr][0x03][reg_hi][reg_lo][count_hi][count_lo][crc_lo][crc_hi]
 *   Request 3 registers (Roll, Pitch, Yaw) từ 0x3D
 *
 * Response:
 *   [addr][0x03][byte_count=6][D0_hi][D0_lo][D1_hi][D1_lo][D2_hi][D2_lo][crc_lo][crc_hi]
 */

#include "angle_sensor.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <math.h>

static const char *TAG = "ANGLE_SENSOR";

/* Modbus addresses theo spec */
static const uint8_t k_addr[ANGLE_NUM] = { 0x50, 0x51, 0x52, 0x53 };
static const char *k_name[ANGLE_NUM]   = { "Boom", "Arm", "Bucket", "Body" };

/* HWT9053-485 register: Roll=0x3D, Pitch=0x3E, Yaw=0x3F */
#define REG_ROLL   0x3D
#define REG_COUNT  3       /* đọc 3 registers cùng lúc */

/* Task */
#define ANGLE_TASK_PERIOD_MS  100

static angle_data_t s_data[ANGLE_NUM];
static SemaphoreHandle_t s_mutex = NULL;

/* ============================================================
 * CRC16 Modbus
 * ============================================================ */
static uint16_t crc16_modbus(const uint8_t *buf, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= buf[i];
        for (int b = 0; b < 8; b++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xA001;
            else         crc >>= 1;
        }
    }
    return crc;
}

/* ============================================================
 * RS485 direction control
 * ============================================================ */
static inline void rs485_tx_mode(void) { gpio_set_level(RS485_DE_PIN, 1); }
static inline void rs485_rx_mode(void) { gpio_set_level(RS485_DE_PIN, 0); }

/* ============================================================
 * Gửi Modbus request + đọc response
 * ============================================================ */
static bool modbus_read_regs(uint8_t addr, uint16_t reg, uint8_t count,
                              int16_t *out_vals)
{
    /* Build request frame (8 bytes) */
    uint8_t req[8];
    req[0] = addr;
    req[1] = 0x03;  /* function: read holding registers */
    req[2] = (reg >> 8) & 0xFF;
    req[3] =  reg & 0xFF;
    req[4] = 0x00;
    req[5] = count;
    uint16_t crc = crc16_modbus(req, 6);
    req[6] = crc & 0xFF;       /* CRC lo */
    req[7] = (crc >> 8) & 0xFF;/* CRC hi */

    /* Flush RX buffer */
    uart_flush_input(RS485_UART_NUM);

    /* TX */
    rs485_tx_mode();
    uart_write_bytes(RS485_UART_NUM, (const char *)req, sizeof(req));
    uart_wait_tx_done(RS485_UART_NUM, pdMS_TO_TICKS(20));
    rs485_rx_mode();

    /* RX: expected = 5 + 2*count bytes */
    int expected = 5 + 2 * count;
    uint8_t resp[16];
    int got = uart_read_bytes(RS485_UART_NUM, resp, expected,
                              pdMS_TO_TICKS(200));
    if (got < expected) {
        return false;
    }

    /* Validate CRC */
    uint16_t crc_calc = crc16_modbus(resp, got - 2);
    uint16_t crc_recv = resp[got-2] | ((uint16_t)resp[got-1] << 8);
    if (crc_calc != crc_recv) {
        ESP_LOGW(TAG, "addr=0x%02X CRC mismatch", addr);
        return false;
    }

    /* addr/func check */
    if (resp[0] != addr || resp[1] != 0x03 || resp[2] != 2*count) {
        return false;
    }

    /* Parse values (big-endian int16) */
    for (int i = 0; i < count; i++) {
        uint16_t raw = ((uint16_t)resp[3 + 2*i] << 8) | resp[4 + 2*i];
        out_vals[i] = (int16_t)raw;
    }
    return true;
}

/* ============================================================
 * FreeRTOS task — đọc 4 sensor mỗi 100ms
 * ============================================================ */
static void angle_task(void *arg)
{
    (void)arg;
    int16_t vals[3];

    while (1) {
        for (int i = 0; i < ANGLE_NUM; i++) {
            int64_t now = esp_timer_get_time();
            bool ok = modbus_read_regs(k_addr[i], REG_ROLL, REG_COUNT, vals);

            xSemaphoreTake(s_mutex, portMAX_DELAY);
            if (ok) {
                /* HWT9053: giá trị × 0.01 độ → chia 100 */
                s_data[i].roll      = vals[0] * 0.01f;
                s_data[i].pitch     = vals[1] * 0.01f;
                s_data[i].yaw       = vals[2] * 0.01f;
                s_data[i].connected = true;
                s_data[i].last_update = now;
            } else {
                /* Kiểm tra timeout */
                int64_t elapsed_ms = (now - s_data[i].last_update) / 1000;
                if (elapsed_ms > ANGLE_TIMEOUT_MS) {
                    s_data[i].connected = false;
                    ESP_LOGW(TAG, "%s: MẤT KẾT NỐI (%lld ms)", k_name[i], elapsed_ms);
                }
            }
            xSemaphoreGive(s_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(ANGLE_TASK_PERIOD_MS));
    }
}

/* ============================================================
 * Init
 * ============================================================ */
esp_err_t angle_sensor_init(void)
{
    /* UART config */
    uart_config_t uart_conf = {
        .baud_rate  = RS485_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };
    ESP_ERROR_CHECK(uart_param_config(RS485_UART_NUM, &uart_conf));
    ESP_ERROR_CHECK(uart_set_pin(RS485_UART_NUM,
                                  RS485_TX_PIN, RS485_RX_PIN,
                                  UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(RS485_UART_NUM, 256, 256, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_set_mode(RS485_UART_NUM, UART_MODE_RS485_HALF_DUPLEX));

    /* DE/RE pin */
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << RS485_DE_PIN),
        .mode         = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    rs485_rx_mode();

    /* Init data */
    memset(s_data, 0, sizeof(s_data));
    int64_t now = esp_timer_get_time();
    for (int i = 0; i < ANGLE_NUM; i++) {
        s_data[i].last_update = now;
    }

    s_mutex = xSemaphoreCreateMutex();
    xTaskCreate(angle_task, "angle_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "RS485 Modbus init OK — 4 sensors @ 9600 baud");
    return ESP_OK;
}

/* ============================================================
 * Public getters
 * ============================================================ */
void angle_sensor_get(angle_idx_t idx, angle_data_t *out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_data[idx];
    xSemaphoreGive(s_mutex);
}

bool angle_sensor_all_connected(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool ok = true;
    for (int i = 0; i < ANGLE_NUM; i++) ok &= s_data[i].connected;
    xSemaphoreGive(s_mutex);
    return ok;
}

float angle_get_boom_pitch(void)
{
    angle_data_t d; angle_sensor_get(ANGLE_IDX_BOOM, &d);
    return d.pitch;
}
float angle_get_arm_pitch(void)
{
    angle_data_t d; angle_sensor_get(ANGLE_IDX_ARM, &d);
    return d.pitch;
}
float angle_get_bucket_pitch(void)
{
    angle_data_t d; angle_sensor_get(ANGLE_IDX_BUCKET, &d);
    return d.pitch;
}
float angle_get_body_roll(void)
{
    angle_data_t d; angle_sensor_get(ANGLE_IDX_BODY, &d);
    return d.roll;
}
