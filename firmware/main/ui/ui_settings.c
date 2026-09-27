#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ui_settings.h"
#include "ui_theme.h"
#include "ui_common.h"
#include "ui_shell.h"
#include "app_state.h"

static lv_obj_t *s_container;
static lv_obj_t *s_wifi_pw_ta;   /* ô mật khẩu Wi-Fi — đọc thật khi bấm KẾT NỐI */
static lv_obj_t *s_host;   /* vùng nội dung dựng lại mỗi lần ui_settings_refresh() */
static lv_obj_t *s_kb;     /* bàn phím dùng chung cho các ô nhập (mật khẩu Wi-Fi, token
                             * Gateway, tài khoản/mật khẩu Camera & P5 Scale) — cùng mẫu
                             * với ô tìm kiếm ở ui_history.c */

/* ── màn khoá PIN (Nhân viên cân) ────────────────────────────────────────── */
static lv_obj_t *s_lock_pin_dots_host;
static lv_obj_t *s_lock_error_label;

/* ── xem trước phiếu in ──────────────────────────────────────────────────── */
static lv_obj_t *s_print_preview;

/* ────────────────────────────────────────────────────────────────────────
 * Widget dùng chung: ô nhập gắn bàn phím ảo
 * ──────────────────────────────────────────────────────────────────────── */
static void kb_focus_event_cb(lv_event_t *e)
{
    uint16_t code = lv_event_get_code(e);
    lv_obj_t *ta = lv_event_get_target(e);
    if (code == LV_EVENT_FOCUSED) {
        lv_keyboard_set_textarea(s_kb, ta);
        lv_obj_clear_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_kb);
    } else if (code == LV_EVENT_DEFOCUSED || code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
    }
}

static lv_obj_t *make_text_field(lv_obj_t *parent, const char *placeholder)
{
    lv_obj_t *ta = lv_textarea_create(parent);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_placeholder_text(ta, placeholder);
    lv_obj_set_style_text_font(ta, UI_FONT_BODY, 0);
    lv_obj_set_width(ta, LV_PCT(100));
    lv_obj_add_event_cb(ta, kb_focus_event_cb, LV_EVENT_ALL, NULL);
    return ta;
}

static void pw_toggle_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *ta = (lv_obj_t *)lv_event_get_user_data(e);
    bool was_hidden = lv_textarea_get_password_mode(ta);
    lv_textarea_set_password_mode(ta, !was_hidden);
    lv_obj_t *lbl = lv_obj_get_child(btn, 0);
    lv_label_set_text(lbl, was_hidden ? "Ẩn" : "Hiện");
}

/* Hàng "ô nhập mật khẩu + nút Hiện/Ẩn" */
static lv_obj_t *make_password_row(lv_obj_t *parent, const char *placeholder)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *ta = lv_textarea_create(row);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_password_mode(ta, true);
    lv_textarea_set_placeholder_text(ta, placeholder);
    lv_obj_set_style_text_font(ta, UI_FONT_BODY, 0);
    lv_obj_set_flex_grow(ta, 1);
    lv_obj_add_event_cb(ta, kb_focus_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t *btn = ui_common_button_outline(row, "Hiện", UI_COLOR_BORDER, UI_COLOR_BODY, UI_FONT_BODY_BOLD);
    lv_obj_add_event_cb(btn, pw_toggle_cb, LV_EVENT_CLICKED, ta);
    return ta;
}

static lv_obj_t *make_field_label(lv_obj_t *parent, const char *text)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, UI_FONT_BODY, 0);
    lv_obj_set_style_text_color(lbl, UI_COLOR_BODY, 0);
    return lbl;
}

static lv_obj_t *make_section_title(lv_obj_t *parent, const char *text)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, UI_FONT_H4_BOLD, 0);
    lv_obj_set_style_text_color(lbl, UI_COLOR_HEADING, 0);
    return lbl;
}

/* ────────────────────────────────────────────────────────────────────────
 * WI-FI (mục 4.10)
 * ──────────────────────────────────────────────────────────────────────── */
static void wifi_select_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    const char *pass = s_wifi_pw_ta ? lv_textarea_get_text(s_wifi_pw_ta) : "";
    app_state_wifi_connect(idx, pass);
    ui_shell_toast("Đang kết nối Wi-Fi...");
    ui_settings_refresh();
}

