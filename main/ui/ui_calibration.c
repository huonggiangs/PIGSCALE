/**
 * @file ui_calibration.c
 * @brief Màn hình HIỆU CHUẨN
 *
 * Luồng:
 *   CÀI ĐẶT → tap "Hiệu chuẩn"
 *     → PIN modal (6 ký tự, mặc định "6688")
 *       → PIN đúng → load màn hình HIỆU CHUẨN
 *
 * Màn hình HIỆU CHUẨN:
 *   ─ Header: ← Quay lại | tiêu đề | nút cảnh báo
 *   ─ Tab chọn loại: XÚC GẦU (6 AP+4 GÓC) | XÚC LẠT (2 AP+2 GÓC+1 TC)
 *   ─ Cột trái: Tare card + Angle sync card
 *   ─ Cột phải: Ma trận cảm biến (P1–P6 / A1–A4 / PR1)
 *   ─ Nút lưu: LƯU KẾT QUẢ HIỆU CHUẨN
 *   ─ Sub-nav: CẤU HÌNH | MẠNG | HIỆU CHUẨN | THÔNG TIN
 */

#include "ui_calibration.h"
#include "ui_theme.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

static const char *TAG = "UI_CALIB";

/* ---- PIN ---- */
#define CALIB_PIN_DEFAULT  "6688"
#define CALIB_PIN_LEN      4

/* ---- Màn hình ---- */
static lv_obj_t *s_screen        = NULL;
static lv_obj_t *s_pin_overlay   = NULL;

/* ---- Sub-nav active ---- */
static int s_subnav_active = 2;  /* 0=CẤU HÌNH, 1=MẠNG, 2=HIỆU CHUẨN, 3=THÔNG TIN */

/* ---- Machine type ---- */
#define MACHINE_GAU  0   /* XÚC GẦU  — 6 AP + 4 GÓC */
#define MACHINE_LAT  1   /* XÚC LẠT  — 2 AP + 2 GÓC + 1 TC */
static int s_machine = MACHINE_GAU;

/* ---- PIN numpad state ---- */
static char     s_pin_buf[CALIB_PIN_LEN + 1] = "";
static lv_obj_t *s_pin_display = NULL;  /* label hiển thị dấu chấm */
static lv_obj_t *s_pin_err_lbl = NULL;  /* label lỗi */

/* ---- Sensor matrix labels (để update khi đổi tab) ---- */
#define MAX_SENSOR_ROWS 10
static lv_obj_t *s_sensor_val[MAX_SENSOR_ROWS];
static int       s_sensor_count = 0;

/* ---- Scrollable container chính ---- */
static lv_obj_t *s_content_scroll = NULL;

/* ============================================================
 * SENSOR DATA — mock, thực tế từ driver
 * ============================================================ */
typedef struct {
    const char *group;   /* "ÁP SUẤT (BAR)", "GÓC (ĐỘ)", "TIỆM CẬN" — NULL = liên tiếp */
    const char *name;    /* "P1", "A1", "PR1" */
    float       value;
    bool        warning; /* true = hiển thị màu vàng */
    bool        text_val;/* true = hiển thị như text (VD: "ACTIVE") */
    const char *text;    /* nội dung nếu text_val */
} sensor_row_t;

/* XÚC GẦU — 6 AP + 4 GÓC */
static const sensor_row_t k_sensors_gau[] = {
    { "ÁP SUẤT (BAR)", "P1", 145.2f, false, false, NULL },
    { NULL,            "P2", 144.8f, false, false, NULL },
    { NULL,            "P3",  12.4f, true,  false, NULL },  /* cảnh báo */
    { NULL,            "P4", 146.0f, false, false, NULL },
    { NULL,            "P5", 142.5f, false, false, NULL },
    { NULL,            "P6", 138.9f, false, false, NULL },
    { "GÓC (ĐỘ)",     "A1",  45.5f, false, false, NULL },
    { NULL,            "A2", -12.0f, false, false, NULL },
    { NULL,            "A3",  15.2f, false, false, NULL },
    { NULL,            "A4",   8.4f, false, false, NULL },
};
#define SENSORS_GAU_COUNT 10

/* XÚC LẠT — 2 AP + 2 GÓC + 1 TC */
static const sensor_row_t k_sensors_lat[] = {
    { "ÁP SUẤT (BAR)", "P1", 148.3f, false, false, NULL },
    { NULL,            "P2", 147.1f, false, false, NULL },
    { "GÓC (ĐỘ)",     "A1",  44.8f, false, false, NULL },
    { NULL,            "A2", -11.2f, false, false, NULL },
    { "TIỆM CẬN",     "PR1",  0.0f, false, true, "ACTIVE" },
};
#define SENSORS_LAT_COUNT 5

/* ============================================================
 * FORWARD DECLARATIONS
 * ============================================================ */
static void build_screen(void);
static void build_header(lv_obj_t *parent);
static void build_machine_tabs(lv_obj_t *parent, int *out_y);
static void build_body(lv_obj_t *parent, int body_y, int body_h);
static void build_tare_card(lv_obj_t *parent);
static void build_angle_card(lv_obj_t *parent);
static void build_sensor_matrix(lv_obj_t *parent);
static void build_save_btn_at(lv_obj_t *parent, int y);
static void build_sub_nav(lv_obj_t *parent);
static void refresh_sensor_matrix(void);

static void pin_key_cb(lv_event_t *e);
static void pin_del_cb(lv_event_t *e);
static void pin_cancel_cb(lv_event_t *e);
static void pin_confirm_cb(lv_event_t *e);
static void machine_tab_cb(lv_event_t *e);
static void save_cb(lv_event_t *e);
static void back_cb(lv_event_t *e);

/* ============================================================
 * PUBLIC API
 * ============================================================ */
lv_obj_t *ui_calibration_get_screen(void) { return s_screen; }

void ui_calibration_screen_init(void)
{
    if (s_screen) return;   /* already initialized */
    build_screen();
    ESP_LOGI(TAG, "Calibration screen initialized");
}

