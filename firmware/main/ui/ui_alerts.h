#pragma once
#include "lvgl.h"
#ifdef __cplusplus
extern "C" {
#endif
lv_obj_t *ui_alerts_create(lv_obj_t *parent);
void ui_alerts_refresh(void);
#ifdef __cplusplus
}
#endif
