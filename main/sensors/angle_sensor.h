#pragma once
/**
 * @file angle_sensor.h
 * @brief HWT9053-485 — IMU góc nghiêng qua RS485 Modbus RTU
 *
 * 4 cảm biến:
 *   A1 = Boom   (addr 0x50)
 *   A2 = Arm    (addr 0x51)
 *   A3 = Bucket (addr 0x52)
 *   A4 = Body   (addr 0x53)
 *
 * Modbus register map (HWT9053-485):
 *   0x3D = Roll  (×0.01 độ, signed int16)
 *   0x3E = Pitch (×0.01 độ, signed int16)
 *   0x3F = Yaw   (×0.01 độ, signed int16)
 *
 * Timeout: 1000ms → trạng thái MẤT KẾT NỐI
 * Refresh: 100ms (10Hz)
 *
 * GPIO mặc định:
 *   UART_TXD → GPIO_RS485_TX
 *   UART_RXD → GPIO_RS485_RX
 *   DE/RE    → GPIO_RS485_DE  (direction control)
 */

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* GPIO — chỉnh theo board */
#define RS485_TX_PIN   17
#define RS485_RX_PIN   16
#define RS485_DE_PIN   18    /* DE+RE pin (HIGH=TX, LOW=RX) */
#define RS485_UART_NUM  1    /* UART1 */
#define RS485_BAUD    9600   /* HWT9053-485 mặc định 9600 */

/* Timeout */
#define ANGLE_TIMEOUT_MS  1000

typedef enum {
    ANGLE_IDX_BOOM   = 0,
    ANGLE_IDX_ARM    = 1,
    ANGLE_IDX_BUCKET = 2,
    ANGLE_IDX_BODY   = 3,
    ANGLE_NUM        = 4,
} angle_idx_t;

typedef struct {
    float   roll;         /* độ */
    float   pitch;        /* độ — đây là góc dùng để tính tải */
    float   yaw;          /* độ */
    bool    connected;    /* false = MẤT KẾT NỐI */
    int64_t last_update;  /* esp_timer_get_time() của lần đọc cuối */
} angle_data_t;

/**
 * @brief Khởi tạo UART RS485, start FreeRTOS task đọc 4 sensor 100ms/lần
 */
esp_err_t angle_sensor_init(void);

/**
 * @brief Lấy dữ liệu góc mới nhất (đã được task cập nhật)
 * @param idx  ANGLE_IDX_BOOM / ARM / BUCKET / BODY
 * @param out  Kết quả
 */
void angle_sensor_get(angle_idx_t idx, angle_data_t *out);

/**
 * @brief Kiểm tra tất cả 4 sensor có kết nối không
 * @return true nếu tất cả connected
 */
bool angle_sensor_all_connected(void);

/** Pitch angle của boom (độ, dương = ngẩng lên) */
float angle_get_boom_pitch(void);

/** Pitch angle của arm (độ) */
float angle_get_arm_pitch(void);

/** Pitch angle của bucket (độ) */
float angle_get_bucket_pitch(void);

/** Roll của body (độ — bù nghiêng máy) */
float angle_get_body_roll(void);
