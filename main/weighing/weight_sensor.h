#pragma once
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Khởi tạo load cell / HX711
 * @param dout_pin  GPIO chân DATA (HX711)
 * @param sck_pin   GPIO chân CLK  (HX711)
 */
void weight_sensor_init(int dout_pin, int sck_pin);

/**
 * @brief Lấy giá trị raw từ ADC
 * @return Raw ADC count (24-bit signed)
 */
int32_t weight_sensor_read_raw(void);

/**
 * @brief Lấy giá trị kg (đã trừ tare, nhân calibration factor)
 */
float weight_sensor_read_kg(void);

/**
 * @brief Tare — đặt zero point
 */
void weight_sensor_tare(void);

/**
 * @brief Calibrate: cung cấp giá trị thực khi đặt quả cân chuẩn
 * @param known_kg  Khối lượng thực của vật chuẩn (kg)
 */
void weight_sensor_calibrate(float known_kg);

/** Trả về true nếu sensor sẵn sàng */
bool weight_sensor_is_ready(void);
