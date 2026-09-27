/**
 * @file ui_orders.c
 * @brief Màn hình ĐƠN HÀNG
 *
 * Layout:
 *   Header  (shared)
 *   ┌─ Topbar: "Quản lý đơn hàng"  +  [TẢI ĐƠN MỚI] ─┐
 *   ├─ Filter tabs: Chờ xử lý / Đang thực hiện / Đã hoàn thành | Hôm nay | Vật liệu
 *   ├─ Grid 2 cột: Order cards
 *   └─ Bottom nav (ĐƠN HÀNG active)
 *
 * Khi user nhấn "Chi tiết" hoặc tap vào card:
 *   → gọi ui_main_load_order() → chuyển màn CÂN + mở numpad
 */

#include "ui_orders.h"
#include "ui_orders.h"
#include "ui_theme.h"
#include "ui_nav.h"
#include "ui_main.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "UI_ORDERS";

/* ---- Mock data ---- */
typedef struct {
    const char *order_id;
    const char *company;
    const char *material;
    float       target_kg;
    const char *status;       /* "sync" | "local" | "active" */
    bool        visible;      /* filter kết quả */
} order_item_t;

static order_item_t s_orders[] = {
    { "ORD-8824-A", "Apex Build Co.",  "Gravel 3/4\"", 24500, "sync",   true },
    { "ORD-8825-B", "Metro Paving",    "Asphalt Base", 18000, "local",  true },
    { "ORD-8826-C", "Hanoi Roads",     "Sand Fine",    30000, "sync",   true },
    { "ORD-8827-D", "Green Concrete",  "Limestone",    22000, "active", true },
    { "ORD-8828-E", "Delta Infra",     "Gravel 1/2\"", 15000, "sync",   true },
    { "ORD-8829-F", "VietBuild Corp",  "Crushed Rock", 28000, "local",  true },
};
#define ORDER_COUNT  (sizeof(s_orders)/sizeof(s_orders[0]))

static lv_obj_t *s_screen       = NULL;
static lv_obj_t *s_order_cont   = NULL;  /* container chứa cards, rebuild khi filter */
static int       s_filter_idx   = 0;     /* 0=Chờ xử lý, 1=Đang, 2=Xong */
static lv_obj_t *s_dl_btn       = NULL;  /* Nút TẢI ĐƠN MỚI */
static lv_obj_t *s_dl_lbl       = NULL;  /* Label của nút */

/* ============================================================
 * FORWARD DECLS
 * ============================================================ */
static void build_header(lv_obj_t *parent);
static void build_filter_bar(lv_obj_t *body);
static void build_order_grid(lv_obj_t *body);
static lv_obj_t *make_order_card(lv_obj_t *parent, order_item_t *o);
static void card_click_cb(lv_event_t *e);
static void detail_click_cb(lv_event_t *e);
static void filter_cb(lv_event_t *e);
static void download_cb(lv_event_t *e);

/* ============================================================
 * Khởi tạo màn hình
 * ============================================================ */
