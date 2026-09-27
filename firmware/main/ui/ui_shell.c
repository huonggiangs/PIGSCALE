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
/* sub bar widgets */
static lv_obj_t *s_sync_badge;
static lv_obj_t *s_sync_badge_label;
static lv_obj_t *s_employee_label;
static lv_obj_t *s_station_btn_label;
static lv_obj_t *s_station_name_label;

static const char *NAV_LABELS[TAB_COUNT] = {"Đơn hàng", "Cân", "Đề xuất", "Lịch sử", "Cài đặt"};
static const void *NAV_ICONS[TAB_COUNT];

/* demo clock — chưa nối RTC/NTP thật, chỉ minh hoạ "cập nhật mỗi 30 giây" */
static int s_demo_hour = 9, s_demo_min = 14;

static void tick_timer_cb(lv_timer_t *t);
static void nav_btn_event_cb(lv_event_t *e);
static void logout_btn_event_cb(lv_event_t *e);
static void station_btn_event_cb(lv_event_t *e);
static void sync_badge_event_cb(lv_event_t *e);

/* ────────────────────────────────────────────────────────────────────────
 * THANH TRẠNG THÁI (mục 4.2)
 * ──────────────────────────────────────────────────────────────────────── */
static lv_obj_t *make_status_cluster(lv_obj_t *parent, const char *label, lv_obj_t **out_dot)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_remove_style_all(c);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(c, 6, 0);
    lv_obj_set_size(c, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);

    *out_dot = ui_common_status_dot(c, LINK_OK);

    lv_obj_t *lbl = lv_label_create(c);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl, UI_FONT_XS, 0);
    return c;
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
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(right, 16, 0);
    lv_obj_set_size(right, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(right, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_clear_flag(right, LV_OBJ_FLAG_SCROLLABLE);

    make_status_cluster(right, "Wi-Fi", &s_dot_wifi);
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
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
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

    /* Khoá PIN Cài đặt: operator phải nhập lại PIN MỖI LẦN mở tab (luongcan.md mục 2) */
    if (tab == TAB_SETTINGS && st->current_tab != TAB_SETTINGS) {
        employee_t *e = (st->current_employee_idx >= 0) ? &st->employees[st->current_employee_idx] : NULL;
        if (e && e->role == ROLE_OPERATOR) {
            st->settings_unlocked = false;
        }
    }

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

    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", s_demo_hour, s_demo_min);
    lv_label_set_text(s_clock_label, buf);

    lv_obj_set_style_bg_color(s_dot_wifi, ui_common_link_color(st->wifi_link), 0);
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
    static int s_tick = 0;
    s_tick++;

    /* đồng hồ: +1 phút mỗi ~30s mô phỏng (thật ra tick 500ms => 60 tick = 30s) */
    if (s_tick % 60 == 0) {
        s_demo_min++;
        if (s_demo_min >= 60) { s_demo_min = 0; s_demo_hour = (s_demo_hour + 1) % 24; }
    }

    app_state_sim_tick();
    app_state_refresh_alerts();
    ui_shell_refresh_chrome();

    if (app_state()->logged_in) {
        app_tab_t tab = app_state()->current_tab;
        if (tab == TAB_WEIGHING) ui_weighing_refresh();
        else if (tab == TAB_SUGGESTIONS) ui_alerts_refresh();
    }
}
