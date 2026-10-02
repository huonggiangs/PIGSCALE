#include <stdint.h>
#include "ui_history.h"
#include "ui_theme.h"
#include "ui_common.h"
#include "app_state.h"
#include <stdio.h>
#include <string.h>

static lv_obj_t *s_container;
static lv_obj_t *s_search_ta;
static lv_obj_t *s_search_kb;
static lv_obj_t *s_summary_row;
static lv_obj_t *s_summary_tickets, *s_summary_qty, *s_summary_weight;
static lv_obj_t *s_list_host;
static lv_obj_t *s_audit_wrap;
static lv_obj_t *s_audit_host;

/* ── modal Huỷ phiếu (mục 4.9) ───────────────────────────────────────────── */
static lv_obj_t *s_cancel_overlay;
static lv_obj_t *s_cancel_reason_ta;
static lv_obj_t *s_cancel_kb;
static char s_cancel_receipt[24];

static void cancel_close_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_common_modal_close(s_cancel_overlay);
    s_cancel_overlay = NULL;
}

static void cancel_confirm_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    const char *reason = lv_textarea_get_text(s_cancel_reason_ta);
    app_state_t *st = app_state();
    employee_t *actor = (st->current_employee_idx >= 0) ? &st->employees[st->current_employee_idx] : NULL;
    app_state_cancel_ticket(s_cancel_receipt, reason[0] ? reason : "(không ghi lý do)",
                             actor ? actor->name : "?");
    ui_common_modal_close(s_cancel_overlay);
    s_cancel_overlay = NULL;
    ui_history_refresh();
}

static void cancel_ta_focus_cb(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_target(e);
    lv_keyboard_set_textarea(s_cancel_kb, ta);
}