static void wifi_scan_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    /* app_state_wifi_scan() đã khai báo ở app_state.h ("Quét lại") — gắn vào
     * đây vì mục 4.10 không có nút riêng khác dùng hàm này. */
    app_state_wifi_scan();
    ui_shell_toast("Đã quét lại danh sách Wi-Fi");
    ui_settings_refresh();
}

static void wifi_connect_submit_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    app_state_t *st = app_state();
    int sel = -1;
    for (int i = 0; i < st->wifi_network_count; i++) {
        if (st->wifi_networks[i].selected) { sel = i; break; }
    }
    if (sel < 0) {
        ui_shell_toast("Vui lòng chọn một mạng Wi-Fi trước");
        return;
    }
    const char *pass = s_wifi_pw_ta ? lv_textarea_get_text(s_wifi_pw_ta) : "";
    app_state_wifi_connect(sel, pass);
    char buf[64];
    snprintf(buf, sizeof(buf), "Đang kết nối Wi-Fi: %s...", st->wifi_networks[sel].ssid);
    ui_shell_toast(buf);
    ui_settings_refresh();
}

static void build_wifi_section(lv_obj_t *host)
{
    app_state_t *st = app_state();
    lv_obj_t *card = ui_common_card(host);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 10, 0);

    lv_obj_t *head = lv_obj_create(card);
    lv_obj_remove_style_all(head);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_width(head, LV_PCT(100));
    lv_obj_set_height(head, LV_SIZE_CONTENT);
    lv_obj_clear_flag(head, LV_OBJ_FLAG_SCROLLABLE);
    make_section_title(head, "WI-FI");

    lv_obj_t *scan_btn = lv_button_create(head);
    lv_obj_set_style_bg_opa(scan_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(scan_btn, 0, 0);
    lv_obj_set_style_pad_all(scan_btn, 4, 0);
    lv_obj_t *scan_row = lv_obj_create(scan_btn);
    lv_obj_remove_style_all(scan_row);
    lv_obj_set_flex_flow(scan_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(scan_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(scan_row, 4, 0);
    lv_obj_set_size(scan_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_clear_flag(scan_row, LV_OBJ_FLAG_SCROLLABLE);
    ui_common_icon(scan_row, &img_icon_refresh, UI_COLOR_PRIMARY);
    lv_obj_t *scan_lbl = lv_label_create(scan_row);
    lv_label_set_text(scan_lbl, "Quét lại");
    lv_obj_set_style_text_color(scan_lbl, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(scan_lbl, UI_FONT_BODY_BOLD, 0);
    lv_obj_add_event_cb(scan_btn, wifi_scan_cb, LV_EVENT_CLICKED, NULL);

    for (int i = 0; i < st->wifi_network_count; i++) {
        wifi_network_t *w = &st->wifi_networks[i];
        lv_obj_t *row = lv_obj_create(card);
        lv_obj_remove_style_all(row);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        lv_obj_set_style_pad_ver(row, 4, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *left = lv_obj_create(row);
        lv_obj_remove_style_all(left);
        lv_obj_set_flex_flow(left, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(left, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(left, 8, 0);
        lv_obj_set_size(left, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_clear_flag(left, LV_OBJ_FLAG_SCROLLABLE);
        ui_common_icon(left, &img_icon_wifi_signal, w->selected ? UI_COLOR_PRIMARY : UI_COLOR_ICON);

        lv_obj_t *info = lv_obj_create(left);
        lv_obj_remove_style_all(info);
        lv_obj_set_flex_flow(info, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_size(info, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_clear_flag(info, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *ssid = lv_label_create(info);
        lv_label_set_text(ssid, w->ssid);
        lv_obj_set_style_text_font(ssid, UI_FONT_BODY_BOLD, 0);
        lv_obj_set_style_text_color(ssid, UI_COLOR_HEADING, 0);
        lv_obj_t *meta = lv_label_create(info);
        char mbuf[48];
        snprintf(mbuf, sizeof(mbuf), "%d/4 vạch · %d dBm", w->bars, w->dbm);
        lv_label_set_text(meta, mbuf);
        lv_obj_set_style_text_font(meta, UI_FONT_XS, 0);
        lv_obj_set_style_text_color(meta, UI_COLOR_BODY, 0);

        if (w->selected) {
            lv_obj_t *sel_btn = ui_common_button(row, "ĐÃ CHỌN", UI_COLOR_PRIMARY_SOFT, UI_COLOR_PRIMARY, UI_FONT_BODY_BOLD);
            lv_obj_add_state(sel_btn, LV_STATE_DISABLED);
        } else {
            lv_obj_t *sel_btn = ui_common_button_outline(row, "Chọn", UI_COLOR_BORDER, UI_COLOR_BODY, UI_FONT_BODY_BOLD);
            lv_obj_add_event_cb(sel_btn, wifi_select_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        }
    }

    make_field_label(card, "Mật khẩu Wi-Fi");
    s_wifi_pw_ta = make_password_row(card, "Nhập mật khẩu...");

    lv_obj_t *connect_btn = ui_common_button(card, "KẾT NỐI", UI_COLOR_PRIMARY, lv_color_white(), UI_FONT_BODY_BOLD);
    lv_obj_set_width(connect_btn, LV_PCT(100));
    lv_obj_add_event_cb(connect_btn, wifi_connect_submit_cb, LV_EVENT_CLICKED, NULL);
}

/* ────────────────────────────────────────────────────────────────────────
 * GATEWAY (mục 4.10)
 * ──────────────────────────────────────────────────────────────────────── */
static void gateway_check_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_shell_toast("Gateway hoạt động bình thường (demo)");
}

static void build_gateway_section(lv_obj_t *host)
{
    lv_obj_t *card = ui_common_card(host);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 8, 0);

    make_section_title(card, "GATEWAY");

    /* Gateway chưa có link_state_t riêng trong app_state.h (chỉ Wi-Fi/P5/Camera
     * có) — mô phỏng ở trạng thái đã tìm thấy thiết bị, khớp mặc định demo
     * (mọi liên kết = LINK_OK trong app_state_init()). */
    lv_obj_t *status_row = lv_obj_create(card);
    lv_obj_remove_style_all(status_row);
    lv_obj_set_flex_flow(status_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(status_row, 6, 0);
    lv_obj_set_size(status_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_clear_flag(status_row, LV_OBJ_FLAG_SCROLLABLE);
    ui_common_status_dot(status_row, LINK_OK);
    make_field_label(status_row, "Đã tìm thấy thiết bị");

    make_field_label(card, "Địa chỉ IP: 192.168.1.10 · Cổng: 8080");

    make_field_label(card, "Token truy cập");
    make_text_field(card, "Nhập token...");

    lv_obj_t *cam_title = make_section_title(card, "TÀI KHOẢN CAMERA");
    lv_obj_set_style_text_font(cam_title, UI_FONT_H5_BOLD, 0);
    lv_obj_set_style_pad_top(cam_title, 4, 0);

    make_field_label(card, "Tên đăng nhập");
    make_text_field(card, "Tên đăng nhập camera...");

    make_field_label(card, "Mật khẩu");
    make_password_row(card, "Mật khẩu camera...");

    lv_obj_t *check_btn = ui_common_button_outline(card, "Kiểm tra kết nối", UI_COLOR_BORDER, UI_COLOR_PRIMARY, UI_FONT_BODY_BOLD);
    lv_obj_set_width(check_btn, LV_PCT(100));
    lv_obj_add_event_cb(check_btn, gateway_check_cb, LV_EVENT_CLICKED, NULL);
}

/* ────────────────────────────────────────────────────────────────────────
 * P5 SCALE (mục 4.10)
 * ──────────────────────────────────────────────────────────────────────── */
static void p5_rescan_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    /* Không có app_state_p5_scan() trong app_state.h — chỉ phản hồi hình
     * thức (toast), giống cách app_state_wifi_scan() cũng không đổi dữ liệu. */
    ui_shell_toast("Đang quét lại P5 Scale... (demo)");
}

static void p5_connect_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_shell_toast("Đang kết nối P5 Scale... (demo)");
}

static void build_p5_section(lv_obj_t *host)
{
    app_state_t *st = app_state();
    lv_obj_t *card = ui_common_card(host);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 8, 0);

    make_section_title(card, "P5 SCALE");

    lv_obj_t *status_row = lv_obj_create(card);
    lv_obj_remove_style_all(status_row);
    lv_obj_set_flex_flow(status_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(status_row, 6, 0);
    lv_obj_set_size(status_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_clear_flag(status_row, LV_OBJ_FLAG_SCROLLABLE);
    ui_common_status_dot(status_row, st->p5_link);
    make_field_label(status_row, st->p5_link == LINK_LOST ? "Không tìm thấy thiết bị" : "Đã tìm thấy thiết bị");

    make_field_label(card, "Địa chỉ IP: 192.168.1.20 · Cổng: 502");

    make_field_label(card, "Tài khoản");
    make_text_field(card, "Tài khoản P5 Scale...");

    make_field_label(card, "Mật khẩu");
    make_password_row(card, "Mật khẩu P5 Scale...");

    lv_obj_t *btn_row = lv_obj_create(card);
    lv_obj_remove_style_all(btn_row);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(btn_row, 10, 0);
    lv_obj_set_width(btn_row, LV_PCT(100));
    lv_obj_set_height(btn_row, LV_SIZE_CONTENT);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *rescan_btn = ui_common_button_outline(btn_row, "Quét lại", UI_COLOR_BORDER, UI_COLOR_BODY, UI_FONT_BODY_BOLD);
    lv_obj_set_flex_grow(rescan_btn, 1);
    lv_obj_add_event_cb(rescan_btn, p5_rescan_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *connect_btn = ui_common_button(btn_row, "Kết nối", UI_COLOR_PRIMARY, lv_color_white(), UI_FONT_BODY_BOLD);
    lv_obj_set_flex_grow(connect_btn, 1);
    lv_obj_add_event_cb(connect_btn, p5_connect_cb, LV_EVENT_CLICKED, NULL);
}

/* ────────────────────────────────────────────────────────────────────────
 * MÁY IN (mục 4.10)
 * ──────────────────────────────────────────────────────────────────────── */
static void printer_select_cb(lv_event_t *e)
{
    printer_type_t t = (printer_type_t)(intptr_t)lv_event_get_user_data(e);
    app_state()->printer_type = t;
    ui_settings_refresh();
}

static void printer_preview_toggle_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (!s_print_preview) return;
    if (lv_obj_has_flag(s_print_preview, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_clear_flag(s_print_preview, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_print_preview, LV_OBJ_FLAG_HIDDEN);
    }
}

static void build_printer_section(lv_obj_t *host)
{
    app_state_t *st = app_state();
    lv_obj_t *card = ui_common_card(host);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 8, 0);

    make_section_title(card, "MÁY IN");

    lv_obj_t *row = lv_obj_create(card);
    lv_obj_remove_style_all(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, 10, 0);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    bool thermal_sel = (st->printer_type == PRINTER_THERMAL_58);
    lv_obj_t *b1 = thermal_sel
        ? ui_common_button(row, "Nhiệt 58mm", UI_COLOR_PRIMARY, lv_color_white(), UI_FONT_BODY_BOLD)
        : ui_common_button_outline(row, "Nhiệt 58mm", UI_COLOR_BORDER, UI_COLOR_BODY, UI_FONT_BODY_BOLD);
    lv_obj_set_flex_grow(b1, 1);
    lv_obj_add_event_cb(b1, printer_select_cb, LV_EVENT_CLICKED, (void *)(intptr_t)PRINTER_THERMAL_58);

    bool a5_sel = (st->printer_type == PRINTER_A5);
    lv_obj_t *b2 = a5_sel
        ? ui_common_button(row, "A5", UI_COLOR_PRIMARY, lv_color_white(), UI_FONT_BODY_BOLD)
        : ui_common_button_outline(row, "A5", UI_COLOR_BORDER, UI_COLOR_BODY, UI_FONT_BODY_BOLD);
    lv_obj_set_flex_grow(b2, 1);
    lv_obj_add_event_cb(b2, printer_select_cb, LV_EVENT_CLICKED, (void *)(intptr_t)PRINTER_A5);

    lv_obj_t *preview_btn = ui_common_button_outline(card, "Xem trước phiếu", UI_COLOR_BORDER, UI_COLOR_PRIMARY, UI_FONT_BODY_BOLD);
    lv_obj_set_width(preview_btn, LV_PCT(100));
    lv_obj_add_event_cb(preview_btn, printer_preview_toggle_cb, LV_EVENT_CLICKED, NULL);

    s_print_preview = lv_obj_create(card);
    lv_obj_set_style_bg_color(s_print_preview, UI_COLOR_MIST_100, 0);
    lv_obj_set_style_bg_opa(s_print_preview, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_print_preview, 8, 0);
    lv_obj_set_style_pad_all(s_print_preview, 10, 0);
    lv_obj_set_width(s_print_preview, LV_PCT(100));
    lv_obj_set_height(s_print_preview, LV_SIZE_CONTENT);
    lv_obj_clear_flag(s_print_preview, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_print_preview, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *preview_txt = lv_label_create(s_print_preview);
    lv_label_set_text(preview_txt,
        "PIG WEIGH\n"
        "------------------------------\n"
        "Đơn hàng: DH-260927-001\n"
        "Loại phiếu: Nhập\n"
        "Số lượng: 48 con\n"
        "Khối lượng: 1.128,5 kg\n"
        "Trung bình: 282,13 kg/con\n"
        "------------------------------\n"
        "Trạm cân: TRẠM CÂN 01");
    lv_label_set_long_mode(preview_txt, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(preview_txt, LV_PCT(100));
    lv_obj_set_style_text_font(preview_txt, UI_FONT_BODY, 0);
    lv_obj_set_style_text_color(preview_txt, UI_COLOR_HEADING, 0);
}

/* ────────────────────────────────────────────────────────────────────────
 * PHIÊN BẢN (mục 4.10)
 * ──────────────────────────────────────────────────────────────────────── */
static void fw_update_check_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_shell_toast("Đang dùng phiên bản mới nhất (demo)");
}

static void build_version_section(lv_obj_t *host)
{
    app_state_t *st = app_state();
    lv_obj_t *card = ui_common_card(host);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 8, 0);

    make_section_title(card, "PHIÊN BẢN");

    char vbuf[64];
    snprintf(vbuf, sizeof(vbuf), "%s · %s", st->fw_version, st->fw_build);
    make_field_label(card, vbuf);

    lv_obj_t *btn = ui_common_button_outline(card, "Kiểm tra cập nhật", UI_COLOR_BORDER, UI_COLOR_PRIMARY, UI_FONT_BODY_BOLD);
    lv_obj_set_width(btn, LV_PCT(100));
    lv_obj_add_event_cb(btn, fw_update_check_cb, LV_EVENT_CLICKED, NULL);
}

/* ────────────────────────────────────────────────────────────────────────
 * HIỂN THỊ (mục 4.10)
 * ──────────────────────────────────────────────────────────────────────── */
static void bright_switch_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    /* Chưa có setter riêng trong app_state.h cho trường bool đơn giản này —
     * gán trực tiếp, giống cách ui_orders.c gán app_state()->order_filter/
     * order_page trực tiếp khi không có hàm chuyên biệt. */
    app_state()->bright_mode = lv_obj_has_state(sw, LV_STATE_CHECKED);
    /* "Tăng tương phản toàn màn hình khi bật" — áp dụng bằng cách đổi nền
     * vùng nội dung (mist -> trắng) trong ui_shell_refresh_chrome(), vì đó
     * là nơi trung tâm duy nhất vẽ lại toàn bộ khung sau đăng nhập; không
     * cần thêm API mới. */
    ui_shell_refresh_chrome();
}

static void build_display_section(lv_obj_t *host)
{
    app_state_t *st = app_state();
    lv_obj_t *card = ui_common_card(host);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 8, 0);
    lv_obj_set_style_pad_bottom(card, 20, 0); /* card cuối — chừa khoảng đáy trước thanh điều hướng */

    make_section_title(card, "HIỂN THỊ");

    lv_obj_t *row = lv_obj_create(card);
    lv_obj_remove_style_all(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *sw_label = make_field_label(row, "Chế độ ánh sáng mạnh");
    lv_obj_set_style_text_color(sw_label, UI_COLOR_HEADING, 0);

    lv_obj_t *sw = lv_switch_create(row);
    lv_obj_set_style_bg_color(sw, UI_COLOR_PRIMARY, LV_PART_INDICATOR | LV_STATE_CHECKED);
    if (st->bright_mode) lv_obj_add_state(sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw, bright_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

/* ────────────────────────────────────────────────────────────────────────
 * Khoá PIN (Nhân viên cân) — mục 4.10
 * ──────────────────────────────────────────────────────────────────────── */
static void lock_refresh_dots(void)
{
    ui_common_clear(s_lock_pin_dots_host);
    ui_common_pin_dots(s_lock_pin_dots_host, (int)strlen(app_state()->pin_input), true);
}

static void lock_try_unlock(void)
{
    if (strlen(app_state()->pin_input) < 4) return;
    if (app_state_try_settings_unlock()) {
        ui_settings_refresh();
    } else {
        lv_label_set_text(s_lock_error_label, "Mã PIN không đúng.");
        lv_obj_clear_flag(s_lock_error_label, LV_OBJ_FLAG_HIDDEN);
        lock_refresh_dots();
    }
}

static void lock_digit_cb(char digit, void *user_data)
{
    LV_UNUSED(user_data);
    app_state_pin_digit(digit);
    lock_refresh_dots();
    lock_try_unlock();
}

static void lock_backspace_cb(void *user_data)
{
    LV_UNUSED(user_data);
    app_state_pin_backspace();
    lock_refresh_dots();
}

static void build_lock_screen(lv_obj_t *host)
{
    app_state_pin_clear();

    lv_obj_t *card = ui_common_card(host);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_ver(card, 40, 0);
    lv_obj_set_style_pad_row(card, 6, 0);

    ui_common_icon(card, &img_icon_lock, UI_COLOR_ICON);

    lv_obj_t *msg = lv_label_create(card);
    lv_label_set_text(msg, "Khu vực yêu cầu quyền quản lý");
    lv_obj_set_style_text_font(msg, UI_FONT_H4_BOLD, 0);
    lv_obj_set_style_text_color(msg, UI_COLOR_HEADING, 0);
    lv_obj_set_style_pad_bottom(msg, 10, 0);

    s_lock_pin_dots_host = lv_obj_create(card);
    lv_obj_remove_style_all(s_lock_pin_dots_host);
    lv_obj_set_size(s_lock_pin_dots_host, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_bottom(s_lock_pin_dots_host, 12, 0);
    lv_obj_clear_flag(s_lock_pin_dots_host, LV_OBJ_FLAG_SCROLLABLE);
    ui_common_pin_dots(s_lock_pin_dots_host, 0, true);

    s_lock_error_label = lv_label_create(card);
    lv_label_set_text(s_lock_error_label, "Mã PIN không đúng.");
    lv_obj_set_style_text_color(s_lock_error_label, UI_COLOR_DANGER, 0);
    lv_obj_set_style_text_font(s_lock_error_label, UI_FONT_H5, 0);
    lv_obj_set_style_pad_bottom(s_lock_error_label, 8, 0);
    lv_obj_add_flag(s_lock_error_label, LV_OBJ_FLAG_HIDDEN);

    ui_common_keypad(card, lock_digit_cb, lock_backspace_cb, NULL, true);
}

/* ────────────────────────────────────────────────────────────────────────
 * API công khai
 * ──────────────────────────────────────────────────────────────────────── */
lv_obj_t *ui_settings_create(lv_obj_t *parent)
{
    s_container = lv_obj_create(parent);
    lv_obj_remove_style_all(s_container);
    lv_obj_set_size(s_container, UI_HOR_RES, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(s_container, UI_PAD_SCREEN, 0);
    lv_obj_set_flex_flow(s_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_container, 12, 0);
    lv_obj_clear_flag(s_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(s_container);
    lv_label_set_text(title, "Cài đặt");
    lv_obj_set_style_text_font(title, UI_FONT_H2_BOLD, 0);
    lv_obj_set_style_text_color(title, UI_COLOR_HEADING, 0);

    s_host = lv_obj_create(s_container);
    lv_obj_remove_style_all(s_host);
    lv_obj_set_flex_flow(s_host, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_host, 14, 0);
    lv_obj_set_width(s_host, LV_PCT(100));
    lv_obj_set_height(s_host, LV_SIZE_CONTENT);
    lv_obj_clear_flag(s_host, LV_OBJ_FLAG_SCROLLABLE);

    s_kb = lv_keyboard_create(lv_obj_get_parent(s_container));
    lv_obj_set_height(s_kb, 200);
    lv_obj_align(s_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_kb);

    return s_container;
}

void ui_settings_refresh(void)
{
    ui_common_clear(s_host);
    s_print_preview = NULL;

    app_state_t *st = app_state();
    employee_t *e = (st->current_employee_idx >= 0) ? &st->employees[st->current_employee_idx] : NULL;
    bool locked = e && (e->role == ROLE_OPERATOR) && !st->settings_unlocked;

    if (locked) {
        build_lock_screen(s_host);
        return;
    }

    build_wifi_section(s_host);
    build_gateway_section(s_host);
    build_p5_section(s_host);
    build_printer_section(s_host);
    build_version_section(s_host);
    build_display_section(s_host);
}
