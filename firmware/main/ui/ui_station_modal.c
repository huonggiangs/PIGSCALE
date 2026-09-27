#include <stdint.h>
#include "ui_station_modal.h"
#include "ui_theme.h"
#include "ui_common.h"
#include "ui_shell.h"
#include "app_state.h"

/* Modal "Chọn trạm cân" (mục 4.11) */

static lv_obj_t *s_overlay;

static void close_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_common_modal_close(s_overlay);
    s_overlay = NULL;
}

static void select_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    app_state_select_station(idx);
    ui_common_modal_close(s_overlay);
    s_overlay = NULL;
    ui_shell_refresh_chrome();
    ui_shell_toast("Đã đổi trạm cân");
}

void ui_station_modal_open(void)
{
    app_state_t *st = app_state();
    lv_coord_t h = 130 + st->station_count * 70;
    if (h > 600) h = 600;

    lv_obj_t *box = ui_common_modal_open(&s_overlay, 420, h);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(box, 10, 0);

    lv_obj_t *title = lv_label_create(box);
    lv_label_set_text(title, "Chọn trạm cân");
    lv_obj_set_style_text_font(title, UI_FONT_H4_BOLD, 0);
    lv_obj_set_style_text_color(title, UI_COLOR_HEADING, 0);

    for (int i = 0; i < st->station_count; i++) {
        bool sel = (i == st->current_station_idx);
        lv_obj_t *btn = lv_button_create(box);
        lv_obj_set_width(btn, LV_PCT(100));
        lv_obj_set_style_radius(btn, 10, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_pad_ver(btn, 14, 0);
        lv_obj_set_style_bg_color(btn, sel ? UI_COLOR_PRIMARY : UI_COLOR_MIST_100, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, st->stations[i].name);
        lv_obj_set_style_text_color(lbl, sel ? lv_color_white() : UI_COLOR_HEADING, 0);
        lv_obj_set_style_text_font(lbl, UI_FONT_BODY_BOLD, 0);
        lv_obj_center(lbl);

        lv_obj_add_event_cb(btn, select_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    lv_obj_t *close_btn = ui_common_button_outline(box, "Đóng", UI_COLOR_BORDER, UI_COLOR_BODY, UI_FONT_BODY_BOLD);
    lv_obj_set_width(close_btn, LV_PCT(100));
    lv_obj_add_event_cb(close_btn, close_cb, LV_EVENT_CLICKED, NULL);
}
