/**
 * @file ui_settings.c
 * @brief Màn hình CÀI ĐẶT
 *
 * Layout:
 *  Header → Scrollable list → Bottom nav
 *
 * Nhóm chính:
 *   • Cấu hình Thiết bị
 *   • Cài đặt Mạng
 *   • Hiệu chuẩn
 *   • Thông tin Ứng dụng
 *
 * Nhóm KỸ THUẬT VIÊN CAO CẤP:
 *   • Đổi mã PIN/Mật khẩu
 */

#include "ui_settings.h"
#include "ui_theme.h"
#include "ui_nav.h"
#include "ui_calibration.h"
#include "esp_log.h"
#include <stdio.h>
#include <time.h>

static const char *TAG = "UI_SETTINGS";

static lv_obj_t *s_screen   = NULL;
static lv_obj_t *s_lbl_time = NULL;
static lv_obj_t *s_lbl_date = NULL;
static lv_timer_t *s_clock_timer = NULL;

/* ============================================================
 * SETTINGS ITEM DATA
 * ============================================================ */
typedef struct {
    const char *icon;
    const char *title;
    const char *subtitle;  /* NULL nếu không có */
    uint32_t    icon_color_hex;
} settings_item_t;

static const settings_item_t k_items_main[] = {
    { LV_SYMBOL_SETTINGS, "Cấu hình Thiết bị",  NULL,    0xF5C800 },
    { LV_SYMBOL_WIFI,     "Cài đặt Mạng",       NULL,    0xF5C800 },
    { LV_SYMBOL_EDIT,     "Hiệu chuẩn",         NULL,    0xF5C800 },
    { LV_SYMBOL_EYE,      "Thông tin Ứng dụng", NULL,    0xF5C800 },
};
#define ITEMS_MAIN_COUNT 4

static const settings_item_t k_item_advanced = {
    LV_SYMBOL_KEY, "Đổi mã PIN/Mật khẩu",
    "Cập nhật thông tin bảo mật hệ thống",
    0xF5A800
};

/* ============================================================
 * FORWARD DECLS
 * ============================================================ */
static void build_header(lv_obj_t *parent);
static void build_settings_list(lv_obj_t *parent);
static lv_obj_t *make_settings_row(lv_obj_t *parent, const settings_item_t *item);
static void settings_row_cb(lv_event_t *e);
static void clock_timer_cb(lv_timer_t *t);

/* ============================================================
 * PUBLIC API
 * ============================================================ */
lv_obj_t *ui_settings_get_screen(void) { return s_screen; }

