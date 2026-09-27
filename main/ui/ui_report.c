/**
 * @file ui_report.c
 * @brief Màn hình BÁO CÁO & Thống kê
 *
 * Layout (800×1280):
 *  ┌────────────────────┐  h=96   Header
 *  ├────────────────────┤  h=80   Title block (Báo cáo & Thống kê + thời gian)
 *  ├────────────────────┤  h=160  Stats row 3 ô (Tổng KL / Chuyến / Hiệu suất)
 *  ├─────────┬──────────┤         Chart (trái) | Map + nút (phải)
 *  └────────────────────┘  h=128  Bottom nav
 *
 * Xuất báo cáo → chọn USB (USB1/USB2/USB3)
 */

#include "ui_report.h"
#include "ui_theme.h"
#include "ui_nav.h"
#include "esp_log.h"
#include <stdio.h>
#include <time.h>

static const char *TAG = "UI_REPORT";

static lv_obj_t *s_screen       = NULL;
static lv_obj_t *s_lbl_time     = NULL;
static lv_obj_t *s_lbl_date     = NULL;
static lv_obj_t *s_lbl_cur_time = NULL;
static lv_timer_t *s_clock_timer = NULL;

/* USB export modal */
static lv_obj_t *s_usb_overlay = NULL;

/* ============================================================
 * FORWARD DECLS
 * ============================================================ */
static void build_header(lv_obj_t *parent);
static void build_title_block(lv_obj_t *parent);
static void build_stats_row(lv_obj_t *parent);
static void build_chart_area(lv_obj_t *parent);
static void show_usb_modal(lv_event_t *e);
static void clock_timer_cb(lv_timer_t *t);

/* ============================================================
 * PUBLIC API
 * ============================================================ */
lv_obj_t *ui_report_get_screen(void) { return s_screen; }

