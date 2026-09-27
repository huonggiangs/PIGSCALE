/**
 * @file ui_main.c
 * @brief Màn hình CÂN chính — pixel-faithful theo screen01.png
 *
 * Layout (480×800):
 *  ┌───────────────────────────────────────┐  y=0,  h=60  Header
 *  ├───────────────────────────────────────┤  y=60, h=90  Order Card
 *  ├───────────────────────────────────────┤  y=160,h=310 Weight Card
 *  ├───────────────────────────────────────┤  y=480,h=100 Stats Row
 *  ├───────────────────────────────────────┤  y=590,h=70  CTA Button
 *  └───────────────────────────────────────┘  y=720,h=80  Bottom Nav (ui_nav.c)
 */

#include "ui_main.h"
#include "ui_theme.h"
#include "ui_nav.h"
#include "weight_logic.h"
#include "esp_log.h"
#include <stdio.h>
#include <time.h>

static const char *TAG = "UI_MAIN";

/* ---- Widgets cần update runtime ---- */
static lv_obj_t *s_screen = NULL;

/* Header */
static lv_obj_t *s_lbl_time;
static lv_obj_t *s_lbl_date;

/* Order card */
static lv_obj_t *s_lbl_order_id;
static lv_obj_t *s_lbl_company;
static lv_obj_t *s_lbl_material;
static lv_obj_t *s_lbl_status;

/* Weight card */
static lv_obj_t *s_lbl_weight;
static lv_obj_t *s_bar_progress;
static lv_obj_t *s_lbl_progress_pct;

/* Stats row */
static lv_obj_t *s_lbl_total_val;
static lv_obj_t *s_lbl_total_pct;
static lv_obj_t *s_lbl_target_val;

/* CTA Button */
static lv_obj_t *s_btn_start;
static lv_obj_t *s_lbl_btn;

/* Timer ticking */
static lv_timer_t *s_clock_timer;

/* Pending orders modal */
static lv_obj_t *s_pending_overlay = NULL;

/* ============================================================
 * FORWARD DECLS
 * ============================================================ */
static void build_header(lv_obj_t *parent);
static void build_order_card(lv_obj_t *parent);
static void build_weight_card(lv_obj_t *parent);
static void build_stats_row(lv_obj_t *parent);
static void build_cta_button(lv_obj_t *parent);
static void clock_timer_cb(lv_timer_t *t);
static void btn_start_cb(lv_event_t *e);
static void show_pending_modal(lv_event_t *e);
static void target_tap_cb(lv_event_t *e);

/* ============================================================
 * PUBLIC API
 * ============================================================ */
