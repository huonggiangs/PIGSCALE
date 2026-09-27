#pragma once
#include "lvgl.h"

/**
 * @brief Khởi tạo màn hình CÂN chính
 */
void ui_main_screen_init(void);

/**
 * @brief Cập nhật giá trị cân (gọi từ weight_logic task)
 * @param weight_kg  Giá trị hiện tại của gàu (kg)
 * @param total_kg   Tổng đã tích (kg)
 * @param target_kg  Mục tiêu đơn hàng (kg)
 */
void ui_main_update_weight(float weight_kg, float total_kg, float target_kg);

/**
 * @brief Cập nhật thông tin đơn hàng
 */
void ui_main_set_order(const char *order_id, const char *company,
                       const char *material, const char *status);

/** Chuyển trạng thái nút BẮT ĐẦU / DỪNG CÂN */
void ui_main_set_weighing_state(bool is_weighing);

/** Mở numpad nhập mục tiêu (gọi từ ui_orders khi chọn đơn hàng) */
void ui_main_show_numpad(void);

/** Set thông tin đơn hàng và mục tiêu từ orders screen */
void ui_main_load_order(const char *order_id, const char *company,
                        const char *material, float target_kg);

/** Trả về screen object (để nav có thể load) */
lv_obj_t *ui_main_get_screen(void);