void ui_orders_screen_init(void)
{
    if (s_screen) return;  /* chỉ init một lần */

    s_screen = lv_obj_create(NULL);
    lv_obj_add_style(s_screen, &style_screen, 0);
    lv_obj_set_size(s_screen, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_scrollbar_mode(s_screen, LV_SCROLLBAR_MODE_OFF);

    build_header(s_screen);

    /* Scrollable body */
    lv_obj_t *body = lv_obj_create(s_screen);
    lv_obj_set_size(body, UI_SCREEN_W,
                    UI_SCREEN_H - UI_HEADER_H - UI_NAV_H);
    lv_obj_set_pos(body, 0, UI_HEADER_H);
    lv_obj_set_style_bg_color(body, UI_COLOR_BG, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_radius(body, 0, 0);
    lv_obj_set_style_pad_all(body, UI_PAD_MD, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START,
                           LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_AUTO);

    /* Topbar */
    lv_obj_t *topbar = lv_obj_create(body);
    lv_obj_remove_style_all(topbar);
    lv_obj_set_size(topbar, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(topbar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(topbar, LV_FLEX_ALIGN_SPACE_BETWEEN,
                           LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_margin_bottom(topbar, UI_PAD_MD, 0);

    lv_obj_t *title_block = lv_obj_create(topbar);
    lv_obj_remove_style_all(title_block);
    lv_obj_set_size(title_block, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(title_block, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(title_block, LV_FLEX_ALIGN_START,
                           LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t *ttl = lv_label_create(title_block);
    lv_label_set_text(ttl, "Quản lý đơn hàng");
    lv_obj_set_style_text_color(ttl, UI_COLOR_TEXT_PRI, 0);
    lv_obj_set_style_text_font(ttl, UI_FONT_MEDIUM, 0);

    lv_obj_t *sub = lv_label_create(title_block);
    lv_label_set_text(sub, "Quản lý và đồng bộ đơn hàng xếp tải từ Cloud");
    lv_obj_set_style_text_color(sub, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(sub, UI_FONT_TINY, 0);

    /* Nút TẢI ĐƠN MỚI */
    s_dl_btn = lv_btn_create(topbar);
    lv_obj_set_style_bg_color(s_dl_btn, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(s_dl_btn, 0, 0);
    lv_obj_set_style_radius(s_dl_btn, UI_RADIUS_SM, 0);
    lv_obj_set_style_pad_hor(s_dl_btn, UI_PAD_SM, 0);
    lv_obj_set_style_pad_ver(s_dl_btn, 6, 0);
    lv_obj_set_size(s_dl_btn, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_add_event_cb(s_dl_btn, download_cb, LV_EVENT_CLICKED, NULL);

    s_dl_lbl = lv_label_create(s_dl_btn);
    lv_label_set_text(s_dl_lbl, LV_SYMBOL_DOWNLOAD "  TẢI ĐƠN MỚI");
    lv_obj_set_style_text_color(s_dl_lbl, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_text_font(s_dl_lbl, UI_FONT_SMALL, 0);
    lv_obj_center(s_dl_lbl);

    build_filter_bar(body);

    /* Container order cards (rebuild khi filter thay đổi) */
    s_order_cont = lv_obj_create(body);
    lv_obj_remove_style_all(s_order_cont);
    lv_obj_set_size(s_order_cont, LV_PCT(100), LV_SIZE_CONTENT);

    static lv_coord_t gcols[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t grows[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT,
                                  LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(s_order_cont, gcols, grows);
    lv_obj_set_style_pad_column(s_order_cont, UI_PAD_SM, 0);
    lv_obj_set_style_pad_row(s_order_cont, UI_PAD_SM, 0);

    build_order_grid(s_order_cont);

    ui_nav_build(s_screen, 1);  /* tab 1 = ĐƠN HÀNG */
    ESP_LOGI(TAG, "Orders screen init OK (%d orders)", (int)ORDER_COUNT);
}

lv_obj_t *ui_orders_get_screen(void) { return s_screen; }

/* ============================================================
 * Header (clone của ui_main — thời gian + title + icons)
 * ============================================================ */
static lv_timer_t *s_order_clock = NULL;
static lv_obj_t *s_o_lbl_time, *s_o_lbl_date;

static void order_clock_cb(lv_timer_t *t)
{
    (void)t;
    time_t now = time(NULL);
    struct tm *ti = localtime(&now);
    if (!ti) return;
    char tb[16], db[16];
    strftime(tb, sizeof(tb), "%H:%M:%S", ti);
    strftime(db, sizeof(db), "%d/%m/%Y", ti);
    if (s_o_lbl_time) lv_label_set_text(s_o_lbl_time, tb);
    if (s_o_lbl_date) lv_label_set_text(s_o_lbl_date, db);
}

static void build_header(lv_obj_t *parent)
{
    lv_obj_t *hdr = lv_obj_create(parent);
    lv_obj_set_size(hdr, UI_SCREEN_W, UI_HEADER_H);
    lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(hdr, UI_COLOR_BG, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_radius(hdr, 0, 0);
    lv_obj_set_style_pad_all(hdr, UI_PAD_MD, 0);

    s_o_lbl_time = lv_label_create(hdr);
    lv_obj_set_style_text_color(s_o_lbl_time, UI_COLOR_ACCENT, 0);   /* vàng, nhất quán toàn app */
    lv_obj_set_style_text_font(s_o_lbl_time, UI_FONT_MEDIUM, 0);
    lv_obj_align(s_o_lbl_time, LV_ALIGN_TOP_LEFT, 8, 4);

    s_o_lbl_date = lv_label_create(hdr);
    lv_obj_set_style_text_color(s_o_lbl_date, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(s_o_lbl_date, UI_FONT_TINY, 0);
    lv_obj_align_to(s_o_lbl_date, s_o_lbl_time, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 2);

    lv_obj_t *ttl = lv_label_create(hdr);
    lv_label_set_text(ttl, "CÂN MÁY XÚC");
    lv_obj_set_style_text_color(ttl, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(ttl, UI_FONT_MEDIUM, 0);
    lv_obj_align(ttl, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *icons = lv_label_create(hdr);
    lv_label_set_text(icons,
        LV_SYMBOL_REFRESH "  4G  " LV_SYMBOL_WIFI "  " LV_SYMBOL_BATTERY_FULL " 85%");
    lv_obj_set_style_text_color(icons, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(icons, UI_FONT_SMALL, 0);
    lv_obj_align(icons, LV_ALIGN_TOP_RIGHT, -8, 4);

    s_order_clock = lv_timer_create(order_clock_cb, 1000, NULL);
    order_clock_cb(NULL);
}

/* ============================================================
 * Filter bar
 * ============================================================ */

typedef struct { int filter_id; lv_obj_t *btn; } filter_data_t;
static filter_data_t s_filter_data[5];

static void filter_cb(lv_event_t *e)
{
    filter_data_t *fd = (filter_data_t *)lv_event_get_user_data(e);
    s_filter_idx = fd->filter_id;
    ESP_LOGI(TAG, "Filter: %d", fd->filter_id);

    /* ── Cập nhật trạng thái nút ── */
    for (int i = 0; i < 5; i++) {
        bool act = (s_filter_data[i].filter_id == fd->filter_id);
        lv_obj_set_style_bg_color(s_filter_data[i].btn,
            act ? UI_COLOR_ACCENT : lv_color_hex(0x2a2a2a), 0);
        lv_obj_set_style_bg_opa(s_filter_data[i].btn, LV_OPA_COVER, 0);
        lv_obj_t *lbl = lv_obj_get_child(s_filter_data[i].btn, 0);
        if (lbl) lv_obj_set_style_text_color(lbl,
            act ? lv_color_hex(0x1A1A1A) : UI_COLOR_TEXT_SEC, 0);
    }

    /* ── Cập nhật visibility của từng đơn theo filter ── */
    for (size_t i = 0; i < ORDER_COUNT; i++) {
        switch (fd->filter_id) {
            case 0: /* Chờ xử lý — sync + local */
                s_orders[i].visible = (strcmp(s_orders[i].status, "active") != 0);
                break;
            case 1: /* Đang thực hiện — active only */
                s_orders[i].visible = (strcmp(s_orders[i].status, "active") == 0);
                break;
            case 2: /* Đã hoàn thành — không có data mock, hiện trống */
                s_orders[i].visible = false;
                break;
            default: /* Hôm nay / Vật liệu — hiện tất cả */
                s_orders[i].visible = true;
                break;
        }
    }

    /* ── Xóa grid cũ và rebuild ── */
    if (s_order_cont) {
        lv_obj_clean(s_order_cont);
        static lv_coord_t gcols[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
        static lv_coord_t grows[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT,
                                      LV_GRID_TEMPLATE_LAST};
        lv_obj_set_grid_dsc_array(s_order_cont, gcols, grows);
        build_order_grid(s_order_cont);
    }
}

static lv_obj_t *make_filter_btn(lv_obj_t *parent, const char *label,
                                   int filter_id, int slot_idx)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_style_bg_color(btn,
        filter_id == 0 ? UI_COLOR_ACCENT : lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_color(btn, UI_COLOR_BORDER, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 20, 0);
    lv_obj_set_style_pad_hor(btn, UI_PAD_SM, 0);
    lv_obj_set_style_pad_ver(btn, 4, 0);
    lv_obj_set_size(btn, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl,
        filter_id == 0 ? lv_color_hex(0x1A1A1A) : UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(lbl, UI_FONT_TINY, 0);
    lv_obj_center(lbl);

    s_filter_data[slot_idx].filter_id = filter_id;
    s_filter_data[slot_idx].btn       = btn;
    lv_obj_add_event_cb(btn, filter_cb, LV_EVENT_CLICKED,
                         &s_filter_data[slot_idx]);
    return btn;
}

static void build_filter_bar(lv_obj_t *body)
{
    lv_obj_t *bar = lv_obj_create(body);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_START,
                           LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_margin_bottom(bar, UI_PAD_SM, 0);

    make_filter_btn(bar, "Chờ xử lý",     0, 0);
    make_filter_btn(bar, "Đang thực hiện", 1, 1);
    make_filter_btn(bar, "Đã hoàn thành",  2, 2);

    /* Separator */
    lv_obj_t *sep = lv_obj_create(bar);
    lv_obj_remove_style_all(sep);
    lv_obj_set_size(sep, 1, 20);
    lv_obj_set_style_bg_color(sep, UI_COLOR_BORDER, 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
    lv_obj_set_style_margin_hor(sep, UI_PAD_SM, 0);

    make_filter_btn(bar, "Hôm nay", 3, 3);
    make_filter_btn(bar, "Vật liệu",4, 4);
}

/* ============================================================
 * Order card
 * ============================================================ */
static void card_click_cb(lv_event_t *e)
{
    order_item_t *o = (order_item_t *)lv_event_get_user_data(e);
    ESP_LOGI(TAG, "Selected: %s target=%.0f kg", o->order_id, o->target_kg);

    /* Nạp thông tin đơn hàng vào màn CÂN */
    if (!ui_main_get_screen()) ui_main_screen_init();
    ui_main_load_order(o->order_id, o->company, o->material, o->target_kg);

    /* Chuyển sang màn CÂN + cập nhật tab active */
    lv_scr_load_anim(ui_main_get_screen(), LV_SCR_LOAD_ANIM_FADE_ON, 180, 0, false);
    ui_nav_set_active(0);
}

static void detail_click_cb(lv_event_t *e)
{
    /* Chi tiết → load đơn và chuyển màn CÂN */
    card_click_cb(e);
}

static lv_obj_t *make_order_card(lv_obj_t *parent, order_item_t *o)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_add_style(card, &style_card, 0);
    lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(card, card_click_cb, LV_EVENT_CLICKED, (void *)o);

    /* Top row: label "MÃ ĐƠN" + badge */
    lv_obj_t *top = lv_obj_create(card);
    lv_obj_remove_style_all(top);
    lv_obj_set_size(top, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top, LV_FLEX_ALIGN_SPACE_BETWEEN,
                           LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_margin_bottom(top, 4, 0);

    lv_obj_t *lbl_key = lv_label_create(top);
    lv_label_set_text(lbl_key, "MÃ ĐƠN");
    lv_obj_set_style_text_color(lbl_key, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(lbl_key, UI_FONT_TINY, 0);

    /* Badge theo status */
    lv_obj_t *badge = lv_obj_create(top);
    lv_obj_set_size(badge, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_hor(badge, 5, 0);
    lv_obj_set_style_pad_ver(badge, 2, 0);
    lv_obj_set_style_radius(badge, 3, 0);
    lv_obj_set_style_border_width(badge, 1, 0);

    lv_obj_t *badge_lbl = lv_label_create(badge);
    lv_obj_set_style_text_font(badge_lbl, UI_FONT_TINY, 0);

    if (strcmp(o->status, "sync") == 0) {
        lv_obj_set_style_bg_color(badge, lv_color_hex(0x1a2e1a), 0);
        lv_obj_set_style_border_color(badge, lv_color_hex(0x2e7d32), 0);
        lv_label_set_text(badge_lbl, "ĐÃ ĐỒNG BỘ");
        lv_obj_set_style_text_color(badge_lbl, lv_color_hex(0x66bb6a), 0);
    } else if (strcmp(o->status, "local") == 0) {
        lv_obj_set_style_bg_color(badge, lv_color_hex(0x2e2810), 0);
        lv_obj_set_style_border_color(badge, UI_COLOR_ACCENT, 0);
        lv_label_set_text(badge_lbl, "NỘI BỘ");
        lv_obj_set_style_text_color(badge_lbl, UI_COLOR_ACCENT, 0);
    } else {
        lv_obj_set_style_bg_color(badge, lv_color_hex(0x0d2137), 0);
        lv_obj_set_style_border_color(badge, lv_color_hex(0x1565c0), 0);
        lv_label_set_text(badge_lbl, "ĐANG CÂN");
        lv_obj_set_style_text_color(badge_lbl, lv_color_hex(0x42a5f5), 0);
    }
    lv_obj_center(badge_lbl);

    /* Order ID */
    lv_obj_t *oid = lv_label_create(card);
    lv_label_set_text(oid, o->order_id);
    lv_obj_set_style_text_color(oid, UI_COLOR_TEXT_PRI, 0);
    lv_obj_set_style_text_font(oid, UI_FONT_NORMAL, 0);
    lv_obj_set_style_margin_bottom(oid, 6, 0);

    /* Fields row: KHÁCH HÀNG + VẬT LIỆU */
    lv_obj_t *fields = lv_obj_create(card);
    lv_obj_remove_style_all(fields);
    lv_obj_set_size(fields, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(fields, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(fields, LV_FLEX_ALIGN_START,
                           LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_margin_bottom(fields, 6, 0);

    const char *fkeys[] = {"KHÁCH HÀNG", "VẬT LIỆU"};
    const char *fvals[] = {o->company,    o->material};
    for (int i = 0; i < 2; i++) {
        lv_obj_t *f = lv_obj_create(fields);
        lv_obj_set_size(f, LV_PCT(48), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(f, lv_color_hex(0x1e1e1e), 0);
        lv_obj_set_style_border_width(f, 0, 0);
        lv_obj_set_style_radius(f, 4, 0);
        lv_obj_set_style_pad_all(f, 4, 0);
        lv_obj_set_style_margin_right(f, i==0 ? 4 : 0, 0);
        lv_obj_set_flex_flow(f, LV_FLEX_FLOW_COLUMN);

        lv_obj_t *fk = lv_label_create(f);
        lv_label_set_text(fk, fkeys[i]);
        lv_obj_set_style_text_color(fk, UI_COLOR_TEXT_SEC, 0);
        lv_obj_set_style_text_font(fk, UI_FONT_TINY, 0);

        lv_obj_t *fv = lv_label_create(f);
        lv_label_set_text(fv, fvals[i]);
        lv_obj_set_style_text_color(fv, UI_COLOR_TEXT_PRI, 0);
        lv_obj_set_style_text_font(fv, UI_FONT_TINY, 0);
    }

    /* Khối lượng mục tiêu */
    lv_obj_t *wt_lbl = lv_label_create(card);
    lv_label_set_text(wt_lbl, "KHỐI LƯỢNG MỤC TIÊU");
    lv_obj_set_style_text_color(wt_lbl, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(wt_lbl, UI_FONT_TINY, 0);
    lv_obj_set_style_margin_bottom(wt_lbl, 2, 0);

    char wt_str[16];
    long v = (long)o->target_kg;
    if (v >= 1000) snprintf(wt_str, sizeof(wt_str), "%ld,%03ld", v/1000, v%1000);
    else           snprintf(wt_str, sizeof(wt_str), "%ld", v);

    lv_obj_t *wt_row = lv_obj_create(card);
    lv_obj_remove_style_all(wt_row);
    lv_obj_set_size(wt_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(wt_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(wt_row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                           LV_FLEX_ALIGN_BOTTOM, LV_FLEX_ALIGN_BOTTOM);

    lv_obj_t *wval = lv_label_create(wt_row);
    lv_label_set_text(wval, wt_str);
    lv_obj_set_style_text_color(wval, UI_COLOR_TEXT_PRI, 0);
    lv_obj_set_style_text_font(wval, UI_FONT_LARGE, 0);

    lv_obj_t *wunit = lv_label_create(wt_row);
    lv_label_set_text(wunit, " KG");
    lv_obj_set_style_text_color(wunit, UI_COLOR_TEXT_SEC, 0);
    lv_obj_set_style_text_font(wunit, UI_FONT_TINY, 0);

    /* Nút Chi tiết */
    lv_obj_t *detail_btn = lv_btn_create(wt_row);
    lv_obj_set_style_bg_color(detail_btn, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(detail_btn, 0, 0);
    lv_obj_set_style_radius(detail_btn, 4, 0);
    lv_obj_set_style_pad_hor(detail_btn, UI_PAD_SM, 0);
    lv_obj_set_style_pad_ver(detail_btn, 4, 0);
    lv_obj_set_size(detail_btn, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_add_event_cb(detail_btn, detail_click_cb, LV_EVENT_CLICKED, (void *)o);

    lv_obj_t *detail_lbl = lv_label_create(detail_btn);
    lv_label_set_text(detail_lbl, "Chi tiết");
    lv_obj_set_style_text_color(detail_lbl, UI_COLOR_TEXT_PRI, 0);
    lv_obj_set_style_text_font(detail_lbl, UI_FONT_TINY, 0);
    lv_obj_center(detail_lbl);

    return card;
}

/* ============================================================
 * Build grid
 * ============================================================ */
static void build_order_grid(lv_obj_t *cont)
{
    int col = 0, row = 0;
    for (size_t i = 0; i < ORDER_COUNT; i++) {
        if (!s_orders[i].visible) continue;
        lv_obj_t *c = make_order_card(cont, &s_orders[i]);
        lv_obj_set_grid_cell(c, LV_GRID_ALIGN_STRETCH, col, 1,
                                LV_GRID_ALIGN_START,   row, 1);
        col++;
        if (col >= 2) { col = 0; row++; }
    }
}

/* ============================================================
 * Download với loading animation
 * ============================================================ */
static void dl_reset_cb(lv_timer_t *t)
{
    (void)t;
    if (s_dl_lbl) lv_label_set_text(s_dl_lbl, LV_SYMBOL_DOWNLOAD "  TẢI ĐƠN MỚI");
    if (s_dl_btn) lv_obj_set_style_bg_color(s_dl_btn, UI_COLOR_ACCENT, 0);
    lv_timer_del(t);  /* one-shot: tự xóa sau khi chạy */
}

static void download_cb(lv_event_t *e)
{
    (void)e;
    /* Hiển thị trạng thái loading */
    if (s_dl_lbl) lv_label_set_text(s_dl_lbl, LV_SYMBOL_REFRESH "  Đang tải...");
    if (s_dl_btn) lv_obj_set_style_bg_color(s_dl_btn, lv_color_hex(0xB8960A), 0);
    /* Sau 2 giây reset lại (simulate xong fetch) */
    lv_timer_create(dl_reset_cb, 2000, NULL);
    ESP_LOGI(TAG, "TẢI ĐƠN MỚI — fetching from cloud API...");
}