void ui_main_screen_init(void)
{
    ui_theme_init();

    s_screen = lv_obj_create(NULL);
    lv_obj_add_style(s_screen, &style_screen, 0);
    lv_obj_set_size(s_screen, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_scrollbar_mode(s_screen, LV_SCROLLBAR_MODE_OFF);

    build_header(s_screen);
    build_order_card(s_screen);
    build_weight_card(s_screen);
    build_stats_row(s_screen);
    build_cta_button(s_screen);
    ui_nav_build(s_screen, 0);  /* tab index 0 = CÂN */

    /* Tick đồng hồ mỗi giây */
    s_clock_timer = lv_timer_create(clock_timer_cb, 1000, NULL);
    clock_timer_cb(NULL);  /* cập nhật ngay */

    lv_scr_load(s_screen);
    ESP_LOGI(TAG, "Main screen loaded");
}

lv_obj_t *ui_main_get_screen(void)
{
    return s_screen;
}

void ui_main_update_weight(float weight_kg, float total_kg, float target_kg)
{
    if (!s_screen) return;

    char buf[32];

    /* Giá trị gàu */
    if (weight_kg < 1000.0f) {
        snprintf(buf, sizeof(buf), "%.0f", weight_kg);
    } else {
        snprintf(buf, sizeof(buf), "%.1f", weight_kg);
    }
    lv_label_set_text(s_lbl_weight, buf);

    /* Progress bar */
    int pct = (target_kg > 0) ? (int)((total_kg / target_kg) * 100.0f) : 0;
    if (pct > 100) pct = 100;
    lv_bar_set_value(s_bar_progress, pct, LV_ANIM_ON);
    snprintf(buf, sizeof(buf), "HOÀN THÀNH %d%%", pct);
    lv_label_set_text(s_lbl_progress_pct, buf);

    /* Tổng đã tci */
    if (total_kg >= 1000.0f) {
        snprintf(buf, sizeof(buf), "%.1f", total_kg);
    } else {
        snprintf(buf, sizeof(buf), "%.0f", total_kg);
    }
    lv_label_set_text(s_lbl_total_val, buf);
    snprintf(buf, sizeof(buf), "%d%% của mục tiêu", pct);
    lv_label_set_text(s_lbl_total_pct, buf);
}

void ui_main_set_order(const char *order_id, const char *company,
                       const char *material, const char *status)
{
    if (s_lbl_order_id) lv_label_set_text(s_lbl_order_id, order_id);
    if (s_lbl_company)  lv_label_set_text(s_lbl_company,  company);
    if (s_lbl_material) lv_label_set_text(s_lbl_material, material);
    if (s_lbl_status)   lv_label_set_text(s_lbl_status,   status);
}

void ui_main_set_weighing_state(bool is_weighing)
{
    if (!s_lbl_btn) return;
    if (is_weighing) {
        lv_label_set_text(s_lbl_btn, LV_SYMBOL_PAUSE "  DỪNG CÂN");
        lv_obj_set_style_bg_color(s_btn_start, lv_color_hex(0xE53935), 0);
        lv_obj_set_style_text_color(s_btn_start, lv_color_hex(0xFFFFFF), 0);
    } else {
        lv_label_set_text(s_lbl_btn, LV_SYMBOL_PLAY "  BẮT ĐẦU CÂN");
        lv_obj_set_style_bg_color(s_btn_start, UI_COLOR_ACCENT, 0);
        lv_obj_set_style_text_color(s_btn_start, lv_color_hex(0x1A1A1A), 0);
    }
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

    /* Thời gian (trái — màu vàng accent) */
    s_lbl_time = lv_label_create(hdr);
    lv_obj_set_style_text_color(s_lbl_time, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(s_lbl_time, UI_FONT_MEDIUM, 0);
    lv_obj_align(s_lbl_time, LV_ALIGN_TOP_LEFT, 8, 4);

    /* Ngày (dưới time) */
    s_lbl_date = lv_label_create(hdr);
    lv_obj_set_style_text_color(s_lbl_date, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(s_lbl_date, UI_FONT_TINY, 0);
    lv_obj_align_to(s_lbl_date, s_lbl_time, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 2);

    /* Tiêu đề giữa */
    lv_obj_t *lbl_title = lv_label_create(hdr);
    lv_label_set_text(lbl_title, "CÂN MÁY XÚC");
    lv_obj_set_style_text_color(lbl_title, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(lbl_title, UI_FONT_MEDIUM, 0);
    lv_obj_align(lbl_title, LV_ALIGN_CENTER, 0, 0);

    /* Icons phải: Sync + 4G + WiFi + Pin% */
    lv_obj_t *lbl_icons = lv_label_create(hdr);
    lv_label_set_text(lbl_icons,
        LV_SYMBOL_REFRESH "  4G  " LV_SYMBOL_WIFI "  " LV_SYMBOL_BATTERY_FULL " 85%");
    lv_obj_set_style_text_color(lbl_icons, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(lbl_icons, UI_FONT_SMALL, 0);
    lv_obj_align(lbl_icons, LV_ALIGN_TOP_RIGHT, -8, 4);
}

/* ============================================================
 * ORDER CARD
 * ============================================================ */
static void build_order_card(lv_obj_t *parent)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, UI_SCREEN_W - 2*UI_PAD_MD, 90);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, UI_HEADER_H + UI_PAD_SM);
    lv_obj_add_style(card, &style_card, 0);
    lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);

    /* Order ID + badge hàng trên */
    s_lbl_order_id = lv_label_create(card);
    lv_label_set_text(s_lbl_order_id, "ĐƠN #ORD-8924");
    lv_obj_set_style_text_color(s_lbl_order_id, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(s_lbl_order_id, UI_FONT_SMALL, 0);
    lv_obj_align(s_lbl_order_id, LV_ALIGN_TOP_LEFT, 0, 0);

    /* Status badge (phải) — nền trong suốt, viền xám, có mũi tên ∨
     * Clickable: mở danh sách đơn chờ xử lý */
    lv_obj_t *badge = lv_obj_create(card);
    lv_obj_set_size(badge, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(badge, LV_OPA_TRANSP, 0);        /* nền trong suốt */
    lv_obj_set_style_border_width(badge, 1, 0);
    lv_obj_set_style_border_color(badge, lv_color_hex(0x666666), 0);
    lv_obj_set_style_radius(badge, UI_RADIUS_SM, 0);
    lv_obj_set_style_pad_hor(badge, 10, 0);
    lv_obj_set_style_pad_ver(badge, 6, 0);
    lv_obj_set_flex_flow(badge, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(badge, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(badge, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_add_flag(badge, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_border_color(badge, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_add_event_cb(badge, show_pending_modal, LV_EVENT_CLICKED, NULL);

    s_lbl_status = lv_label_create(badge);
    lv_label_set_text(s_lbl_status, "CHỜ XỬ LÝ");
    lv_obj_set_style_text_color(s_lbl_status, UI_COLOR_TEXT_PRI, 0);
    lv_obj_set_style_text_font(s_lbl_status, UI_FONT_TINY, 0);

    /* Mũi tên xổ xuống */
    lv_obj_t *lbl_chev = lv_label_create(badge);
    lv_label_set_text(lbl_chev, LV_SYMBOL_DOWN);
    lv_obj_set_style_text_color(lbl_chev, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(lbl_chev, UI_FONT_TINY, 0);

    /* Company name */
    s_lbl_company = lv_label_create(card);
    lv_label_set_text(s_lbl_company, "Acme Construction Ltd.");
    lv_obj_set_style_text_color(s_lbl_company, UI_COLOR_TEXT_PRI, 0);
    lv_obj_set_style_text_font(s_lbl_company, UI_FONT_MEDIUM, 0);
    lv_obj_align(s_lbl_company, LV_ALIGN_TOP_LEFT, 0, 20);

    /* Material line */
    lv_obj_t *row = lv_obj_create(card);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_align(row, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *truck_icon = lv_label_create(row);
    lv_label_set_text(truck_icon, LV_SYMBOL_DRIVE);
    lv_obj_set_style_text_color(truck_icon, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(truck_icon, UI_FONT_SMALL, 0);

    s_lbl_material = lv_label_create(row);
    lv_label_set_text(s_lbl_material, "  Đá dăm loại 1");
    lv_obj_set_style_text_color(s_lbl_material, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(s_lbl_material, UI_FONT_SMALL, 0);
}

/* ============================================================
 * WEIGHT CARD
 * ============================================================ */
static void build_weight_card(lv_obj_t *parent)
{
    int card_y = UI_HEADER_H + 90 + 2*UI_PAD_SM + UI_PAD_MD;

    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, UI_SCREEN_W - 2*UI_PAD_MD, UI_WEIGHT_CARD_H);
    lv_obj_set_pos(card, UI_PAD_MD, card_y);
    lv_obj_add_style(card, &style_card, 0);
    /* Weight card nền tối hơn card thường */
    lv_obj_set_style_bg_color(card, lv_color_hex(0x161616), 0);
    lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);

    /* Header row: "GÀU HIỆN TẠI:" + "KG" */
    lv_obj_t *lbl_head = lv_label_create(card);
    lv_label_set_text(lbl_head, "GÀU HIỆN TẠI:");
    lv_obj_set_style_text_color(lbl_head, UI_COLOR_TEXT_PRI, 0);
    lv_obj_set_style_text_font(lbl_head, UI_FONT_LARGE, 0);
    lv_obj_align(lbl_head, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *lbl_unit = lv_label_create(card);
    lv_label_set_text(lbl_unit, "KG");
    lv_obj_set_style_text_color(lbl_unit, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(lbl_unit, UI_FONT_LARGE, 0);
    lv_obj_align(lbl_unit, LV_ALIGN_TOP_RIGHT, 0, 0);

    /* Big weight number */
    s_lbl_weight = lv_label_create(card);
    lv_label_set_text(s_lbl_weight, "0");
    lv_obj_set_style_text_color(s_lbl_weight, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(s_lbl_weight, UI_FONT_WEIGHT_NUM, 0);
    lv_obj_align(s_lbl_weight, LV_ALIGN_CENTER, 0, -20);

    /* Progress bar container */
    lv_obj_t *prog_cont = lv_obj_create(card);
    lv_obj_remove_style_all(prog_cont);
    lv_obj_set_size(prog_cont, LV_PCT(100), 36);
    lv_obj_align(prog_cont, LV_ALIGN_BOTTOM_MID, 0, 0);

    s_bar_progress = lv_bar_create(prog_cont);
    lv_obj_set_size(s_bar_progress, LV_PCT(100), 36);
    lv_obj_align(s_bar_progress, LV_ALIGN_CENTER, 0, 0);
    lv_bar_set_range(s_bar_progress, 0, 100);
    lv_bar_set_value(s_bar_progress, 0, LV_ANIM_OFF);

    /* Bar track style */
    lv_obj_set_style_bg_color(s_bar_progress, UI_COLOR_PROGRESS_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_bar_progress, UI_OPA_FULL, LV_PART_MAIN);
    lv_obj_set_style_radius(s_bar_progress, 18, LV_PART_MAIN);

    /* Bar indicator style */
    lv_obj_set_style_bg_color(s_bar_progress, UI_COLOR_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_bar_progress, UI_OPA_FULL, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_bar_progress, 18, LV_PART_INDICATOR);

    /* Percentage label trên bar */
    s_lbl_progress_pct = lv_label_create(prog_cont);
    lv_label_set_text(s_lbl_progress_pct, "HOÀN THÀNH 0%");
    lv_obj_set_style_text_color(s_lbl_progress_pct, UI_COLOR_TEXT_PRI, 0);
    lv_obj_set_style_text_font(s_lbl_progress_pct, UI_FONT_TINY, 0);
    lv_obj_align(s_lbl_progress_pct, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_align(s_lbl_progress_pct, LV_TEXT_ALIGN_CENTER, 0);
}

/* ============================================================
 * STATS ROW
 * ============================================================ */
static void build_stats_row(lv_obj_t *parent)
{
    int row_y = UI_HEADER_H + 90 + UI_WEIGHT_CARD_H + 3*UI_PAD_SM + 2*UI_PAD_MD;

    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, UI_SCREEN_W - 2*UI_PAD_MD, UI_STATS_H);
    lv_obj_set_pos(row, UI_PAD_MD, row_y);
    lv_obj_set_style_bg_color(row, UI_COLOR_BG, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                               LV_FLEX_ALIGN_CENTER,
                               LV_FLEX_ALIGN_CENTER);

    /* --- Ô trái: Tổng đã tci --- */
    lv_obj_t *left = lv_obj_create(row);
    lv_obj_set_size(left, LV_PCT(48), LV_PCT(100));
    lv_obj_add_style(left, &style_card, 0);
    lv_obj_set_scrollbar_mode(left, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *lbl_total_hdr = lv_label_create(left);
    lv_label_set_text(lbl_total_hdr, "Tổng đã tải " LV_SYMBOL_EDIT);
    lv_obj_set_style_text_color(lbl_total_hdr, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(lbl_total_hdr, UI_FONT_TINY, 0);
    lv_obj_align(lbl_total_hdr, LV_ALIGN_TOP_LEFT, 0, 0);

    s_lbl_total_val = lv_label_create(left);
    lv_label_set_text(s_lbl_total_val, "0");
    lv_obj_set_style_text_color(s_lbl_total_val, UI_COLOR_TEXT_PRI, 0);
    lv_obj_set_style_text_font(s_lbl_total_val, UI_FONT_LARGE, 0);
    lv_obj_align(s_lbl_total_val, LV_ALIGN_MID_LEFT, 0, 8);

    lv_obj_t *lbl_unit_l = lv_label_create(left);
    lv_label_set_text(lbl_unit_l, " kg");
    lv_obj_set_style_text_color(lbl_unit_l, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(lbl_unit_l, UI_FONT_SMALL, 0);
    lv_obj_align_to(lbl_unit_l, s_lbl_total_val, LV_ALIGN_OUT_RIGHT_BOTTOM, 2, 0);

    s_lbl_total_pct = lv_label_create(left);
    lv_label_set_text(s_lbl_total_pct, "0% của mục tiêu");
    lv_obj_set_style_text_color(s_lbl_total_pct, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(s_lbl_total_pct, UI_FONT_TINY, 0);
    lv_obj_align(s_lbl_total_pct, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    /* --- Ô phải: Mục tiêu --- */
    lv_obj_t *right = lv_obj_create(row);
    lv_obj_set_size(right, LV_PCT(48), LV_PCT(100));
    lv_obj_add_style(right, &style_card, 0);
    lv_obj_set_scrollbar_mode(right, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *lbl_target_hdr = lv_label_create(right);
    lv_label_set_text(lbl_target_hdr, "MỤC TIÊU");
    lv_obj_set_style_text_color(lbl_target_hdr, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(lbl_target_hdr, UI_FONT_TINY, 0);
    lv_obj_align(lbl_target_hdr, LV_ALIGN_TOP_RIGHT, 0, 0);

    s_lbl_target_val = lv_label_create(right);
    lv_label_set_text(s_lbl_target_val, "25,000");
    lv_obj_set_style_text_color(s_lbl_target_val, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(s_lbl_target_val, UI_FONT_LARGE, 0);
    lv_obj_align(s_lbl_target_val, LV_ALIGN_MID_RIGHT, 0, 8);

    lv_obj_t *lbl_unit_r = lv_label_create(right);
    lv_label_set_text(lbl_unit_r, " kg");
    lv_obj_set_style_text_color(lbl_unit_r, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(lbl_unit_r, UI_FONT_SMALL, 0);
    lv_obj_align_to(lbl_unit_r, s_lbl_target_val, LV_ALIGN_OUT_RIGHT_BOTTOM, 2, 0);

    /* Tap vào ô MỤC TIÊU → mở numpad nhập mục tiêu */
    lv_obj_add_flag(right, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_border_color(right, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_add_event_cb(right, target_tap_cb, LV_EVENT_CLICKED, NULL);
}

/* ============================================================
 * CTA BUTTON
 * ============================================================ */
static void build_cta_button(lv_obj_t *parent)
{
    /* Đặt ngay trên bottom nav */
    int btn_y = UI_SCREEN_H - UI_NAV_H - UI_BTN_H - UI_PAD_MD;

    s_btn_start = lv_btn_create(parent);
    lv_obj_set_size(s_btn_start, UI_SCREEN_W - 2*UI_PAD_MD, UI_BTN_H);
    lv_obj_set_pos(s_btn_start, UI_PAD_MD, btn_y);
    lv_obj_add_style(s_btn_start, &style_btn_primary, 0);
    lv_obj_add_style(s_btn_start, &style_btn_primary_pressed, LV_STATE_PRESSED);
    lv_obj_add_event_cb(s_btn_start, btn_start_cb, LV_EVENT_CLICKED, NULL);

    s_lbl_btn = lv_label_create(s_btn_start);
    lv_label_set_text(s_lbl_btn, LV_SYMBOL_PLAY "  BẮT ĐẦU CÂN");
    lv_obj_set_style_text_font(s_lbl_btn, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(s_lbl_btn, lv_color_hex(0x1A1A1A), 0);
    lv_obj_center(s_lbl_btn);
}

/* ============================================================
 * NUMPAD POPUP — Nhập mục tiêu cân
 * ============================================================ */

/* Numpad state */
static char     s_np_buf[12]  = "0";
static lv_obj_t *s_np_overlay  = NULL;
static lv_obj_t *s_np_display  = NULL;  /* label hiển thị số đang nhập */

static void np_refresh_display(void)
{
    if (!s_np_display) return;
    /* Format với dấu phẩy ngàn */
    long val = atol(s_np_buf);
    char fmt[16];
    if (val >= 1000) snprintf(fmt, sizeof(fmt), "%ld,%03ld", val/1000, val%1000);
    else             snprintf(fmt, sizeof(fmt), "%ld", val);
    lv_label_set_text(s_np_display, fmt);
}

static void np_key_cb(lv_event_t *e)
{
    const char *digit = (const char *)lv_event_get_user_data(e);
    size_t len = strlen(s_np_buf);
    if (strcmp(s_np_buf, "0") == 0 && strcmp(digit, "0") != 0) {
        strncpy(s_np_buf, digit, sizeof(s_np_buf)-1);
    } else if (len < 7) {
        strncat(s_np_buf, digit, sizeof(s_np_buf)-len-1);
    }
    np_refresh_display();
}

static void np_del_cb(lv_event_t *e)
{
    (void)e;
    size_t len = strlen(s_np_buf);
    if (len > 1) s_np_buf[len-1] = '\0';
    else         strncpy(s_np_buf, "0", sizeof(s_np_buf));
    np_refresh_display();
}

static void np_cancel_cb(lv_event_t *e)
{
    (void)e;
    if (s_np_overlay) { lv_obj_del(s_np_overlay); s_np_overlay = NULL; }
}

static void np_confirm_cb(lv_event_t *e)
{
    (void)e;
    float target = (float)atol(s_np_buf);
    if (target < 100.0f) return;  /* bỏ qua nếu < 100 kg */

    /* Cập nhật mục tiêu */
    weight_logic_set_target(target);
    char buf[20];
    long v = (long)target;
    if (v >= 1000) snprintf(buf, sizeof(buf), "%ld,%03ld", v/1000, v%1000);
    else           snprintf(buf, sizeof(buf), "%ld", v);
    if (s_lbl_target_val) lv_label_set_text(s_lbl_target_val, buf);

    /* Đóng popup + bắt đầu cân */
    np_cancel_cb(NULL);
    weight_logic_start();
    ui_main_set_weighing_state(true);
}

static lv_obj_t *np_make_btn(lv_obj_t *parent, const char *label,
                               lv_event_cb_t cb, void *user_data,
                               lv_color_t bg, lv_color_t fg)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(btn, bg, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, UI_RADIUS_MD, 0);
    lv_obj_set_style_pad_hor(btn, 20, 0);
    lv_obj_set_style_pad_ver(btn, 14, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, fg, 0);
    lv_obj_set_style_text_font(lbl, UI_FONT_MEDIUM, 0);
    lv_obj_center(lbl);
    return btn;
}

void ui_main_show_numpad(void)
{
    if (s_np_overlay) return;  /* đã mở */
    strncpy(s_np_buf, "0", sizeof(s_np_buf));

    /* Semi-transparent overlay */
    s_np_overlay = lv_obj_create(s_screen);
    lv_obj_set_size(s_np_overlay, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_pos(s_np_overlay, 0, 0);
    lv_obj_set_style_bg_color(s_np_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_np_overlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(s_np_overlay, 0, 0);
    lv_obj_set_style_radius(s_np_overlay, 0, 0);
    lv_obj_set_scrollbar_mode(s_np_overlay, LV_SCROLLBAR_MODE_OFF);

    /* Dialog box */
    lv_obj_t *dlg = lv_obj_create(s_np_overlay);
    lv_obj_set_size(dlg, UI_SCREEN_W - 60, LV_SIZE_CONTENT);
    lv_obj_center(dlg);
    lv_obj_set_style_bg_color(dlg, lv_color_hex(0x242424), 0);
    lv_obj_set_style_border_color(dlg, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(dlg, 2, 0);
    lv_obj_set_style_radius(dlg, UI_RADIUS_MD, 0);
    lv_obj_set_style_pad_all(dlg, UI_PAD_MD, 0);
    lv_obj_set_scrollbar_mode(dlg, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(dlg, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(dlg, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    /* Title row */
    lv_obj_t *title_row = lv_obj_create(dlg);
    lv_obj_remove_style_all(title_row);
    lv_obj_set_size(title_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(title_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title_row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                           LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *ttl = lv_label_create(title_row);
    lv_label_set_text(ttl, "NHẬP MỤC TIÊU CÂN");
    lv_obj_set_style_text_color(ttl, UI_COLOR_TEXT_PRI, 0);
    lv_obj_set_style_text_font(ttl, UI_FONT_SMALL, 0);

    lv_obj_t *btn_x = lv_btn_create(title_row);
    lv_obj_set_size(btn_x, 28, 28);
    lv_obj_set_style_bg_color(btn_x, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(btn_x, 0, 0);
    lv_obj_set_style_radius(btn_x, 14, 0);
    lv_obj_add_event_cb(btn_x, np_cancel_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *xl = lv_label_create(btn_x);
    lv_label_set_text(xl, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(xl, UI_COLOR_TEXT_SEC, 0);
    lv_obj_center(xl);

    /* Display */
    lv_obj_t *disp = lv_obj_create(dlg);
    lv_obj_set_size(disp, LV_PCT(100), 70);
    lv_obj_set_style_bg_color(disp, lv_color_hex(0x111111), 0);
    lv_obj_set_style_border_width(disp, 0, 0);
    lv_obj_set_style_radius(disp, UI_RADIUS_SM, 0);
    lv_obj_set_style_margin_ver(disp, UI_PAD_SM, 0);

    lv_obj_t *sub = lv_label_create(disp);
    lv_label_set_text(sub, "TRỌNG LƯỢNG MỤC TIÊU");
    lv_obj_set_style_text_color(sub, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(sub, UI_FONT_TINY, 0);
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 6);

    s_np_display = lv_label_create(disp);
    lv_label_set_text(s_np_display, "0");
    lv_obj_set_style_text_color(s_np_display, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(s_np_display, UI_FONT_LARGE, 0);
    lv_obj_align(s_np_display, LV_ALIGN_CENTER, 0, 4);

    lv_obj_t *unit = lv_label_create(disp);
    lv_label_set_text(unit, "KG");
    lv_obj_set_style_text_color(unit, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(unit, UI_FONT_TINY, 0);
    lv_obj_align(unit, LV_ALIGN_BOTTOM_MID, 0, -4);

    /* Numpad grid */
    lv_obj_t *grid = lv_obj_create(dlg);
    lv_obj_remove_style_all(grid);
    lv_obj_set_size(grid, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(grid, 0, 0);

    static lv_coord_t cols[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t rows[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT,
                                 LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(grid, cols, rows);
    lv_obj_set_style_pad_column(grid, UI_PAD_SM, 0);
    lv_obj_set_style_pad_row(grid, UI_PAD_SM, 0);

    lv_color_t btn_bg = lv_color_hex(0x2e2e2e);
    lv_color_t btn_fg = UI_COLOR_TEXT_PRI;

    static const char *digits[] = {"1","2","3","4","5","6","7","8","9",".",  "0"};
    static const int   dcols[]  = { 0,  1,  2,  0,  1,  2,  0,  1,  2,  0,   1 };
    static const int   drows[]  = { 0,  0,  0,  1,  1,  1,  2,  2,  2,  3,   3 };

    for (int i = 0; i < 11; i++) {
        lv_obj_t *b = lv_btn_create(grid);
        lv_obj_set_grid_cell(b, LV_GRID_ALIGN_STRETCH, dcols[i], 1,
                                LV_GRID_ALIGN_CENTER,  drows[i], 1);
        lv_obj_set_style_bg_color(b, btn_bg, 0);
        lv_obj_set_style_border_width(b, 0, 0);
        lv_obj_set_style_radius(b, UI_RADIUS_SM, 0);
        lv_obj_set_style_pad_ver(b, 12, 0);
        lv_obj_add_event_cb(b, np_key_cb, LV_EVENT_CLICKED, (void *)digits[i]);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, digits[i]);
        lv_obj_set_style_text_color(l, btn_fg, 0);
        lv_obj_set_style_text_font(l, UI_FONT_MEDIUM, 0);
        lv_obj_center(l);
    }
    /* Backspace */
    lv_obj_t *bdel = lv_btn_create(grid);
    lv_obj_set_grid_cell(bdel, LV_GRID_ALIGN_STRETCH, 2, 1,
                               LV_GRID_ALIGN_CENTER,  3, 1);
    lv_obj_set_style_bg_color(bdel, btn_bg, 0);
    lv_obj_set_style_border_width(bdel, 0, 0);
    lv_obj_set_style_radius(bdel, UI_RADIUS_SM, 0);
    lv_obj_set_style_pad_ver(bdel, 12, 0);
    lv_obj_add_event_cb(bdel, np_del_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *dl = lv_label_create(bdel);
    lv_label_set_text(dl, LV_SYMBOL_BACKSPACE);
    lv_obj_set_style_text_color(dl, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(dl, UI_FONT_MEDIUM, 0);
    lv_obj_center(dl);

    /* Cancel / Confirm buttons */
    lv_obj_t *btn_row = lv_obj_create(dlg);
    lv_obj_remove_style_all(btn_row);
    lv_obj_set_size(btn_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_margin_top(btn_row, UI_PAD_SM, 0);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                           LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    np_make_btn(btn_row, "HỦY", np_cancel_cb, NULL,
                lv_color_hex(0x333333), UI_COLOR_TEXT_SEC);
    np_make_btn(btn_row, "BẮT ĐẦU", np_confirm_cb, NULL,
                UI_COLOR_ACCENT, lv_color_hex(0x1A1A1A));
}

/* ============================================================
 * CALLBACKS
 * ============================================================ */
static void btn_start_cb(lv_event_t *e)
{
    (void)e;
    if (weight_logic_is_running()) {
        /* Đang cân → DỪNG */
        weight_logic_stop();
        ui_main_set_weighing_state(false);
    } else {
        /* Chưa cân → mở numpad nhập mục tiêu */
        ui_main_show_numpad();
    }
}

static void target_tap_cb(lv_event_t *e)
{
    (void)e;
    /* Mở numpad để nhập/sửa mục tiêu cân */
    ui_main_show_numpad();
}

static void clock_timer_cb(lv_timer_t *t)
{
    (void)t;
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    if (!tm_info) return;

    char time_buf[16];
    char date_buf[16];
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm_info);
    strftime(date_buf, sizeof(date_buf), "%d/%m/%Y",  tm_info);

    if (s_lbl_time) lv_label_set_text(s_lbl_time, time_buf);
    if (s_lbl_date) lv_label_set_text(s_lbl_date, date_buf);
}

/* ============================================================
 * Load order từ màn ĐƠN HÀNG (gọi khi user chọn đơn)
 * ============================================================ */
void ui_main_load_order(const char *order_id, const char *company,
                        const char *material, float target_kg)
{
    /* Cập nhật order card */
    ui_main_set_order(order_id, company, material, "CHỜ XỬ LÝ");

    /* Cập nhật target label */
    char buf[20];
    long v = (long)target_kg;
    if (v >= 1000) snprintf(buf, sizeof(buf), "%ld,%03ld", v/1000, v%1000);
    else           snprintf(buf, sizeof(buf), "%ld", v);
    if (s_lbl_target_val) lv_label_set_text(s_lbl_target_val, buf);

    /* Pre-fill numpad với target từ đơn hàng */
    snprintf(s_np_buf, sizeof(s_np_buf), "%ld", v);
    weight_logic_set_target(target_kg);

    /* Chuyển sang màn hình CÂN + mở numpad */
    lv_scr_load_anim(s_screen, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, false);
    lv_timer_ready(s_clock_timer);
    ui_main_show_numpad();
}

/* ============================================================
 * PENDING ORDERS MODAL — Đơn hàng đang chờ từ phần mềm ngoài
 * ============================================================ */

typedef struct {
    const char *order_id;
    const char *company;
    const char *material;
    float       target_kg;
    const char *badge_txt;
    uint32_t    badge_color_hex;
} pending_order_t;

/* Mock data — thực tế sẽ điền từ network/database */
static const pending_order_t k_pending[] = {
    { "ORD-8824-A", "Apex Build Co.",    "Gravel 3/4\"",       24500.0f, "ĐÃ ĐỒNG BỘ", 0x1A7F3C },
    { "ORD-8825-B", "Metro Paving",      "Asphalt Base",       18000.0f, "NỘI BỘ",     0x7A5C00 },
    { "ORD-8826-C", "Skyline Logistics", "Sand Grade A",       32000.0f, "ĐÃ ĐỒNG BỘ", 0x1A7F3C },
    { "ORD-8827-D", "Urban Infra",       "Recycled Concrete",  15500.0f, "NỘI BỘ",     0x7A5C00 },
};
#define PENDING_COUNT ((int)(sizeof(k_pending)/sizeof(k_pending[0])))

static void pending_close_cb(lv_event_t *e)
{
    (void)e;
    if (s_pending_overlay) {
        lv_obj_del(s_pending_overlay);
        s_pending_overlay = NULL;
    }
}

static void pending_select_cb(lv_event_t *e)
{
    const pending_order_t *o = (const pending_order_t *)lv_event_get_user_data(e);
    if (s_pending_overlay) {
        lv_obj_del(s_pending_overlay);
        s_pending_overlay = NULL;
    }
    if (o) {
        ui_main_load_order(o->order_id, o->company, o->material, o->target_kg);
    }
}

static void show_pending_modal(lv_event_t *e)
{
    (void)e;
    if (s_pending_overlay) return;   /* đã mở */

    /* ── Overlay toàn màn hình ── */
    s_pending_overlay = lv_obj_create(s_screen);
    lv_obj_set_size(s_pending_overlay, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_pos(s_pending_overlay, 0, 0);
    lv_obj_set_style_bg_color(s_pending_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_pending_overlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(s_pending_overlay, 0, 0);
    lv_obj_set_style_radius(s_pending_overlay, 0, 0);
    lv_obj_set_scrollbar_mode(s_pending_overlay, LV_SCROLLBAR_MODE_OFF);
    /* click nền để đóng */
    lv_obj_add_flag(s_pending_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_pending_overlay, pending_close_cb, LV_EVENT_CLICKED, NULL);

    /* ── Dialog box ── */
    lv_obj_t *dlg = lv_obj_create(s_pending_overlay);
    lv_obj_set_size(dlg, UI_SCREEN_W - 40, LV_PCT(80));
    lv_obj_align(dlg, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(dlg, lv_color_hex(0x1E1E1E), 0);
    lv_obj_set_style_border_color(dlg, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(dlg, 2, 0);
    lv_obj_set_style_radius(dlg, UI_RADIUS_MD, 0);
    lv_obj_set_style_pad_all(dlg, UI_PAD_MD, 0);
    lv_obj_set_scrollbar_mode(dlg, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(dlg, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(dlg, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(dlg, LV_OBJ_FLAG_SCROLLABLE);  /* scroll chỉ ở list bên trong */
    /* chặn click xuyên qua dialog */
    lv_obj_add_flag(dlg, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_clear_flag(dlg, LV_OBJ_FLAG_EVENT_BUBBLE);

    /* ── Title row ── */
    lv_obj_t *title_row = lv_obj_create(dlg);
    lv_obj_remove_style_all(title_row);
    lv_obj_set_size(title_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(title_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title_row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *ttl = lv_label_create(title_row);
    lv_label_set_text(ttl, "CHỌN ĐƠN HÀNG");
    lv_obj_set_style_text_color(ttl, UI_COLOR_TEXT_PRI, 0);
    lv_obj_set_style_text_font(ttl, UI_FONT_MEDIUM, 0);

    lv_obj_t *btn_x = lv_btn_create(title_row);
    lv_obj_set_size(btn_x, 30, 30);
    lv_obj_set_style_bg_color(btn_x, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(btn_x, 0, 0);
    lv_obj_set_style_radius(btn_x, 15, 0);
    lv_obj_add_event_cb(btn_x, pending_close_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *xl = lv_label_create(btn_x);
    lv_label_set_text(xl, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(xl, UI_COLOR_TEXT_SEC, 0);
    lv_obj_center(xl);

    /* ── Scrollable list ── */
    lv_obj_t *list = lv_obj_create(dlg);
    lv_obj_set_size(list, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_max_height(list, 480, 0);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_style_margin_top(list, UI_PAD_SM, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(list, UI_PAD_SM, 0);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_ACTIVE);

    for (int i = 0; i < PENDING_COUNT; i++) {
        const pending_order_t *o = &k_pending[i];

        lv_obj_t *card = lv_obj_create(list);
        lv_obj_set_size(card, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x2A2A2A), 0);
        lv_obj_set_style_bg_opa(card, UI_OPA_FULL, 0);
        lv_obj_set_style_border_color(card, UI_COLOR_BORDER, 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_radius(card, UI_RADIUS_SM, 0);
        lv_obj_set_style_pad_all(card, UI_PAD_MD, 0);
        lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_border_color(card, UI_COLOR_ACCENT, LV_STATE_PRESSED);
        lv_obj_add_event_cb(card, pending_select_cb, LV_EVENT_CLICKED,
                            (void *)o);

        /* Hàng trên: order_id (trái) + badge (phải) */
        lv_obj_t *lbl_id = lv_label_create(card);
        lv_label_set_text(lbl_id, o->order_id);
        lv_obj_set_style_text_color(lbl_id, UI_COLOR_TEXT_SEC, 0);
        lv_obj_set_style_text_font(lbl_id, UI_FONT_TINY, 0);
        lv_obj_align(lbl_id, LV_ALIGN_TOP_LEFT, 0, 0);

        lv_obj_t *bdg = lv_obj_create(card);
        lv_obj_set_size(bdg, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(bdg, lv_color_hex(o->badge_color_hex), 0);
        lv_obj_set_style_bg_opa(bdg, UI_OPA_FULL, 0);
        lv_obj_set_style_border_width(bdg, 0, 0);
        lv_obj_set_style_radius(bdg, UI_RADIUS_SM, 0);
        lv_obj_set_style_pad_hor(bdg, 8, 0);
        lv_obj_set_style_pad_ver(bdg, 3, 0);
        lv_obj_align(bdg, LV_ALIGN_TOP_RIGHT, 0, 0);
        lv_obj_t *bdg_lbl = lv_label_create(bdg);
        lv_label_set_text(bdg_lbl, o->badge_txt);
        lv_obj_set_style_text_color(bdg_lbl, UI_COLOR_TEXT_PRI, 0);
        lv_obj_set_style_text_font(bdg_lbl, UI_FONT_TINY, 0);
        lv_obj_center(bdg_lbl);

        /* Company name */
        lv_obj_t *lbl_co = lv_label_create(card);
        lv_label_set_text(lbl_co, o->company);
        lv_obj_set_style_text_color(lbl_co, UI_COLOR_TEXT_PRI, 0);
        lv_obj_set_style_text_font(lbl_co, UI_FONT_MEDIUM, 0);
        lv_obj_align(lbl_co, LV_ALIGN_TOP_LEFT, 0, 18);

        /* Material */
        lv_obj_t *lbl_mat = lv_label_create(card);
        lv_label_set_text(lbl_mat, o->material);
        lv_obj_set_style_text_color(lbl_mat, UI_COLOR_TEXT_SEC, 0);
        lv_obj_set_style_text_font(lbl_mat, UI_FONT_SMALL, 0);
        lv_obj_align(lbl_mat, LV_ALIGN_TOP_LEFT, 0, 42);

        /* Target weight */
        char wbuf[20];
        long wv = (long)o->target_kg;
        if (wv >= 1000) snprintf(wbuf, sizeof(wbuf), "%ld,%03ld KG", wv/1000, wv%1000);
        else            snprintf(wbuf, sizeof(wbuf), "%ld KG", wv);
        lv_obj_t *lbl_w = lv_label_create(card);
        lv_label_set_text(lbl_w, wbuf);
        lv_obj_set_style_text_color(lbl_w, UI_COLOR_TEXT_PRI, 0);
        lv_obj_set_style_text_font(lbl_w, UI_FONT_LARGE, 0);
        lv_obj_align(lbl_w, LV_ALIGN_TOP_LEFT, 0, 64);
    }
}