static void open_cancel_modal(const char *receipt_no, const char *order_code)
{
    snprintf(s_cancel_receipt, sizeof(s_cancel_receipt), "%s", receipt_no);

    lv_obj_t *box = ui_common_modal_open(&s_cancel_overlay, 480, 460);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(box, 10, 0);

    lv_obj_t *title = lv_label_create(box);
    char buf[64];
    snprintf(buf, sizeof(buf), "Huỷ phiếu cân %s", order_code);
    lv_label_set_text(title, buf);
    lv_obj_set_style_text_font(title, UI_FONT_H4_BOLD, 0);
    lv_obj_set_style_text_color(title, UI_COLOR_HEADING, 0);

    lv_obj_t *reason_lbl = lv_label_create(box);
    lv_label_set_text(reason_lbl, "Lý do huỷ");
    lv_obj_set_style_text_font(reason_lbl, UI_FONT_BODY, 0);
    lv_obj_set_style_text_color(reason_lbl, UI_COLOR_BODY, 0);

    s_cancel_reason_ta = lv_textarea_create(box);
    lv_textarea_set_placeholder_text(s_cancel_reason_ta, "Nhập lý do...");
    lv_obj_set_style_text_font(s_cancel_reason_ta, UI_FONT_BODY, 0);
    lv_obj_set_width(s_cancel_reason_ta, LV_PCT(100));
    lv_obj_set_height(s_cancel_reason_ta, 70);
    lv_obj_add_event_cb(s_cancel_reason_ta, cancel_ta_focus_cb, LV_EVENT_FOCUSED, NULL);

    lv_obj_t *btn_row = lv_obj_create(box);
    lv_obj_remove_style_all(btn_row);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(btn_row, 10, 0);
    lv_obj_set_width(btn_row, LV_PCT(100));
    lv_obj_set_height(btn_row, LV_SIZE_CONTENT);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *close_btn = ui_common_button_outline(btn_row, "Đóng", UI_COLOR_BORDER, UI_COLOR_BODY, UI_FONT_BODY_BOLD);
    lv_obj_set_flex_grow(close_btn, 1);
    lv_obj_add_event_cb(close_btn, cancel_close_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *ok_btn = ui_common_button(btn_row, "Xác nhận huỷ", UI_COLOR_DANGER, lv_color_white(), UI_FONT_BODY_BOLD);
    lv_obj_set_flex_grow(ok_btn, 1);
    lv_obj_add_event_cb(ok_btn, cancel_confirm_cb, LV_EVENT_CLICKED, NULL);

    s_cancel_kb = lv_keyboard_create(box);
    lv_obj_set_height(s_cancel_kb, 150);
    lv_keyboard_set_textarea(s_cancel_kb, s_cancel_reason_ta);
}

static void cancel_btn_cb(lv_event_t *e)
{
    history_ticket_t *h = (history_ticket_t *)lv_event_get_user_data(e);
    open_cancel_modal(h->receipt_no, h->order_code);
}

/* ── tìm kiếm ─────────────────────────────────────────────────────────────── */
static void search_ta_event_cb(lv_event_t *e)
{
    uint16_t code = lv_event_get_code(e);
    if (code == LV_EVENT_FOCUSED) {
        lv_keyboard_set_textarea(s_search_kb, s_search_ta);
        lv_obj_clear_flag(s_search_kb, LV_OBJ_FLAG_HIDDEN);
    } else if (code == LV_EVENT_DEFOCUSED || code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        lv_obj_add_flag(s_search_kb, LV_OBJ_FLAG_HIDDEN);
    } else if (code == LV_EVENT_VALUE_CHANGED) {
        ui_history_refresh();
    }
}

static void build_ticket_card(lv_obj_t *parent, history_ticket_t *h)
{
    app_state_t *st = app_state();
    employee_t *actor = (st->current_employee_idx >= 0) ? &st->employees[st->current_employee_idx] : NULL;
    bool can_cancel = actor && (actor->role == ROLE_MANAGER || actor->role == ROLE_TECHNICIAN);

    lv_obj_t *card = ui_common_card(parent);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 4, 0);

    lv_obj_t *head = lv_obj_create(card);
    lv_obj_remove_style_all(head);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(head, 8, 0);
    lv_obj_set_width(head, LV_PCT(100));
    lv_obj_set_height(head, LV_SIZE_CONTENT);
    lv_obj_clear_flag(head, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *code = lv_label_create(head);
    lv_label_set_text(code, h->order_code);
    lv_obj_set_style_text_font(code, UI_FONT_H4_BOLD, 0);
    lv_obj_set_style_text_color(code, UI_COLOR_HEADING, 0);
    bool is_import = (h->type == TICKET_IMPORT);
    ui_common_badge(head, is_import ? "Nhập" : "Xuất",
                     is_import ? UI_COLOR_SUCCESS_BG : UI_COLOR_WARNING_BG,
                     is_import ? UI_COLOR_SUCCESS : UI_COLOR_ORANGE_700);

    char buf[128];
    lv_obj_t *cust = lv_label_create(card);
    snprintf(buf, sizeof(buf), "%s · %s", h->customer, h->plate);
    lv_label_set_text(cust, buf);
    lv_obj_set_style_text_font(cust, UI_FONT_BODY, 0);
    lv_obj_set_style_text_color(cust, UI_COLOR_BODY, 0);

    lv_obj_t *stat = lv_label_create(card);
    char wbuf[32]; ui_fmt_weight_kg(wbuf, sizeof(wbuf), h->actual_weight_kg);
    snprintf(buf, sizeof(buf), "%d con thực tế · %s kg%s", h->actual_qty, wbuf,
             h->is_manual ? " · Nhập tay" : "");
    lv_label_set_text(stat, buf);
    lv_obj_set_style_text_font(stat, UI_FONT_BODY_BOLD, 0);
    lv_obj_set_style_text_color(stat, UI_COLOR_HEADING, 0);

    lv_obj_t *cancel_btn;
    if (can_cancel) {
        cancel_btn = ui_common_button_outline(card, "Huỷ phiếu", UI_COLOR_RED_OUTLINE, UI_COLOR_DANGER, UI_FONT_BODY_BOLD);
        lv_obj_set_width(cancel_btn, LV_PCT(100));
        lv_obj_set_style_pad_top(cancel_btn, 4, 0);
        lv_obj_add_event_cb(cancel_btn, cancel_btn_cb, LV_EVENT_CLICKED, h);
    } else {
        cancel_btn = ui_common_button(card, "Cần quyền quản lý", UI_COLOR_MIST_100, UI_COLOR_ICON, UI_FONT_BODY_BOLD);
        lv_obj_set_width(cancel_btn, LV_PCT(100));
        lv_obj_set_style_pad_top(cancel_btn, 4, 0);
        lv_obj_add_state(cancel_btn, LV_STATE_DISABLED);
    }
}

lv_obj_t *ui_history_create(lv_obj_t *parent)
{
    s_container = lv_obj_create(parent);
    lv_obj_remove_style_all(s_container);
    lv_obj_set_size(s_container, UI_HOR_RES, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(s_container, UI_PAD_SCREEN, 0);
    lv_obj_set_flex_flow(s_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_container, 12, 0);
    lv_obj_clear_flag(s_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(s_container);
    lv_label_set_text(title, "Lịch sử cân");
    lv_obj_set_style_text_font(title, UI_FONT_H2_BOLD, 0);
    lv_obj_set_style_text_color(title, UI_COLOR_HEADING, 0);

    s_search_ta = lv_textarea_create(s_container);
    lv_textarea_set_one_line(s_search_ta, true);
    lv_textarea_set_placeholder_text(s_search_ta, "Tìm theo mã đơn / khách hàng / biển số");
    lv_obj_set_style_text_font(s_search_ta, UI_FONT_BODY, 0);
    lv_obj_set_width(s_search_ta, LV_PCT(100));
    lv_obj_add_event_cb(s_search_ta, search_ta_event_cb, LV_EVENT_ALL, NULL);

    s_summary_row = lv_obj_create(s_container);
    lv_obj_remove_style_all(s_summary_row);
    lv_obj_set_flex_flow(s_summary_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(s_summary_row, 10, 0);
    lv_obj_set_width(s_summary_row, LV_PCT(100));
    lv_obj_set_height(s_summary_row, LV_SIZE_CONTENT);
    lv_obj_clear_flag(s_summary_row, LV_OBJ_FLAG_SCROLLABLE);
    /* ui_common_card() không tự đặt flex layout — phải set COLUMN ở đây,
     * nếu không 2 label con (tiêu đề nhỏ + số lớn) đều mặc định nằm ở
     * (0,0) và CHỒNG KHÍT lên nhau (lỗi "ký tự chồng lên nhau" trên card
     * Tổng số phiếu/Tổng số con/Tổng khối lượng). */
    lv_obj_t *c1 = ui_common_card(s_summary_row); lv_obj_set_flex_grow(c1, 1);
    lv_obj_set_flex_flow(c1, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(c1, 4, 0);
    lv_obj_t *t1 = lv_label_create(c1); lv_label_set_text(t1, "Tổng số phiếu");
    lv_obj_set_style_text_font(t1, UI_FONT_XS, 0); lv_obj_set_style_text_color(t1, UI_COLOR_BODY, 0);
    s_summary_tickets = lv_label_create(c1);
    lv_obj_set_style_text_font(s_summary_tickets, UI_FONT_H3_BOLD, 0);
    lv_obj_set_style_text_color(s_summary_tickets, UI_COLOR_HEADING, 0);

    lv_obj_t *c2 = ui_common_card(s_summary_row); lv_obj_set_flex_grow(c2, 1);
    lv_obj_set_flex_flow(c2, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(c2, 4, 0);
    lv_obj_t *t2 = lv_label_create(c2); lv_label_set_text(t2, "Tổng số con");
    lv_obj_set_style_text_font(t2, UI_FONT_XS, 0); lv_obj_set_style_text_color(t2, UI_COLOR_BODY, 0);
    s_summary_qty = lv_label_create(c2);
    lv_obj_set_style_text_font(s_summary_qty, UI_FONT_H3_BOLD, 0);
    lv_obj_set_style_text_color(s_summary_qty, UI_COLOR_HEADING, 0);

    lv_obj_t *c3 = ui_common_card(s_summary_row); lv_obj_set_flex_grow(c3, 1);
    lv_obj_set_flex_flow(c3, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(c3, 4, 0);
    lv_obj_t *t3 = lv_label_create(c3); lv_label_set_text(t3, "Tổng khối lượng");
    lv_obj_set_style_text_font(t3, UI_FONT_XS, 0); lv_obj_set_style_text_color(t3, UI_COLOR_BODY, 0);
    s_summary_weight = lv_label_create(c3);
    lv_obj_set_style_text_font(s_summary_weight, UI_FONT_H3_BOLD, 0);
    lv_obj_set_style_text_color(s_summary_weight, UI_COLOR_HEADING, 0);

    s_list_host = lv_obj_create(s_container);
    lv_obj_remove_style_all(s_list_host);
    lv_obj_set_flex_flow(s_list_host, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_list_host, 12, 0);
    lv_obj_set_width(s_list_host, LV_PCT(100));
    lv_obj_set_height(s_list_host, LV_SIZE_CONTENT);
    lv_obj_clear_flag(s_list_host, LV_OBJ_FLAG_SCROLLABLE);

    s_audit_wrap = lv_obj_create(s_container);
    lv_obj_remove_style_all(s_audit_wrap);
    lv_obj_set_flex_flow(s_audit_wrap, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_audit_wrap, 6, 0);
    lv_obj_set_width(s_audit_wrap, LV_PCT(100));
    lv_obj_set_height(s_audit_wrap, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_bottom(s_audit_wrap, 20, 0);
    lv_obj_clear_flag(s_audit_wrap, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *audit_title = lv_label_create(s_audit_wrap);
    lv_label_set_text(audit_title, "Nhật ký chỉnh sửa");
    lv_obj_set_style_text_font(audit_title, UI_FONT_H4_BOLD, 0);
    lv_obj_set_style_text_color(audit_title, UI_COLOR_HEADING, 0);
    s_audit_host = lv_obj_create(s_audit_wrap);
    lv_obj_remove_style_all(s_audit_host);
    lv_obj_set_flex_flow(s_audit_host, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_audit_host, 6, 0);
    lv_obj_set_width(s_audit_host, LV_PCT(100));
    lv_obj_set_height(s_audit_host, LV_SIZE_CONTENT);
    lv_obj_clear_flag(s_audit_host, LV_OBJ_FLAG_SCROLLABLE);

    s_search_kb = lv_keyboard_create(lv_obj_get_parent(s_container));
    lv_obj_set_height(s_search_kb, 200);
    lv_obj_align(s_search_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(s_search_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_search_kb);

    return s_container;
}

void ui_history_refresh(void)
{
    ui_common_clear(s_list_host);
    ui_common_clear(s_audit_host);

    const char *kw = lv_textarea_get_text(s_search_ta);
    history_ticket_t *results[APP_MAX_HISTORY];
    int total = app_state_search_history(kw, results, APP_MAX_HISTORY);

    int tickets = 0, qty_sum = 0;
    float weight_sum = 0;
    for (int i = 0; i < total; i++) {
        if (results[i]->cancelled) continue;
        tickets++;
        qty_sum += results[i]->actual_qty;
        weight_sum += results[i]->actual_weight_kg;
    }

    char buf[48];
    snprintf(buf, sizeof(buf), "%d", tickets);
    lv_label_set_text(s_summary_tickets, buf);
    snprintf(buf, sizeof(buf), "%d", qty_sum);
    lv_label_set_text(s_summary_qty, buf);
    char wbuf[32]; ui_fmt_weight_kg(wbuf, sizeof(wbuf), weight_sum);
    snprintf(buf, sizeof(buf), "%s kg", wbuf);
    lv_label_set_text(s_summary_weight, buf);

    bool any = false;
    for (int i = 0; i < total; i++) {
        if (results[i]->cancelled) continue;
        build_ticket_card(s_list_host, results[i]);
        any = true;
    }
    if (!any) {
        lv_obj_t *empty = lv_label_create(s_list_host);
        lv_label_set_text(empty, "Không có phiếu cân phù hợp.");
        lv_obj_set_style_text_color(empty, UI_COLOR_BODY, 0);
        lv_obj_set_style_text_font(empty, UI_FONT_BODY, 0);
    }

    app_state_t *st = app_state();
    if (st->audit_log_count > 0) {
        lv_obj_clear_flag(s_audit_wrap, LV_OBJ_FLAG_HIDDEN);
        for (int i = 0; i < st->audit_log_count; i++) {
            audit_log_t *log = &st->audit_log[i];
            lv_obj_t *row = lv_label_create(s_audit_host);
            char lbuf[256];
            snprintf(lbuf, sizeof(lbuf), "%s — %s bởi %s lúc %s. Lý do: %s",
                     log->order_code, log->action, log->actor, log->timestamp, log->reason);
            lv_label_set_text(row, lbuf);
            lv_label_set_long_mode(row, LV_LABEL_LONG_WRAP);
            lv_obj_set_width(row, LV_PCT(100));
            lv_obj_set_style_text_font(row, UI_FONT_BODY, 0);
            lv_obj_set_style_text_color(row, UI_COLOR_BODY, 0);
        }
    } else {
        lv_obj_add_flag(s_audit_wrap, LV_OBJ_FLAG_HIDDEN);
    }
}
