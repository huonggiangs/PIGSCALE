#include <stdint.h>
#include "ui_orders.h"
#include "ui_theme.h"
#include "ui_common.h"
#include "ui_shell.h"
#include "app_state.h"
#include <stdio.h>

static lv_obj_t *s_container;
static lv_obj_t *s_filter_btns[4];
static lv_obj_t *s_list_host;
static lv_obj_t *s_pager_host;

static const char *FILTER_LABELS[4] = {"Tất cả", "Chờ cân", "Đang cân", "Đã cân"};

static void refresh_filter_highlight(void)
{
    order_filter_t cur = app_state()->order_filter;
    for (int i = 0; i < 4; i++) {
        bool sel = ((int)cur == i);
        lv_obj_t *btn = s_filter_btns[i];
        lv_obj_set_style_bg_color(btn, sel ? UI_COLOR_PRIMARY : UI_COLOR_MIST_100, 0);
        lv_obj_t *lbl = lv_obj_get_child(btn, 0);
        lv_obj_set_style_text_color(lbl, sel ? lv_color_white() : UI_COLOR_BODY, 0);
    }
}

static void filter_btn_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    app_state()->order_filter = (order_filter_t)idx;
    app_state()->order_page = 0;
    ui_orders_refresh();
}

static void refresh_btn_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_shell_toast("Đã làm mới danh sách đơn hàng");
    ui_orders_refresh();
}

static void start_weighing_cb(lv_event_t *e)
{
    const char *order_id = (const char *)lv_event_get_user_data(e);
    app_state_start_weighing(order_id);
    ui_shell_switch_tab(TAB_WEIGHING);
}

static void view_weighing_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_shell_switch_tab(TAB_WEIGHING);
}

static void page_btn_cb(lv_event_t *e)
{
    int page = (int)(intptr_t)lv_event_get_user_data(e);
    app_state()->order_page = page;
    ui_orders_refresh();
}

static const char *status_label(order_status_t s)
{
    switch (s) {
        case ORDER_PENDING:  return "Chờ cân";
        case ORDER_WEIGHING: return "Đang cân";
        default:             return "Hoàn tất";
    }
}

