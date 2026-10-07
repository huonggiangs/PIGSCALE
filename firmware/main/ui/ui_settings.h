#pragma once
#include "lvgl.h"
#ifdef __cplusplus
extern "C" {
#endif
lv_obj_t *ui_settings_create(lv_obj_t *parent);
void ui_settings_refresh(void);
void ui_settings_close_keyboard(void);

/** Gọi mỗi 500ms khi tab Cài đặt đang mở (từ ui_shell tick) — chỉ cập nhật
 *  đồng hồ + trạng thái đồng bộ NTP tại chỗ, KHÔNG dựng lại cả cây UI (tránh
 *  mất focus bàn phím/ô nhập đang gõ dở). No-op nếu card thời gian chưa
 *  được dựng (vd. đang ở màn khoá PIN). */
void ui_settings_tick(void);
#ifdef __cplusplus
}
#endif
