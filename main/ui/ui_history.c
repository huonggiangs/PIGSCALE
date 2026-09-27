/**
 * @file ui_history.c
 * @brief Màn hình LỊCH SỬ CÂN — danh sách các chuyến tải đã hoàn thành
 *
 * Layout (480×800):
 *  ┌───────────────────┐  h=60  Header (clock, title, icons)
 *  ├───────────────────┤  h=90  Title block (LỊCH SỬ CÂN + Xuất Báo Cáo)
 *  ├───────────────────┤  h=72  Filter bar (ngày / vật liệu / lọc)
 *  ├───────────────────┤  flex  Danh sách scrollable
 *  └───────────────────┘  h=80  Bottom nav
 */

#include "ui_history.h"
#include "ui_theme.h"
#include "ui_nav.h"
#include "esp_log.h"
#include <stdio.h>
#include <time.h>

static const char *TAG = "UI_HISTORY";

static lv_obj_t *s_screen    = NULL;
static lv_obj_t *s_lbl_time  = NULL;
static lv_obj_t *s_lbl_date  = NULL;
static lv_timer_t *s_clock_timer = NULL;

/* ============================================================
 * MOCK DATA
 * ============================================================ */
typedef struct {
    const char *company;
    const char *material;
    const char *plate;
    const char *time_str;   /* "HH:MM" */
    float       total_kg;
} history_record_t;

static const history_record_t k_records[] = {
    { "Cty XD Hòa Bình",    "Đá 1x2",       "51C-123.45",  "14:30",  24500.0f },
    { "VLXD Tuấn Kiệt",     "Cát Xây Dựng", "60A-987.65",  "11:15",  18200.0f },
    { "Tập đoàn VinGroup",  "Đất Đỏ",       "29C-555.22",  "08:45",  32000.0f },
    { "Phú Mỹ Hưng",        "Đá dăm",       "30H-111.33",  "16:20",  15800.0f },
    { "An Phát JSC",         "Cát vàng",     "51D-222.44",  "09:00",  22400.0f },
};
#define RECORD_COUNT ((int)(sizeof(k_records)/sizeof(k_records[0])))

/* ============================================================
 * FORWARD DECLS
 * ============================================================ */
static void build_header(lv_obj_t *parent);
static void build_title_block(lv_obj_t *parent);
static void build_filter_bar(lv_obj_t *parent);
static void build_record_list(lv_obj_t *parent);
static void clock_timer_cb(lv_timer_t *t);

/* ============================================================
 * PUBLIC API
 * ============================================================ */
lv_obj_t *ui_history_get_screen(void) { return s_screen; }

void ui_history_screen_init(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_add_style(s_screen, &style_screen, 0);
    lv_obj_set_size(s_screen, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_scrollbar_mode(s_screen, LV_SCROLLBAR_MODE_OFF);

    build_header(s_screen);
    build_title_block(s_screen);
    build_filter_bar(s_screen);
    build_record_list(s_screen);
    ui_nav_build(s_screen, 2);   /* tab 2 = LỊCH SỬ */

    s_clock_timer = lv_timer_create(clock_timer_cb, 1000, NULL);
    clock_timer_cb(NULL);

    ESP_LOGI(TAG, "History screen initialized");
}

/* ============================================================
 * HEADER (clone từ ui_main)
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
    lv_obj_set_style_text_color(s_lbl_time, UI_COLOR_ACCENT, 0);   /* vàng — nhất quán toàn app */
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
 * TITLE BLOCK — "LỊCH SỬ CÂN" + nút Xuất Báo Cáo
 * ============================================================ */
static void export_cb(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "Xuất báo cáo...");
    /* TODO: xuất CSV/PDF qua SPIFFS rồi gửi qua network */
}

