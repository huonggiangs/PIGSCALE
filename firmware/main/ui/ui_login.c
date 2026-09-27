#include <stdint.h>
#include "ui_login.h"
#include "ui_theme.h"
#include "ui_common.h"
#include "ui_shell.h"
#include "app_state.h"
#include <stdio.h>
#include <string.h>

static lv_obj_t *s_screen;
static lv_obj_t *s_employee_btns[APP_MAX_EMPLOYEES];
static lv_obj_t *s_pin_dots_host;
static lv_obj_t *s_error_label;
static lv_obj_t *s_station_label;

static void refresh_employee_highlight(void)
{
    app_state_t *st = app_state();
    for (int i = 0; i < st->employee_count; i++) {
        bool sel = (i == st->current_employee_idx);
        lv_obj_set_style_bg_color(s_employee_btns[i], sel ? UI_COLOR_PRIMARY : lv_color_white(), 0);
        lv_obj_set_style_bg_opa(s_employee_btns[i], sel ? LV_OPA_COVER : LV_OPA_10, 0);
    }
}

static void refresh_pin_dots(void)
{
    ui_common_clear(s_pin_dots_host);
    ui_common_pin_dots(s_pin_dots_host, (int)strlen(app_state()->pin_input), false);
}

static void try_auto_login(void)
{
    app_state_t *st = app_state();
    if (strlen(st->pin_input) < 4) return;

    if (app_state_try_login()) {
        lv_obj_add_flag(s_error_label, LV_OBJ_FLAG_HIDDEN);
        refresh_pin_dots();
        ui_shell_on_login_success();
    } else {
        lv_label_set_text(s_error_label, "Mã PIN không đúng.");
        lv_obj_clear_flag(s_error_label, LV_OBJ_FLAG_HIDDEN);
        refresh_pin_dots();
    }
}

static void keypad_digit_cb(char digit, void *user_data)
{
    LV_UNUSED(user_data);
    if (app_state()->current_employee_idx < 0) return;
    app_state_pin_digit(digit);
    refresh_pin_dots();
    try_auto_login();
}

static void keypad_backspace_cb(void *user_data)
{
    LV_UNUSED(user_data);
    app_state_pin_backspace();
    refresh_pin_dots();
}

static void employee_btn_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    app_state_select_employee(idx);
    lv_obj_add_flag(s_error_label, LV_OBJ_FLAG_HIDDEN);
    refresh_employee_highlight();
    refresh_pin_dots();
}

