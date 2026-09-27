/**
 * @file ui_main.c
 * @brief LVGL UI — CÂN MÁY XÚC V3 (màn hình chính)
 *
 * Màn hình này ánh xạ layout 800×1280 từ preview_screens.html:
 *
 *  ┌─────────────────────────────┐  h=96   Header
 *  ├─────────────────────────────┤  ~130   Order Card
 *  ├─────────────────────────────┤  ~490   Weight Display
 *  ├─────────────────────────────┤  ~192   Stats Row
 *  ├─────────────────────────────┤  ~100   CTA Button
 *  └─────────────────────────────┘  h=128  Bottom Nav (5 tabs)
 *
 * Design tokens:
 *   BG     #1A1A1A   CARD   #242424   ACCENT #F5C800
 *   TEXT   #FFFFFF   SEC    #888888   BORDER #333333
 *   OK     #22C55E   WARN   #EAB308   ERR    #EF4444
 *
 * Lưu ý: Đây là skeleton — thay thế các label/số cứng bằng sensor_hub calls
 *         khi tích hợp phần cứng.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"

#include "ui_main.h"
#include "eth_manager.h"

static const char *TAG = "ui_main";

/* ── Design tokens ─────────────────────────────────────────────────────────── */
#define COLOR_BG          lv_color_hex(0x1A1A1A)
#define COLOR_CARD        lv_color_hex(0x242424)
#define COLOR_ACCENT      lv_color_hex(0xF5C800)
#define COLOR_TEXT_PRI    lv_color_hex(0xFFFFFF)
#define COLOR_TEXT_SEC    lv_color_hex(0x888888)
#define COLOR_BORDER      lv_color_hex(0x333333)
#define COLOR_OK          lv_color_hex(0x22C55E)
#define COLOR_WARN        lv_color_hex(0xEAB308)
#define COLOR_ERR         lv_color_hex(0xEF4444)

/* ── LVGL object handles (cần update từ tasks khác) ────────────────────────── */
static lv_obj_t *s_lbl_weight  = NULL;  /* "0.0" tấn */
static lv_obj_t *s_lbl_unit    = NULL;  /* "TẤN" */
static lv_obj_t *s_bar_prog    = NULL;  /* progress bar */
static lv_obj_t *s_lbl_total   = NULL;  /* tổng chuyến */
static lv_obj_t *s_lbl_target  = NULL;  /* mục tiêu */
static lv_obj_t *s_btn_cta     = NULL;  /* BẮT ĐẦU CÂN */
static lv_obj_t *s_lbl_ip      = NULL;  /* IP Ethernet (góc phải header) */

/* ── Trạng thái UI ─────────────────────────────────────────────────────────── */
typedef enum { UI_IDLE = 0, UI_WEIGHING, UI_COMPLETE } ui_state_t;
static volatile ui_state_t s_ui_state = UI_IDLE;

/* ── Helpers ───────────────────────────────────────────────────────────────── */
static lv_style_t s_style_card;
static lv_style_t s_style_btn_accent;
static lv_style_t s_style_btn_dark;

static void styles_init(void)
{
    /* Card */
    lv_style_init(&s_style_card);
    lv_style_set_bg_color(&s_style_card, COLOR_CARD);
    lv_style_set_bg_opa(&s_style_card, LV_OPA_COVER);
    lv_style_set_border_color(&s_style_card, COLOR_BORDER);
    lv_style_set_border_width(&s_style_card, 1);
    lv_style_set_radius(&s_style_card, 12);
    lv_style_set_pad_all(&s_style_card, 16);

    /* Button accent (vàng) */
    lv_style_init(&s_style_btn_accent);
    lv_style_set_bg_color(&s_style_btn_accent, COLOR_ACCENT);
    lv_style_set_bg_opa(&s_style_btn_accent, LV_OPA_COVER);
    lv_style_set_text_color(&s_style_btn_accent, lv_color_hex(0x1A1A1A));
    lv_style_set_radius(&s_style_btn_accent, 12);
    lv_style_set_shadow_width(&s_style_btn_accent, 0);

    /* Button dark */
    lv_style_init(&s_style_btn_dark);
    lv_style_set_bg_color(&s_style_btn_dark, COLOR_CARD);
    lv_style_set_bg_opa(&s_style_btn_dark, LV_OPA_COVER);
    lv_style_set_border_color(&s_style_btn_dark, COLOR_BORDER);
    lv_style_set_border_width(&s_style_btn_dark, 1);
    lv_style_set_text_color(&s_style_btn_dark, COLOR_TEXT_PRI);
    lv_style_set_radius(&s_style_btn_dark, 12);
    lv_style_set_shadow_width(&s_style_btn_dark, 0);
}

