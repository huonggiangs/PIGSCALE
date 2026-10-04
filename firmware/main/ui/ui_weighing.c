#include <stdint.h>
#include "ui_weighing.h"
#include "ui_theme.h"
#include "ui_common.h"
#include "ui_shell.h"
#include "app_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static lv_obj_t *s_container;
static lv_obj_t *s_empty_msg;
static lv_obj_t *s_active_wrap;

static lv_obj_t *s_order_title;
static lv_obj_t *s_camera_wrap;
static lv_obj_t *s_camera_status_dot;   /* màu theo app_state()->camera_link — real-time */
static lv_obj_t *s_camera_lost_wrap;    /* ghi chú tĩnh "xem trực tiếp chưa khả dụng" */
static lv_obj_t *s_camera_overlay_label;  /* chữ trạng thái lớn: Đã/Chưa kết nối */

static lv_obj_t *s_weight_value_label;
static lv_obj_t *s_weight_status_label;
static lv_obj_t *s_line_count_label;
static lv_obj_t *s_snapshot_count_label;

static lv_obj_t *s_manual_btn;
static lv_obj_t *s_reconcile_bar;
static lv_obj_t *s_reconcile_label;

static lv_obj_t *s_ticket_import_btn, *s_ticket_export_btn;
static lv_obj_t *s_ticket_summary_label;
static lv_obj_t *s_print_btn;

static lv_obj_t *s_reweigh_btn;
static lv_obj_t *s_reweigh_label;
static lv_obj_t *s_confirm_btn;
static lv_obj_t *s_confirm_label;

/* ── modal Nhập tay ──────────────────────────────────────────────────────── */
static lv_obj_t *s_manual_overlay;
static lv_obj_t *s_manual_weight_ta;
static lv_obj_t *s_manual_qty_ta;
static lv_obj_t *s_manual_kb;

/* ── modal Xác nhận ──────────────────────────────────────────────────────── */
static void open_confirm_modal(void);

static const char *p5_state_text(p5_state_t s)
{
    switch (s) {
        case P5_STATE_STABLE: return "ỔN ĐỊNH";
        case P5_STATE_MOVING: return "ĐANG ĐỘNG";
        case P5_STATE_LOST:   return "MẤT KẾT NỐI P5 SCALE";
        default:               return "NHẬP THỦ CÔNG";
    }
}
static lv_color_t p5_state_color(p5_state_t s)
{
    switch (s) {
        case P5_STATE_STABLE: return UI_COLOR_SUCCESS;
        case P5_STATE_LOST:   return UI_COLOR_DANGER;
        case P5_STATE_MANUAL: return UI_COLOR_WARNING;
        default:               return UI_COLOR_PRIMARY;
    }
}
static const char *reconcile_text(reconcile_status_t r)
{
    switch (r) {
        case RECONCILE_MATCH:    return "KHỚP";
        case RECONCILE_MISMATCH: return "LỆCH";
        case RECONCILE_UNSURE:   return "CHƯA CHẮC";
        case RECONCILE_INVALID:  return "KHÔNG HỢP LỆ";
        case RECONCILE_MANUAL:   return "NHẬP THỦ CÔNG";
        default: return "";
    }
}
static lv_color_t reconcile_bg(reconcile_status_t r)
{
    switch (r) {
        case RECONCILE_MATCH:  return UI_COLOR_SUCCESS_BG;
        case RECONCILE_UNSURE:
        case RECONCILE_MANUAL: return UI_COLOR_WARNING_BG;
        default:                return UI_COLOR_DANGER_BG;
    }
}
static lv_color_t reconcile_fg(reconcile_status_t r)
{
    switch (r) {
        case RECONCILE_MATCH:  return UI_COLOR_SUCCESS;
        case RECONCILE_UNSURE:
        case RECONCILE_MANUAL: return UI_COLOR_ORANGE_700;
        default:                return UI_COLOR_DANGER;
    }
}

static lv_obj_t *make_stat_card(lv_obj_t *parent, const char *title, lv_obj_t **out_value_label)
{
    lv_obj_t *card = ui_common_card(parent);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_width(card, LV_PCT(100));

    lv_obj_t *t = lv_label_create(card);
    lv_label_set_text(t, title);
    lv_obj_set_style_text_font(t, UI_FONT_XS, 0);
    lv_obj_set_style_text_color(t, UI_COLOR_BODY, 0);

    lv_obj_t *v = lv_label_create(card);
    lv_label_set_text(v, "--");
    lv_obj_set_style_text_font(v, UI_FONT_H2_BOLD, 0);
    lv_obj_set_style_text_color(v, UI_COLOR_HEADING, 0);
    *out_value_label = v;
    return card;
}