lv_obj_t *ui_login_create(lv_obj_t *parent)
{
    s_screen = lv_obj_create(parent);
    lv_obj_remove_style_all(s_screen);
    lv_obj_set_size(s_screen, UI_HOR_RES, UI_VER_RES);
    lv_obj_set_pos(s_screen, 0, 0);
    lv_obj_set_style_bg_color(s_screen, UI_COLOR_HEADING, 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_screen, 32, 0);
    lv_obj_set_flex_flow(s_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_screen, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *logo = lv_image_create(s_screen);
    lv_image_set_src(logo, &img_logo_pig_weigh);
    lv_obj_set_style_pad_bottom(logo, 10, 0);

    lv_obj_t *brand = lv_label_create(s_screen);
    lv_label_set_text(brand, "PIG WEIGH");
    lv_obj_set_style_text_color(brand, lv_color_white(), 0);
    lv_obj_set_style_text_font(brand, UI_FONT_BRAND, 0);
    lv_obj_set_style_pad_bottom(brand, 4, 0);

    lv_obj_t *tagline = lv_label_create(s_screen);
    lv_label_set_text(tagline, "THÔNG MINH · CHÍNH XÁC · BỀN BỈ");
    lv_obj_set_style_text_color(tagline, UI_COLOR_ON_DARK_MUTED, 0);
    lv_obj_set_style_text_font(tagline, UI_FONT_BODY_BOLD, 0);
    lv_obj_set_style_pad_bottom(tagline, 14, 0);

    s_station_label = lv_label_create(s_screen);
    lv_label_set_text(s_station_label, "TRẠM CÂN 01");
    lv_obj_set_style_text_color(s_station_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_station_label, UI_FONT_STATION, 0);
    lv_obj_set_style_pad_bottom(s_station_label, 6, 0);

    lv_obj_t *hint = lv_label_create(s_screen);
    lv_label_set_text(hint, "Đăng nhập để tiếp tục");
    lv_obj_set_style_text_color(hint, UI_COLOR_ON_DARK_MUTED, 0);
    lv_obj_set_style_text_font(hint, UI_FONT_H4, 0);
    lv_obj_set_style_pad_bottom(hint, 24, 0);

    /* hàng 3 nút chọn nhân viên */
    lv_obj_t *emp_row = lv_obj_create(s_screen);
    lv_obj_remove_style_all(emp_row);
    lv_obj_set_flex_flow(emp_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(emp_row, 12, 0);
    lv_obj_set_size(emp_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_bottom(emp_row, 24, 0);
    lv_obj_clear_flag(emp_row, LV_OBJ_FLAG_SCROLLABLE);

    app_state_t *st = app_state();
    for (int i = 0; i < st->employee_count; i++) {
        lv_obj_t *btn = lv_button_create(emp_row);
        lv_obj_set_style_radius(btn, 12, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        /* LV_SIZE_CONTENT thay vì 150x74 cố định: kích thước chữ có thể đổi
           (vd. phóng 1.5x) — nút phải tự co giãn theo, không bị cắt/tràn. */
        lv_obj_set_size(btn, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_pad_hor(btn, 20, 0);
        lv_obj_set_style_pad_ver(btn, 14, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_10, 0);
        lv_obj_set_style_bg_color(btn, lv_color_white(), 0);

        lv_obj_t *col = lv_obj_create(btn);
        lv_obj_remove_style_all(col);
        lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_size(col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_center(col);
        lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *name = lv_label_create(col);
        lv_label_set_text(name, st->employees[i].name);
        lv_obj_set_style_text_color(name, lv_color_white(), 0);
        lv_obj_set_style_text_font(name, UI_FONT_BODY_BOLD, 0);

        lv_obj_t *role = lv_label_create(col);
        lv_label_set_text(role, app_role_label(st->employees[i].role));
        lv_obj_set_style_text_color(role, UI_COLOR_ON_DARK_MUTED, 0);
        lv_obj_set_style_text_font(role, UI_FONT_XS, 0);

        lv_obj_add_event_cb(btn, employee_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        s_employee_btns[i] = btn;
    }

    /* PIN dots */
    s_pin_dots_host = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_pin_dots_host);
    lv_obj_set_size(s_pin_dots_host, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_bottom(s_pin_dots_host, 12, 0);
    lv_obj_clear_flag(s_pin_dots_host, LV_OBJ_FLAG_SCROLLABLE);
    ui_common_pin_dots(s_pin_dots_host, 0, false);

    s_error_label = lv_label_create(s_screen);
    lv_label_set_text(s_error_label, "Mã PIN không đúng.");
    lv_obj_set_style_text_color(s_error_label, UI_COLOR_ON_DARK_ERROR, 0);
    lv_obj_set_style_text_font(s_error_label, UI_FONT_H5, 0);
    lv_obj_set_style_pad_bottom(s_error_label, 8, 0);
    lv_obj_add_flag(s_error_label, LV_OBJ_FLAG_HIDDEN);

    ui_common_keypad(s_screen, keypad_digit_cb, keypad_backspace_cb, NULL, false);

    lv_obj_t *demo_note = lv_label_create(s_screen);
    lv_label_set_text(demo_note, "Demo PIN: NV001=1111 · QL001=2222 · KT001=3333");
    lv_obj_set_style_text_color(demo_note, UI_COLOR_ON_DARK_HINT, 0);
    lv_obj_set_style_text_font(demo_note, UI_FONT_BODY, 0);
    lv_obj_set_style_pad_top(demo_note, 16, 0);

    refresh_employee_highlight();
    return s_screen;
}

void ui_login_reset(void)
{
    app_state_pin_clear();
    refresh_pin_dots();
    refresh_employee_highlight();
    lv_obj_add_flag(s_error_label, LV_OBJ_FLAG_HIDDEN);
    if (app_state()->current_station_idx >= 0) {
        lv_label_set_text(s_station_label, app_state()->stations[app_state()->current_station_idx].name);
    }
}
