#pragma once
#include "lvgl.h"

/**
 * @brief Khởi tạo module nav (gọi một lần trong app_main sau display_init)
 */
void ui_nav_init(void);

/**
 * @brief Vẽ bottom nav bar lên màn hình @p parent
 * @param parent     Screen object (lv_scr_act hoặc custom screen)
 * @param active_tab Index tab active: 0=CÂN, 1=ĐƠN HÀNG, 2=LỊCH SỬ, 3=BÁO CÁO, 4=CÀI ĐẶT
 */
void ui_nav_build(lv_obj_t *parent, int active_tab);

/** Đổi tab active (dùng khi chuyển màn hình từ bên ngoài) */
void ui_nav_set_active(int tab_index);