static void build_order_card(lv_obj_t *parent, order_t *o, bool any_other_weighing)
{
    lv_obj_t *card = ui_common_card(parent);
    lv_obj_set_style_pad_bottom(card, 14, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 6, 0);

    /* hàng mã đơn + badges */
    lv_obj_t *head = lv_obj_create(card);
    lv_obj_remove_style_all(head);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(head, 8, 0);
    lv_obj_set_width(head, LV_PCT(100));
    lv_obj_set_height(head, LV_SIZE_CONTENT);
    lv_obj_clear_flag(head, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *code = lv_label_create(head);
    lv_label_set_text(code, o->code);
    lv_obj_set_style_text_font(code, UI_FONT_H4_BOLD, 0);
    lv_obj_set_style_text_color(code, UI_COLOR_HEADING, 0);

    bool is_import = (o->ticket_type == TICKET_IMPORT);
    ui_common_badge(head, is_import ? "Nhập" : "Xuất",
                     is_import ? UI_COLOR_SUCCESS_BG : UI_COLOR_WARNING_BG,
                     is_import ? UI_COLOR_SUCCESS : UI_COLOR_ORANGE_700);

    lv_color_t st_bg = UI_COLOR_MIST_100, st_fg = UI_COLOR_BODY;
    if (o->status == ORDER_WEIGHING) { st_bg = UI_COLOR_PRIMARY_SOFT; st_fg = UI_COLOR_PRIMARY; }
    else if (o->status == ORDER_DONE) { st_bg = UI_COLOR_SUCCESS_BG; st_fg = UI_COLOR_SUCCESS; }
    ui_common_badge(head, status_label(o->status), st_bg, st_fg);

    lv_obj_t *cust = lv_label_create(card);
    char buf[96];
    snprintf(buf, sizeof(buf), "Khách hàng: %s", o->customer);
    lv_label_set_text(cust, buf);
    lv_obj_set_style_text_font(cust, UI_FONT_BODY, 0);
    lv_obj_set_style_text_color(cust, UI_COLOR_BODY, 0);

    lv_obj_t *plan = lv_label_create(card);
    snprintf(buf, sizeof(buf), "Kế hoạch: %d con", o->planned_qty);
    lv_label_set_text(plan, buf);
    lv_obj_set_style_text_font(plan, UI_FONT_BODY, 0);
    lv_obj_set_style_text_color(plan, UI_COLOR_BODY, 0);

    lv_obj_t *plate = lv_label_create(card);
    snprintf(buf, sizeof(buf), "Xe: %s", o->plate);
    lv_label_set_text(plate, buf);
    lv_obj_set_style_text_font(plate, UI_FONT_BODY, 0);
    lv_obj_set_style_text_color(plate, UI_COLOR_BODY, 0);

    if (o->status == ORDER_DONE) {
        lv_obj_t *sync_row = lv_obj_create(card);
        lv_obj_remove_style_all(sync_row);
        lv_obj_set_flex_flow(sync_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(sync_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(sync_row, 6, 0);
        lv_obj_set_size(sync_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_clear_flag(sync_row, LV_OBJ_FLAG_SCROLLABLE);

        if (o->synced) {
            ui_common_icon(sync_row, &img_icon_check, UI_COLOR_SUCCESS);
            lv_obj_t *l = lv_label_create(sync_row);
            snprintf(buf, sizeof(buf), "Đã đồng bộ về server · %s", o->receipt_no);
            lv_label_set_text(l, buf);
            lv_obj_set_style_text_font(l, UI_FONT_BODY_BOLD, 0);
            lv_obj_set_style_text_color(l, UI_COLOR_SUCCESS, 0);
        } else if (o->pending_sync) {
            /* ⏳ không có glyph trong font Inter tiếng Việt và không có SVG
             * icon riêng trong assets/icons/ — dùng icon cảnh báo có sẵn của
             * font Montserrat built-in (LV_SYMBOL_WARNING) thay thế, tách
             * label riêng để không lẫn font với chữ tiếng Việt. */
            lv_obj_t *icon = lv_label_create(sync_row);
            lv_label_set_text(icon, LV_SYMBOL_WARNING);
            lv_obj_set_style_text_font(icon, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(icon, UI_COLOR_WARNING, 0);

            lv_obj_t *l = lv_label_create(sync_row);
            lv_label_set_text(l, "Đang chờ đồng bộ (chưa có mạng)");
            lv_obj_set_style_text_font(l, UI_FONT_BODY_BOLD, 0);
            lv_obj_set_style_text_color(l, UI_COLOR_WARNING, 0);
        }
    }

    if (o->status == ORDER_PENDING && !any_other_weighing) {
        lv_obj_t *btn = ui_common_button(card, "Bắt đầu cân", UI_COLOR_PRIMARY, lv_color_white(), UI_FONT_BODY_BOLD);
        lv_obj_set_width(btn, LV_PCT(100));
        lv_obj_set_style_pad_top(btn, 6, 0);
        lv_obj_add_event_cb(btn, start_weighing_cb, LV_EVENT_CLICKED, (void *)o->id);
    } else if (o->status == ORDER_WEIGHING) {
        lv_obj_t *btn = ui_common_button(card, "Xem phiên đang cân", UI_COLOR_PRIMARY_SOFT, UI_COLOR_PRIMARY, UI_FONT_BODY_BOLD);
        lv_obj_set_width(btn, LV_PCT(100));
        lv_obj_set_style_pad_top(btn, 6, 0);
        lv_obj_add_event_cb(btn, view_weighing_cb, LV_EVENT_CLICKED, NULL);
    }
}

lv_obj_t *ui_orders_create(lv_obj_t *parent)
{
    s_container = lv_obj_create(parent);
    lv_obj_remove_style_all(s_container);
    lv_obj_set_size(s_container, UI_HOR_RES, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(s_container, UI_PAD_SCREEN, 0);
    lv_obj_set_flex_flow(s_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_container, 12, 0);
    lv_obj_clear_flag(s_container, LV_OBJ_FLAG_SCROLLABLE);

    /* tiêu đề + làm mới */
    lv_obj_t *head = lv_obj_create(s_container);
    lv_obj_remove_style_all(head);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_width(head, LV_PCT(100));
    lv_obj_set_height(head, LV_SIZE_CONTENT);
    lv_obj_clear_flag(head, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(head);
    lv_label_set_text(title, "Đơn hàng cần cân");
    lv_obj_set_style_text_font(title, UI_FONT_H2_BOLD, 0);
    lv_obj_set_style_text_color(title, UI_COLOR_HEADING, 0);

    lv_obj_t *refresh_btn = lv_button_create(head);
    lv_obj_set_style_bg_opa(refresh_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(refresh_btn, 0, 0);
    lv_obj_t *refresh_row = lv_obj_create(refresh_btn);
    lv_obj_remove_style_all(refresh_row);
    lv_obj_set_flex_flow(refresh_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(refresh_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(refresh_row, 6, 0);
    lv_obj_set_size(refresh_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_clear_flag(refresh_row, LV_OBJ_FLAG_SCROLLABLE);
    ui_common_icon(refresh_row, &img_icon_refresh, UI_COLOR_PRIMARY);
    lv_obj_t *refresh_lbl = lv_label_create(refresh_row);
    lv_label_set_text(refresh_lbl, "Làm mới");
    lv_obj_set_style_text_color(refresh_lbl, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(refresh_lbl, UI_FONT_BODY_BOLD, 0);
    lv_obj_add_event_cb(refresh_btn, refresh_btn_cb, LV_EVENT_CLICKED, NULL);

    /* hàng 4 nút lọc */
    lv_obj_t *filter_row = lv_obj_create(s_container);
    lv_obj_remove_style_all(filter_row);
    lv_obj_set_flex_flow(filter_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(filter_row, 8, 0);
    lv_obj_set_width(filter_row, LV_PCT(100));
    lv_obj_set_height(filter_row, LV_SIZE_CONTENT);
    lv_obj_clear_flag(filter_row, LV_OBJ_FLAG_SCROLLABLE);
    for (int i = 0; i < 4; i++) {
        lv_obj_t *btn = lv_button_create(filter_row);
        lv_obj_set_style_radius(btn, 999, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_pad_hor(btn, 14, 0);
        lv_obj_set_style_pad_ver(btn, 8, 0);
        lv_obj_set_height(btn, LV_SIZE_CONTENT);
        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, FILTER_LABELS[i]);
        lv_obj_set_style_text_font(lbl, UI_FONT_BODY_BOLD, 0);
        lv_obj_add_event_cb(btn, filter_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        s_filter_btns[i] = btn;
    }

    s_list_host = lv_obj_create(s_container);
    lv_obj_remove_style_all(s_list_host);
    lv_obj_set_flex_flow(s_list_host, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_list_host, 12, 0);
    lv_obj_set_width(s_list_host, LV_PCT(100));
    lv_obj_set_height(s_list_host, LV_SIZE_CONTENT);
    lv_obj_clear_flag(s_list_host, LV_OBJ_FLAG_SCROLLABLE);

    s_pager_host = lv_obj_create(s_container);
    lv_obj_remove_style_all(s_pager_host);
    lv_obj_set_flex_flow(s_pager_host, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_pager_host, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(s_pager_host, 8, 0);
    lv_obj_set_width(s_pager_host, LV_PCT(100));
    lv_obj_set_height(s_pager_host, LV_SIZE_CONTENT);
    lv_obj_clear_flag(s_pager_host, LV_OBJ_FLAG_SCROLLABLE);

    refresh_filter_highlight();
    return s_container;
}

static lv_obj_t *pager_btn(lv_obj_t *parent, const char *text, bool active, int page)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_size(btn, 36, 36);
    lv_obj_set_style_bg_color(btn, active ? UI_COLOR_PRIMARY : UI_COLOR_MIST_100, 0);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, active ? lv_color_white() : UI_COLOR_BODY, 0);
    lv_obj_set_style_text_font(lbl, UI_FONT_BODY_BOLD, 0);
    lv_obj_center(lbl);
    lv_obj_add_event_cb(btn, page_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)page);
    return btn;
}

void ui_orders_refresh(void)
{
    refresh_filter_highlight();
    ui_common_clear(s_list_host);
    ui_common_clear(s_pager_host);

    order_t *filtered[APP_MAX_ORDERS];
    int total = app_state_get_filtered_orders(filtered, APP_MAX_ORDERS);

    bool any_weighing = false;
    for (int i = 0; i < app_state()->order_count; i++) {
        if (app_state()->orders[i].status == ORDER_WEIGHING) { any_weighing = true; break; }
    }

    int page_count = (total + APP_ORDERS_PER_PAGE - 1) / APP_ORDERS_PER_PAGE;
    if (page_count < 1) page_count = 1;
    if (app_state()->order_page >= page_count) app_state()->order_page = page_count - 1;
    if (app_state()->order_page < 0) app_state()->order_page = 0;

    int start = app_state()->order_page * APP_ORDERS_PER_PAGE;
    int end = start + APP_ORDERS_PER_PAGE;
    if (end > total) end = total;

    if (total == 0) {
        lv_obj_t *empty = lv_label_create(s_list_host);
        lv_label_set_text(empty, "Không có đơn hàng phù hợp.");
        lv_obj_set_style_text_color(empty, UI_COLOR_BODY, 0);
        lv_obj_set_style_text_font(empty, UI_FONT_BODY, 0);
    }

    for (int i = start; i < end; i++) {
        build_order_card(s_list_host, filtered[i], any_weighing);
    }

    if (page_count > 1) {
        /* "‹"/"›" nằm trong dải Unicode đã nhúng ở font_inter_* — không dùng
         * LV_SYMBOL_LEFT/RIGHT (font Montserrat built-in) để tránh lẫn font. */
        pager_btn(s_pager_host, "‹", false, app_state()->order_page > 0 ? app_state()->order_page - 1 : 0);
        for (int p = 0; p < page_count; p++) {
            char buf[12];
            snprintf(buf, sizeof(buf), "%d", p + 1);
            pager_btn(s_pager_host, buf, p == app_state()->order_page, p);
        }
        pager_btn(s_pager_host, "›", false,
                  app_state()->order_page < page_count - 1 ? app_state()->order_page + 1 : page_count - 1);
    }
}