/* ── CTA button event ──────────────────────────────────────────────────────── */
static void btn_cta_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    if (s_ui_state == UI_IDLE) {
        s_ui_state = UI_WEIGHING;
        lv_obj_t *lbl = lv_obj_get_child(s_btn_cta, 0);
        lv_label_set_text(lbl, "RESET");
        lv_obj_remove_style(s_btn_cta, &s_style_btn_accent, 0);
        lv_obj_add_style(s_btn_cta, &s_style_btn_dark, 0);
        ESP_LOGI(TAG, "BẮT ĐẦU CÂN → WEIGHING");
    } else {
        s_ui_state = UI_IDLE;
        lv_label_set_text(s_lbl_weight, "0.0");
        lv_bar_set_value(s_bar_prog, 0, LV_ANIM_ON);
        lv_obj_t *lbl = lv_obj_get_child(s_btn_cta, 0);
        lv_label_set_text(lbl, "BẮT ĐẦU CÂN");
        lv_obj_remove_style(s_btn_cta, &s_style_btn_dark, 0);
        lv_obj_add_style(s_btn_cta, &s_style_btn_accent, 0);
        ESP_LOGI(TAG, "RESET → IDLE");
    }
}

/* ── ui_main_build — tạo toàn bộ LVGL objects ─────────────────────────────── */
static void ui_main_build(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    styles_init();

    /* ═══════════════════════════════════════════════════════
     * HEADER (h=96)
     * ═══════════════════════════════════════════════════════ */
    lv_obj_t *hdr = lv_obj_create(scr);
    lv_obj_set_size(hdr, 800, 96);
    lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(hdr, COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_radius(hdr, 0, 0);
    lv_obj_set_style_pad_hor(hdr, 24, 0);
    lv_obj_set_style_pad_ver(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    /* Title */
    lv_obj_t *lbl_title = lv_label_create(hdr);
    lv_label_set_text(lbl_title, "CAN MAY XUC V3");
    lv_obj_set_style_text_color(lbl_title, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_20, 0);
    lv_obj_align(lbl_title, LV_ALIGN_LEFT_MID, 0, 0);

    /* IP label (góc phải) */
    s_lbl_ip = lv_label_create(hdr);
    lv_label_set_text(s_lbl_ip, "ETH: ---.---.---.---");
    lv_obj_set_style_text_color(s_lbl_ip, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(s_lbl_ip, &lv_font_montserrat_14, 0);
    lv_obj_align(s_lbl_ip, LV_ALIGN_RIGHT_MID, 0, 0);

    /* ═══════════════════════════════════════════════════════
     * ORDER CARD (h=130, y=96)
     * ═══════════════════════════════════════════════════════ */
    lv_obj_t *ord = lv_obj_create(scr);
    lv_obj_set_size(ord, 752, 118);
    lv_obj_align(ord, LV_ALIGN_TOP_MID, 0, 104);
    lv_obj_add_style(ord, &s_style_card, 0);
    lv_obj_clear_flag(ord, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_ord_num = lv_label_create(ord);
    lv_label_set_text(lbl_ord_num, "DON HANG: #DH-001");
    lv_obj_set_style_text_color(lbl_ord_num, COLOR_TEXT_PRI, 0);
    lv_obj_set_style_text_font(lbl_ord_num, &lv_font_montserrat_18, 0);
    lv_obj_align(lbl_ord_num, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *lbl_ord_co = lv_label_create(ord);
    lv_label_set_text(lbl_ord_co, "CONG TY: ---");
    lv_obj_set_style_text_color(lbl_ord_co, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(lbl_ord_co, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_ord_co, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_obj_t *lbl_ord_mat = lv_label_create(ord);
    lv_label_set_text(lbl_ord_mat, "VAT LIEU: ---");
    lv_obj_set_style_text_color(lbl_ord_mat, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(lbl_ord_mat, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_ord_mat, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    /* ═══════════════════════════════════════════════════════
     * WEIGHT DISPLAY (h=490, y=242)
     * ═══════════════════════════════════════════════════════ */
    lv_obj_t *wgt = lv_obj_create(scr);
    lv_obj_set_size(wgt, 752, 478);
    lv_obj_align(wgt, LV_ALIGN_TOP_MID, 0, 234);
    lv_obj_add_style(wgt, &s_style_card, 0);
    lv_obj_clear_flag(wgt, LV_OBJ_FLAG_SCROLLABLE);

    /* Số cân lớn */
    s_lbl_weight = lv_label_create(wgt);
    lv_label_set_text(s_lbl_weight, "0.0");
    lv_obj_set_style_text_color(s_lbl_weight, COLOR_TEXT_PRI, 0);
    lv_obj_set_style_text_font(s_lbl_weight, &lv_font_montserrat_48, 0);
    lv_obj_align(s_lbl_weight, LV_ALIGN_CENTER, 0, -40);

    s_lbl_unit = lv_label_create(wgt);
    lv_label_set_text(s_lbl_unit, "TAN");
    lv_obj_set_style_text_color(s_lbl_unit, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(s_lbl_unit, &lv_font_montserrat_20, 0);
    lv_obj_align(s_lbl_unit, LV_ALIGN_CENTER, 0, 30);

    /* Progress bar */
    s_bar_prog = lv_bar_create(wgt);
    lv_obj_set_size(s_bar_prog, 680, 20);
    lv_obj_align(s_bar_prog, LV_ALIGN_BOTTOM_MID, 0, -24);
    lv_bar_set_range(s_bar_prog, 0, 100);
    lv_bar_set_value(s_bar_prog, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_bar_prog, COLOR_BORDER, 0);
    lv_obj_set_style_bg_color(s_bar_prog, COLOR_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_bar_prog, 10, 0);
    lv_obj_set_style_radius(s_bar_prog, 10, LV_PART_INDICATOR);

    /* ═══════════════════════════════════════════════════════
     * STATS ROW (h=192, y=734)
     * ═══════════════════════════════════════════════════════ */
    lv_obj_t *stats = lv_obj_create(scr);
    lv_obj_set_size(stats, 752, 180);
    lv_obj_align(stats, LV_ALIGN_TOP_MID, 0, 724);
    lv_obj_set_style_bg_color(stats, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(stats, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(stats, 0, 0);
    lv_obj_set_style_radius(stats, 0, 0);
    lv_obj_set_style_pad_hor(stats, 0, 0);
    lv_obj_clear_flag(stats, LV_OBJ_FLAG_SCROLLABLE);

    /* Tổng chuyến */
    lv_obj_t *card_total = lv_obj_create(stats);
    lv_obj_set_size(card_total, 360, 168);
    lv_obj_align(card_total, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_style(card_total, &s_style_card, 0);
    lv_obj_clear_flag(card_total, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_tot_hdr = lv_label_create(card_total);
    lv_label_set_text(lbl_tot_hdr, "TONG CHUYEN");
    lv_obj_set_style_text_color(lbl_tot_hdr, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(lbl_tot_hdr, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_tot_hdr, LV_ALIGN_TOP_LEFT, 0, 0);

    s_lbl_total = lv_label_create(card_total);
    lv_label_set_text(s_lbl_total, "0");
    lv_obj_set_style_text_color(s_lbl_total, COLOR_TEXT_PRI, 0);
    lv_obj_set_style_text_font(s_lbl_total, &lv_font_montserrat_48, 0);
    lv_obj_align(s_lbl_total, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    /* Mục tiêu */
    lv_obj_t *card_tgt = lv_obj_create(stats);
    lv_obj_set_size(card_tgt, 360, 168);
    lv_obj_align(card_tgt, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_style(card_tgt, &s_style_card, 0);
    lv_obj_clear_flag(card_tgt, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_tgt_hdr = lv_label_create(card_tgt);
    lv_label_set_text(lbl_tgt_hdr, "MUC TIEU");
    lv_obj_set_style_text_color(lbl_tgt_hdr, COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(lbl_tgt_hdr, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_tgt_hdr, LV_ALIGN_TOP_LEFT, 0, 0);

    s_lbl_target = lv_label_create(card_tgt);
    lv_label_set_text(s_lbl_target, "100");
    lv_obj_set_style_text_color(s_lbl_target, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(s_lbl_target, &lv_font_montserrat_48, 0);
    lv_obj_align(s_lbl_target, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    /* ═══════════════════════════════════════════════════════
     * CTA BUTTON (h=100, y=916)
     * ═══════════════════════════════════════════════════════ */
    s_btn_cta = lv_button_create(scr);
    lv_obj_set_size(s_btn_cta, 752, 88);
    lv_obj_align(s_btn_cta, LV_ALIGN_TOP_MID, 0, 908);
    lv_obj_add_style(s_btn_cta, &s_style_btn_accent, 0);
    lv_obj_add_event_cb(s_btn_cta, btn_cta_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_cta = lv_label_create(s_btn_cta);
    lv_label_set_text(lbl_cta, "BAT DAU CAN");
    lv_obj_set_style_text_font(lbl_cta, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl_cta);

    /* ═══════════════════════════════════════════════════════
     * BOTTOM NAV (h=128, y=1008~1152 → bottom)
     * ═══════════════════════════════════════════════════════ */
    lv_obj_t *nav = lv_obj_create(scr);
    lv_obj_set_size(nav, 800, 120);
    lv_obj_align(nav, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(nav, COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(nav, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(nav, 0, 0);
    lv_obj_set_style_radius(nav, 0, 0);
    lv_obj_set_style_pad_hor(nav, 0, 0);
    lv_obj_set_style_pad_ver(nav, 0, 0);
    lv_obj_set_layout(nav, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(nav, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(nav, LV_FLEX_ALIGN_SPACE_AROUND,
                               LV_FLEX_ALIGN_CENTER,
                               LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(nav, LV_OBJ_FLAG_SCROLLABLE);

    /* 5 tab labels — tab đầu (CAN) active = vàng */
    static const char *tab_names[] = {"CAN", "DON\nHANG", "LICH\nSU", "BAO\nCAO", "CAI\nDAT"};
    for (int i = 0; i < 5; i++) {
        lv_obj_t *btn = lv_button_create(nav);
        lv_obj_set_size(btn, 140, 100);
        lv_obj_set_style_bg_color(btn, COLOR_CARD, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 0, 0);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, tab_names[i]);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(lbl, (i == 0) ? COLOR_ACCENT : COLOR_TEXT_SEC, 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_center(lbl);
    }

    ESP_LOGI(TAG, "UI build xong");
}

/* ── IP update timer (gọi mỗi 2s) ─────────────────────────────────────────── */
static void update_ip_label_cb(lv_timer_t *tmr)
{
    eth_ip_info_t info;
    char buf[48];
    if (eth_manager_get_ip(&info) == ESP_OK) {
        snprintf(buf, sizeof(buf), "ETH: %s", info.ip);
    } else {
        snprintf(buf, sizeof(buf), "ETH: ---.---.---.---");
    }
    lv_label_set_text(s_lbl_ip, buf);
}

/* ── ui_main_start ─────────────────────────────────────────────────────────── */
void ui_main_start(void)
{
    ESP_LOGI(TAG, "Khởi tạo UI trong LVGL task...");

    lvgl_port_lock(0);
    ui_main_build();
    /* Timer cập nhật IP mỗi 2 giây */
    lv_timer_create(update_ip_label_cb, 2000, NULL);
    lvgl_port_unlock();

    ESP_LOGI(TAG, "UI LVGL sẵn sàng");
}