/* ── nút nhập tay ─────────────────────────────────────────────────────────── */
static void manual_kb_event_cb(lv_event_t *e)
{
    lv_obj_t *kb = lv_event_get_target(e);
    uint16_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        LV_UNUSED(kb);
    }
}

static void manual_done_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    const char *w_txt = lv_textarea_get_text(s_manual_weight_ta);
    const char *q_txt = lv_textarea_get_text(s_manual_qty_ta);
    float w = (float)atof(w_txt);
    int q = atoi(q_txt);
    if (w <= 0 || q <= 0) {
        ui_shell_toast("Nhập đủ Khối lượng và Số con");
        return;
    }
    app_state_weighing_manual_input(w, q);
    ui_common_modal_close(s_manual_overlay);
    s_manual_overlay = NULL;
    ui_weighing_refresh();
    ui_shell_toast("Đã ghi nhận số liệu nhập tay");
}

static void manual_ta_focus_cb(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_target(e);
    lv_keyboard_set_textarea(s_manual_kb, ta);
}

static void open_manual_modal(void)
{
    lv_obj_t *box = ui_common_modal_open(&s_manual_overlay, 480, 520);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(box, 12, 0);

    lv_obj_t *title = lv_label_create(box);
    lv_label_set_text(title, "Nhập tay khối lượng / số con");
    lv_obj_set_style_text_font(title, UI_FONT_H4_BOLD, 0);
    lv_obj_set_style_text_color(title, UI_COLOR_HEADING, 0);

    lv_obj_t *w_lbl = lv_label_create(box);
    lv_label_set_text(w_lbl, "Khối lượng (kg)");
    lv_obj_set_style_text_font(w_lbl, UI_FONT_BODY, 0);
    lv_obj_set_style_text_color(w_lbl, UI_COLOR_BODY, 0);
    s_manual_weight_ta = lv_textarea_create(box);
    lv_textarea_set_one_line(s_manual_weight_ta, true);
    lv_textarea_set_accepted_chars(s_manual_weight_ta, "0123456789.");
    lv_obj_set_style_text_font(s_manual_weight_ta, UI_FONT_BODY, 0);
    lv_obj_set_width(s_manual_weight_ta, LV_PCT(100));
    lv_obj_add_event_cb(s_manual_weight_ta, manual_ta_focus_cb, LV_EVENT_FOCUSED, NULL);

    lv_obj_t *q_lbl = lv_label_create(box);
    lv_label_set_text(q_lbl, "Số con");
    lv_obj_set_style_text_font(q_lbl, UI_FONT_BODY, 0);
    lv_obj_set_style_text_color(q_lbl, UI_COLOR_BODY, 0);
    s_manual_qty_ta = lv_textarea_create(box);
    lv_textarea_set_one_line(s_manual_qty_ta, true);
    lv_textarea_set_accepted_chars(s_manual_qty_ta, "0123456789");
    lv_obj_set_style_text_font(s_manual_qty_ta, UI_FONT_BODY, 0);
    lv_obj_set_width(s_manual_qty_ta, LV_PCT(100));
    lv_obj_add_event_cb(s_manual_qty_ta, manual_ta_focus_cb, LV_EVENT_FOCUSED, NULL);

    lv_obj_t *done = ui_common_button(box, "Xong", UI_COLOR_PRIMARY, lv_color_white(), UI_FONT_BODY_BOLD);
    lv_obj_set_width(done, LV_PCT(100));
    lv_obj_add_event_cb(done, manual_done_cb, LV_EVENT_CLICKED, NULL);

    s_manual_kb = lv_keyboard_create(box);
    lv_keyboard_set_mode(s_manual_kb, LV_KEYBOARD_MODE_NUMBER);
    lv_obj_set_height(s_manual_kb, 160);
    lv_obj_add_event_cb(s_manual_kb, manual_kb_event_cb, LV_EVENT_ALL, NULL);
    lv_keyboard_set_textarea(s_manual_kb, s_manual_weight_ta);
}

static void manual_btn_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    open_manual_modal();
}

static void reweigh_btn_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    app_state_weighing_reweigh();
    ui_weighing_refresh();
}