static void build_title_block(lv_obj_t *parent)
{
    int blk_y = UI_HEADER_H + UI_PAD_MD;

    lv_obj_t *blk = lv_obj_create(parent);
    lv_obj_set_size(blk, UI_SCREEN_W - 2*UI_PAD_MD, 90);
    lv_obj_set_pos(blk, UI_PAD_MD, blk_y);
    lv_obj_set_style_bg_opa(blk, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(blk, 0, 0);
    lv_obj_set_style_pad_all(blk, 0, 0);
    lv_obj_set_scrollbar_mode(blk, LV_SCROLLBAR_MODE_OFF);

    /* Tiêu đề lớn bên trái */
    lv_obj_t *lbl_h = lv_label_create(blk);
    lv_label_set_text(lbl_h, "LỊCH SỬ CÂN");
    lv_obj_set_style_text_color(lbl_h, UI_COLOR_TEXT_PRI, 0);
    lv_obj_set_style_text_font(lbl_h, UI_FONT_XLARGE, 0);
    lv_obj_align(lbl_h, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *lbl_sub = lv_label_create(blk);
    lv_label_set_text(lbl_sub, "Xem lại các chuyến tải đã hoàn thành.");
    lv_obj_set_style_text_color(lbl_sub, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(lbl_sub, UI_FONT_SMALL, 0);
    lv_obj_align(lbl_sub, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    /* Nút Xuất Báo Cáo (phải) */
    lv_obj_t *btn_exp = lv_btn_create(blk);
    lv_obj_set_size(btn_exp, LV_SIZE_CONTENT, 46);
    lv_obj_align(btn_exp, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(btn_exp, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_bg_color(btn_exp, UI_COLOR_ACCENT_DIM, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn_exp, 0, 0);
    lv_obj_set_style_radius(btn_exp, UI_RADIUS_SM, 0);
    lv_obj_set_style_pad_hor(btn_exp, UI_PAD_MD, 0);
    lv_obj_add_event_cb(btn_exp, export_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_exp = lv_label_create(btn_exp);
    lv_label_set_text(lbl_exp, LV_SYMBOL_UPLOAD "  Xuất Báo Cáo");
    lv_obj_set_style_text_color(lbl_exp, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_text_font(lbl_exp, UI_FONT_SMALL, 0);
    lv_obj_center(lbl_exp);
}

/* ============================================================
 * FILTER BAR — Ngày / Vật liệu / Lọc
 * ============================================================ */
static void build_filter_bar(lv_obj_t *parent)
{
    int bar_y = UI_HEADER_H + UI_PAD_MD + 90 + UI_PAD_SM;

    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_size(bar, UI_SCREEN_W - 2*UI_PAD_MD, 72);
    lv_obj_set_pos(bar, UI_PAD_MD, bar_y);
    lv_obj_add_style(bar, &style_card, 0);
    lv_obj_set_scrollbar_mode(bar, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_BETWEEN,
                               LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* ── Ô NGÀY ── */
    lv_obj_t *f_date = lv_obj_create(bar);
    lv_obj_set_size(f_date, LV_PCT(38), LV_PCT(80));
    lv_obj_set_style_bg_color(f_date, lv_color_hex(0x2E2E2E), 0);
    lv_obj_set_style_bg_opa(f_date, UI_OPA_FULL, 0);
    lv_obj_set_style_border_color(f_date, UI_COLOR_BORDER, 0);
    lv_obj_set_style_border_width(f_date, 1, 0);
    lv_obj_set_style_radius(f_date, UI_RADIUS_SM, 0);
    lv_obj_set_style_pad_hor(f_date, UI_PAD_SM, 0);
    lv_obj_set_scrollbar_mode(f_date, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *lbl_date_hdr = lv_label_create(f_date);
    lv_label_set_text(lbl_date_hdr, "NGÀY");
    lv_obj_set_style_text_color(lbl_date_hdr, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(lbl_date_hdr, UI_FONT_TINY, 0);
    lv_obj_align(lbl_date_hdr, LV_ALIGN_TOP_LEFT, 0, 2);

    /* Ngày hiện tại */
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char dbuf[24] = "Hôm nay";
    if (tm_info) strftime(dbuf, sizeof(dbuf), "%d/%m/%Y", tm_info);

    lv_obj_t *lbl_date_val = lv_label_create(f_date);
    lv_label_set_text(lbl_date_val, dbuf);
    lv_obj_set_style_text_color(lbl_date_val, UI_COLOR_TEXT_PRI, 0);
    lv_obj_set_style_text_font(lbl_date_val, UI_FONT_TINY, 0);
    lv_obj_align(lbl_date_val, LV_ALIGN_BOTTOM_LEFT, 0, -2);

    lv_obj_t *lbl_cal = lv_label_create(f_date);
    lv_label_set_text(lbl_cal, LV_SYMBOL_DIRECTORY);
    lv_obj_set_style_text_color(lbl_cal, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(lbl_cal, UI_FONT_SMALL, 0);
    lv_obj_align(lbl_cal, LV_ALIGN_LEFT_MID, -2, 0);

    /* ── Ô VẬT LIỆU ── */
    lv_obj_t *f_mat = lv_obj_create(bar);
    lv_obj_set_size(f_mat, LV_PCT(38), LV_PCT(80));
    lv_obj_set_style_bg_color(f_mat, lv_color_hex(0x2E2E2E), 0);
    lv_obj_set_style_bg_opa(f_mat, UI_OPA_FULL, 0);
    lv_obj_set_style_border_color(f_mat, UI_COLOR_BORDER, 0);
    lv_obj_set_style_border_width(f_mat, 1, 0);
    lv_obj_set_style_radius(f_mat, UI_RADIUS_SM, 0);
    lv_obj_set_style_pad_hor(f_mat, UI_PAD_SM, 0);
    lv_obj_set_scrollbar_mode(f_mat, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *lbl_mat_hdr = lv_label_create(f_mat);
    lv_label_set_text(lbl_mat_hdr, "VẬT LIỆU");
    lv_obj_set_style_text_color(lbl_mat_hdr, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(lbl_mat_hdr, UI_FONT_TINY, 0);
    lv_obj_align(lbl_mat_hdr, LV_ALIGN_TOP_LEFT, 0, 2);

    lv_obj_t *lbl_mat_val = lv_label_create(f_mat);
    lv_label_set_text(lbl_mat_val, "Tất cả vật liệu");
    lv_obj_set_style_text_color(lbl_mat_val, UI_COLOR_TEXT_PRI, 0);
    lv_obj_set_style_text_font(lbl_mat_val, UI_FONT_TINY, 0);
    lv_obj_align(lbl_mat_val, LV_ALIGN_BOTTOM_LEFT, 0, -2);

    lv_obj_t *lbl_arr = lv_label_create(f_mat);
    lv_label_set_text(lbl_arr, LV_SYMBOL_DOWN);
    lv_obj_set_style_text_color(lbl_arr, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(lbl_arr, UI_FONT_TINY, 0);
    lv_obj_align(lbl_arr, LV_ALIGN_RIGHT_MID, 0, 0);

    /* ── Nút LỌC ── */
    lv_obj_t *btn_filter = lv_btn_create(bar);
    lv_obj_set_size(btn_filter, LV_PCT(18), LV_PCT(80));
    lv_obj_set_style_bg_color(btn_filter, lv_color_hex(0x2E2E2E), 0);
    lv_obj_set_style_bg_color(btn_filter, lv_color_hex(0x3A3A3A), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(btn_filter, UI_COLOR_BORDER, 0);
    lv_obj_set_style_border_width(btn_filter, 1, 0);
    lv_obj_set_style_radius(btn_filter, UI_RADIUS_SM, 0);

    lv_obj_t *lbl_flt = lv_label_create(btn_filter);
    lv_label_set_text(lbl_flt, LV_SYMBOL_FILTER "  Lọc");
    lv_obj_set_style_text_color(lbl_flt, UI_COLOR_TEXT_PRI, 0);
    lv_obj_set_style_text_font(lbl_flt, UI_FONT_TINY, 0);
    lv_obj_center(lbl_flt);
}

/* ============================================================
 * RECORD LIST — danh sách các lần cân
 * ============================================================ */
static void build_record_list(lv_obj_t *parent)
{
    int list_y = UI_HEADER_H + UI_PAD_MD + 90 + UI_PAD_SM + 72 + UI_PAD_SM;
    int list_h = UI_SCREEN_H - list_y - UI_NAV_H - UI_PAD_SM;

    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, UI_SCREEN_W - 2*UI_PAD_MD, list_h);
    lv_obj_set_pos(cont, UI_PAD_MD, list_y);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(cont, UI_PAD_SM, 0);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_ACTIVE);

    for (int i = 0; i < RECORD_COUNT; i++) {
        const history_record_t *r = &k_records[i];

        /* Row container */
        lv_obj_t *row = lv_obj_create(cont);
        lv_obj_set_size(row, LV_PCT(100), 100);  /* tăng cho 800×1280 */
        lv_obj_add_style(row, &style_card, 0);
        lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_pad_all(row, UI_PAD_SM, 0);

        /* ── Truck icon (trái) ── */
        lv_obj_t *ico_box = lv_obj_create(row);
        lv_obj_set_size(ico_box, 48, 48);
        lv_obj_align(ico_box, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_set_style_bg_color(ico_box, lv_color_hex(0x2E2E2E), 0);
        lv_obj_set_style_bg_opa(ico_box, UI_OPA_FULL, 0);
        lv_obj_set_style_border_color(ico_box, UI_COLOR_BORDER, 0);
        lv_obj_set_style_border_width(ico_box, 1, 0);
        lv_obj_set_style_radius(ico_box, UI_RADIUS_SM, 0);

        lv_obj_t *lbl_ico = lv_label_create(ico_box);
        lv_label_set_text(lbl_ico, LV_SYMBOL_DRIVE);
        lv_obj_set_style_text_color(lbl_ico, UI_COLOR_TEXT_SEC, 0);
        lv_obj_set_style_text_font(lbl_ico, UI_FONT_MEDIUM, 0);
        lv_obj_center(lbl_ico);

        /* ── Info block (giữa) ── */
        lv_obj_t *info = lv_obj_create(row);
        lv_obj_remove_style_all(info);
        lv_obj_set_size(info, LV_PCT(55), LV_PCT(100));
        lv_obj_align(info, LV_ALIGN_LEFT_MID, 56, 0);
        lv_obj_set_flex_flow(info, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(info, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_START);

        /* Company */
        lv_obj_t *lbl_co = lv_label_create(info);
        lv_label_set_text(lbl_co, r->company);
        lv_obj_set_style_text_color(lbl_co, UI_COLOR_TEXT_PRI, 0);
        lv_obj_set_style_text_font(lbl_co, UI_FONT_NORMAL, 0);

        /* Material + BS: plate */
        char sub[48];
        snprintf(sub, sizeof(sub), "%s  •  BS: %s", r->material, r->plate);
        lv_obj_t *lbl_sub = lv_label_create(info);
        lv_label_set_text(lbl_sub, sub);
        lv_obj_set_style_text_color(lbl_sub, UI_COLOR_TEXT_SEC, 0);
        lv_obj_set_style_text_font(lbl_sub, UI_FONT_TINY, 0);

        /* ── Weight block (phải) ── */
        lv_obj_t *wblk = lv_obj_create(row);
        lv_obj_remove_style_all(wblk);
        lv_obj_set_size(wblk, LV_PCT(32), LV_PCT(100));
        lv_obj_align(wblk, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_set_flex_flow(wblk, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(wblk, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END,
                              LV_FLEX_ALIGN_END);

        /* Time */
        lv_obj_t *lbl_time = lv_label_create(wblk);
        lv_label_set_text(lbl_time, r->time_str);
        lv_obj_set_style_text_color(lbl_time, UI_COLOR_TEXT_SEC, 0);
        lv_obj_set_style_text_font(lbl_time, UI_FONT_SMALL, 0);

        /* "TỔNG TRỌNG LƯỢNG" label */
        lv_obj_t *lbl_twl = lv_label_create(wblk);
        lv_label_set_text(lbl_twl, "TỔNG TRỌNG LƯỢNG");
        lv_obj_set_style_text_color(lbl_twl, UI_COLOR_TEXT_SEC, 0);
        lv_obj_set_style_text_font(lbl_twl, UI_FONT_TINY, 0);

        /* Weight value in yellow */
        char wbuf[20];
        long wv = (long)r->total_kg;
        if (wv >= 1000) snprintf(wbuf, sizeof(wbuf), "%ld,%03ld", wv/1000, wv%1000);
        else            snprintf(wbuf, sizeof(wbuf), "%ld", wv);
        lv_obj_t *lbl_wval = lv_label_create(wblk);
        lv_label_set_text(lbl_wval, wbuf);
        lv_obj_set_style_text_color(lbl_wval, UI_COLOR_ACCENT, 0);
        lv_obj_set_style_text_font(lbl_wval, UI_FONT_LARGE, 0);

        /* "KG" suffix nhỏ */
        lv_obj_t *lbl_wunit = lv_label_create(wblk);
        lv_label_set_text(lbl_wunit, "KG");
        lv_obj_set_style_text_color(lbl_wunit, UI_COLOR_TEXT_SEC, 0);
        lv_obj_set_style_text_font(lbl_wunit, UI_FONT_TINY, 0);
    }
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
