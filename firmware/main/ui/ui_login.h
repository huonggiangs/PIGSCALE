#pragma once
#include "lvgl.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Dựng màn hình đăng nhập (mục 4.1), trả về container full-screen */
lv_obj_t *ui_login_create(lv_obj_t *parent);
/* Reset PIN/lỗi khi quay lại màn đăng nhập (sau khi đăng xuất) */
void ui_login_reset(void);
#ifdef __cplusplus
}
#endif
