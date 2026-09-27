#pragma once
/**
 * @file ui_main.h
 * @brief LVGL UI entry point — CÂN MÁY XÚC V3
 *
 * Giao diện dark-theme 800×1280, tiếng Việt.
 * Gọi ui_main_start() SAU khi LVGL port đã init (display_init + gt9271_init).
 */

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Khởi tạo và hiển thị màn hình chính trong LVGL task.
 *
 * Hàm này gọi lvgl_port_lock(), tạo tất cả objects LVGL, rồi unlock.
 * Không block — LVGL task tự quản lý render loop.
 */
void ui_main_start(void);

#ifdef __cplusplus
}
#endif
