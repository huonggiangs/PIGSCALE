#pragma once
/**
 * @file ui_calibration.h
 * @brief Màn hình HIỆU CHUẨN — PIN bảo vệ + 2 loại máy xúc
 *
 * Truy cập qua: CÀI ĐẶT → Hiệu chuẩn → PIN modal → Màn hình hiệu chuẩn
 *
 * Sub-navigation trong màn hình này:
 *   CẤU HÌNH | MẠNG | HIỆU CHUẨN (active) | THÔNG TIN
 */

#include "lvgl.h"

/**
 * @brief Khởi tạo màn hình hiệu chuẩn (gọi một lần khi boot).
 *        Màn hình được lazy-init: chỉ tạo khi user vượt qua PIN.
 */
void ui_calibration_screen_init(void);

/**
 * @brief Lấy con trỏ màn hình (NULL nếu chưa init).
 */
lv_obj_t *ui_calibration_get_screen(void);

/**
 * @brief Hiện hộp thoại nhập PIN trên màn parent.
 *        Sau khi nhập đúng PIN → load màn hình hiệu chuẩn.
 *
 * @param parent  Màn hình Settings đang hiển thị (để đặt overlay đúng chỗ)
 */
void ui_calibration_show_pin(lv_obj_t *parent);
