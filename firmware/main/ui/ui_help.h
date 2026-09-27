#pragma once
#include "lvgl.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Nút nổi "Trợ giúp" + modal (mục 4.12) — gắn vào parent (shell root) */
void ui_help_create_button(lv_obj_t *parent);
void ui_help_open(void);
#ifdef __cplusplus
}
#endif
