#include <stdint.h>
#include "ui_help.h"
#include "ui_theme.h"
#include "ui_common.h"
#include "app_state.h"

/* Nút nổi "Trợ giúp" + modal (mục 4.12) */

static lv_obj_t *s_overlay;

static void close_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_common_modal_close(s_overlay);
    s_overlay = NULL;
}

static void help_btn_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_help_open();
}

void ui_help_create_button(lv_obj_t *parent)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, 64, 64);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(btn, 12, 0);
    lv_obj_set_style_shadow_color(btn, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_30, 0);
    /* Góc dưới phải, nổi lên trên thanh điều hướng (mục 3) — dựng SAU
     * build_bottom_nav() trong ui_shell.c nên đã ở lớp vẽ trên cùng; đè nhẹ
     * lên mép trên thanh điều hướng để đúng nghĩa "nổi trên". */
    lv_obj_align(btn, LV_ALIGN_BOTTOM_RIGHT, -20, -(UI_BOTTOMNAV_H - 24));

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, "?");
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl, UI_FONT_H2_BOLD, 0);
    lv_obj_center(lbl);

    lv_obj_add_event_cb(btn, help_btn_cb, LV_EVENT_CLICKED, NULL);
}

void ui_help_open(void)
{
    app_state_refresh_alerts();
    app_state_t *st = app_state();

    lv_obj_t *box = ui_common_modal_open(&s_overlay, 480, 480);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(box, 10, 0);
    lv_obj_set_scroll_dir(box, LV_DIR_VER);

    lv_obj_t *title = lv_label_create(box);
    lv_label_set_text(title, "Trợ giúp tại chỗ");
    lv_obj_set_style_text_font(title, UI_FONT_H4_BOLD, 0);
    lv_obj_set_style_text_color(title, UI_COLOR_HEADING, 0);

    lv_obj_t *hotline = lv_label_create(box);
    lv_label_set_text(hotline, "Hotline kỹ thuật: 1900 1234");
    lv_obj_set_style_text_font(hotline, UI_FONT_BODY_BOLD, 0);
    lv_obj_set_style_text_color(hotline, UI_COLOR_HEADING, 0);

    lv_obj_t *hours = lv_label_create(box);
    lv_label_set_text(hours, "Khung giờ hỗ trợ: 07:00 – 21:00 (Thứ 2 – Chủ nhật)");
    lv_obj_set_style_text_font(hours, UI_FONT_BODY, 0);
    lv_obj_set_style_text_color(hours, UI_COLOR_BODY, 0);

    lv_obj_t *sec_title = lv_label_create(box);
    lv_label_set_text(sec_title, "Xử lý nhanh sự cố hiện tại");
    lv_obj_set_style_text_font(sec_title, UI_FONT_H5_BOLD, 0);
    lv_obj_set_style_text_color(sec_title, UI_COLOR_HEADING, 0);
    lv_obj_set_style_pad_top(sec_title, 4, 0);

    int shown = 0;
    for (int i = 0; i < st->alert_count; i++) {
        alert_t *a = &st->alerts[i];
        if (!a->active) continue;

        /* Tách "→ " (Montserrat built-in) khỏi chữ tiếng Việt (font Inter tuỳ
         * biến không có glyph mũi tên) — cùng quy tắc đã áp dụng ở
         * ui_alerts.c mục xây action_row. */
        lv_obj_t *row = lv_obj_create(box);
        lv_obj_remove_style_all(row);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(row, 6, 0);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *arrow = lv_label_create(row);
        lv_label_set_text(arrow, LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_font(arrow, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(arrow, UI_COLOR_PRIMARY, 0);

        lv_obj_t *text = lv_label_create(row);
        lv_label_set_text(text, a->action);
        lv_label_set_long_mode(text, LV_LABEL_LONG_WRAP);
        lv_obj_set_flex_grow(text, 1);
        lv_obj_set_style_text_font(text, UI_FONT_BODY, 0);
        lv_obj_set_style_text_color(text, UI_COLOR_PRIMARY, 0);

        shown++;
    }
    if (shown == 0) {
        lv_obj_t *empty = lv_label_create(box);
        lv_label_set_text(empty, "Không có sự cố nào hiện tại.");
        lv_obj_set_style_text_font(empty, UI_FONT_BODY, 0);
        lv_obj_set_style_text_color(empty, UI_COLOR_BODY, 0);
    }

    lv_obj_t *close_btn = ui_common_button_outline(box, "Đóng", UI_COLOR_BORDER, UI_COLOR_BODY, UI_FONT_BODY_BOLD);
    lv_obj_set_width(close_btn, LV_PCT(100));
    lv_obj_add_event_cb(close_btn, close_cb, LV_EVENT_CLICKED, NULL);
}