void ui_calibration_show_pin(lv_obj_t *parent)
{
    if (s_pin_overlay) return;

    strncpy(s_pin_buf, "", sizeof(s_pin_buf));

    /* ── Overlay ── */
    s_pin_overlay = lv_obj_create(parent);
    lv_obj_set_size(s_pin_overlay, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_pos(s_pin_overlay, 0, 0);
    lv_obj_set_style_bg_color(s_pin_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_pin_overlay, LV_OPA_80, 0);
    lv_obj_set_style_border_width(s_pin_overlay, 0, 0);
    lv_obj_set_style_radius(s_pin_overlay, 0, 0);
    lv_obj_set_scrollbar_mode(s_pin_overlay, LV_SCROLLBAR_MODE_OFF);

    /* ── Dialog ── */
    lv_obj_t *dlg = lv_obj_create(s_pin_overlay);
    lv_obj_set_size(dlg, 520, LV_SIZE_CONTENT);
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

    /* Icon + Title */
    lv_obj_t *ttl = lv_label_create(dlg);
    lv_label_set_text(ttl, LV_SYMBOL_KEY "  NHẬP MÃ PIN");
    lv_obj_set_style_text_color(ttl, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(ttl, UI_FONT_MEDIUM, 0);

    lv_obj_t *sub = lv_label_create(dlg);
    lv_label_set_text(sub, "Cần xác thực để truy cập Hiệu chuẩn");
    lv_obj_set_style_text_color(sub, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(sub, UI_FONT_SMALL, 0);

    /* PIN dots display */
    lv_obj_t *dot_row = lv_obj_create(dlg);
    lv_obj_remove_style_all(dot_row);
    lv_obj_set_size(dot_row, LV_PCT(100), 60);
    lv_obj_set_flex_flow(dot_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dot_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(dot_row, 16, 0);

    s_pin_display = lv_label_create(dot_row);
    lv_label_set_text(s_pin_display, "○  ○  ○  ○");
    lv_obj_set_style_text_color(s_pin_display, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(s_pin_display, UI_FONT_LARGE, 0);

    /* Error label */
    s_pin_err_lbl = lv_label_create(dlg);
    lv_label_set_text(s_pin_err_lbl, "");
    lv_obj_set_style_text_color(s_pin_err_lbl, lv_color_hex(0xEF4444), 0);
    lv_obj_set_style_text_font(s_pin_err_lbl, UI_FONT_SMALL, 0);

    /* Numpad grid */
    lv_obj_t *grid = lv_obj_create(dlg);
    lv_obj_remove_style_all(grid);
    lv_obj_set_size(grid, LV_PCT(100), LV_SIZE_CONTENT);

    static lv_coord_t cols[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t rows[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT,
                                 LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(grid, cols, rows);
    lv_obj_set_style_pad_column(grid, UI_PAD_SM, 0);
    lv_obj_set_style_pad_row(grid, UI_PAD_SM, 0);

    static const char *digits[] = {"1","2","3","4","5","6","7","8","9",".", "0"};
    static const int dcols[] = {0,1,2,0,1,2,0,1,2,0,1};
    static const int drows[] = {0,0,0,1,1,1,2,2,2,3,3};

    lv_color_t btn_bg = lv_color_hex(0x2E2E2E);

    for (int i = 0; i < 11; i++) {
        lv_obj_t *b = lv_btn_create(grid);
        lv_obj_set_grid_cell(b, LV_GRID_ALIGN_STRETCH, dcols[i], 1,
                                LV_GRID_ALIGN_CENTER,  drows[i], 1);
        lv_obj_set_style_bg_color(b, btn_bg, 0);
        lv_obj_set_style_border_width(b, 0, 0);
        lv_obj_set_style_radius(b, UI_RADIUS_SM, 0);
        lv_obj_set_style_pad_ver(b, 14, 0);
        lv_obj_add_event_cb(b, pin_key_cb, LV_EVENT_CLICKED, (void *)digits[i]);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, digits[i]);
        lv_obj_set_style_text_color(l, UI_COLOR_TEXT_PRI, 0);
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
    lv_obj_set_style_pad_ver(bdel, 14, 0);
    lv_obj_add_event_cb(bdel, pin_del_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *dl = lv_label_create(bdel);
    lv_label_set_text(dl, LV_SYMBOL_BACKSPACE);
    lv_obj_set_style_text_color(dl, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(dl, UI_FONT_MEDIUM, 0);
    lv_obj_center(dl);

    /* HỦY + XÁC NHẬN */
    lv_obj_t *btn_row = lv_obj_create(dlg);
    lv_obj_remove_style_all(btn_row);
    lv_obj_set_size(btn_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *b_cancel = lv_btn_create(btn_row);
    lv_obj_set_flex_grow(b_cancel, 1);
    lv_obj_set_height(b_cancel, 60);
    lv_obj_set_style_bg_color(b_cancel, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(b_cancel, 0, 0);
    lv_obj_set_style_radius(b_cancel, UI_RADIUS_SM, 0);
    lv_obj_add_event_cb(b_cancel, pin_cancel_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lc = lv_label_create(b_cancel);
    lv_label_set_text(lc, "HỦY");
    lv_obj_set_style_text_color(lc, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(lc, UI_FONT_MEDIUM, 0);
    lv_obj_center(lc);

    /* Spacer */
    lv_obj_t *sp = lv_obj_create(btn_row);
    lv_obj_remove_style_all(sp);
    lv_obj_set_size(sp, UI_PAD_SM, 1);

    lv_obj_t *b_ok = lv_btn_create(btn_row);
    lv_obj_set_flex_grow(b_ok, 2);
    lv_obj_set_height(b_ok, 60);
    lv_obj_set_style_bg_color(b_ok, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_bg_color(b_ok, UI_COLOR_ACCENT_DIM, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(b_ok, 0, 0);
    lv_obj_set_style_radius(b_ok, UI_RADIUS_SM, 0);
    lv_obj_add_event_cb(b_ok, pin_confirm_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lok = lv_label_create(b_ok);
    lv_label_set_text(lok, LV_SYMBOL_OK "  XÁC NHẬN");
    lv_obj_set_style_text_color(lok, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_text_font(lok, UI_FONT_MEDIUM, 0);
    lv_obj_center(lok);
}

/* ============================================================
 * PIN CALLBACKS
 * ============================================================ */
static void pin_refresh_dots(void)
{
    if (!s_pin_display) return;
    size_t len = strlen(s_pin_buf);
    char dots[32] = "";
    for (int i = 0; i < CALIB_PIN_LEN; i++) {
        if (i > 0) strncat(dots, "  ", sizeof(dots)-strlen(dots)-1);
        strncat(dots, i < (int)len ? "●" : "○", sizeof(dots)-strlen(dots)-1);
    }
    lv_label_set_text(s_pin_display, dots);
}

static void pin_key_cb(lv_event_t *e)
{
    const char *digit = (const char *)lv_event_get_user_data(e);
    size_t len = strlen(s_pin_buf);
    if (len < CALIB_PIN_LEN && digit[0] >= '0' && digit[0] <= '9') {
        s_pin_buf[len]   = digit[0];
        s_pin_buf[len+1] = '\0';
    }
    if (s_pin_err_lbl) lv_label_set_text(s_pin_err_lbl, "");
    pin_refresh_dots();
}

static void pin_del_cb(lv_event_t *e)
{
    (void)e;
    size_t len = strlen(s_pin_buf);
    if (len > 0) s_pin_buf[len-1] = '\0';
    pin_refresh_dots();
}

static void pin_cancel_cb(lv_event_t *e)
{
    (void)e;
    if (s_pin_overlay) { lv_obj_del(s_pin_overlay); s_pin_overlay = NULL; }
    strncpy(s_pin_buf, "", sizeof(s_pin_buf));
}

static void pin_confirm_cb(lv_event_t *e)
{
    (void)e;
    if (strcmp(s_pin_buf, CALIB_PIN_DEFAULT) == 0) {
        /* PIN đúng → load màn hình hiệu chuẩn */
        lv_obj_t *overlay_parent = lv_obj_get_parent(s_pin_overlay);
        if (s_pin_overlay) { lv_obj_del(s_pin_overlay); s_pin_overlay = NULL; }
        strncpy(s_pin_buf, "", sizeof(s_pin_buf));

        /* Khởi tạo màn hình nếu chưa có */
        if (!s_screen) build_screen();
        lv_scr_load_anim(s_screen, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, false);
        (void)overlay_parent;
    } else {
        /* PIN sai */
        if (s_pin_err_lbl) lv_label_set_text(s_pin_err_lbl, "Mã PIN không đúng. Vui lòng thử lại.");
        strncpy(s_pin_buf, "", sizeof(s_pin_buf));
        pin_refresh_dots();
        ESP_LOGW(TAG, "PIN sai");
    }
}

/* ============================================================
 * BUILD CALIBRATION SCREEN
 * ============================================================ */
static void build_screen(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_add_style(s_screen, &style_screen, 0);
    lv_obj_set_size(s_screen, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_scrollbar_mode(s_screen, LV_SCROLLBAR_MODE_OFF);

    build_header(s_screen);

    /* Y sau header */
    int body_y = UI_HEADER_H + UI_PAD_SM;

    /* Machine tabs */
    int tab_h = 80 + UI_PAD_MD + UI_PAD_SM;
    build_machine_tabs(s_screen, &body_y);

    /* Body scrollable */
    int body_h = UI_SCREEN_H - body_y - 80 - UI_NAV_H - UI_PAD_SM;  /* 80 = save btn */

    s_content_scroll = lv_obj_create(s_screen);
    lv_obj_set_size(s_content_scroll, UI_SCREEN_W - 2*UI_PAD_MD, body_h);
    lv_obj_set_pos(s_content_scroll, UI_PAD_MD, body_y);
    lv_obj_set_style_bg_opa(s_content_scroll, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_content_scroll, 0, 0);
    lv_obj_set_style_pad_all(s_content_scroll, 0, 0);
    lv_obj_set_scrollbar_mode(s_content_scroll, LV_SCROLLBAR_MODE_ACTIVE);

    build_body(s_content_scroll, 0, body_h);
    (void)tab_h;

    /* Save button */
    int save_y = UI_SCREEN_H - UI_NAV_H - 80 - UI_PAD_SM;
    build_save_btn_at(s_screen, save_y);

    build_sub_nav(s_screen);
}

static void build_save_btn_at(lv_obj_t *parent, int y)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, UI_SCREEN_W - 2*UI_PAD_MD, 72);
    lv_obj_set_pos(btn, UI_PAD_MD, y);
    lv_obj_set_style_bg_color(btn, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_bg_color(btn, UI_COLOR_ACCENT_DIM, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, UI_RADIUS_MD, 0);
    lv_obj_add_event_cb(btn, save_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, LV_SYMBOL_SAVE "  LƯU KẾT QUẢ HIỆU CHUẨN");
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_text_font(lbl, UI_FONT_MEDIUM, 0);
    lv_obj_center(lbl);
}

/* ============================================================
 * HEADER — ← Quay lại + tiêu đề + cảnh báo
 * ============================================================ */
static void build_header(lv_obj_t *parent)
{
    lv_obj_t *hdr = lv_obj_create(parent);
    lv_obj_set_size(hdr, UI_SCREEN_W, UI_HEADER_H);
    lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(hdr, UI_COLOR_BG, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_radius(hdr, 0, 0);
    lv_obj_set_style_pad_hor(hdr, UI_PAD_MD, 0);
    lv_obj_set_style_pad_ver(hdr, UI_PAD_SM, 0);

    /* ← Quay lại */
    lv_obj_t *btn_back = lv_btn_create(hdr);
    lv_obj_set_size(btn_back, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btn_back, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_back, 0, 0);
    lv_obj_set_style_shadow_width(btn_back, 0, 0);
    lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_event_cb(btn_back, back_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, LV_SYMBOL_LEFT "  Quay lại");
    lv_obj_set_style_text_color(lbl_back, UI_COLOR_TEXT_PRI, 0);
    lv_obj_set_style_text_font(lbl_back, UI_FONT_MEDIUM, 0);
    lv_obj_center(lbl_back);

    /* Divider dưới header */
    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_set_size(line, UI_SCREEN_W, 1);
    lv_obj_set_pos(line, 0, UI_HEADER_H);
    lv_obj_set_style_bg_color(line, UI_COLOR_BORDER, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_radius(line, 0, 0);
}

/* ============================================================
 * MACHINE TABS — XÚC GẦU / XÚC LẠT
 * ============================================================ */
static lv_obj_t *s_tab_btns[2];

static void machine_tab_cb(lv_event_t *e)
{
    int *idx = (int *)lv_event_get_user_data(e);
    s_machine = *idx;

    /* Update tab button colors */
    for (int i = 0; i < 2; i++) {
        bool act = (i == s_machine);
        lv_obj_set_style_bg_color(s_tab_btns[i],
            act ? UI_COLOR_ACCENT : lv_color_hex(0x2A2A2A), 0);
        lv_obj_t *lbl = lv_obj_get_child(s_tab_btns[i], 0);
        if (lbl) lv_obj_set_style_text_color(lbl,
            act ? lv_color_hex(0x1A1A1A) : UI_COLOR_TEXT_SEC, 0);
    }

    /* Rebuild body content */
    if (s_content_scroll) {
        lv_obj_clean(s_content_scroll);
        build_body(s_content_scroll, 0, lv_obj_get_height(s_content_scroll));
    }
}

static int s_tab_idx[2] = {0, 1};

static void build_machine_tabs(lv_obj_t *parent, int *out_y)
{
    int tab_area_y = UI_HEADER_H + UI_PAD_MD;

    /* Label "LOẠI MÁY XÚC" */
    lv_obj_t *lbl_type = lv_label_create(parent);
    lv_label_set_text(lbl_type, "LOẠI MÁY XÚC");
    lv_obj_set_style_text_color(lbl_type, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(lbl_type, UI_FONT_TINY, 0);
    lv_obj_set_pos(lbl_type, UI_PAD_MD, tab_area_y);

    tab_area_y += 28;

    /* Tab container */
    lv_obj_t *tabs = lv_obj_create(parent);
    lv_obj_set_size(tabs, UI_SCREEN_W - 2*UI_PAD_MD, 80);
    lv_obj_set_pos(tabs, UI_PAD_MD, tab_area_y);
    lv_obj_remove_style_all(tabs);
    lv_obj_set_style_pad_column(tabs, 0, 0);
    lv_obj_set_flex_flow(tabs, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tabs, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(tabs, lv_color_hex(0x1E1E1E), 0);
    lv_obj_set_style_bg_opa(tabs, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(tabs, UI_RADIUS_SM, 0);
    lv_obj_set_style_border_color(tabs, UI_COLOR_BORDER, 0);
    lv_obj_set_style_border_width(tabs, 1, 0);
    lv_obj_set_scrollbar_mode(tabs, LV_SCROLLBAR_MODE_OFF);

    typedef struct { const char *line1; const char *line2; } tab_label_t;
    static const tab_label_t k_tabs[2] = {
        { "XÚC GẦU",   "6 AP + 4 GÓC"       },
        { "XÚC LẠT",   "2 AP + 2 GÓC + 1 TC" },
    };

    for (int i = 0; i < 2; i++) {
        lv_obj_t *btn = lv_btn_create(tabs);
        lv_obj_set_flex_grow(btn, 1);
        lv_obj_set_height(btn, 80);
        lv_obj_set_style_bg_color(btn,
            i == s_machine ? UI_COLOR_ACCENT : lv_color_hex(0x2A2A2A), 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 0, 0);
        lv_obj_add_event_cb(btn, machine_tab_cb, LV_EVENT_CLICKED, &s_tab_idx[i]);
        s_tab_btns[i] = btn;

        lv_obj_t *col = lv_obj_create(btn);
        lv_obj_remove_style_all(col);
        lv_obj_set_size(col, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_center(col);
        lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t *l1 = lv_label_create(col);
        lv_label_set_text(l1, k_tabs[i].line1);
        lv_obj_set_style_text_color(l1,
            i == s_machine ? lv_color_hex(0x1A1A1A) : UI_COLOR_TEXT_PRI, 0);
        lv_obj_set_style_text_font(l1, UI_FONT_MEDIUM, 0);

        lv_obj_t *l2 = lv_label_create(col);
        lv_label_set_text(l2, k_tabs[i].line2);
        lv_obj_set_style_text_color(l2,
            i == s_machine ? lv_color_hex(0x1A1A1A) : UI_COLOR_TEXT_SEC, 0);
        lv_obj_set_style_text_font(l2, UI_FONT_TINY, 0);
    }

    *out_y = tab_area_y + 80 + UI_PAD_MD;
}

/* ============================================================
 * BODY — 2 cột: trái (tare+angle) + phải (sensor matrix)
 * ============================================================ */
static void build_body(lv_obj_t *parent, int body_y, int body_h)
{
    int col_w = (lv_obj_get_width(parent) - UI_PAD_SM) / 2;
    (void)body_h;

    /* Cột trái */
    lv_obj_t *left = lv_obj_create(parent);
    lv_obj_set_size(left, col_w, LV_SIZE_CONTENT);
    lv_obj_set_pos(left, 0, body_y);
    lv_obj_remove_style_all(left);
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(left, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(left, UI_PAD_SM, 0);

    build_tare_card(left);
    build_angle_card(left);

    /* Cột phải */
    lv_obj_t *right = lv_obj_create(parent);
    lv_obj_set_size(right, col_w, LV_SIZE_CONTENT);
    lv_obj_set_pos(right, col_w + UI_PAD_SM, body_y);
    lv_obj_remove_style_all(right);

    build_sensor_matrix(right);
}

/* ============================================================
 * TARE CARD — ĐƯA CẢM BIẾN TẠI VỀ 0
 * ============================================================ */
static void build_tare_card(lv_obj_t *parent)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_add_style(card, &style_card, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_pad_all(card, UI_PAD_MD, 0);
    lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(card, UI_PAD_SM, 0);

    /* Header row: icon + title + badge */
    lv_obj_t *hdr_row = lv_obj_create(card);
    lv_obj_remove_style_all(hdr_row);
    lv_obj_set_size(hdr_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(hdr_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hdr_row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *title_col = lv_obj_create(hdr_row);
    lv_obj_remove_style_all(title_col);
    lv_obj_set_size(title_col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(title_col, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title_col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(title_col, UI_PAD_SM, 0);

    lv_obj_t *ico = lv_label_create(title_col);
    lv_label_set_text(ico, LV_SYMBOL_CHARGE);
    lv_obj_set_style_text_color(ico, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(ico, UI_FONT_MEDIUM, 0);

    lv_obj_t *ttl_col2 = lv_obj_create(title_col);
    lv_obj_remove_style_all(ttl_col2);
    lv_obj_set_size(ttl_col2, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(ttl_col2, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *ttl = lv_label_create(ttl_col2);
    lv_label_set_text(ttl, "ĐƯA CẢM BIẾN TẠI VỀ 0");
    lv_obj_set_style_text_color(ttl, UI_COLOR_TEXT_PRI, 0);
    lv_obj_set_style_text_font(ttl, UI_FONT_SMALL, 0);

    lv_obj_t *sub = lv_label_create(ttl_col2);
    lv_label_set_text(sub, "Trừ bì trọng lượng gầu rỗng");
    lv_obj_set_style_text_color(sub, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(sub, UI_FONT_TINY, 0);

    /* Badge SẴN SÀNG */
    lv_obj_t *badge = lv_obj_create(hdr_row);
    lv_obj_set_size(badge, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(badge, lv_color_hex(0x166534), 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(badge, 0, 0);
    lv_obj_set_style_radius(badge, UI_RADIUS_SM, 0);
    lv_obj_set_style_pad_hor(badge, 10, 0);
    lv_obj_set_style_pad_ver(badge, 4, 0);
    lv_obj_t *bdg_l = lv_label_create(badge);
    lv_label_set_text(bdg_l, "SẴN SÀNG");
    lv_obj_set_style_text_color(bdg_l, lv_color_hex(0x4ADE80), 0);
    lv_obj_set_style_text_font(bdg_l, UI_FONT_TINY, 0);
    lv_obj_center(bdg_l);

    /* KQ hiện tại → Mục tiêu */
    lv_obj_t *kq_row = lv_obj_create(card);
    lv_obj_remove_style_all(kq_row);
    lv_obj_set_size(kq_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(kq_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(kq_row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *kq_l = lv_obj_create(kq_row);
    lv_obj_remove_style_all(kq_l);
    lv_obj_set_flex_flow(kq_l, LV_FLEX_FLOW_COLUMN);
    lv_obj_t *kq_h = lv_label_create(kq_l);
    lv_label_set_text(kq_h, "KẾT QUẢ HIỆN TẠI");
    lv_obj_set_style_text_color(kq_h, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(kq_h, UI_FONT_TINY, 0);
    lv_obj_t *kq_v = lv_label_create(kq_l);
    lv_label_set_text(kq_v, "0.04 KG");
    lv_obj_set_style_text_color(kq_v, UI_COLOR_TEXT_PRI, 0);
    lv_obj_set_style_text_font(kq_v, UI_FONT_LARGE, 0);

    lv_obj_t *arr = lv_label_create(kq_row);
    lv_label_set_text(arr, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(arr, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(arr, UI_FONT_MEDIUM, 0);

    lv_obj_t *mt_l = lv_obj_create(kq_row);
    lv_obj_remove_style_all(mt_l);
    lv_obj_set_flex_flow(mt_l, LV_FLEX_FLOW_COLUMN);
    lv_obj_t *mt_h = lv_label_create(mt_l);
    lv_label_set_text(mt_h, "MỤC TIÊU");
    lv_obj_set_style_text_color(mt_h, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(mt_h, UI_FONT_TINY, 0);
    lv_obj_t *mt_v = lv_label_create(mt_l);
    lv_label_set_text(mt_v, "0.00 KG");
    lv_obj_set_style_text_color(mt_v, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(mt_v, UI_FONT_LARGE, 0);

    /* Input: Giá trị quả cân chuẩn */
    lv_obj_t *inp_box = lv_obj_create(card);
    lv_obj_set_size(inp_box, LV_PCT(100), 72);
    lv_obj_set_style_bg_color(inp_box, lv_color_hex(0x111111), 0);
    lv_obj_set_style_border_color(inp_box, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(inp_box, 1, 0);
    lv_obj_set_style_radius(inp_box, UI_RADIUS_SM, 0);
    lv_obj_set_style_pad_all(inp_box, UI_PAD_SM, 0);
    lv_obj_set_scrollbar_mode(inp_box, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *inp_lbl = lv_label_create(inp_box);
    lv_label_set_text(inp_lbl, "GIÁ TRỊ QUẢ CÂN CHUẨN (KG)");
    lv_obj_set_style_text_color(inp_lbl, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(inp_lbl, UI_FONT_TINY, 0);
    lv_obj_align(inp_lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *inp_val = lv_label_create(inp_box);
    lv_label_set_text(inp_val, "0.00");
    lv_obj_set_style_text_color(inp_val, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(inp_val, UI_FONT_LARGE, 0);
    lv_obj_align(inp_val, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_obj_t *inp_unit = lv_label_create(inp_box);
    lv_label_set_text(inp_unit, "KG");
    lv_obj_set_style_text_color(inp_unit, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(inp_unit, UI_FONT_SMALL, 0);
    lv_obj_align(inp_unit, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    /* Nút BẮT ĐẦU HIỆU CHUẨN */
    lv_obj_t *btn_start = lv_btn_create(card);
    lv_obj_set_size(btn_start, LV_PCT(100), 60);
    lv_obj_set_style_bg_color(btn_start, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_bg_color(btn_start, UI_COLOR_ACCENT_DIM, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn_start, 0, 0);
    lv_obj_set_style_radius(btn_start, UI_RADIUS_SM, 0);
    lv_obj_t *lbl_s = lv_label_create(btn_start);
    lv_label_set_text(lbl_s, LV_SYMBOL_PLAY "  BẮT ĐẦU HIỆU CHUẨN");
    lv_obj_set_style_text_color(lbl_s, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_text_font(lbl_s, UI_FONT_MEDIUM, 0);
    lv_obj_center(lbl_s);
}

/* ============================================================
 * ANGLE SYNC CARD — ĐỒNG BỘ MÁY ĐO ĐỘ NGHIÊNG
 * ============================================================ */
static void build_angle_card(lv_obj_t *parent)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_add_style(card, &style_card, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_pad_all(card, UI_PAD_MD, 0);
    lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(card, UI_PAD_SM, 0);

    /* Header row */
    lv_obj_t *hdr_row = lv_obj_create(card);
    lv_obj_remove_style_all(hdr_row);
    lv_obj_set_size(hdr_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(hdr_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hdr_row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *lft = lv_obj_create(hdr_row);
    lv_obj_remove_style_all(lft);
    lv_obj_set_flex_flow(lft, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(lft, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(lft, UI_PAD_SM, 0);

    lv_obj_t *ico = lv_label_create(lft);
    lv_label_set_text(ico, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_color(ico, UI_COLOR_TEXT_PRI, 0);
    lv_obj_set_style_text_font(ico, UI_FONT_MEDIUM, 0);

    lv_obj_t *ttl_c = lv_obj_create(lft);
    lv_obj_remove_style_all(ttl_c);
    lv_obj_set_flex_flow(ttl_c, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *ttl = lv_label_create(ttl_c);
    lv_label_set_text(ttl, "ĐỒNG BỘ MÁY ĐO ĐỘ\nNGHIÊNG");
    lv_obj_set_style_text_color(ttl, UI_COLOR_TEXT_PRI, 0);
    lv_obj_set_style_text_font(ttl, UI_FONT_SMALL, 0);

    lv_obj_t *sub = lv_label_create(ttl_c);
    lv_label_set_text(sub, "Cân bằng máy trước khi bắt đầu");
    lv_obj_set_style_text_color(sub, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(sub, UI_FONT_TINY, 0);

    /* Badge CHỜ XỬ LÝ */
    lv_obj_t *badge = lv_obj_create(hdr_row);
    lv_obj_set_size(badge, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(badge, lv_color_hex(0x3A3A3A), 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(badge, 0, 0);
    lv_obj_set_style_radius(badge, UI_RADIUS_SM, 0);
    lv_obj_set_style_pad_hor(badge, 10, 0);
    lv_obj_set_style_pad_ver(badge, 4, 0);
    lv_obj_t *bdg_l = lv_label_create(badge);
    lv_label_set_text(bdg_l, "CHỜ XỬ LÝ");
    lv_obj_set_style_text_color(bdg_l, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(bdg_l, UI_FONT_TINY, 0);
    lv_obj_center(bdg_l);

    /* Độ chúi / Độ nghiêng */
    lv_obj_t *angles = lv_obj_create(card);
    lv_obj_remove_style_all(angles);
    lv_obj_set_size(angles, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(angles, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(angles, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    typedef struct { const char *hdr; const char *val; } angle_t;
    static const angle_t k_angles[2] = {
        { "ĐỘ CHÚI",   "+2.4°" },
        { "ĐỘ NGHIÊNG", "-0.8°" },
    };

    for (int i = 0; i < 2; i++) {
        lv_obj_t *ac = lv_obj_create(angles);
        lv_obj_add_style(ac, &style_card, 0);
        lv_obj_set_flex_grow(ac, 1);
        lv_obj_set_height(ac, 72);
        lv_obj_set_style_pad_all(ac, UI_PAD_SM, 0);
        lv_obj_set_scrollbar_mode(ac, LV_SCROLLBAR_MODE_OFF);

        lv_obj_t *lh = lv_label_create(ac);
        lv_label_set_text(lh, k_angles[i].hdr);
        lv_obj_set_style_text_color(lh, UI_COLOR_TEXT_SEC, 0);
        lv_obj_set_style_text_font(lh, UI_FONT_TINY, 0);
        lv_obj_align(lh, LV_ALIGN_TOP_MID, 0, 0);

        lv_obj_t *lv2 = lv_label_create(ac);
        lv_label_set_text(lv2, k_angles[i].val);
        lv_obj_set_style_text_color(lv2, UI_COLOR_TEXT_PRI, 0);
        lv_obj_set_style_text_font(lv2, UI_FONT_LARGE, 0);
        lv_obj_align(lv2, LV_ALIGN_BOTTOM_MID, 0, 0);
    }

    /* Nút HIỆU CHUẨN GÓC */
    lv_obj_t *btn = lv_btn_create(card);
    lv_obj_set_size(btn, LV_PCT(100), 52);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x2A2A2A), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x3A3A3A), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(btn, UI_COLOR_BORDER, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, UI_RADIUS_SM, 0);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, "HIỆU CHUẨN GÓC");
    lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT_PRI, 0);
    lv_obj_set_style_text_font(lbl, UI_FONT_MEDIUM, 0);
    lv_obj_center(lbl);
}

/* ============================================================
 * SENSOR MATRIX CARD — phải
 * ============================================================ */
static lv_obj_t *s_matrix_card = NULL;

static void build_sensor_matrix(lv_obj_t *parent)
{
    s_matrix_card = lv_obj_create(parent);
    lv_obj_set_size(s_matrix_card, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_add_style(s_matrix_card, &style_card, 0);
    lv_obj_set_style_border_color(s_matrix_card, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(s_matrix_card, 1, 0);
    lv_obj_set_style_pad_all(s_matrix_card, UI_PAD_MD, 0);
    lv_obj_set_scrollbar_mode(s_matrix_card, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(s_matrix_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_matrix_card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(s_matrix_card, UI_PAD_SM, 0);

    /* Title */
    lv_obj_t *ttl_row = lv_obj_create(s_matrix_card);
    lv_obj_remove_style_all(ttl_row);
    lv_obj_set_size(ttl_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(ttl_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ttl_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(ttl_row, UI_PAD_SM, 0);

    lv_obj_t *ico = lv_label_create(ttl_row);
    lv_label_set_text(ico, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_color(ico, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(ico, UI_FONT_MEDIUM, 0);

    lv_obj_t *ttl = lv_label_create(ttl_row);
    lv_label_set_text(ttl, "MA TRẬN CẢM BIẾN");
    lv_obj_set_style_text_color(ttl, UI_COLOR_TEXT_PRI, 0);
    lv_obj_set_style_text_font(ttl, UI_FONT_MEDIUM, 0);

    /* Sensor rows */
    refresh_sensor_matrix();

    /* Chú giải */
    lv_obj_t *note_box = lv_obj_create(s_matrix_card);
    lv_obj_set_size(note_box, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(note_box, lv_color_hex(0x111111), 0);
    lv_obj_set_style_bg_opa(note_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(note_box, 0, 0);
    lv_obj_set_style_radius(note_box, UI_RADIUS_SM, 0);
    lv_obj_set_style_pad_all(note_box, UI_PAD_SM, 0);
    lv_obj_set_scrollbar_mode(note_box, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(note_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(note_box, 4, 0);

    lv_obj_t *note_h = lv_label_create(note_box);
    lv_label_set_text(note_h, "CHÚ GIẢI VỊ TRÍ");
    lv_obj_set_style_text_color(note_h, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(note_h, UI_FONT_TINY, 0);

    const char *notes_gau =
        "P1-P6: Xy lanh Cán chính, Tay gầu & Gầu\n"
        "A1-A4: Thân máy & Các đoạn cần";
    const char *notes_lat =
        "P1-P2: Xy lanh nâng & nghiêng\n"
        "A1-A2: Thân máy & Gầu lật\n"
        "PR1: Cảm biến tiệm cận";

    lv_obj_t *note_c = lv_label_create(note_box);
    lv_label_set_text(note_c, s_machine == MACHINE_GAU ? notes_gau : notes_lat);
    lv_label_set_long_mode(note_c, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(note_c, LV_PCT(100));
    lv_obj_set_style_text_color(note_c, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(note_c, UI_FONT_TINY, 0);

    /* Timestamp */
    lv_obj_t *ts = lv_label_create(s_matrix_card);
    lv_label_set_text(ts, "ĐMNG BL  LCN CUK I: 14:02:33");
    lv_obj_set_style_text_color(ts, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(ts, UI_FONT_TINY, 0);
}

static void refresh_sensor_matrix(void)
{
    /* Xóa các row cũ nếu rebuild */
    /* (s_matrix_card rebuilt from scratch mỗi lần đổi tab) */

    const sensor_row_t *sensors;
    int count;
    if (s_machine == MACHINE_GAU) {
        sensors = k_sensors_gau;
        count   = SENSORS_GAU_COUNT;
    } else {
        sensors = k_sensors_lat;
        count   = SENSORS_LAT_COUNT;
    }

    const char *last_group = NULL;
    for (int i = 0; i < count; i++) {
        const sensor_row_t *sr = &sensors[i];

        /* Group header */
        if (sr->group && sr->group != last_group) {
            last_group = sr->group;
            lv_obj_t *gh = lv_label_create(s_matrix_card);
            lv_label_set_text(gh, sr->group);
            lv_obj_set_style_text_color(gh, UI_COLOR_TEXT_SEC, 0);
            lv_obj_set_style_text_font(gh, UI_FONT_TINY, 0);
        }

        /* Sensor row */
        lv_obj_t *row = lv_obj_create(s_matrix_card);
        lv_obj_set_size(row, LV_PCT(100), 52);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x1A1A1A), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(0x2A2A2A), 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_radius(row, UI_RADIUS_SM, 0);
        lv_obj_set_style_pad_hor(row, UI_PAD_MD, 0);
        lv_obj_set_style_pad_ver(row, 0, 0);
        lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);

        /* Green dot */
        lv_obj_t *dot = lv_obj_create(row);
        lv_obj_set_size(dot, 14, 14);
        lv_obj_set_style_bg_color(dot,
            sr->warning ? lv_color_hex(0xEAB308) : lv_color_hex(0x22C55E), 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_set_style_radius(dot, 7, 0);
        lv_obj_align(dot, LV_ALIGN_LEFT_MID, 0, 0);

        /* Name */
        lv_obj_t *name = lv_label_create(row);
        lv_label_set_text(name, sr->name);
        lv_obj_set_style_text_color(name, UI_COLOR_TEXT_PRI, 0);
        lv_obj_set_style_text_font(name, UI_FONT_MEDIUM, 0);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 26, 0);

        /* Value */
        if (sr->text_val) {
            lv_obj_t *val = lv_label_create(row);
            lv_label_set_text(val, sr->text);
            lv_obj_set_style_text_color(val, lv_color_hex(0x22C55E), 0);
            lv_obj_set_style_text_font(val, UI_FONT_MEDIUM, 0);
            lv_obj_align(val, LV_ALIGN_RIGHT_MID, 0, 0);
        } else {
            char vbuf[16];
            snprintf(vbuf, sizeof(vbuf), "%.1f", sr->value);
            lv_obj_t *val = lv_label_create(row);
            lv_label_set_text(val, vbuf);
            lv_obj_set_style_text_color(val,
                sr->warning ? lv_color_hex(0xEAB308) : UI_COLOR_TEXT_PRI, 0);
            lv_obj_set_style_text_font(val, UI_FONT_LARGE, 0);
            lv_obj_align(val, LV_ALIGN_RIGHT_MID, 0, 0);
        }
    }
}


/* ============================================================
 * SUB-NAVIGATION — CẤU HÌNH | MẠNG | HIỆU CHUẨN | THÔNG TIN
 * ============================================================ */
static lv_obj_t *s_subnav_btns[4];

typedef struct { const char *icon; const char *label; } subnav_item_t;
static const subnav_item_t k_subnav[4] = {
    { LV_SYMBOL_SETTINGS, "CẤU HÌNH"  },
    { "<>",               "MẠNG"      },
    { LV_SYMBOL_REFRESH,  "HIỆU CHUẨN"},
    { LV_SYMBOL_LOOP,     "THÔNG TIN" },
};

static int s_subnav_idx[4] = {0, 1, 2, 3};

static void subnav_cb(lv_event_t *e)
{
    int *idx = (int *)lv_event_get_user_data(e);
    s_subnav_active = *idx;
    for (int i = 0; i < 4; i++) {
        bool act = (i == s_subnav_active);
        if (!s_subnav_btns[i]) continue;
        lv_obj_set_style_bg_color(s_subnav_btns[i],
            act ? lv_color_hex(0x1A1A1A) : lv_color_hex(0x111111), 0);
        /* Update child label colors */
        lv_obj_t *child = lv_obj_get_child(s_subnav_btns[i], 0);
        while (child) {
            lv_obj_set_style_text_color(child,
                act ? UI_COLOR_ACCENT : UI_COLOR_TEXT_DIM, 0);
            child = lv_obj_get_child(s_subnav_btns[i],
                        (int)lv_obj_get_index(child) + 1);
        }
        /* Active indicator line */
        if (act) {
            lv_obj_set_style_border_color(s_subnav_btns[i], UI_COLOR_ACCENT, 0);
            lv_obj_set_style_border_side(s_subnav_btns[i], LV_BORDER_SIDE_BOTTOM, 0);
            lv_obj_set_style_border_width(s_subnav_btns[i], 3, 0);
        } else {
            lv_obj_set_style_border_width(s_subnav_btns[i], 0, 0);
        }
    }
    ESP_LOGI(TAG, "Sub-nav: %s", k_subnav[*idx].label);
}

static void build_sub_nav(lv_obj_t *parent)
{
    int nav_y = UI_SCREEN_H - UI_NAV_H;

    lv_obj_t *nav = lv_obj_create(parent);
    lv_obj_set_size(nav, UI_SCREEN_W, UI_NAV_H);
    lv_obj_set_pos(nav, 0, nav_y);
    lv_obj_set_style_bg_color(nav, lv_color_hex(0x111111), 0);
    lv_obj_set_style_border_color(nav, UI_COLOR_BORDER, 0);
    lv_obj_set_style_border_side(nav, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(nav, 1, 0);
    lv_obj_set_style_radius(nav, 0, 0);
    lv_obj_set_style_pad_all(nav, 0, 0);
    lv_obj_set_flex_flow(nav, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(nav, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(nav, LV_SCROLLBAR_MODE_OFF);

    for (int i = 0; i < 4; i++) {
        lv_obj_t *btn = lv_btn_create(nav);
        lv_obj_set_size(btn, LV_PCT(25), UI_NAV_H);
        lv_obj_set_style_bg_color(btn,
            i == s_subnav_active ? lv_color_hex(0x1A1A1A) : lv_color_hex(0x111111), 0);
        lv_obj_set_style_border_width(btn, i == s_subnav_active ? 3 : 0, 0);
        lv_obj_set_style_border_color(btn, UI_COLOR_ACCENT, 0);
        lv_obj_set_style_border_side(btn, LV_BORDER_SIDE_TOP, 0);
        lv_obj_set_style_radius(btn, 0, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_add_event_cb(btn, subnav_cb, LV_EVENT_CLICKED, &s_subnav_idx[i]);
        s_subnav_btns[i] = btn;

        lv_obj_t *col = lv_obj_create(btn);
        lv_obj_remove_style_all(col);
        lv_obj_set_size(col, LV_PCT(100), LV_PCT(100));
        lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(col, 4, 0);

        lv_color_t col_clr = (i == s_subnav_active) ? UI_COLOR_ACCENT : UI_COLOR_TEXT_DIM;

        lv_obj_t *ico = lv_label_create(col);
        lv_label_set_text(ico, k_subnav[i].icon);
        lv_obj_set_style_text_color(ico, col_clr, 0);
        lv_obj_set_style_text_font(ico, UI_FONT_MEDIUM, 0);

        lv_obj_t *lbl = lv_label_create(col);
        lv_label_set_text(lbl, k_subnav[i].label);
        lv_obj_set_style_text_color(lbl, col_clr, 0);
        lv_obj_set_style_text_font(lbl, UI_FONT_TINY, 0);
    }
}

/* ============================================================
 * MISC CALLBACKS
 * ============================================================ */
static void save_cb(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "Lưu kết quả hiệu chuẩn");
    /* TODO: ghi vào NVS / gửi lên server */
}

static void back_cb(lv_event_t *e)
{
    (void)e;
    /* Quay lại màn hình CÀI ĐẶT */
    extern lv_obj_t *ui_settings_get_screen(void);
    lv_obj_t *settings_scr = ui_settings_get_screen();
    if (settings_scr) {
        lv_scr_load_anim(settings_scr, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, false);
    }
}
