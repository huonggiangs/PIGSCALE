#pragma once
#include "lvgl.h"
#ifdef __cplusplus
extern "C" {
#endif
lv_obj_t *ui_settings_create(lv_obj_t *parent);
void ui_settings_refresh(void);
#ifdef __cplusplus
}
#endif
