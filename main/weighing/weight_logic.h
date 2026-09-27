#pragma once
/**
 * @file weight_logic.h
 * @brief Tính tải trọng gàu từ áp suất thủy lực + góc nghiêng
 *
 * Công thức: Moment balance quanh chốt boom
 * Xem weight_logic.c để biết chi tiết toán học.
 */
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    WEIGHT_STATE_IDLE = 0,
    WEIGHT_STATE_WEIGHING,
    WEIGHT_STATE_COMPLETE,
    WEIGHT_STATE_ERROR,
} weight_state_t;

typedef struct {
    float payload_kg;     /* tải trọng tính được (kg) */
    float total_kg;       /* tổng đã tích lũy (kg) */
    float target_kg;      /* mục tiêu đơn hàng (kg) */
    float boom_angle;     /* độ */
    float arm_angle;      /* độ */
    float body_roll;      /* độ — nghiêng máy */
    bool  sensor_ok[6];   /* trạng thái P1-P6 */
    bool  angle_ok[4];    /* trạng thái A1-A4 */
    weight_state_t state;
} weight_data_t;

void weight_logic_init(void);
void weight_logic_start(void);
void weight_logic_stop(void);
void weight_logic_reset(void);
bool weight_logic_is_running(void);
weight_state_t weight_logic_get_state(void);
void weight_logic_get_data(weight_data_t *out);
void weight_logic_set_target(float target_kg);

/* Calibration */
void weight_logic_tare(void);              /* zero point với gàu rỗng */
void weight_logic_save_calibration(void);  /* lưu vào NVS */
void weight_logic_load_calibration(void);  /* đọc từ NVS */
