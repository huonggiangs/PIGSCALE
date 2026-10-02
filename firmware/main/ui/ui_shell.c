#include <stdint.h>
#include "ui_shell.h"
#include "ui_theme.h"
#include "ui_common.h"
#include "ui_login.h"
#include "ui_orders.h"
#include "ui_weighing.h"
#include "ui_alerts.h"
#include "ui_history.h"
#include "ui_settings.h"
#include "ui_station_modal.h"
#include "ui_help.h"
#include <stdio.h>
#include <time.h>

/* ── Trạng thái nội bộ của shell ─────────────────────────────────────────── */
static lv_obj_t *s_login_screen;
static lv_obj_t *s_shell_root;
static lv_obj_t *s_status_bar;
static lv_obj_t *s_sub_bar;
static lv_obj_t *s_content_area;
static lv_obj_t *s_bottom_nav;

static lv_obj_t *s_tab_pages[TAB_COUNT];
static lv_obj_t *s_nav_btn[TAB_COUNT];
static lv_obj_t *s_nav_icon[TAB_COUNT];
static lv_obj_t *s_nav_label[TAB_COUNT];

/* status bar widgets cần cập nhật động */
static lv_obj_t *s_clock_label;
static lv_obj_t *s_dot_wifi, *s_dot_p5, *s_dot_cam;
static lv_obj_t *s_wifi_icon;   /* icon sóng Wi-Fi trên header — màu đổi theo wifi_link */
/* sub bar widgets */
static lv_obj_t *s_sync_badge;
static lv_obj_t *s_sync_badge_label;
static lv_obj_t *s_employee_label;
static lv_obj_t *s_station_btn_label;
static lv_obj_t *s_station_name_label;

static const char *NAV_LABELS[TAB_COUNT] = {"Đơn hàng", "Cân", "Đề xuất", "Lịch sử", "Cài đặt"};
static const void *NAV_ICONS[TAB_COUNT];

/* Màn chờ (screensaver) — tự hiện khi không chạm màn hình quá
 * SETTINGS_IDLE_MS trong lúc đang ở tab Cài đặt (xem tick_timer_cb). Chỉ áp
 * dụng cho tab Cài đặt: đây là màn cấu hình/quản trị, đứng yên lâu không
 * thao tác thường là do nhân viên rời đi — tự thoát về Đơn hàng vừa tránh
 * lộ màn hình cấu hình, vừa khiến PIN Cài đặt (operator) phải nhập lại ở
 * lần vào sau (logic khoá PIN có sẵn trong ui_shell_switch_tab). Không áp
 * dụng cho tab Cân vì đang đo/cân thật sự cần luôn thấy số liệu trên màn. */
#define SETTINGS_IDLE_MS   120000   /* 2 phút */
static lv_obj_t *s_standby_overlay;
static lv_obj_t *s_standby_time_label;
static lv_obj_t *s_standby_date_label;

static void tick_timer_cb(lv_timer_t *t);
static void nav_btn_event_cb(lv_event_t *e);
static void logout_btn_event_cb(lv_event_t *e);
static void station_btn_event_cb(lv_event_t *e);
static void sync_badge_event_cb(lv_event_t *e);
static void standby_enter(void);
static void standby_dismiss(void);

/* ────────────────────────────────────────────────────────────────────────
 * THANH TRẠNG THÁI (mục 4.2)
 * ──────────────────────────────────────────────────────────────────────── */
static lv_obj_t *make_status_cluster_ex(lv_obj_t *parent, const char *label, lv_obj_t **out_dot,
                                         const void *icon_src, lv_obj_t **out_icon)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_remove_style_all(c);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(c, 6, 0);
    lv_obj_set_size(c, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);

    /* Icon (nếu có) dựng NGAY THỨ TỰ ĐÚNG từ đầu, không dùng move_to_index().
       Nguyên nhân thật của việc Wi-Fi/P5/Cam "biến mất" khỏi header (xác
       nhận qua log hdr_dbg2) là ở "right" (cha, xem build_status_bar):
       flex align END trên trục chính kết hợp LV_SIZE_CONTENT khiến LVGL
       tính sai bề rộng tự động — đã sửa ở nơi tạo "right" (dùng START). */
    if (icon_src && out_icon) {
        *out_icon = ui_common_icon(c, icon_src, lv_color_white());
    }

    *out_dot = ui_common_status_dot(c, LINK_OK);

    lv_obj_t *lbl = lv_label_create(c);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl, UI_FONT_XS, 0);
    return c;
}

