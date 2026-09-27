#pragma once
#include "lvgl.h"

/* Độ phân giải màn hình — chỉnh theo panel thực tế */
#define DISP_HOR_RES   480
#define DISP_VER_RES   800
#define DISP_COLOR_DEPTH 16   /* RGB565 */

/**
 * @brief Khởi tạo display panel + LVGL port
 *        Gọi một lần trong app_main trước mọi lv_* call
 */
void display_driver_init(void);

/** Lấy mutex LVGL — bắt buộc trước mọi lv_* call từ task khác */
void lvgl_lock(void);
void lvgl_unlock(void);