static void confirm_btn_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (!app_state_weighing_can_confirm()) return;
    open_confirm_modal();
}

static void ticket_type_cb(lv_event_t *e)
{
    ticket_type_t t = (ticket_type_t)(intptr_t)lv_event_get_user_data(e);
    app_state_weighing_set_ticket_type(t);
    ui_weighing_refresh();
}

/* ── modal xác nhận (mục 4.6) ────────────────────────────────────────────── */
static lv_obj_t *s_confirm_overlay;

static void confirm_modal_back_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_common_modal_close(s_confirm_overlay);
    s_confirm_overlay = NULL;
}

static void confirm_modal_confirm_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    app_state_weighing_confirm();
    ui_common_modal_close(s_confirm_overlay);
    s_confirm_overlay = NULL;
    ui_shell_refresh_chrome();
    ui_weighing_refresh();
    ui_shell_toast("Đã chốt phiếu cân");
}

static lv_obj_t *confirm_row(lv_obj_t *parent, const char *label, const char *value, lv_color_t value_color)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *l = lv_label_create(row);
    lv_label_set_text(l, label);
    lv_obj_set_style_text_font(l, UI_FONT_BODY, 0);
    lv_obj_set_style_text_color(l, UI_COLOR_BODY, 0);

    lv_obj_t *v = lv_label_create(row);
    lv_label_set_text(v, value);
    lv_obj_set_style_text_font(v, UI_FONT_BODY_BOLD, 0);
    lv_obj_set_style_text_color(v, value_color, 0);
    return row;
}