static lv_obj_t *make_status_cluster(lv_obj_t *parent, const char *label, lv_obj_t **out_dot)
{
    return make_status_cluster_ex(parent, label, out_dot, NULL, NULL);
}

static void build_status_bar(lv_obj_t *parent)
{
    s_status_bar = lv_obj_create(parent);
    lv_obj_remove_style_all(s_status_bar);
    lv_obj_set_size(s_status_bar, UI_HOR_RES, UI_STATUSBAR_H);
    lv_obj_set_pos(s_status_bar, 0, 0);
    lv_obj_set_style_bg_color(s_status_bar, UI_COLOR_HEADING, 0);
    lv_obj_set_style_bg_opa(s_status_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(s_status_bar, UI_PAD_SCREEN, 0);
    lv_obj_clear_flag(s_status_bar, LV_OBJ_FLAG_SCROLLABLE);

    /* cụm trại + trạm (trái) */
    lv_obj_t *left = lv_obj_create(s_status_bar);
    lv_obj_remove_style_all(left);
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_size(left, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(left, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_clear_flag(left, LV_OBJ_FLAG_SCROLLABLE);

    s_station_name_label = lv_label_create(left);
    lv_label_set_text(s_station_name_label, "TRẠM CÂN 01");
    lv_obj_set_style_text_color(s_station_name_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_station_name_label, UI_FONT_H4_BOLD, 0);

    lv_obj_t *sub = lv_label_create(left);
    lv_label_set_text(sub, "Trại Chăn Nuôi Xanh · Chuồng cân 01");
    lv_obj_set_style_text_color(sub, UI_COLOR_ON_DARK_MUTED, 0);
    lv_obj_set_style_text_font(sub, UI_FONT_XS, 0);

    /* cụm trạng thái + giờ (phải) */
    lv_obj_t *right = lv_obj_create(s_status_bar);
    lv_obj_remove_style_all(right);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_ROW);
    /* LV_FLEX_ALIGN_START (không phải END): với container LV_SIZE_CONTENT,
       flex align END trên trục chính khiến LVGL tính sai bề rộng nội dung
       (chỉ tính theo phần tử cuối), đẩy các phần tử trước ra toạ độ âm rồi
       bị clip mất — đây là nguyên nhân thật của lỗi "thiếu Wi-Fi/P5/Cam".
       Việc căn phải cả cụm đã có sẵn qua lv_obj_align(RIGHT_MID) bên dưới. */
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(right, 16, 0);
    lv_obj_set_size(right, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(right, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_clear_flag(right, LV_OBJ_FLAG_SCROLLABLE);

    /* Cụm Wi-Fi riêng: thêm icon sóng Wi-Fi (bên cạnh chấm trạng thái sẵn có)
       để thấy trạng thái ĐÃ KẾT NỐI kèm cường độ tín hiệu thật (đổi màu theo
       wifi_link — suy ra từ số vạch RSSI thật trong app_state_wifi_sync()),
       không chỉ một chấm tròn chung như P5/Cam. */
    make_status_cluster_ex(right, "Wi-Fi", &s_dot_wifi, &img_icon_wifi_signal, &s_wifi_icon);
    make_status_cluster(right, "P5 Scale", &s_dot_p5);
    make_status_cluster(right, "Cam", &s_dot_cam);

    s_clock_label = lv_label_create(right);
    lv_obj_set_style_text_color(s_clock_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_clock_label, UI_FONT_H4_BOLD, 0);
}

/* ────────────────────────────────────────────────────────────────────────
 * THANH PHỤ (mục 4.3)
 * ──────────────────────────────────────────────────────────────────────── */
static void build_sub_bar(lv_obj_t *parent)
{
    s_sub_bar = lv_obj_create(parent);
    lv_obj_remove_style_all(s_sub_bar);
    lv_obj_set_size(s_sub_bar, UI_HOR_RES, UI_SUBBAR_H);
    lv_obj_set_pos(s_sub_bar, 0, UI_STATUSBAR_H);
    lv_obj_set_style_bg_color(s_sub_bar, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(s_sub_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_sub_bar, 1, 0);
    lv_obj_set_style_border_side(s_sub_bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(s_sub_bar, UI_COLOR_BORDER, 0);
    lv_obj_set_style_pad_hor(s_sub_bar, UI_PAD_SCREEN, 0);
    lv_obj_clear_flag(s_sub_bar, LV_OBJ_FLAG_SCROLLABLE);

    /* trái: đổi trạm cân + badge đồng bộ */
    lv_obj_t *left = lv_obj_create(s_sub_bar);
    lv_obj_remove_style_all(left);
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(left, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(left, 10, 0);
    lv_obj_set_size(left, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(left, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_clear_flag(left, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *station_btn = lv_button_create(left);
    lv_obj_set_style_bg_opa(station_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(station_btn, 0, 0);
    lv_obj_set_style_pad_all(station_btn, 4, 0);
    lv_obj_t *station_row = lv_obj_create(station_btn);
    lv_obj_remove_style_all(station_row);
    lv_obj_set_flex_flow(station_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(station_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(station_row, 4, 0);
    lv_obj_set_size(station_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_clear_flag(station_row, LV_OBJ_FLAG_SCROLLABLE);
    /* lv_obj_create() mặc định BẬT LV_OBJ_FLAG_CLICKABLE — nằm đè lên
       station_btn nên cướp mất điểm chạm đúng chỗ có chữ "Đổi trạm cân"
       (xem giải thích đầy đủ trong ui_login.c). */
    lv_obj_remove_flag(station_row, LV_OBJ_FLAG_CLICKABLE);
    s_station_btn_label = lv_label_create(station_row);
    lv_label_set_text(s_station_btn_label, "Đổi trạm cân");
    lv_obj_set_style_text_color(s_station_btn_label, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(s_station_btn_label, UI_FONT_BODY_BOLD, 0);
    lv_obj_t *caret = lv_label_create(station_row);
    lv_label_set_text(caret, LV_SYMBOL_DOWN);
    lv_obj_set_style_text_font(caret, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(caret, UI_COLOR_PRIMARY, 0);
    lv_obj_add_event_cb(station_btn, station_btn_event_cb, LV_EVENT_CLICKED, NULL);

    /* Badge "⏳ N chờ đồng bộ" — ⏳ không có trong font Inter tiếng Việt và
     * không có SVG icon riêng, dùng LV_SYMBOL_WARNING (Montserrat built-in)
     * ở label riêng, tách khỏi label chữ tiếng Việt. */
    s_sync_badge = lv_obj_create(left);
    lv_obj_remove_style_all(s_sync_badge);
    lv_obj_set_style_bg_color(s_sync_badge, UI_COLOR_WARNING_BG, 0);
    lv_obj_set_style_bg_opa(s_sync_badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_sync_badge, UI_RADIUS_BADGE, 0);
    lv_obj_set_style_pad_hor(s_sync_badge, 10, 0);
    lv_obj_set_style_pad_ver(s_sync_badge, 4, 0);
    lv_obj_set_height(s_sync_badge, LV_SIZE_CONTENT);
    lv_obj_set_width(s_sync_badge, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s_sync_badge, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_sync_badge, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(s_sync_badge, 5, 0);
    lv_obj_clear_flag(s_sync_badge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_sync_badge, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_sync_badge, sync_badge_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *sync_icon = lv_label_create(s_sync_badge);
    lv_label_set_text(sync_icon, LV_SYMBOL_WARNING);
    lv_obj_set_style_text_font(sync_icon, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sync_icon, UI_COLOR_ORANGE_700, 0);

    s_sync_badge_label = lv_label_create(s_sync_badge);
    lv_obj_set_style_text_color(s_sync_badge_label, UI_COLOR_ORANGE_700, 0);
    lv_obj_set_style_text_font(s_sync_badge_label, UI_FONT_XS, 0);

    /* phải: nhân viên + đăng xuất */
    lv_obj_t *right = lv_obj_create(s_sub_bar);
    lv_obj_remove_style_all(right);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_ROW);
    /* Cùng lỗi END+SIZE_CONTENT như status bar phía trên — dùng START để
       LVGL tính đúng bề rộng nội dung (tránh "Nhân viên cân" bị cắt chữ). */
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(right, 14, 0);
    lv_obj_set_size(right, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(right, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_clear_flag(right, LV_OBJ_FLAG_SCROLLABLE);

    s_employee_label = lv_label_create(right);
    lv_obj_set_style_text_color(s_employee_label, UI_COLOR_BODY, 0);
    lv_obj_set_style_text_font(s_employee_label, UI_FONT_BODY, 0);

    lv_obj_t *logout_btn = lv_button_create(right);
    lv_obj_set_style_bg_opa(logout_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(logout_btn, 0, 0);
    lv_obj_set_style_pad_all(logout_btn, 4, 0);
    lv_obj_t *logout_lbl = lv_label_create(logout_btn);
    lv_label_set_text(logout_lbl, "Đăng xuất");
    lv_obj_set_style_text_color(logout_lbl, UI_COLOR_DANGER, 0);
    lv_obj_set_style_text_font(logout_lbl, UI_FONT_BODY_BOLD, 0);
    lv_obj_add_event_cb(logout_btn, logout_btn_event_cb, LV_EVENT_CLICKED, NULL);
}

/* ────────────────────────────────────────────────────────────────────────
 * THANH ĐIỀU HƯỚNG DƯỚI (mục 4.13)
 * ──────────────────────────────────────────────────────────────────────── */
static void build_bottom_nav(lv_obj_t *parent)
{
    NAV_ICONS[TAB_ORDERS] = &img_icon_nav_orders;
    NAV_ICONS[TAB_WEIGHING] = &img_icon_nav_weighing;
    NAV_ICONS[TAB_SUGGESTIONS] = &img_icon_nav_suggestions;
    NAV_ICONS[TAB_HISTORY] = &img_icon_nav_history;
    NAV_ICONS[TAB_SETTINGS] = &img_icon_nav_settings;

    s_bottom_nav = lv_obj_create(parent);
    lv_obj_remove_style_all(s_bottom_nav);
    lv_obj_set_size(s_bottom_nav, UI_HOR_RES, UI_BOTTOMNAV_H);
    lv_obj_set_pos(s_bottom_nav, 0, UI_VER_RES - UI_BOTTOMNAV_H);
    lv_obj_set_style_bg_color(s_bottom_nav, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(s_bottom_nav, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_bottom_nav, 1, 0);
    lv_obj_set_style_border_side(s_bottom_nav, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(s_bottom_nav, UI_COLOR_BORDER, 0);
    lv_obj_clear_flag(s_bottom_nav, LV_OBJ_FLAG_SCROLLABLE);

    lv_coord_t seg_w = UI_HOR_RES / TAB_COUNT;
    for (int i = 0; i < TAB_COUNT; i++) {
        lv_obj_t *btn = lv_obj_create(s_bottom_nav);
        lv_obj_remove_style_all(btn);
        lv_obj_set_size(btn, seg_w, UI_BOTTOMNAV_H);
        lv_obj_set_pos(btn, i * seg_w, 0);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(btn, nav_btn_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        lv_obj_t *icon = ui_common_icon(btn, NAV_ICONS[i], UI_COLOR_ICON);
        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, NAV_LABELS[i]);
        lv_obj_set_style_text_font(lbl, UI_FONT_XS, 0);
        lv_obj_set_style_text_color(lbl, UI_COLOR_ICON, 0);
        lv_obj_set_style_pad_top(lbl, 4, 0);

        s_nav_btn[i] = btn;
        s_nav_icon[i] = icon;
        s_nav_label[i] = lbl;
    }
}

static void nav_btn_event_cb(lv_event_t *e)
{
    int tab = (int)(intptr_t)lv_event_get_user_data(e);
    ui_shell_switch_tab((app_tab_t)tab);
}

static void logout_btn_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_shell_logout();
}

static void station_btn_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_station_modal_open();
}

static void sync_badge_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (app_state()->pending_sync_count <= 0) return;
    app_state_sync_now();
    ui_shell_refresh_chrome();
    ui_orders_refresh();
    ui_shell_toast("Đã đồng bộ toàn bộ phiếu chờ");
}

/* ────────────────────────────────────────────────────────────────────────
 * MÀN CHỜ (screensaver) — xem ghi chú SETTINGS_IDLE_MS phía trên
 * ──────────────────────────────────────────────────────────────────────── */
static void standby_update_clock(void)
{
    time_t now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);
    static const char *k_wd[7] = {"Chủ nhật", "Thứ hai", "Thứ ba", "Thứ tư", "Thứ năm", "Thứ sáu", "Thứ bảy"};

    char tbuf[8];
    snprintf(tbuf, sizeof(tbuf), "%02d:%02d", tmv.tm_hour, tmv.tm_min);
    lv_label_set_text(s_standby_time_label, tbuf);

    char dbuf[48];
    snprintf(dbuf, sizeof(dbuf), "%s, %02d/%02d/%04d",
             k_wd[tmv.tm_wday], tmv.tm_mday, tmv.tm_mon + 1, tmv.tm_year + 1900);
    lv_label_set_text(s_standby_date_label, dbuf);
}

static void standby_overlay_click_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    standby_dismiss();
}

static void build_standby_overlay(lv_obj_t *parent)
{
    s_standby_overlay = lv_obj_create(parent);
    lv_obj_remove_style_all(s_standby_overlay);
    lv_obj_set_size(s_standby_overlay, UI_HOR_RES, UI_VER_RES);
    lv_obj_set_pos(s_standby_overlay, 0, 0);
    lv_obj_set_style_bg_color(s_standby_overlay, UI_COLOR_HEADING, 0);
    lv_obj_set_style_bg_opa(s_standby_overlay, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_standby_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_standby_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_standby_overlay, standby_overlay_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(s_standby_overlay, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *col = lv_obj_create(s_standby_overlay);
    lv_obj_remove_style_all(col);
    lv_obj_set_size(col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_center(col);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, 12, 0);
    lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
    /* lv_obj_create() mặc định BẬT CLICKABLE, nằm đè lên overlay — phải bật
     * EVENT_BUBBLE để chạm vào "col" (và các label con) vẫn nổi bọt lên
     * overlay's click handler, KHÔNG dùng remove_flag(CLICKABLE) ở đây vì
     * col không có handler riêng (xem gotcha đầy đủ trong ui_login.c). */
    lv_obj_add_flag(col, LV_OBJ_FLAG_EVENT_BUBBLE);

    /* Phóng to gấp đôi (x2) số giờ:phút bằng transform scale quanh tâm —
     * cùng kỹ thuật ui_standby.c của dự án Mayxucv3 (không có font nào lớn
     * hơn UI_FONT_DISPLAY đã compile sẵn, scale bitmap tại chỗ rẻ hơn và
     * đủ nét cho màn chờ nhìn từ xa). Khung kích thước CỐ ĐỊNH để biết
     * chính xác tâm theo px — bắt buộc cho transform_pivot. */
    lv_obj_t *clock_box = lv_obj_create(col);
    lv_obj_remove_style_all(clock_box);
    lv_obj_set_size(clock_box, 460, 110);
    lv_obj_set_flex_flow(clock_box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(clock_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(clock_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(clock_box, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_style_transform_pivot_x(clock_box, 230, 0);
    lv_obj_set_style_transform_pivot_y(clock_box, 55, 0);
    lv_obj_set_style_transform_scale_x(clock_box, 512, 0);   /* 256 = 1.0x -> 512 = 2.0x */
    lv_obj_set_style_transform_scale_y(clock_box, 512, 0);

    s_standby_time_label = lv_label_create(clock_box);
    lv_label_set_text(s_standby_time_label, "--:--");
    lv_obj_set_style_text_color(s_standby_time_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_standby_time_label, UI_FONT_DISPLAY, 0);
    lv_obj_add_flag(s_standby_time_label, LV_OBJ_FLAG_EVENT_BUBBLE);

    s_standby_date_label = lv_label_create(col);
    lv_label_set_text(s_standby_date_label, "--");
    lv_obj_set_style_text_color(s_standby_date_label, UI_COLOR_ON_DARK_MUTED, 0);
    lv_obj_set_style_text_font(s_standby_date_label, UI_FONT_H3, 0);
    lv_obj_add_flag(s_standby_date_label, LV_OBJ_FLAG_EVENT_BUBBLE);

    lv_obj_t *hint = lv_label_create(col);
    lv_label_set_text(hint, "Chạm màn hình để quay lại");
    lv_obj_set_style_text_color(hint, UI_COLOR_ON_DARK_HINT, 0);
    lv_obj_set_style_text_font(hint, UI_FONT_BODY, 0);
    lv_obj_add_flag(hint, LV_OBJ_FLAG_EVENT_BUBBLE);
}

static void standby_enter(void)
{
    if (!s_standby_overlay) return;
    standby_update_clock();
    lv_obj_clear_flag(s_standby_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_standby_overlay);
}

static void standby_dismiss(void)
{
    if (!s_standby_overlay || lv_obj_has_flag(s_standby_overlay, LV_OBJ_FLAG_HIDDEN)) return;
    lv_obj_add_flag(s_standby_overlay, LV_OBJ_FLAG_HIDDEN);
    /* Tab Cài đặt "không có tác động" (chưa lưu gì) nên về thẳng Đơn hàng là
     * an toàn. */
    ui_shell_switch_tab(TAB_ORDERS);
}

/* ────────────────────────────────────────────────────────────────────────
 * DỰNG SHELL
 * ──────────────────────────────────────────────────────────────────────── */
void ui_shell_build(void)
{
    lv_obj_t *root = lv_screen_active();
    lv_obj_set_style_bg_color(root, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    s_login_screen = ui_login_create(root);

    s_shell_root = lv_obj_create(root);
    lv_obj_remove_style_all(s_shell_root);
    lv_obj_set_size(s_shell_root, UI_HOR_RES, UI_VER_RES);
    lv_obj_set_pos(s_shell_root, 0, 0);
    lv_obj_clear_flag(s_shell_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_shell_root, LV_OBJ_FLAG_HIDDEN);

    build_status_bar(s_shell_root);
    build_sub_bar(s_shell_root);

    s_content_area = lv_obj_create(s_shell_root);
    lv_obj_remove_style_all(s_content_area);
    lv_obj_set_size(s_content_area, UI_HOR_RES, UI_CONTENT_H);
    lv_obj_set_pos(s_content_area, 0, UI_CONTENT_Y);
    lv_obj_set_style_bg_color(s_content_area, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(s_content_area, LV_OPA_COVER, 0);
    lv_obj_set_scroll_dir(s_content_area, LV_DIR_VER);

    s_tab_pages[TAB_ORDERS] = ui_orders_create(s_content_area);
    s_tab_pages[TAB_WEIGHING] = ui_weighing_create(s_content_area);
    s_tab_pages[TAB_SUGGESTIONS] = ui_alerts_create(s_content_area);
    s_tab_pages[TAB_HISTORY] = ui_history_create(s_content_area);
    s_tab_pages[TAB_SETTINGS] = ui_settings_create(s_content_area);
    for (int i = 0; i < TAB_COUNT; i++) {
        lv_obj_add_flag(s_tab_pages[i], LV_OBJ_FLAG_HIDDEN);
    }

    build_bottom_nav(s_shell_root);
    ui_help_create_button(s_shell_root);

    /* Dựng SAU CÙNG trên "root" (không phải s_shell_root) để luôn nổi trên
     * cả shell lẫn màn đăng nhập khi được hiện (move_foreground phòng hờ
     * nếu có gì dựng thêm sau này). */
    build_standby_overlay(root);

    lv_timer_create(tick_timer_cb, 500, NULL);
}

void ui_shell_on_login_success(void)
{
    lv_obj_add_flag(s_login_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_shell_root, LV_OBJ_FLAG_HIDDEN);
    ui_shell_refresh_chrome();
    ui_shell_switch_tab(TAB_ORDERS);
}

void ui_shell_logout(void)
{
    app_state_logout();
    lv_obj_add_flag(s_shell_root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_login_screen, LV_OBJ_FLAG_HIDDEN);
    ui_login_reset();
}

void ui_shell_switch_tab(app_tab_t tab)
{
    app_state_t *st = app_state();

    st->current_tab = tab;
    for (int i = 0; i < TAB_COUNT; i++) {
        bool active = (i == (int)tab);
        if (active) lv_obj_clear_flag(s_tab_pages[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_tab_pages[i], LV_OBJ_FLAG_HIDDEN);

        lv_color_t c = active ? UI_COLOR_PRIMARY : UI_COLOR_ICON;
        lv_obj_set_style_image_recolor(s_nav_icon[i], c, 0);
        lv_obj_set_style_text_color(s_nav_label[i], c, 0);
    }

    switch (tab) {
        case TAB_ORDERS:      ui_orders_refresh(); break;
        case TAB_WEIGHING:    ui_weighing_refresh(); break;
        case TAB_SUGGESTIONS: app_state_refresh_alerts(); ui_alerts_refresh(); break;
        case TAB_HISTORY:     ui_history_refresh(); break;
        case TAB_SETTINGS:    ui_settings_refresh(); break;
        default: break;
    }
}

void ui_shell_toast(const char *text)
{
    ui_common_toast(s_content_area, text);
}

void ui_shell_refresh_chrome(void)
{
    app_state_t *st = app_state();

    /* Giờ thật (không còn mô phỏng) — localtime() áp TZ GMT+7 đã set ở
     * main.c, giờ hệ thống (UTC) tự cập nhật qua NTP khi có WiFi. */
    time_t now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", tmv.tm_hour, tmv.tm_min);
    lv_label_set_text(s_clock_label, buf);

    lv_obj_set_style_bg_color(s_dot_wifi, ui_common_link_color(st->wifi_link), 0);
    lv_obj_set_style_image_recolor(s_wifi_icon, ui_common_link_color(st->wifi_link), 0);
    lv_obj_set_style_bg_color(s_dot_p5, ui_common_link_color(st->p5_link), 0);
    lv_obj_set_style_bg_color(s_dot_cam, ui_common_link_color(st->camera_link), 0);

    if (st->pending_sync_count > 0) {
        char sbuf[32];
        snprintf(sbuf, sizeof(sbuf), "%d chờ đồng bộ", st->pending_sync_count);
        lv_label_set_text(s_sync_badge_label, sbuf);
        lv_obj_clear_flag(s_sync_badge, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_sync_badge, LV_OBJ_FLAG_HIDDEN);
    }

    if (st->current_employee_idx >= 0) {
        employee_t *e = &st->employees[st->current_employee_idx];
        char ebuf[96];
        snprintf(ebuf, sizeof(ebuf), "%s · %s", e->name, app_role_label(e->role));
        lv_label_set_text(s_employee_label, ebuf);
    }

    if (st->current_station_idx >= 0 && st->current_station_idx < st->station_count) {
        lv_label_set_text(s_station_name_label, st->stations[st->current_station_idx].name);
    }

    /* "Chế độ ánh sáng mạnh" (mục 4.10, tab Cài đặt) — tăng tương phản toàn
     * màn hình khi bật, bằng cách đổi nền vùng nội dung từ xám nhạt sang
     * trắng thuần (đây là nơi trung tâm duy nhất vẽ lại khung sau đăng nhập,
     * nên không cần thêm hàm/API riêng cho việc này). */
    lv_obj_set_style_bg_color(s_content_area, st->bright_mode ? UI_COLOR_WHITE : UI_COLOR_BG, 0);
}

static void tick_timer_cb(lv_timer_t *t)
{
    LV_UNUSED(t);

    app_state_sim_tick();
    app_state_refresh_alerts();
    ui_shell_refresh_chrome();

    bool standby_active = s_standby_overlay && !lv_obj_has_flag(s_standby_overlay, LV_OBJ_FLAG_HIDDEN);
    if (standby_active) {
        standby_update_clock();
        return;   /* đang che toàn màn hình — không cần vẽ lại tab bên dưới */
    }

    if (app_state()->logged_in) {
        app_tab_t tab = app_state()->current_tab;
        if (tab == TAB_WEIGHING) ui_weighing_refresh();
        else if (tab == TAB_SUGGESTIONS) ui_alerts_refresh();
        else if (tab == TAB_SETTINGS) {
            ui_settings_tick();
            if (lv_display_get_inactive_time(NULL) >= SETTINGS_IDLE_MS) standby_enter();
        }
    }
}