void ui_settings_screen_init(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_add_style(s_screen, &style_screen, 0);
    lv_obj_set_size(s_screen, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_scrollbar_mode(s_screen, LV_SCROLLBAR_MODE_OFF);

    build_header(s_screen);
    build_settings_list(s_screen);
    ui_nav_build(s_screen, 4);   /* tab 4 = CÀI ĐẶT */

    s_clock_timer = lv_timer_create(clock_timer_cb, 1000, NULL);
    clock_timer_cb(NULL);

    ESP_LOGI(TAG, "Settings screen initialized");
}

/* ============================================================
 * HEADER
 * ============================================================ */
static void build_header(lv_obj_t *parent)
{
    lv_obj_t *hdr = lv_obj_create(parent);
    lv_obj_set_size(hdr, UI_SCREEN_W, UI_HEADER_H);
    lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(hdr, UI_COLOR_BG, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_radius(hdr, 0, 0);
    lv_obj_set_style_pad_all(hdr, UI_PAD_MD, 0);

    s_lbl_time = lv_label_create(hdr);
    lv_obj_set_style_text_color(s_lbl_time, UI_COLOR_TEXT_PRI, 0);
    lv_obj_set_style_text_font(s_lbl_time, UI_FONT_MEDIUM, 0);
    lv_obj_align(s_lbl_time, LV_ALIGN_TOP_LEFT, 24, 4);

    s_lbl_date = lv_label_create(hdr);
    lv_obj_set_style_text_color(s_lbl_date, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(s_lbl_date, UI_FONT_TINY, 0);
    lv_obj_align_to(s_lbl_date, s_lbl_time, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 2);

    lv_obj_t *lbl_title = lv_label_create(hdr);
    lv_label_set_text(lbl_title, "CÂN MÁY XÚC");
    lv_obj_set_style_text_color(lbl_title, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(lbl_title, UI_FONT_MEDIUM, 0);
    lv_obj_align(lbl_title, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *lbl_icons = lv_label_create(hdr);
    lv_label_set_text(lbl_icons,
        LV_SYMBOL_REFRESH "  4G  " LV_SYMBOL_WIFI "  " LV_SYMBOL_BATTERY_FULL " 85%");
    lv_obj_set_style_text_color(lbl_icons, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(lbl_icons, UI_FONT_SMALL, 0);
    lv_obj_align(lbl_icons, LV_ALIGN_TOP_RIGHT, -8, 4);
}

/* ============================================================
 * SETTINGS LIST
 * ============================================================ */
static lv_obj_t *make_settings_row(lv_obj_t *parent, const settings_item_t *item)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(row, UI_COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(row, UI_OPA_FULL, 0);
    lv_obj_set_style_border_color(row, UI_COLOR_BORDER, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_set_style_radius(row, UI_RADIUS_SM, 0);
    lv_obj_set_style_pad_all(row, UI_PAD_MD, 0);
    lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, settings_row_cb, LV_EVENT_CLICKED, (void *)item);

    /* ── Icon box ── */
    lv_obj_t *ico_box = lv_obj_create(row);
    lv_obj_set_size(ico_box, 70, 70);
    lv_obj_align(ico_box, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(ico_box, lv_color_hex(0x0D2033), 0);
    lv_obj_set_style_bg_opa(ico_box, UI_OPA_FULL, 0);
    lv_obj_set_style_border_color(ico_box, lv_color_hex(item->icon_color_hex), 0);
    lv_obj_set_style_border_width(ico_box, 1, 0);
    lv_obj_set_style_radius(ico_box, UI_RADIUS_SM, 0);

    lv_obj_t *lbl_ico = lv_label_create(ico_box);
    lv_label_set_text(lbl_ico, item->icon);
    lv_obj_set_style_text_color(lbl_ico, lv_color_hex(item->icon_color_hex), 0);
    lv_obj_set_style_text_font(lbl_ico, UI_FONT_LARGE, 0);
    lv_obj_center(lbl_ico);

    /* ── Text block ── */
    lv_obj_t *txt = lv_obj_create(row);
    lv_obj_remove_style_all(txt);
    lv_obj_set_size(txt, LV_PCT(100) - 70 - UI_PAD_MD*2 - 30, LV_SIZE_CONTENT);
    lv_obj_align(txt, LV_ALIGN_LEFT_MID, 70 + UI_PAD_MD, 0);
    lv_obj_set_flex_flow(txt, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(txt, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t *lbl_title = lv_label_create(txt);
    lv_label_set_text(lbl_title, item->title);
    lv_obj_set_style_text_color(lbl_title, UI_COLOR_TEXT_PRI, 0);
    lv_obj_set_style_text_font(lbl_title, UI_FONT_MEDIUM, 0);

    if (item->subtitle) {
        lv_obj_t *lbl_sub = lv_label_create(txt);
        lv_label_set_text(lbl_sub, item->subtitle);
        lv_obj_set_style_text_color(lbl_sub, UI_COLOR_TEXT_SEC, 0);
        lv_obj_set_style_text_font(lbl_sub, UI_FONT_SMALL, 0);
        lv_obj_set_style_margin_top(lbl_sub, 4, 0);
    }

    /* ── Chevron ── */
    lv_obj_t *chev = lv_label_create(row);
    lv_label_set_text(chev, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(chev, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(chev, UI_FONT_MEDIUM, 0);
    lv_obj_align(chev, LV_ALIGN_RIGHT_MID, 0, 0);

    return row;
}

static void build_settings_list(lv_obj_t *parent)
{
    int list_y = UI_HEADER_H + UI_PAD_MD;
    int list_h = UI_SCREEN_H - list_y - UI_NAV_H - UI_PAD_MD;

    lv_obj_t *list = lv_obj_create(parent);
    lv_obj_set_size(list, UI_SCREEN_W - 2*UI_PAD_MD, list_h);
    lv_obj_set_pos(list, UI_PAD_MD, list_y);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(list, UI_PAD_SM, 0);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_ACTIVE);

    /* Nhóm chính */
    for (int i = 0; i < ITEMS_MAIN_COUNT; i++) {
        make_settings_row(list, &k_items_main[i]);
    }

    /* Divider + section label */
    lv_obj_t *div_box = lv_obj_create(list);
    lv_obj_remove_style_all(div_box);
    lv_obj_set_size(div_box, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_top(div_box, UI_PAD_SM, 0);

    lv_obj_t *lbl_section = lv_label_create(div_box);
    lv_label_set_text(lbl_section, "KỸ THUẬT VIÊN CAO CẤP");
    lv_obj_set_style_text_color(lbl_section, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(lbl_section, UI_FONT_TINY, 0);

    /* Divider line */
    lv_obj_t *line = lv_obj_create(div_box);
    lv_obj_set_size(line, LV_PCT(100), 1);
    lv_obj_align_to(line, lbl_section, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 4);
    lv_obj_set_style_bg_color(line, UI_COLOR_BORDER, 0);
    lv_obj_set_style_bg_opa(line, UI_OPA_FULL, 0);
    lv_obj_set_style_border_width(line, 0, 0);

    /* Advanced item */
    make_settings_row(list, &k_item_advanced);
}

/* ============================================================
 * CALLBACKS
 * ============================================================ */
static void settings_row_cb(lv_event_t *e)
{
    const settings_item_t *item = (const settings_item_t *)lv_event_get_user_data(e);
    if (!item) return;
    ESP_LOGI(TAG, "Settings: %s", item->title);

    /* Hiệu chuẩn → yêu cầu PIN trước */
    if (item == &k_items_main[2]) {   /* index 2 = Hiệu chuẩn */
        ui_calibration_show_pin(s_screen);
        return;
    }
    /* TODO: mở sub-screen cho các mục khác */
}

/* ============================================================
 * CLOCK TIMER
 * ============================================================ */
static void clock_timer_cb(lv_timer_t *t)
{
    (void)t;
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    if (!tm_info) return;
    char time_buf[16], date_buf[16];
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm_info);
    strftime(date_buf, sizeof(date_buf), "%d/%m/%Y",  tm_info);
    if (s_lbl_time) lv_label_set_text(s_lbl_time, time_buf);
    if (s_lbl_date) lv_label_set_text(s_lbl_date, date_buf);
}
