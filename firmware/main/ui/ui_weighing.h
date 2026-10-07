#pragma once
#include "lvgl.h"
#ifdef __cplusplus
extern "C" {
#endif
lv_obj_t *ui_weighing_create(lv_obj_t *parent);
void ui_weighing_refresh(void);
/* Gọi khi chuyển sang/rời tab CÂN — dừng giải mã RTSP khi tab không hiện
 * để không tốn CPU/băng thông lúc đang ở tab khác. */
void ui_weighing_set_visible(bool visible);
#ifdef __cplusplus
}
#endif