void ui_report_screen_init(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_add_style(s_screen, &style_screen, 0);
    lv_obj_set_size(s_screen, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_scrollbar_mode(s_screen, LV_SCROLLBAR_MODE_OFF);

    build_header(s_screen);
    build_title_block(s_screen);
    build_stats_row(s_screen);
    build_chart_area(s_screen);
    ui_nav_build(s_screen, 3);   /* tab 3 = BÁO CÁO */

    s_clock_timer = lv_timer_create(clock_timer_cb, 1000, NULL);
    clock_timer_cb(NULL);

    ESP_LOGI(TAG, "Report screen initialized");
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
    lv_obj_set_style_text_color(s_lbl_time, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(s_lbl_time, UI_FONT_MEDIUM, 0);
    lv_obj_align(s_lbl_time, LV_ALIGN_TOP_LEFT, 8, 4);

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
 * TITLE BLOCK
 * ============================================================ */
static void build_title_block(lv_obj_t *parent)
{
    int blk_y = UI_HEADER_H + UI_PAD_SM;

    lv_obj_t *blk = lv_obj_create(parent);
    lv_obj_set_size(blk, UI_SCREEN_W - 2*UI_PAD_MD, 80);
    lv_obj_set_pos(blk, UI_PAD_MD, blk_y);
    lv_obj_set_style_bg_opa(blk, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(blk, 0, 0);
    lv_obj_set_style_pad_all(blk, 0, 0);
    lv_obj_set_scrollbar_mode(blk, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *lbl_h = lv_label_create(blk);
    lv_label_set_text(lbl_h, "Báo cáo & Thống kê");
    lv_obj_set_style_text_color(lbl_h, UI_COLOR_TEXT_PRI, 0);
    lv_obj_set_style_text_font(lbl_h, UI_FONT_LARGE, 0);
    lv_obj_align(lbl_h, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *lbl_sub = lv_label_create(blk);
    lv_label_set_text(lbl_sub, "Tổng quan hiệu suất hôm nay");
    lv_obj_set_style_text_color(lbl_sub, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(lbl_sub, UI_FONT_SMALL, 0);
    lv_obj_align(lbl_sub, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    s_lbl_cur_time = lv_label_create(blk);
    lv_label_set_text(s_lbl_cur_time, "--:-- --");
    lv_obj_set_style_text_color(s_lbl_cur_time, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(s_lbl_cur_time, UI_FONT_LARGE, 0);
    lv_obj_align(s_lbl_cur_time, LV_ALIGN_RIGHT_MID, 0, 0);
}

/* ============================================================
 * STATS ROW — 3 cards
 * ============================================================ */
static void build_stats_row(lv_obj_t *parent)
{
    int row_y  = UI_HEADER_H + UI_PAD_SM + 80 + UI_PAD_SM;
    int card_w = (UI_SCREEN_W - 2*UI_PAD_MD - 2*UI_PAD_SM) / 3;
    int card_h = 160;

    typedef struct {
        const char *icon;
        const char *label;
        const char *value;
        const char *unit;
        uint32_t    val_color;
    } stat_t;

    static const stat_t stats[3] = {
        { LV_SYMBOL_DOWNLOAD, "TỔNG KHỐI\nLƯỢNG",   "245,000", "KG", 0xF5C800 },
        { LV_SYMBOL_DRIVE,    "CHUYẾN HOÀN\nTHÀNH", "12",      "",   0x64B5F6 },
        { LV_SYMBOL_CHARGE,   "HIỆU SUẤT",          "94",      "%",  0x66BB6A },
    };

    for (int i = 0; i < 3; i++) {
        int cx = UI_PAD_MD + i * (card_w + UI_PAD_SM);

        lv_obj_t *card = lv_obj_create(parent);
        lv_obj_set_size(card, card_w, card_h);
        lv_obj_set_pos(card, cx, row_y);
        lv_obj_add_style(card, &style_card, 0);
        lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_pad_all(card, UI_PAD_SM, 0);

        lv_obj_t *lbl_ico = lv_label_create(card);
        lv_label_set_text(lbl_ico, stats[i].icon);
        lv_obj_set_style_text_color(lbl_ico, UI_COLOR_TEXT_SEC, 0);
        lv_obj_set_style_text_font(lbl_ico, UI_FONT_MEDIUM, 0);
        lv_obj_align(lbl_ico, LV_ALIGN_TOP_MID, 0, 0);

        lv_obj_t *lbl_name = lv_label_create(card);
        lv_label_set_text(lbl_name, stats[i].label);
        lv_label_set_long_mode(lbl_name, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(lbl_name, card_w - 2*UI_PAD_SM);
        lv_obj_set_style_text_color(lbl_name, UI_COLOR_TEXT_SEC, 0);
        lv_obj_set_style_text_font(lbl_name, UI_FONT_TINY, 0);
        lv_obj_set_style_text_align(lbl_name, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(lbl_name, LV_ALIGN_TOP_MID, 0, 28);

        lv_obj_t *lbl_val = lv_label_create(card);
        lv_label_set_text(lbl_val, stats[i].value);
        lv_obj_set_style_text_color(lbl_val, lv_color_hex(stats[i].val_color), 0);
        lv_obj_set_style_text_font(lbl_val, UI_FONT_LARGE, 0);
        lv_obj_align(lbl_val, LV_ALIGN_BOTTOM_MID, 0, -16);

        if (stats[i].unit[0]) {
            lv_obj_t *lbl_unit = lv_label_create(card);
            lv_label_set_text(lbl_unit, stats[i].unit);
            lv_obj_set_style_text_color(lbl_unit, UI_COLOR_TEXT_SEC, 0);
            lv_obj_set_style_text_font(lbl_unit, UI_FONT_TINY, 0);
            lv_obj_align(lbl_unit, LV_ALIGN_BOTTOM_MID, 0, 0);
        }
    }
}

/* ============================================================
 * USB EXPORT MODAL — chọn USB1/USB2/USB3
 * ============================================================ */
typedef struct {
    int         usb_idx;
    bool        connected;
    const char *label;
    const char *detail;
} usb_slot_t;

/* Dữ liệu USB — thực tế đọc từ USB MSC driver */
static const usb_slot_t k_usb_slots[3] = {
    { 1, true,  "USB 1", "32 GB — SẴN SÀNG"  },
    { 2, true,  "USB 2", "16 GB — SẴN SÀNG"  },
    { 3, false, "USB 3", "Chưa kết nối"        },
};

static void usb_close_cb(lv_event_t *e)
{
    (void)e;
    if (s_usb_overlay) { lv_obj_del(s_usb_overlay); s_usb_overlay = NULL; }
}

static void usb_save_cb(lv_event_t *e)
{
    const usb_slot_t *slot = (const usb_slot_t *)lv_event_get_user_data(e);
    ESP_LOGI(TAG, "Lưu báo cáo → USB%d", slot ? slot->usb_idx : 0);
    /* TODO: gọi API xuất file CSV/PDF thực tế */
    if (s_usb_overlay) { lv_obj_del(s_usb_overlay); s_usb_overlay = NULL; }
}

static void show_usb_modal(lv_event_t *e)
{
    (void)e;
    if (s_usb_overlay) return;

    /* Overlay */
    s_usb_overlay = lv_obj_create(s_screen);
    lv_obj_set_size(s_usb_overlay, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_pos(s_usb_overlay, 0, 0);
    lv_obj_set_style_bg_color(s_usb_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_usb_overlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(s_usb_overlay, 0, 0);
    lv_obj_set_style_radius(s_usb_overlay, 0, 0);
    lv_obj_set_scrollbar_mode(s_usb_overlay, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(s_usb_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_usb_overlay, usb_close_cb, LV_EVENT_CLICKED, NULL);

    /* Dialog */
    lv_obj_t *dlg = lv_obj_create(s_usb_overlay);
    lv_obj_set_size(dlg, UI_SCREEN_W - 80, LV_SIZE_CONTENT);
    lv_obj_center(dlg);
    lv_obj_set_style_bg_color(dlg, lv_color_hex(0x1E1E1E), 0);
    lv_obj_set_style_border_color(dlg, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(dlg, 2, 0);
    lv_obj_set_style_radius(dlg, UI_RADIUS_MD, 0);
    lv_obj_set_style_pad_all(dlg, UI_PAD_MD, 0);
    lv_obj_set_scrollbar_mode(dlg, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(dlg, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(dlg, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(dlg, UI_PAD_SM, 0);
    lv_obj_clear_flag(dlg, LV_OBJ_FLAG_EVENT_BUBBLE);

    /* Title row */
    lv_obj_t *title_row = lv_obj_create(dlg);
    lv_obj_remove_style_all(title_row);
    lv_obj_set_size(title_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(title_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title_row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *ttl = lv_label_create(title_row);
    lv_label_set_text(ttl, LV_SYMBOL_USB "  CHỌN THIẾT BỊ USB");
    lv_obj_set_style_text_color(ttl, UI_COLOR_TEXT_PRI, 0);
    lv_obj_set_style_text_font(ttl, UI_FONT_MEDIUM, 0);

    lv_obj_t *btn_x = lv_btn_create(title_row);
    lv_obj_set_size(btn_x, 36, 36);
    lv_obj_set_style_bg_color(btn_x, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(btn_x, 0, 0);
    lv_obj_set_style_radius(btn_x, 18, 0);
    lv_obj_add_event_cb(btn_x, usb_close_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *xl = lv_label_create(btn_x);
    lv_label_set_text(xl, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(xl, UI_COLOR_TEXT_SEC, 0);
    lv_obj_center(xl);

    /* Subtitle */
    lv_obj_t *lbl_sub = lv_label_create(dlg);
    lv_label_set_text(lbl_sub, "Dữ liệu báo cáo sẽ được lưu vào USB đã chọn");
    lv_obj_set_style_text_color(lbl_sub, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(lbl_sub, UI_FONT_SMALL, 0);

    /* 3 USB slots */
    for (int i = 0; i < 3; i++) {
        const usb_slot_t *slot = &k_usb_slots[i];

        lv_obj_t *row = lv_obj_create(dlg);
        lv_obj_set_size(row, LV_PCT(100), 80);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x2A2A2A), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(row,
            slot->connected ? UI_COLOR_BORDER : lv_color_hex(0x222222), 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_radius(row, UI_RADIUS_SM, 0);
        lv_obj_set_style_pad_hor(row, UI_PAD_MD, 0);
        lv_obj_set_style_pad_ver(row, 0, 0);
        lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);

        if (slot->connected) {
            lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_border_color(row, UI_COLOR_ACCENT, LV_STATE_PRESSED);
            lv_obj_add_event_cb(row, usb_save_cb, LV_EVENT_CLICKED, (void *)slot);
        }

        /* USB icon */
        lv_obj_t *ico = lv_label_create(row);
        lv_label_set_text(ico, LV_SYMBOL_USB);
        lv_obj_set_style_text_color(ico,
            slot->connected ? UI_COLOR_ACCENT : UI_COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(ico, UI_FONT_LARGE, 0);
        lv_obj_align(ico, LV_ALIGN_LEFT_MID, 0, 0);

        /* Label */
        lv_obj_t *lbl = lv_label_create(row);
        lv_label_set_text(lbl, slot->label);
        lv_obj_set_style_text_color(lbl,
            slot->connected ? UI_COLOR_TEXT_PRI : UI_COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(lbl, UI_FONT_MEDIUM, 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 60, -12);

        /* Detail/status */
        lv_obj_t *det = lv_label_create(row);
        lv_label_set_text(det, slot->detail);
        lv_obj_set_style_text_color(det,
            slot->connected ? lv_color_hex(0x22C55E) : UI_COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(det, UI_FONT_SMALL, 0);
        lv_obj_align(det, LV_ALIGN_LEFT_MID, 60, 14);

        /* Arrow nếu connected */
        if (slot->connected) {
            lv_obj_t *arr = lv_label_create(row);
            lv_label_set_text(arr, LV_SYMBOL_RIGHT);
            lv_obj_set_style_text_color(arr, UI_COLOR_TEXT_SEC, 0);
            lv_obj_set_style_text_font(arr, UI_FONT_MEDIUM, 0);
            lv_obj_align(arr, LV_ALIGN_RIGHT_MID, 0, 0);
        }
    }

    /* Nút Hủy */
    lv_obj_t *btn_cancel = lv_btn_create(dlg);
    lv_obj_set_size(btn_cancel, LV_PCT(100), 60);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(btn_cancel, 0, 0);
    lv_obj_set_style_radius(btn_cancel, UI_RADIUS_SM, 0);
    lv_obj_add_event_cb(btn_cancel, usb_close_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_c = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_c, "HỦY");
    lv_obj_set_style_text_color(lbl_c, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(lbl_c, UI_FONT_MEDIUM, 0);
    lv_obj_center(lbl_c);
}

/* ============================================================
 * CHART AREA — Bar chart (trái) + Map & nút (phải)
 * ============================================================ */
static void rename_cb(lv_event_t *e) { (void)e; ESP_LOGI(TAG, "Đổi tên..."); }

static void build_chart_area(lv_obj_t *parent)
{
    int area_y = UI_HEADER_H + UI_PAD_SM + 80 + UI_PAD_SM + 160 + UI_PAD_SM;
    int area_h = UI_SCREEN_H - area_y - UI_NAV_H - UI_PAD_SM;
    int col_w  = (UI_SCREEN_W - 2*UI_PAD_MD - UI_PAD_SM) / 2;

    /* ── Cột trái: Bar chart ── */
    lv_obj_t *chart_card = lv_obj_create(parent);
    lv_obj_set_size(chart_card, col_w, area_h);
    lv_obj_set_pos(chart_card, UI_PAD_MD, area_y);
    lv_obj_add_style(chart_card, &style_card, 0);
    lv_obj_set_scrollbar_mode(chart_card, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_all(chart_card, UI_PAD_SM, 0);

    lv_obj_t *lbl_ctitle = lv_label_create(chart_card);
    lv_label_set_text(lbl_ctitle, "PHÂN BỐ TẢI TRỌNG THEO GIỜ");
    lv_obj_set_style_text_color(lbl_ctitle, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(lbl_ctitle, UI_FONT_TINY, 0);
    lv_obj_align(lbl_ctitle, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *legend = lv_obj_create(chart_card);
    lv_obj_remove_style_all(legend);
    lv_obj_set_size(legend, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(legend, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_flex_flow(legend, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(legend, 4, 0);

    lv_obj_t *leg1 = lv_label_create(legend);
    lv_label_set_text(leg1, LV_SYMBOL_STOP " Đá dăm");
    lv_obj_set_style_text_color(leg1, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(leg1, UI_FONT_TINY, 0);

    lv_obj_t *leg2 = lv_label_create(legend);
    lv_label_set_text(leg2, LV_SYMBOL_STOP " Cát");
    lv_obj_set_style_text_color(leg2, lv_color_hex(0x90CAF9), 0);
    lv_obj_set_style_text_font(leg2, UI_FONT_TINY, 0);

    lv_obj_t *chart = lv_chart_create(chart_card);
    lv_obj_set_size(chart, LV_PCT(100), area_h - 36);
    lv_obj_align(chart, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_chart_set_type(chart, LV_CHART_TYPE_BAR);
    lv_chart_set_point_count(chart, 6);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 35000);
    lv_obj_set_style_bg_color(chart, lv_color_hex(0x1A1A1A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(chart, UI_OPA_FULL, LV_PART_MAIN);
    lv_obj_set_style_border_width(chart, 0, LV_PART_MAIN);
    lv_obj_set_style_line_color(chart, lv_color_hex(0x2A2A2A), LV_PART_MAIN);

    lv_chart_series_t *s1 = lv_chart_add_series(chart, UI_COLOR_ACCENT,
                                                 LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_next_value(chart, s1,  8000);
    lv_chart_set_next_value(chart, s1, 12000);
    lv_chart_set_next_value(chart, s1, 18000);
    lv_chart_set_next_value(chart, s1, 14000);
    lv_chart_set_next_value(chart, s1, 28000);
    lv_chart_set_next_value(chart, s1, 25000);

    lv_chart_series_t *s2 = lv_chart_add_series(chart, lv_color_hex(0x90CAF9),
                                                 LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_next_value(chart, s2,  3000);
    lv_chart_set_next_value(chart, s2,  8000);
    lv_chart_set_next_value(chart, s2, 12000);
    lv_chart_set_next_value(chart, s2, 10000);
    lv_chart_set_next_value(chart, s2, 20000);
    lv_chart_set_next_value(chart, s2, 18000);

    lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_X, 0, 0, 6, 1, true, 36);
    lv_obj_set_style_text_font(chart, UI_FONT_TINY, LV_PART_TICK_LABEL);
    lv_obj_set_style_text_color(chart, UI_COLOR_TEXT_SEC, LV_PART_TICK_LABEL);

    /* ── Cột phải ── */
    int right_x = UI_PAD_MD + col_w + UI_PAD_SM;
    int map_h   = area_h - UI_PAD_SM - 68;

    lv_obj_t *map_card = lv_obj_create(parent);
    lv_obj_set_size(map_card, col_w, map_h);
    lv_obj_set_pos(map_card, right_x, area_y);
    lv_obj_add_style(map_card, &style_card, 0);
    lv_obj_set_scrollbar_mode(map_card, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(map_card, lv_color_hex(0x1C1C1C), 0);
    lv_obj_set_style_pad_all(map_card, UI_PAD_SM, 0);

    lv_obj_t *lbl_pin = lv_label_create(map_card);
    lv_label_set_text(lbl_pin,
        LV_SYMBOL_GPS "  VỊ TRÍ HIỆN TẠI\n"
        "   JR4J+FJC, Bát Xát,\n"
        "   Lào Cai, Việt Nam");
    lv_label_set_long_mode(lbl_pin, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl_pin, col_w - 2*UI_PAD_SM);
    lv_obj_set_style_text_color(lbl_pin, UI_COLOR_TEXT_PRI, 0);
    lv_obj_set_style_text_font(lbl_pin, UI_FONT_TINY, 0);

    /* Nút Đổi tên + Xuất báo cáo */
    int btn_y = area_y + map_h + UI_PAD_SM;
    int btn_w = (col_w - UI_PAD_SM) / 2;

    lv_obj_t *btn_rename = lv_btn_create(parent);
    lv_obj_set_size(btn_rename, btn_w, 60);
    lv_obj_set_pos(btn_rename, right_x, btn_y);
    lv_obj_set_style_bg_color(btn_rename, lv_color_hex(0x2A2A2A), 0);
    lv_obj_set_style_bg_color(btn_rename, lv_color_hex(0x3A3A3A), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(btn_rename, UI_COLOR_BORDER, 0);
    lv_obj_set_style_border_width(btn_rename, 1, 0);
    lv_obj_set_style_radius(btn_rename, UI_RADIUS_SM, 0);
    lv_obj_add_event_cb(btn_rename, rename_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lr = lv_label_create(btn_rename);
    lv_label_set_text(lr, LV_SYMBOL_EDIT "\nĐổi tên");
    lv_obj_set_style_text_color(lr, UI_COLOR_TEXT_PRI, 0);
    lv_obj_set_style_text_font(lr, UI_FONT_TINY, 0);
    lv_obj_set_style_text_align(lr, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lr);

    /* Xuất báo cáo → mở USB modal */
    lv_obj_t *btn_export = lv_btn_create(parent);
    lv_obj_set_size(btn_export, btn_w, 60);
    lv_obj_set_pos(btn_export, right_x + btn_w + UI_PAD_SM, btn_y);
    lv_obj_set_style_bg_color(btn_export, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_bg_color(btn_export, UI_COLOR_ACCENT_DIM, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn_export, 0, 0);
    lv_obj_set_style_radius(btn_export, UI_RADIUS_SM, 0);
    lv_obj_add_event_cb(btn_export, show_usb_modal, LV_EVENT_CLICKED, NULL);
    lv_obj_t *le = lv_label_create(btn_export);
    lv_label_set_text(le, LV_SYMBOL_USB "\nXuất báo cáo");
    lv_obj_set_style_text_color(le, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_text_font(le, UI_FONT_TINY, 0);
    lv_obj_set_style_text_align(le, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(le);
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

    char time_buf[16], date_buf[16], curtime_buf[16];
    strftime(time_buf,    sizeof(time_buf),    "%H:%M:%S", tm_info);
    strftime(date_buf,    sizeof(date_buf),    "%d/%m/%Y",  tm_info);
    strftime(curtime_buf, sizeof(curtime_buf), "%I:%M %p",  tm_info);

    if (s_lbl_time)      lv_label_set_text(s_lbl_time,     time_buf);
    if (s_lbl_date)      lv_label_set_text(s_lbl_date,     date_buf);
    if (s_lbl_cur_time)  lv_label_set_text(s_lbl_cur_time, curtime_buf);
}