static void open_confirm_modal(void)
{
    weighing_session_t *w = &app_state()->weighing;
    order_t *o = app_state_find_order(w->order_id);
    if (!o) return;

    lv_obj_t *box = ui_common_modal_open(&s_confirm_overlay, 480, 420);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(box, 10, 0);

    lv_obj_t *title = lv_label_create(box);
    lv_label_set_text(title, "Xác nhận kết quả cân");
    lv_obj_set_style_text_font(title, UI_FONT_H4_BOLD, 0);
    lv_obj_set_style_text_color(title, UI_COLOR_HEADING, 0);
    lv_obj_set_style_pad_bottom(title, 8, 0);

    char buf[64];
    confirm_row(box, "Đơn hàng", o->code, UI_COLOR_HEADING);
    int qty = w->manual_mode ? w->manual_qty : (w->has_line_count ? w->line_count : w->snapshot_count);
    snprintf(buf, sizeof(buf), "%d con", qty);
    confirm_row(box, "Số lượng", buf, UI_COLOR_HEADING);
    ui_fmt_weight_kg(buf, sizeof(buf), w->weight_kg);
    strncat(buf, " kg", sizeof(buf) - strlen(buf) - 1);
    confirm_row(box, "Khối lượng", buf, UI_COLOR_HEADING);
    float avg = qty > 0 ? (w->weight_kg / qty) : 0;
    ui_fmt_weight_kg(buf, sizeof(buf), avg);
    strncat(buf, " kg/con", sizeof(buf) - strlen(buf) - 1);
    confirm_row(box, "Trung bình", buf, UI_COLOR_HEADING);
    confirm_row(box, "Đối chiếu", reconcile_text(w->reconcile), reconcile_fg(w->reconcile));

    lv_obj_t *btn_row = lv_obj_create(box);
    lv_obj_remove_style_all(btn_row);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(btn_row, 10, 0);
    lv_obj_set_style_pad_top(btn_row, 16, 0);
    lv_obj_set_width(btn_row, LV_PCT(100));
    lv_obj_set_height(btn_row, LV_SIZE_CONTENT);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *back = ui_common_button_outline(btn_row, "Quay lại", UI_COLOR_BORDER, UI_COLOR_BODY, UI_FONT_BODY_BOLD);
    lv_obj_set_flex_grow(back, 1);
    lv_obj_add_event_cb(back, confirm_modal_back_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *ok = ui_common_button(btn_row, "Xác nhận", UI_COLOR_SUCCESS, lv_color_white(), UI_FONT_BODY_BOLD);
    lv_obj_set_flex_grow(ok, 1);
    lv_obj_add_event_cb(ok, confirm_modal_confirm_cb, LV_EVENT_CLICKED, NULL);
}

/* ────────────────────────────────────────────────────────────────────────
 * Dựng khung tĩnh (gọi 1 lần)
 * ──────────────────────────────────────────────────────────────────────── */
lv_obj_t *ui_weighing_create(lv_obj_t *parent)
{
    s_container = lv_obj_create(parent);
    lv_obj_remove_style_all(s_container);
    lv_obj_set_size(s_container, UI_HOR_RES, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(s_container, UI_PAD_SCREEN, 0);
    lv_obj_clear_flag(s_container, LV_OBJ_FLAG_SCROLLABLE);

    s_empty_msg = lv_label_create(s_container);
    lv_label_set_text(s_empty_msg, "Chưa có đơn hàng đang cân.\nChọn một đơn ở tab Đơn hàng để bắt đầu.");
    lv_obj_set_style_text_align(s_empty_msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(s_empty_msg, UI_FONT_H4, 0);
    lv_obj_set_style_text_color(s_empty_msg, UI_COLOR_BODY, 0);
    lv_obj_center(s_empty_msg);

    s_active_wrap = lv_obj_create(s_container);
    lv_obj_remove_style_all(s_active_wrap);
    lv_obj_set_width(s_active_wrap, LV_PCT(100));
    lv_obj_set_height(s_active_wrap, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s_active_wrap, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_active_wrap, 12, 0);
    lv_obj_clear_flag(s_active_wrap, LV_OBJ_FLAG_SCROLLABLE);

    s_order_title = lv_label_create(s_active_wrap);
    lv_obj_set_style_text_font(s_order_title, UI_FONT_H3_BOLD, 0);
    lv_obj_set_style_text_color(s_order_title, UI_COLOR_HEADING, 0);

    /* hàng: camera (trái) + 3 thẻ số liệu (phải) */
    lv_obj_t *top_row = lv_obj_create(s_active_wrap);
    lv_obj_remove_style_all(top_row);
    lv_obj_set_flex_flow(top_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(top_row, 12, 0);
    lv_obj_set_width(top_row, LV_PCT(100));
    lv_obj_set_height(top_row, LV_SIZE_CONTENT);
    lv_obj_clear_flag(top_row, LV_OBJ_FLAG_SCROLLABLE);

    /* Khung camera: trước đây hiện ẢNH TĨNH HARDCODE (img_camera_01_frame —
     * một khung hình/"ảnh heo" giả lập cố định, không phải video thật) kèm
     * dòng chữ "Đếm theo chiều dài thân · N heo" lấy từ số liệu mô phỏng
     * (app_state_sim_tick cũ) — cả hai đều đã bỏ. Chưa có URL RTSP/ONVIF
     * thật của camera Vivoo (xem network/camera_client.h) nên KHÔNG hiện
     * video giả — chỉ hiện TRẠNG THÁI KẾT NỐI THẬT (real-time, cập nhật
     * mỗi tick qua app_state()->camera_link — xem app_state_camera_sync). */
    s_camera_wrap = lv_obj_create(top_row);
    lv_obj_remove_style_all(s_camera_wrap);
    lv_obj_set_size(s_camera_wrap, 460, 259); /* 16:9 */
    lv_obj_set_style_radius(s_camera_wrap, UI_RADIUS_CARD, 0);
    lv_obj_set_style_clip_corner(s_camera_wrap, true, 0);
    lv_obj_set_style_bg_color(s_camera_wrap, UI_COLOR_HEADING, 0);
    lv_obj_set_style_bg_opa(s_camera_wrap, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(s_camera_wrap, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_camera_wrap, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_camera_wrap, 10, 0);
    lv_obj_clear_flag(s_camera_wrap, LV_OBJ_FLAG_SCROLLABLE);

    s_camera_status_dot = ui_common_status_dot(s_camera_wrap, LINK_LOST);

    s_camera_overlay_label = lv_label_create(s_camera_wrap);
    lv_obj_set_style_text_font(s_camera_overlay_label, UI_FONT_H4_BOLD, 0);
    lv_obj_set_style_text_color(s_camera_overlay_label, lv_color_white(), 0);

    s_camera_lost_wrap = lv_label_create(s_camera_wrap);
    lv_label_set_long_mode(s_camera_lost_wrap, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_camera_lost_wrap, 380);
    lv_obj_set_style_text_align(s_camera_lost_wrap, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_camera_lost_wrap, "Xem trực tiếp chưa khả dụng — cần URL RTSP/ONVIF do nhà sản xuất xác nhận");
    lv_obj_set_style_text_font(s_camera_lost_wrap, UI_FONT_XS, 0);
    lv_obj_set_style_text_color(s_camera_lost_wrap, UI_COLOR_ON_DARK_HINT, 0);

    lv_obj_t *stat_col = lv_obj_create(top_row);
    lv_obj_remove_style_all(stat_col);
    lv_obj_set_flex_flow(stat_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(stat_col, 10, 0);
    lv_obj_set_flex_grow(stat_col, 1);
    lv_obj_set_height(stat_col, LV_SIZE_CONTENT);
    lv_obj_clear_flag(stat_col, LV_OBJ_FLAG_SCROLLABLE);

    make_stat_card(stat_col, "KHỐI LƯỢNG P5 SCALE", &s_weight_value_label);
    s_weight_status_label = lv_label_create(lv_obj_get_parent(s_weight_value_label));
    lv_obj_set_style_text_font(s_weight_status_label, UI_FONT_XS, 0);

    make_stat_card(stat_col, "SỐ CON QUA VẠCH", &s_line_count_label);
    make_stat_card(stat_col, "SỐ CON ẢNH TĨNH", &s_snapshot_count_label);

    /* nút nhập tay */
    s_manual_btn = ui_common_button(s_active_wrap, "Nhập tay khối lượng / số con",
                                     UI_COLOR_WARNING_BG, UI_COLOR_ORANGE_700, UI_FONT_BODY_BOLD);
    lv_obj_set_width(s_manual_btn, LV_PCT(100));
    lv_obj_add_event_cb(s_manual_btn, manual_btn_cb, LV_EVENT_CLICKED, NULL);

    /* thanh đối chiếu */
    s_reconcile_bar = lv_obj_create(s_active_wrap);
    lv_obj_remove_style_all(s_reconcile_bar);
    lv_obj_set_width(s_reconcile_bar, LV_PCT(100));
    lv_obj_set_height(s_reconcile_bar, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(s_reconcile_bar, 10, 0);
    lv_obj_set_style_pad_all(s_reconcile_bar, 12, 0);
    lv_obj_clear_flag(s_reconcile_bar, LV_OBJ_FLAG_SCROLLABLE);
    s_reconcile_label = lv_label_create(s_reconcile_bar);
    lv_obj_set_style_text_font(s_reconcile_label, UI_FONT_H4_BOLD, 0);
    lv_obj_center(s_reconcile_label);

    /* thẻ phiếu cân */
    lv_obj_t *ticket_card = ui_common_card(s_active_wrap);
    lv_obj_set_flex_flow(ticket_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(ticket_card, 8, 0);

    lv_obj_t *ticket_title = lv_label_create(ticket_card);
    lv_label_set_text(ticket_title, "PHIẾU CÂN");
    lv_obj_set_style_text_font(ticket_title, UI_FONT_H5_BOLD, 0);
    lv_obj_set_style_text_color(ticket_title, UI_COLOR_HEADING, 0);

    lv_obj_t *ticket_type_row = lv_obj_create(ticket_card);
    lv_obj_remove_style_all(ticket_type_row);
    lv_obj_set_flex_flow(ticket_type_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(ticket_type_row, 8, 0);
    lv_obj_set_width(ticket_type_row, LV_PCT(100));
    lv_obj_set_height(ticket_type_row, LV_SIZE_CONTENT);
    lv_obj_clear_flag(ticket_type_row, LV_OBJ_FLAG_SCROLLABLE);
    s_ticket_import_btn = ui_common_button(ticket_type_row, "Nhập", UI_COLOR_PRIMARY, lv_color_white(), UI_FONT_BODY_BOLD);
    lv_obj_set_flex_grow(s_ticket_import_btn, 1);
    lv_obj_add_event_cb(s_ticket_import_btn, ticket_type_cb, LV_EVENT_CLICKED, (void *)(intptr_t)TICKET_IMPORT);
    s_ticket_export_btn = ui_common_button(ticket_type_row, "Xuất", UI_COLOR_MIST_100, UI_COLOR_BODY, UI_FONT_BODY_BOLD);
    lv_obj_set_flex_grow(s_ticket_export_btn, 1);
    lv_obj_add_event_cb(s_ticket_export_btn, ticket_type_cb, LV_EVENT_CLICKED, (void *)(intptr_t)TICKET_EXPORT);

    s_ticket_summary_label = lv_label_create(ticket_card);
    lv_obj_set_style_text_font(s_ticket_summary_label, UI_FONT_BODY, 0);
    lv_obj_set_style_text_color(s_ticket_summary_label, UI_COLOR_BODY, 0);
    lv_label_set_long_mode(s_ticket_summary_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_ticket_summary_label, LV_PCT(100));

    s_print_btn = ui_common_button(ticket_card, "In phiếu cân", UI_COLOR_MIST_100, UI_COLOR_BODY, UI_FONT_BODY_BOLD);
    lv_obj_set_width(s_print_btn, LV_PCT(100));

    /* 2 nút cuối */
    lv_obj_t *bottom_row = lv_obj_create(s_active_wrap);
    lv_obj_remove_style_all(bottom_row);
    lv_obj_set_flex_flow(bottom_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(bottom_row, 10, 0);
    lv_obj_set_width(bottom_row, LV_PCT(100));
    lv_obj_set_height(bottom_row, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_bottom(bottom_row, 20, 0);
    lv_obj_clear_flag(bottom_row, LV_OBJ_FLAG_SCROLLABLE);

    s_reweigh_btn = ui_common_button(bottom_row, "Cân lại", UI_COLOR_MIST_100, UI_COLOR_BODY, UI_FONT_BODY_BOLD);
    lv_obj_set_flex_grow(s_reweigh_btn, 1);
    s_reweigh_label = lv_obj_get_child(s_reweigh_btn, 0);
    lv_obj_add_event_cb(s_reweigh_btn, reweigh_btn_cb, LV_EVENT_CLICKED, NULL);

    s_confirm_btn = ui_common_button(bottom_row, "Xác nhận", UI_COLOR_SUCCESS, lv_color_white(), UI_FONT_BODY_BOLD);
    lv_obj_set_flex_grow(s_confirm_btn, 2);
    s_confirm_label = lv_obj_get_child(s_confirm_btn, 0);
    lv_obj_add_event_cb(s_confirm_btn, confirm_btn_cb, LV_EVENT_CLICKED, NULL);

    return s_container;
}

/* ────────────────────────────────────────────────────────────────────────
 * Cập nhật theo trạng thái (gọi mỗi lần vào tab + mỗi tick mô phỏng)
 * ──────────────────────────────────────────────────────────────────────── */
void ui_weighing_refresh(void)
{
    weighing_session_t *w = &app_state()->weighing;

    if (!w->active) {
        lv_obj_clear_flag(s_empty_msg, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_active_wrap, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_add_flag(s_empty_msg, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_active_wrap, LV_OBJ_FLAG_HIDDEN);

    order_t *o = app_state_find_order(w->order_id);
    char buf[256];
    if (o) {
        snprintf(buf, sizeof(buf), "%s — %s", o->code, o->customer);
        lv_label_set_text(s_order_title, buf);
    }

    /* Đọc TRỰC TIẾP app_state()->camera_link (không dùng w->camera_connected
     * — đó chỉ là ảnh chụp trạng thái lúc BẮT ĐẦU phiên cân) để trạng thái
     * hiển thị ở đây thật sự "real-time", theo đúng kết quả tự kiểm tra lại
     * định kỳ của app_state_camera_sync(). */
    link_state_t cam_link = app_state()->camera_link;
    bool cam_checking = app_state_camera_is_checking();
    lv_obj_set_style_bg_color(s_camera_status_dot, ui_common_link_color(cam_link), 0);
    lv_label_set_text(s_camera_overlay_label, cam_checking ? "Đang kiểm tra..." :
                       (cam_link == LINK_OK ? "Camera: Đã kết nối" : "Camera: Chưa kết nối"));

    ui_fmt_weight_kg(buf, sizeof(buf), w->weight_kg);
    char wbuf[sizeof(buf) + 8];
    snprintf(wbuf, sizeof(wbuf), "%s kg", buf);
    lv_label_set_text(s_weight_value_label, wbuf);
    lv_label_set_text(s_weight_status_label, p5_state_text(w->p5_state));
    lv_obj_set_style_text_color(s_weight_status_label, p5_state_color(w->p5_state), 0);

    if (w->has_line_count) {
        char lbuf[16]; ui_fmt_int(lbuf, sizeof(lbuf), w->line_count);
        lv_label_set_text(s_line_count_label, lbuf);
    } else {
        lv_label_set_text(s_line_count_label, "--");
    }
    if (w->has_snapshot_count) {
        char sbuf[16]; ui_fmt_int(sbuf, sizeof(sbuf), w->snapshot_count);
        lv_label_set_text(s_snapshot_count_label, sbuf);
    } else {
        lv_label_set_text(s_snapshot_count_label, "--");
    }

    bool device_lost = (w->p5_state == P5_STATE_LOST) || (cam_link != LINK_OK);
    if (device_lost && !w->manual_mode) lv_obj_clear_flag(s_manual_btn, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_manual_btn, LV_OBJ_FLAG_HIDDEN);

    if (w->reconcile != RECONCILE_NONE) {
        lv_obj_clear_flag(s_reconcile_bar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(s_reconcile_bar, reconcile_bg(w->reconcile), 0);
        lv_obj_set_style_bg_opa(s_reconcile_bar, LV_OPA_COVER, 0);
        if (w->reconcile == RECONCILE_MISMATCH && w->has_line_count && w->has_snapshot_count) {
            snprintf(buf, sizeof(buf), "%s (lệch %d con)", reconcile_text(w->reconcile),
                     abs(w->line_count - w->snapshot_count));
        } else {
            snprintf(buf, sizeof(buf), "%s", reconcile_text(w->reconcile));
        }
        lv_label_set_text(s_reconcile_label, buf);
        lv_obj_set_style_text_color(s_reconcile_label, reconcile_fg(w->reconcile), 0);
    } else {
        lv_obj_add_flag(s_reconcile_bar, LV_OBJ_FLAG_HIDDEN);
    }

    bool is_import = (w->ticket_type == TICKET_IMPORT);
    lv_obj_set_style_bg_color(s_ticket_import_btn, is_import ? UI_COLOR_PRIMARY : UI_COLOR_MIST_100, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(s_ticket_import_btn, 0), is_import ? lv_color_white() : UI_COLOR_BODY, 0);
    lv_obj_set_style_bg_color(s_ticket_export_btn, !is_import ? UI_COLOR_PRIMARY : UI_COLOR_MIST_100, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(s_ticket_export_btn, 0), !is_import ? lv_color_white() : UI_COLOR_BODY, 0);

    int qty = w->manual_mode ? w->manual_qty : (w->has_line_count ? w->line_count : (w->has_snapshot_count ? w->snapshot_count : 0));
    char wkg[32]; ui_fmt_weight_kg(wkg, sizeof(wkg), w->weight_kg);
    char avgkg[32]; ui_fmt_weight_kg(avgkg, sizeof(avgkg), qty > 0 ? w->weight_kg / qty : 0);
    snprintf(buf, sizeof(buf), "Loại phiếu: %s   Đơn hàng: %s\nSố lượng: %d con   Khối lượng: %s kg\nTrung bình: %s kg/con",
             is_import ? "Nhập" : "Xuất", o ? o->code : "--", qty, wkg, avgkg);
    lv_label_set_text(s_ticket_summary_label, buf);

    bool can_confirm = app_state_weighing_can_confirm();
    lv_obj_set_style_bg_color(s_print_btn, can_confirm ? UI_COLOR_PRIMARY_SOFT : UI_COLOR_MIST_100, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(s_print_btn, 0), can_confirm ? UI_COLOR_PRIMARY : UI_COLOR_ICON, 0);
    if (can_confirm) lv_obj_remove_state(s_print_btn, LV_STATE_DISABLED);
    else lv_obj_add_state(s_print_btn, LV_STATE_DISABLED);

    bool mismatch = (w->reconcile == RECONCILE_MISMATCH);
    lv_obj_set_style_bg_color(s_reweigh_btn, mismatch ? UI_COLOR_GOLD_400 : UI_COLOR_MIST_100, 0);
    lv_obj_set_style_text_color(s_reweigh_label, mismatch ? UI_COLOR_HEADING : UI_COLOR_BODY, 0);

    if (can_confirm) {
        lv_obj_set_style_bg_color(s_confirm_btn, UI_COLOR_SUCCESS, 0);
        lv_obj_remove_state(s_confirm_btn, LV_STATE_DISABLED);
    } else {
        lv_obj_set_style_bg_color(s_confirm_btn, UI_COLOR_ICON, 0);
        lv_obj_add_state(s_confirm_btn, LV_STATE_DISABLED);
    }
    snprintf(buf, sizeof(buf), "Xác nhận %d con", qty);
    lv_label_set_text(s_confirm_label, buf);
}
