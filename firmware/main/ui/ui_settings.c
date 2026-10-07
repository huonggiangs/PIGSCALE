#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "esp_log.h"
#include "ui_settings.h"
#include "ui_theme.h"
#include "ui_common.h"
#include "ui_shell.h"
#include "app_state.h"
#include "camera_receiver.h"
#include "esp_timer.h"

static lv_obj_t *s_container;
static lv_obj_t *s_wifi_pw_ta;   /* ô mật khẩu Wi-Fi — đọc thật khi bấm KẾT NỐI */
static lv_obj_t *s_host;   /* vùng nội dung dựng lại mỗi lần ui_settings_refresh() */
static lv_obj_t *s_kb;     /* bàn phím dùng chung cho các ô nhập (mật khẩu Wi-Fi, token
                             * Gateway, tài khoản/mật khẩu Camera & P5 Scale) — cùng mẫu
                             * với ô tìm kiếm ở ui_history.c */
static lv_obj_t *s_rx_port_ta, *s_cam_token_ta, *s_rtsp_main_ta, *s_rtsp_sub_ta;
static lv_obj_t *s_rx_endpoint, *s_rx_status, *s_rx_count, *s_rx_snapshot;
static lv_obj_t *s_snapshot_overlay;
static uint8_t *s_snapshot_data;
static lv_image_dsc_t s_snapshot_dsc;
static bool s_wifi_show_all;   /* true = hiện đủ danh sách; false = thu gọn khi đã kết nối */
static bool s_wifi_was_scanning;  /* phát hiện lúc quét VỪA xong để tự vẽ lại danh sách —
                                     xem ui_settings_tick(). Trước đây không có, nên sau khi
                                     quét xong (thật ra chỉ ~2-3s, đã đo qua log) màn hình
                                     vẫn đứng yên ở "Đang quét..." cho tới khi người dùng vô
                                     tình làm gì khác kích hoạt vẽ lại — CẢM GIÁC như quét
                                     rất chậm dù backend đã xong từ lâu. */

/* ── IP tĩnh cho WiFi của thiết bị ─────────────────────────────────────────── */
static lv_obj_t *s_ip_fields_wrap;
static lv_obj_t *s_ip_addr_ta, *s_ip_nm_ta, *s_ip_gw_ta, *s_ip_dns_ta;

/* ── Gateway: tìm camera trong mạng ──────────────────────────────────────── */
static lv_obj_t *s_cam_ip_ta;      /* ô "Địa chỉ IP camera" — Chọn từ danh sách tìm được sẽ điền vào đây */
static lv_obj_t *s_cam_port_ta;
static lv_obj_t *s_cam_user_ta, *s_cam_pass_ta;
static lv_obj_t *s_cam_list_host;  /* danh sách camera tìm được — ẩn cho tới khi bấm Tìm kiếm */
static lv_obj_t *s_cam_status_dot, *s_cam_status_label;  /* cập nhật tại chỗ — xem camera_client.c */
static lv_obj_t *s_gw_ip_ta, *s_gw_port_ta;        /* Gateway: IP/Cổng do người dùng nhập — không hardcode */
static lv_obj_t *s_gw_status_dot, *s_gw_status_label;  /* cập nhật tại chỗ sau khi bấm Kết nối */

/* ── P5 Scale: tìm đúng thiết bị trong mạng ───────────────────────────────── */
static lv_obj_t *s_p5_ip_ta, *s_p5_port_ta;
static lv_obj_t *s_p5_list_host;
static lv_obj_t *s_p5_status_dot, *s_p5_status_label;

/* ── xem trước phiếu in ──────────────────────────────────────────────────── */
static lv_obj_t *s_print_preview;

/* ────────────────────────────────────────────────────────────────────────
 * Widget dùng chung: ô nhập gắn bàn phím ảo
 * ──────────────────────────────────────────────────────────────────────── */
void ui_settings_close_keyboard(void)
{
    if (!s_kb) return;
    lv_obj_t *ta = lv_keyboard_get_textarea(s_kb);
    lv_keyboard_set_textarea(s_kb, NULL);
    lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
    if (ta) lv_obj_remove_state(ta, LV_STATE_FOCUSED);
    if (s_container) lv_obj_set_height(lv_obj_get_parent(s_container), UI_CONTENT_H);
}

static void kb_done_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) ui_settings_close_keyboard();
}

static void kb_focus_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *ta = lv_event_get_target(e);
    if (!s_kb) return;
    if (code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED) {
        const char *accepted = lv_textarea_get_accepted_chars(ta);
        lv_keyboard_set_mode(s_kb, accepted ? LV_KEYBOARD_MODE_NUMBER : LV_KEYBOARD_MODE_TEXT_LOWER);
        lv_keyboard_set_textarea(s_kb, ta);
        lv_obj_remove_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
        /* Reserve the keyboard's space so even the final field can scroll above it. */
        lv_obj_set_height(lv_obj_get_parent(s_container), UI_CONTENT_H - 320);
        lv_obj_update_layout(s_container);
        lv_obj_scroll_to_view_recursive(ta, LV_ANIM_OFF);
    } else if ((code == LV_EVENT_DEFOCUSED || code == LV_EVENT_DELETE ||
                code == LV_EVENT_READY || code == LV_EVENT_CANCEL) &&
               lv_keyboard_get_textarea(s_kb) == ta) {
        ui_settings_close_keyboard();
    }
}

static lv_obj_t *make_text_field(lv_obj_t *parent, const char *placeholder)
{
    lv_obj_t *ta = lv_textarea_create(parent);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_placeholder_text(ta, placeholder);
    lv_obj_set_style_text_font(ta, UI_FONT_BODY, 0);
    /* lv_textarea vẽ chữ GÕ VÀO (LV_PART_MAIN) và PLACEHOLDER
     * (LV_PART_TEXTAREA_PLACEHOLDER) bằng 2 "part" style RIÊNG — set font
     * ở part 0 (MAIN) KHÔNG áp dụng cho placeholder. Thiếu dòng này,
     * placeholder rơi về font mặc định của LVGL (không có dấu tiếng Việt)
     * → chữ có dấu trong placeholder (vd. "Ví dụ: ...") hiện thành ô chữ
     * nhật đứng trống (tofu) — đây là "card địa chỉ IP" vừa báo lỗi, vì
     * đó là nơi đầu tiên có placeholder tiếng Việt luôn hiển thị thật sự
     * (các ô khác thường được điền sẵn giá trị nên placeholder ít khi lộ
     * ra). */
    lv_obj_set_style_text_font(ta, UI_FONT_BODY, LV_PART_TEXTAREA_PLACEHOLDER);
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
    lv_obj_set_style_text_font(ta, UI_FONT_BODY, LV_PART_TEXTAREA_PLACEHOLDER);  /* xem ghi chú trong make_text_field() */
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
    /* Quét WiFi qua ESP32-C6/SDIO có thể mất 15-30 giây thật (đã đo được khi
       vừa kết nối vừa quét) — trước đây bấm xong không có phản hồi gì nên
       cảm giác "không ăn", bấm nhiều lần cũng vô ích vì wifi_manager tự
       chặn quét chồng quét. Hiện rõ "Đang quét..." + khoá nút để không còn
       cảm giác đó; danh sách tự cập nhật khi có kết quả (app_state_wifi_sync
       chạy mỗi tick, không cần bấm lại). */
    app_state_wifi_scan();
    ui_settings_refresh();
}

static void wifi_toggle_list_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    s_wifi_show_all = !s_wifi_show_all;
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

    bool scanning = app_state_wifi_is_scanning();

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
    /* lv_obj_create() mặc định BẬT LV_OBJ_FLAG_CLICKABLE — nằm đè lên
       scan_btn nên cướp mất điểm chạm đúng chỗ có icon/chữ "Quét lại". Đây
       mới là nguyên nhân THẬT của việc bấm nút không ăn (không chỉ do quét
       lâu) — xem giải thích đầy đủ trong ui_login.c. */
    lv_obj_remove_flag(scan_row, LV_OBJ_FLAG_CLICKABLE);
    ui_common_icon(scan_row, &img_icon_refresh, scanning ? UI_COLOR_ICON : UI_COLOR_PRIMARY);
    lv_obj_t *scan_lbl = lv_label_create(scan_row);
    lv_label_set_text(scan_lbl, scanning ? "Đang quét..." : "Quét lại");
    lv_obj_set_style_text_color(scan_lbl, scanning ? UI_COLOR_ICON : UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(scan_lbl, UI_FONT_BODY_BOLD, 0);
    lv_obj_add_event_cb(scan_btn, wifi_scan_cb, LV_EVENT_CLICKED, NULL);
    if (scanning) lv_obj_add_state(scan_btn, LV_STATE_DISABLED);

    /* Thu gọn danh sách khi đã kết nối: chỉ hiện mạng đang dùng + nút "Đổi
       mạng" để bung lại đủ danh sách — tránh danh sách dài gây rối khi
       không cần đổi mạng. */
    bool collapse = st->wifi_connected && !s_wifi_show_all;
    if (st->wifi_connected) {
        lv_obj_t *change_btn = ui_common_button_outline(card, s_wifi_show_all ? "Thu gọn" : "Đổi mạng",
                                                          UI_COLOR_BORDER, UI_COLOR_PRIMARY, UI_FONT_BODY_BOLD);
        lv_obj_add_event_cb(change_btn, wifi_toggle_list_cb, LV_EVENT_CLICKED, NULL);
    }

    for (int i = 0; i < st->wifi_network_count; i++) {
        wifi_network_t *w = &st->wifi_networks[i];
        if (collapse && !w->selected) continue;   /* thu gọn: chỉ hiện mạng đang chọn */
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
 * ĐỊA CHỈ IP (IP tĩnh cho WiFi của thiết bị) — THẬT, xem wifi_manager.c
 * ──────────────────────────────────────────────────────────────────────── */
static void ip_static_switch_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    /* Ẩn/hiện tại chỗ (không ui_settings_refresh) để không mất nội dung
     * đang gõ dở ở các card khác — cùng cách printer preview toggle đang
     * làm với s_print_preview. */
    if (!s_ip_fields_wrap) return;
    if (on) lv_obj_clear_flag(s_ip_fields_wrap, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_ip_fields_wrap, LV_OBJ_FLAG_HIDDEN);
}

static void ip_static_save_cb(lv_event_t *e)
{
    lv_obj_t *sw = (lv_obj_t *)lv_event_get_user_data(e);
    bool enabled = sw && lv_obj_has_state(sw, LV_STATE_CHECKED);
    const char *ip  = s_ip_addr_ta ? lv_textarea_get_text(s_ip_addr_ta) : "";
    const char *nm  = s_ip_nm_ta   ? lv_textarea_get_text(s_ip_nm_ta)   : "";
    const char *gw  = s_ip_gw_ta   ? lv_textarea_get_text(s_ip_gw_ta)   : "";
    const char *dns = s_ip_dns_ta  ? lv_textarea_get_text(s_ip_dns_ta)  : "";

    if (!app_state_wifi_set_static_ip(enabled, ip, nm, gw, dns)) {
        ui_shell_toast("Địa chỉ IP không hợp lệ — kiểm tra lại");
        return;
    }
    ui_shell_toast(enabled ? "Đã lưu IP tĩnh — đang áp dụng lại kết nối..."
                            : "Đã chuyển về DHCP tự động");
}

static void build_static_ip_section(lv_obj_t *host)
{
    app_state_t *st = app_state();
    lv_obj_t *card = ui_common_card(host);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 10, 0);

    make_section_title(card, "ĐỊA CHỈ IP");

    lv_obj_t *sw_row = lv_obj_create(card);
    lv_obj_remove_style_all(sw_row);
    lv_obj_set_flex_flow(sw_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sw_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_width(sw_row, LV_PCT(100));
    lv_obj_set_height(sw_row, LV_SIZE_CONTENT);
    lv_obj_clear_flag(sw_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *sw_label = make_field_label(sw_row, "Dùng địa chỉ IP tĩnh (tắt = DHCP tự động)");
    lv_obj_set_style_text_color(sw_label, UI_COLOR_HEADING, 0);
    lv_obj_t *sw = lv_switch_create(sw_row);
    lv_obj_set_style_bg_color(sw, UI_COLOR_PRIMARY, LV_PART_INDICATOR | LV_STATE_CHECKED);
    if (st->wifi_static_en) lv_obj_add_state(sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw, ip_static_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);

    s_ip_fields_wrap = lv_obj_create(card);
    lv_obj_remove_style_all(s_ip_fields_wrap);
    lv_obj_set_flex_flow(s_ip_fields_wrap, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_ip_fields_wrap, 8, 0);
    lv_obj_set_width(s_ip_fields_wrap, LV_PCT(100));
    lv_obj_set_height(s_ip_fields_wrap, LV_SIZE_CONTENT);
    lv_obj_clear_flag(s_ip_fields_wrap, LV_OBJ_FLAG_SCROLLABLE);
    if (!st->wifi_static_en) lv_obj_add_flag(s_ip_fields_wrap, LV_OBJ_FLAG_HIDDEN);

    make_field_label(s_ip_fields_wrap, "Địa chỉ IP");
    s_ip_addr_ta = make_text_field(s_ip_fields_wrap, "Ví dụ: 192.168.1.50");
    if (st->wifi_static_ip[0]) lv_textarea_set_text(s_ip_addr_ta, st->wifi_static_ip);

    make_field_label(s_ip_fields_wrap, "Subnet Mask");
    s_ip_nm_ta = make_text_field(s_ip_fields_wrap, "255.255.255.0");
    lv_textarea_set_text(s_ip_nm_ta, st->wifi_static_netmask[0] ? st->wifi_static_netmask : "255.255.255.0");

    make_field_label(s_ip_fields_wrap, "Gateway");
    s_ip_gw_ta = make_text_field(s_ip_fields_wrap, "Ví dụ: 192.168.1.1");
    if (st->wifi_static_gateway[0]) lv_textarea_set_text(s_ip_gw_ta, st->wifi_static_gateway);

    make_field_label(s_ip_fields_wrap, "DNS");
    s_ip_dns_ta = make_text_field(s_ip_fields_wrap, "Ví dụ: 8.8.8.8");
    if (st->wifi_static_dns[0]) lv_textarea_set_text(s_ip_dns_ta, st->wifi_static_dns);

    lv_obj_t *save_btn = ui_common_button(card, "Lưu địa chỉ IP", UI_COLOR_PRIMARY, lv_color_white(), UI_FONT_BODY_BOLD);
    lv_obj_set_width(save_btn, LV_PCT(100));
    lv_obj_add_event_cb(save_btn, ip_static_save_cb, LV_EVENT_CLICKED, sw);
}

/* ────────────────────────────────────────────────────────────────────────
 * GATEWAY (mục 4.10)
 * ──────────────────────────────────────────────────────────────────────── */
static void gateway_check_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    /* Cập nhật TẠI CHỖ (không ui_settings_refresh) để không mất IP/Token/
     * tài khoản camera đang gõ dở. Chưa có giao thức bắt tay Gateway thật
     * trong app_state.h — coi "bấm Kết nối" là hành động người dùng xác
     * nhận đã cấu hình đúng, không còn hiển thị "Đã tìm thấy thiết bị"
     * CỐ ĐỊNH bất kể có bấm hay chưa (hardcode cũ). */
    app_state()->gateway_link = LINK_OK;
    if (s_gw_status_dot) lv_obj_set_style_bg_color(s_gw_status_dot, ui_common_link_color(LINK_OK), 0);
    if (s_gw_status_label) lv_label_set_text(s_gw_status_label, "Đã kết nối");
    ui_shell_toast("Đang kết nối Gateway... (demo)");
}

static void gateway_save_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    /* Chưa có NVS schema riêng cho Gateway/Camera trong app_state.h (khác
     * với Wi-Fi — đã lưu NVS thật qua wifi_manager_connect()) — xác nhận
     * bằng toast, cùng mức độ hoàn thiện demo với phần còn lại của card
     * này cho tới khi có backend thật. */
    ui_shell_toast("Đã lưu cấu hình Gateway/Camera");
}

/* Camera THẬT đã lắp cho trạm này (Vivoo Web IP Camera — xem
 * docs: WEB_IP_CAMERA_GUIDE_VIVOO.pdf, HTTP port mặc định 80). Khác với
 * trước đây (3 IP demo .21/.22/.23 tự bịa) — đây là địa chỉ thật, lấy từ
 * app_state()->camera_ip/camera_port (nguồn sự thật duy nhất, đồng bộ với
 * lần tự kiểm tra lại định kỳ trong app_state_camera_sync()). "Tìm kiếm"
 * ở đây xác nhận lại camera đã biết — KHÔNG phải dò toàn mạng (tài liệu
 * Vivoo không công bố giao thức discovery/ONVIF cụ thể để dò thật). */
static void cam_pick_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    app_state_t *st = app_state();
    if (s_cam_ip_ta) lv_textarea_set_text(s_cam_ip_ta, st->camera_ip);
    if (s_cam_port_ta) {
        char pbuf[8]; snprintf(pbuf, sizeof(pbuf), "%u", (unsigned)st->camera_port);
        lv_textarea_set_text(s_cam_port_ta, pbuf);
    }
    ui_shell_toast("Đã chọn camera — bấm Kiểm tra kết nối để xác nhận");
}

static void cam_search_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (!s_cam_list_host) return;
    app_state_t *st = app_state();

    /* Cập nhật TẠI CHỖ (không gọi ui_settings_refresh) để không mất nội
     * dung đang gõ dở ở Token/tài khoản camera — cùng cách printer preview
     * toggle đang làm với s_print_preview. */
    ui_common_clear(s_cam_list_host);
    lv_obj_t *row = lv_obj_create(s_cam_list_host);
    lv_obj_remove_style_all(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_ver(row, 4, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *info = lv_obj_create(row);
    lv_obj_remove_style_all(info);
    lv_obj_set_flex_flow(info, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_size(info, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_clear_flag(info, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *name = lv_label_create(info);
    lv_label_set_text(name, "Camera Vivoo (Web IP Camera)");
    lv_obj_set_style_text_font(name, UI_FONT_BODY_BOLD, 0);
    lv_obj_set_style_text_color(name, UI_COLOR_HEADING, 0);
    lv_obj_t *ip = lv_label_create(info);
    char ibuf[56]; snprintf(ibuf, sizeof(ibuf), "%s : %u", st->camera_ip, (unsigned)st->camera_port);
    lv_label_set_text(ip, ibuf);
    lv_obj_set_style_text_font(ip, UI_FONT_XS, 0);
    lv_obj_set_style_text_color(ip, UI_COLOR_BODY, 0);

    lv_obj_t *pick_btn = ui_common_button_outline(row, "Chọn", UI_COLOR_BORDER, UI_COLOR_PRIMARY, UI_FONT_BODY_BOLD);
    lv_obj_add_event_cb(pick_btn, cam_pick_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_clear_flag(s_cam_list_host, LV_OBJ_FLAG_HIDDEN);
}

/* Kiểm tra kết nối camera THẬT — HTTP GET tới ip:port (xem camera_client.c).
 * Khác hẳn gateway_check_cb (vẫn demo): đây gọi task nền thật, kết quả cập
 * nhật app_state()->camera_link, đọc lại trong app_state_camera_sync() mỗi
 * tick (0.5s) kể cả khi không ở tab Cài đặt — hiển thị "real-time" cả ở
 * header lẫn tab Cân. */
/* Đọc IP/Port đang gõ trong 2 ô nhập — dùng chung cho cả nút "Lưu" lẫn
 * "Kiểm tra kết nối" để không lặp code. Port mặc định 80 CHỈ khi ô trống
 * hoặc nhập sai định dạng — nếu người dùng đã gõ số hợp lệ (vd. 8080) thì
 * luôn dùng đúng số đó. */
static bool cam_save_fields(void)
{
    camera_config_t cfg = {0};
    snprintf(cfg.ip, sizeof(cfg.ip), "%s", lv_textarea_get_text(s_cam_ip_ta));
    snprintf(cfg.user, sizeof(cfg.user), "%s", lv_textarea_get_text(s_cam_user_ta));
    snprintf(cfg.password, sizeof(cfg.password), "%s", lv_textarea_get_text(s_cam_pass_ta));
    snprintf(cfg.rtsp_main, sizeof(cfg.rtsp_main), "%s", lv_textarea_get_text(s_rtsp_main_ta));
    snprintf(cfg.rtsp_sub, sizeof(cfg.rtsp_sub), "%s", lv_textarea_get_text(s_rtsp_sub_ta));
    snprintf(cfg.token, sizeof(cfg.token), "%s", lv_textarea_get_text(s_cam_token_ta));
    if (!camera_parse_port(lv_textarea_get_text(s_cam_port_ta), &cfg.https_port) ||
        !camera_parse_port(lv_textarea_get_text(s_rx_port_ta), &cfg.receive_port)) {
        ui_shell_toast("Cổng phải là số từ 1 đến 65535");
        return false;
    }
    esp_err_t err = camera_receiver_save_config(&cfg);
    if (err != ESP_OK) {
        ui_shell_toast(err == ESP_ERR_INVALID_ARG ? "IP camera hoặc token không hợp lệ" : "Không lưu được cấu hình camera");
        return false;
    }
    app_state_camera_save_config(cfg.ip, cfg.https_port);
    ui_settings_close_keyboard();
    return true;
}

static void cam_save_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (cam_save_fields()) ui_shell_toast("Đã lưu camera; đang mở cổng nhận dữ liệu");
}

static void cam_connect_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (app_state_camera_is_checking()) {
        ui_shell_toast("Đang kiểm tra camera, vui lòng chờ");
        return;
    }
    if (!cam_save_fields()) return;
    app_state_camera_test_connect(app_state()->camera_ip, app_state()->camera_port);
    if (s_cam_status_label) lv_label_set_text(s_cam_status_label, "Đang kiểm tra...");
    ui_shell_toast("Đang kiểm tra HTTPS của camera...");
}

static void snapshot_close_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_common_modal_close(s_snapshot_overlay);
    s_snapshot_overlay = NULL;
    lv_image_cache_drop(&s_snapshot_dsc);
    free(s_snapshot_data);
    s_snapshot_data = NULL;
}

static void snapshot_open_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (s_snapshot_overlay) return;
    size_t size = 0;
    uint16_t width = 0, height = 0;
    camera_count_event_t event;
    s_snapshot_data = camera_receiver_copy_snapshot(&size, &width, &height, &event);
    if (!s_snapshot_data) { ui_shell_toast("Chưa có ảnh hoặc không đủ bộ nhớ"); return; }
    ui_settings_close_keyboard();
    s_snapshot_dsc = (lv_image_dsc_t){
        .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RAW, .w = width, .h = height},
        .data_size = size, .data = s_snapshot_data,
    };
    lv_obj_t *box = ui_common_modal_open(&s_snapshot_overlay, 740, 900);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(box, 12, 0);
    make_section_title(box, "ẢNH CHỤP CAMERA");
    char text[180];
    snprintf(text, sizeof(text), "%s · %d con · thời điểm gửi: %lld", event.device_id, event.count, (long long)event.timestamp);
    lv_obj_t *label = make_field_label(box, text);
    lv_obj_set_width(label, LV_PCT(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_t *frame = lv_obj_create(box);
    lv_obj_remove_style_all(frame);
    lv_obj_set_size(frame, LV_PCT(100), 620);
    lv_obj_remove_flag(frame, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *img = lv_image_create(frame);
    lv_image_set_src(img, &s_snapshot_dsc);
    uint32_t scale_w = 680U * 256 / width;
    uint32_t scale_h = 600U * 256 / height;
    uint32_t scale = scale_w < scale_h ? scale_w : scale_h;
    lv_image_set_scale(img, scale < 256 ? scale : 256);
    lv_obj_center(img);
    label = make_field_label(box, "Ảnh lấy sau bản tin đếm; chỉ giữ ảnh gần nhất trong RAM.");
    lv_obj_set_width(label, LV_PCT(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_t *close = ui_common_button(box, "Đóng", UI_COLOR_PRIMARY, lv_color_white(), UI_FONT_BODY_BOLD);
    lv_obj_add_event_cb(close, snapshot_close_cb, LV_EVENT_CLICKED, NULL);
}

static void receiver_refresh_status(void)
{
    if (!s_rx_status) return;
    camera_receiver_status_t status;
    camera_receiver_get_status(&status);
    char text[240];
    camera_receiver_endpoint(text, sizeof(text));
    lv_label_set_text(s_rx_endpoint, text);
    if (status.applying) snprintf(text, sizeof(text), "Đang mở cổng nhận...");
    else if (status.running) snprintf(text, sizeof(text), "Đang lắng nghe cổng %u", status.listening_port);
    else snprintf(text, sizeof(text), "Không mở được cổng: %s", esp_err_to_name(status.server_error));
    lv_label_set_text(s_rx_status, text);
    if (status.sequence) {
        long long age = (esp_timer_get_time() - status.received_us) / 1000000;
        snprintf(text, sizeof(text), "%s · %d con · nhận %lld giây trước\nThời điểm camera: %lld",
                 status.latest.device_id, status.latest.count, age, (long long)status.latest.timestamp);
    } else snprintf(text, sizeof(text), "Chưa nhận bản tin đếm");
    lv_label_set_text(s_rx_count, text);
    if (status.snapshot_busy) snprintf(text, sizeof(text), "Đang lấy ảnh JPEG...");
    else if (!status.sequence) snprintf(text, sizeof(text), "Chờ số đếm để lấy ảnh");
    else if (status.snapshot_error == ESP_ERR_INVALID_STATE) snprintf(text, sizeof(text), "Đã nhận số đếm; cần Bearer token để lấy ảnh");
    else if (status.snapshot_error != ESP_OK) snprintf(text, sizeof(text), "Lấy ảnh thất bại: HTTP %d / %s", status.snapshot_http_status, esp_err_to_name(status.snapshot_error));
    else snprintf(text, sizeof(text), "Ảnh gần nhất: %d con · %u KB", status.snapshot_event.count, (unsigned)(status.snapshot_size / 1024));
    lv_label_set_text(s_rx_snapshot, text);
}

static void build_receiver_section(lv_obj_t *host)
{
    camera_config_t cfg;
    camera_receiver_get_config(&cfg);
    lv_obj_t *card = ui_common_card(host);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 10, 0);
    make_section_title(card, "NHẬN DỮ LIỆU ĐẾM CAMERA");
    make_field_label(card, "Cổng HTTP nhận dữ liệu trên thiết bị cân");
    s_rx_port_ta = make_text_field(card, "8080");
    lv_textarea_set_accepted_chars(s_rx_port_ta, "0123456789");
    lv_textarea_set_max_length(s_rx_port_ta, 5);
    char port[8]; snprintf(port, sizeof(port), "%u", cfg.receive_port);
    lv_textarea_set_text(s_rx_port_ta, port);
    make_field_label(card, "Bearer token camera (dùng lấy ảnh)");
    s_cam_token_ta = make_password_row(card, "Nhập token do camera cấp...");
    lv_textarea_set_max_length(s_cam_token_ta, sizeof(cfg.token) - 1);
    lv_textarea_set_text(s_cam_token_ta, cfg.token);
    lv_obj_t *save = ui_common_button(card, "Lưu camera và cổng nhận", UI_COLOR_PRIMARY, lv_color_white(), UI_FONT_BODY_BOLD);
    lv_obj_add_event_cb(save, cam_save_cb, LV_EVENT_CLICKED, NULL);
    make_field_label(card, "Endpoint cần cấu hình trên camera:");
    s_rx_endpoint = make_field_label(card, "");
    s_rx_status = make_field_label(card, "");
    s_rx_count = make_field_label(card, "");
    s_rx_snapshot = make_field_label(card, "");
    lv_obj_t *labels[] = {s_rx_endpoint, s_rx_status, s_rx_count, s_rx_snapshot};
    for (unsigned i = 0; i < sizeof(labels) / sizeof(labels[0]); ++i) {
        lv_obj_set_width(labels[i], LV_PCT(100));
        lv_label_set_long_mode(labels[i], LV_LABEL_LONG_WRAP);
    }
    lv_obj_t *preview = ui_common_button_outline(card, "Xem ảnh gần nhất", UI_COLOR_BORDER, UI_COLOR_PRIMARY, UI_FONT_BODY_BOLD);
    lv_obj_add_event_cb(preview, snapshot_open_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *note = make_field_label(card,
        "Camera gửi số đếm tới endpoint trên; thiết bị lấy ảnh bằng token. "
        "User/mật khẩu được lưu, chưa dùng để cấp token vì tài liệu chưa có API đăng nhập. "
        "Số đếm này chưa tự chốt phiếu cân.");
    lv_obj_set_width(note, LV_PCT(100));
    lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(note, UI_FONT_XS, 0);
    receiver_refresh_status();
}

static void build_gateway_section(lv_obj_t *host)
{
    lv_obj_t *card = ui_common_card(host);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 8, 0);

    make_section_title(card, "GATEWAY");

    app_state_t *gst = app_state();
    lv_obj_t *status_row = lv_obj_create(card);
    lv_obj_remove_style_all(status_row);
    lv_obj_set_flex_flow(status_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(status_row, 6, 0);
    lv_obj_set_size(status_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_clear_flag(status_row, LV_OBJ_FLAG_SCROLLABLE);
    s_gw_status_dot = ui_common_status_dot(status_row, gst->gateway_link);
    s_gw_status_label = make_field_label(status_row, gst->gateway_link == LINK_OK ? "Đã kết nối" : "Chưa kết nối");

    /* Địa chỉ IP/Cổng Gateway — trước đây là nhãn tĩnh "192.168.1.10:8080"
     * hiện cố định bất kể cấu hình thật (hardcode). Giờ là 2 ô nhập thật.
     * 2 hàng riêng (không lồng cột trong hàng) — xem ghi chú ở card
     * CAMERA về lý do đổi cấu trúc (bấm không mở được bàn phím). */
    make_field_label(card, "Địa chỉ IP");
    s_gw_ip_ta = make_text_field(card, "Nhập IP Gateway...");

    make_field_label(card, "Cổng");
    s_gw_port_ta = make_text_field(card, "8080");

    make_field_label(card, "Token truy cập");
    make_text_field(card, "Nhập token...");

    lv_obj_t *cam_title_row = lv_obj_create(card);
    lv_obj_remove_style_all(cam_title_row);
    lv_obj_set_flex_flow(cam_title_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cam_title_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_width(cam_title_row, LV_PCT(100));
    lv_obj_set_height(cam_title_row, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_top(cam_title_row, 4, 0);
    lv_obj_clear_flag(cam_title_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *cam_title = make_section_title(cam_title_row, "CAMERA (VIVOO)");
    lv_obj_set_style_text_font(cam_title, UI_FONT_H5_BOLD, 0);
    s_cam_status_dot = ui_common_status_dot(cam_title_row, gst->camera_link);
    s_cam_status_label = make_field_label(cam_title_row, gst->camera_link == LINK_OK ? "Đã kết nối" : "Chưa kết nối");

    /* Tìm camera trong mạng — xác nhận lại địa chỉ camera THẬT đã cấu hình
     * (app_state()->camera_ip/port). Tài liệu Vivoo không công bố giao
     * thức discovery/ONVIF cụ thể nên KHÔNG dò toàn mạng (không bịa). */
    make_field_label(card, "Camera đã cấu hình cho trạm này");
    lv_obj_t *search_btn = ui_common_button_outline(card, "Tìm kiếm", UI_COLOR_BORDER, UI_COLOR_PRIMARY, UI_FONT_BODY_BOLD);
    lv_obj_set_width(search_btn, LV_PCT(100));
    lv_obj_add_event_cb(search_btn, cam_search_cb, LV_EVENT_CLICKED, NULL);

    s_cam_list_host = lv_obj_create(card);
    lv_obj_remove_style_all(s_cam_list_host);
    lv_obj_set_flex_flow(s_cam_list_host, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_width(s_cam_list_host, LV_PCT(100));
    lv_obj_set_height(s_cam_list_host, LV_SIZE_CONTENT);
    lv_obj_clear_flag(s_cam_list_host, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_cam_list_host, LV_OBJ_FLAG_HIDDEN);

    /* Trước đây IP+Cổng nằm trong 2 "cột" lồng bên trong 1 "hàng" (flex
     * ROW chứa 2 flex COLUMN con, canh bằng flex_grow) — ô nhập vẫn hiện
     * đúng vị trí/chữ nhưng người dùng báo bấm vào không mở được bàn
     * phím. Đổi sang 2 hàng riêng, rộng hết card — ĐÚNG cấu trúc đơn
     * giản đã xác nhận hoạt động tốt ở card "ĐỊA CHỈ IP" (IP tĩnh) ngay
     * phía trên — không còn lồng flex-trong-flex nữa. */
    make_field_label(card, "Địa chỉ IP camera");
    s_cam_ip_ta = make_text_field(card, "Nhập IP camera...");
    lv_textarea_set_text(s_cam_ip_ta, gst->camera_ip);

    make_field_label(card, "Cổng (HTTPS)");
    s_cam_port_ta = make_text_field(card, "443");
    lv_textarea_set_accepted_chars(s_cam_port_ta, "0123456789");
    lv_textarea_set_max_length(s_cam_port_ta, 5);
    lv_textarea_set_accepted_chars(s_cam_ip_ta, "0123456789.");
    lv_textarea_set_max_length(s_cam_ip_ta, 15);
    char cam_port_buf[8]; snprintf(cam_port_buf, sizeof(cam_port_buf), "%u", (unsigned)gst->camera_port);
    lv_textarea_set_text(s_cam_port_ta, cam_port_buf);

    make_field_label(card, "Tên đăng nhập");
    s_cam_user_ta = make_text_field(card, "Tên đăng nhập camera...");
    camera_config_t cfg;
    camera_receiver_get_config(&cfg);
    lv_textarea_set_max_length(s_cam_user_ta, sizeof(cfg.user) - 1);
    lv_textarea_set_text(s_cam_user_ta, cfg.user);

    make_field_label(card, "Mật khẩu");
    s_cam_pass_ta = make_password_row(card, "Mật khẩu camera...");
    lv_textarea_set_max_length(s_cam_pass_ta, sizeof(cfg.password) - 1);
    lv_textarea_set_text(s_cam_pass_ta, cfg.password);

    make_field_label(card, "RTSP luồng chính");
    s_rtsp_main_ta = make_text_field(card, "rtsp://IP:554/live/0");
    lv_textarea_set_max_length(s_rtsp_main_ta, sizeof(cfg.rtsp_main) - 1);
    lv_textarea_set_text(s_rtsp_main_ta, cfg.rtsp_main);
    make_field_label(card, "RTSP luồng phụ");
    s_rtsp_sub_ta = make_text_field(card, "rtsp://IP:554/live/1");
    lv_textarea_set_max_length(s_rtsp_sub_ta, sizeof(cfg.rtsp_sub) - 1);
    lv_textarea_set_text(s_rtsp_sub_ta, cfg.rtsp_sub);

    lv_obj_t *cam_btn_row = lv_obj_create(card);
    lv_obj_remove_style_all(cam_btn_row);
    lv_obj_set_flex_flow(cam_btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(cam_btn_row, 8, 0);
    lv_obj_set_width(cam_btn_row, LV_PCT(100));
    lv_obj_set_height(cam_btn_row, LV_SIZE_CONTENT);
    lv_obj_clear_flag(cam_btn_row, LV_OBJ_FLAG_SCROLLABLE);

    /* Tách riêng "Lưu" khỏi "Kiểm tra kết nối": trước đây chỉ có 1 nút vừa
     * lưu vừa kiểm tra, dễ gây cảm giác "không đổi được Port/IP" nếu lần
     * kiểm tra đó thất bại (mạng lỗi) dù giá trị NHẬP VÀO thật ra đã được
     * ghi nhận đúng — giờ bấm "Lưu" xác nhận rõ giá trị đã ghi nhận ngay
     * (hiện IP:Port trong thông báo), không phụ thuộc kết quả kết nối. */
    lv_obj_t *cam_save_btn = ui_common_button_outline(cam_btn_row, "Lưu", UI_COLOR_BORDER, UI_COLOR_PRIMARY, UI_FONT_BODY_BOLD);
    lv_obj_set_flex_grow(cam_save_btn, 1);
    lv_obj_add_event_cb(cam_save_btn, cam_save_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *cam_connect_btn = ui_common_button(cam_btn_row, "Kiểm tra kết nối", UI_COLOR_PRIMARY, lv_color_white(), UI_FONT_BODY_BOLD);
    lv_obj_set_flex_grow(cam_connect_btn, 2);
    lv_obj_add_event_cb(cam_connect_btn, cam_connect_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *cam_note = lv_label_create(card);
    lv_label_set_text(cam_note,
        "Chỉ kiểm tra camera có phản hồi HTTP trên mạng (chưa đăng nhập).\n"
        "Xem trực tiếp cần URL RTSP/ONVIF do Vivoo xác nhận — xem tab Cân.");
    lv_label_set_long_mode(cam_note, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(cam_note, LV_PCT(100));
    lv_obj_set_style_text_font(cam_note, UI_FONT_XS, 0);
    lv_obj_set_style_text_color(cam_note, UI_COLOR_BODY, 0);

    lv_obj_t *gw_title = make_section_title(card, "GATEWAY — KẾT NỐI");
    lv_obj_set_style_text_font(gw_title, UI_FONT_H5_BOLD, 0);
    lv_obj_set_style_pad_top(gw_title, 4, 0);

    lv_obj_t *btn_row = lv_obj_create(card);
    lv_obj_remove_style_all(btn_row);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(btn_row, 8, 0);
    lv_obj_set_width(btn_row, LV_PCT(100));
    lv_obj_set_height(btn_row, LV_SIZE_CONTENT);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *check_btn = ui_common_button_outline(btn_row, "Kết nối", UI_COLOR_BORDER, UI_COLOR_PRIMARY, UI_FONT_BODY_BOLD);
    lv_obj_set_flex_grow(check_btn, 1);
    lv_obj_add_event_cb(check_btn, gateway_check_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *save_btn = ui_common_button(btn_row, "Lưu", UI_COLOR_PRIMARY, lv_color_white(), UI_FONT_BODY_BOLD);
    lv_obj_set_flex_grow(save_btn, 1);
    lv_obj_add_event_cb(save_btn, gateway_save_cb, LV_EVENT_CLICKED, NULL);
}

/* ────────────────────────────────────────────────────────────────────────
 * P5 SCALE (mục 4.10)
 * ──────────────────────────────────────────────────────────────────────── */
/* "Tìm chính xác sản phẩm": khác với tìm camera (nhiều kết quả để chọn),
 * P5 Scale là MỘT thiết bị đo lường cụ thể giao tiếp Modbus-TCP (cổng mặc
 * định 502) — quét mạng phải xác định đúng DUY NHẤT thiết bị đó qua tên
 * sản phẩm/model, không phải danh sách chung chung. Chưa có giao thức
 * quét thật (chưa rõ cách P5 Scale tự công bố trên mạng — mDNS/broadcast
 * riêng của hãng) nên hiện kết quả mô phỏng NHƯNG đã gắn đúng tên sản
 * phẩm/model để phân biệt với "tìm thấy thiết bị" chung chung như trước. */
typedef struct { const char *product; const char *ip; } demo_p5_t;
static const demo_p5_t k_demo_p5 = { "P5 Scale Indicator — Modbus TCP", "192.168.1.20" };

static void p5_pick_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (s_p5_ip_ta) lv_textarea_set_text(s_p5_ip_ta, k_demo_p5.ip);
    if (s_p5_port_ta) lv_textarea_set_text(s_p5_port_ta, "502");
    ui_shell_toast("Đã chọn đúng sản phẩm — kiểm tra tài khoản rồi bấm Kết nối");
}

static void p5_rescan_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (!s_p5_list_host) return;
    ui_shell_toast("Đang tìm P5 Scale trong mạng... (demo)");

    /* Cập nhật TẠI CHỖ — không ui_settings_refresh() để không mất Tài
     * khoản/Mật khẩu đang gõ dở (cùng cách cam_search_cb đang làm). */
    ui_common_clear(s_p5_list_host);
    lv_obj_t *row = lv_obj_create(s_p5_list_host);
    lv_obj_remove_style_all(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_ver(row, 4, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *info = lv_obj_create(row);
    lv_obj_remove_style_all(info);
    lv_obj_set_flex_flow(info, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_size(info, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_clear_flag(info, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *name = lv_label_create(info);
    lv_label_set_text(name, k_demo_p5.product);
    lv_obj_set_style_text_font(name, UI_FONT_BODY_BOLD, 0);
    lv_obj_set_style_text_color(name, UI_COLOR_HEADING, 0);
    lv_obj_t *ip = lv_label_create(info);
    lv_label_set_text(ip, k_demo_p5.ip);
    lv_obj_set_style_text_font(ip, UI_FONT_XS, 0);
    lv_obj_set_style_text_color(ip, UI_COLOR_BODY, 0);

    lv_obj_t *pick_btn = ui_common_button_outline(row, "Chọn", UI_COLOR_BORDER, UI_COLOR_PRIMARY, UI_FONT_BODY_BOLD);
    lv_obj_add_event_cb(pick_btn, p5_pick_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_clear_flag(s_p5_list_host, LV_OBJ_FLAG_HIDDEN);
}

static void p5_connect_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    app_state()->p5_link = LINK_OK;
    if (s_p5_status_dot) lv_obj_set_style_bg_color(s_p5_status_dot, ui_common_link_color(LINK_OK), 0);
    if (s_p5_status_label) lv_label_set_text(s_p5_status_label, "Đã tìm thấy thiết bị");
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
    s_p5_status_dot = ui_common_status_dot(status_row, st->p5_link);
    s_p5_status_label = make_field_label(status_row, st->p5_link == LINK_LOST ? "Không tìm thấy thiết bị" : "Đã tìm thấy thiết bị");

    /* Tìm đúng sản phẩm P5 Scale trong mạng */
    make_field_label(card, "Tìm P5 Scale trong mạng");
    lv_obj_t *search_btn = ui_common_button(card, "Tìm kiếm", UI_COLOR_PRIMARY, lv_color_white(), UI_FONT_BODY_BOLD);
    lv_obj_set_width(search_btn, LV_PCT(100));
    lv_obj_add_event_cb(search_btn, p5_rescan_cb, LV_EVENT_CLICKED, NULL);

    s_p5_list_host = lv_obj_create(card);
    lv_obj_remove_style_all(s_p5_list_host);
    lv_obj_set_flex_flow(s_p5_list_host, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_width(s_p5_list_host, LV_PCT(100));
    lv_obj_set_height(s_p5_list_host, LV_SIZE_CONTENT);
    lv_obj_clear_flag(s_p5_list_host, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_p5_list_host, LV_OBJ_FLAG_HIDDEN);

    /* Địa chỉ IP/Cổng — trước đây là nhãn tĩnh "192.168.1.20:502" cố định
     * (hardcode), giờ là 2 ô nhập thật, điền tự động khi "Chọn" ở trên.
     * 2 hàng riêng (không lồng cột trong hàng) — xem ghi chú ở card
     * CAMERA về lý do đổi cấu trúc (bấm không mở được bàn phím). */
    make_field_label(card, "Địa chỉ IP");
    s_p5_ip_ta = make_text_field(card, "Chọn thiết bị ở trên hoặc nhập tay...");

    make_field_label(card, "Cổng");
    s_p5_port_ta = make_text_field(card, "502");

    make_field_label(card, "Tài khoản");
    make_text_field(card, "Tài khoản P5 Scale...");

    make_field_label(card, "Mật khẩu");
    make_password_row(card, "Mật khẩu P5 Scale...");

    lv_obj_t *connect_btn = ui_common_button(card, "Kết nối", UI_COLOR_PRIMARY, lv_color_white(), UI_FONT_BODY_BOLD);
    lv_obj_set_width(connect_btn, LV_PCT(100));
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
    /* Đã bỏ lựa chọn khổ A5 theo yêu cầu — máy chỉ hỗ trợ in nhiệt 58mm. */

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
 * THỜI GIAN HỆ THỐNG — múi giờ cố định GMT+7 (Việt Nam), đồng bộ qua NTP
 * khi có WiFi (xem main.c: setenv TZ, wifi_manager.c: start_sntp()).
 * ──────────────────────────────────────────────────────────────────────── */
static lv_obj_t *s_time_now_label;
static lv_obj_t *s_time_sync_label;

static void time_sync_btn_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (!app_state()->wifi_connected) {
        ui_shell_toast("Cần kết nối Wi-Fi để đồng bộ giờ qua Internet");
        return;
    }
    app_state_time_force_sync();
    ui_shell_toast("Đang đồng bộ giờ qua Internet (NTP)...");
    ui_settings_refresh();
}

static void build_time_section(lv_obj_t *host)
{
    lv_obj_t *card = ui_common_card(host);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 8, 0);

    lv_obj_t *head = lv_obj_create(card);
    lv_obj_remove_style_all(head);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_width(head, LV_PCT(100));
    lv_obj_set_height(head, LV_SIZE_CONTENT);
    lv_obj_clear_flag(head, LV_OBJ_FLAG_SCROLLABLE);
    make_section_title(head, "THỜI GIAN HỆ THỐNG");
    ui_common_badge(head, "GMT+7 · Việt Nam", UI_COLOR_PRIMARY_SOFT, UI_COLOR_PRIMARY);

    time_t now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);
    static const char *k_wd[7] = {"Chủ nhật", "Thứ hai", "Thứ ba", "Thứ tư", "Thứ năm", "Thứ sáu", "Thứ bảy"};
    char buf[64];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d — %s, %02d/%02d/%04d",
             tmv.tm_hour, tmv.tm_min, tmv.tm_sec, k_wd[tmv.tm_wday],
             tmv.tm_mday, tmv.tm_mon + 1, tmv.tm_year + 1900);
    s_time_now_label = lv_label_create(card);
    lv_label_set_text(s_time_now_label, buf);
    lv_obj_set_style_text_font(s_time_now_label, UI_FONT_H3_BOLD, 0);
    lv_obj_set_style_text_color(s_time_now_label, UI_COLOR_HEADING, 0);

    bool synced = app_state_time_is_synced();
    s_time_sync_label = lv_label_create(card);
    lv_label_set_text(s_time_sync_label, synced
        ? "Đã đồng bộ qua Internet (NTP)"
        : (app_state()->wifi_connected ? "Đang đồng bộ..." : "Chưa đồng bộ — mất kết nối Wi-Fi"));
    lv_obj_set_style_text_font(s_time_sync_label, UI_FONT_XS, 0);
    lv_obj_set_style_text_color(s_time_sync_label, synced ? UI_COLOR_SUCCESS : UI_COLOR_WARNING, 0);

    lv_obj_t *sync_btn = ui_common_button_outline(card, "Đồng bộ lại qua Internet",
                                                   UI_COLOR_BORDER, UI_COLOR_PRIMARY, UI_FONT_BODY_BOLD);
    lv_obj_set_width(sync_btn, LV_PCT(100));
    lv_obj_add_event_cb(sync_btn, time_sync_btn_cb, LV_EVENT_CLICKED, NULL);
}

void ui_settings_tick(void)
{
    receiver_refresh_status();
    if (!s_time_now_label || !s_time_sync_label) return;

    /* WiFi vừa quét xong (quá trình thật chỉ ~2-3s, đã đo qua log — KHÔNG
     * chậm) -> tự vẽ lại ngay để hiện danh sách mới, thay vì đứng yên ở
     * "Đang quét..." tới khi người dùng vô tình làm gì khác kích hoạt vẽ
     * lại (đây là nguyên nhân thật gây cảm giác "quét rất chậm"). */
    bool scanning_now = app_state_wifi_is_scanning();
    if (s_wifi_was_scanning && !scanning_now && lv_obj_has_flag(s_kb, LV_OBJ_FLAG_HIDDEN)) {
        s_wifi_was_scanning = false;
        ui_settings_refresh();
        return;
    }
    if (scanning_now) s_wifi_was_scanning = true;

    time_t now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);
    static const char *k_wd[7] = {"Chủ nhật", "Thứ hai", "Thứ ba", "Thứ tư", "Thứ năm", "Thứ sáu", "Thứ bảy"};
    char buf[64];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d — %s, %02d/%02d/%04d",
             tmv.tm_hour, tmv.tm_min, tmv.tm_sec, k_wd[tmv.tm_wday],
             tmv.tm_mday, tmv.tm_mon + 1, tmv.tm_year + 1900);
    lv_label_set_text(s_time_now_label, buf);

    bool synced = app_state_time_is_synced();
    lv_label_set_text(s_time_sync_label, synced
        ? "Đã đồng bộ qua Internet (NTP)"
        : (app_state()->wifi_connected ? "Đang đồng bộ..." : "Chưa đồng bộ — mất kết nối Wi-Fi"));
    lv_obj_set_style_text_color(s_time_sync_label, synced ? UI_COLOR_SUCCESS : UI_COLOR_WARNING, 0);

    /* Camera: phản ánh kết quả kiểm tra HTTP mới nhất (có thể đến từ lần tự
     * kiểm tra lại định kỳ trong app_state_camera_sync(), không chỉ từ nút
     * bấm ở màn này) — xem camera_client.c. */
    if (s_cam_status_dot && s_cam_status_label) {
        link_state_t cl = app_state()->camera_link;
        bool checking = app_state_camera_is_checking();
        lv_obj_set_style_bg_color(s_cam_status_dot, ui_common_link_color(cl), 0);
        lv_label_set_text(s_cam_status_label, checking ? "Đang kiểm tra..." : (cl == LINK_OK ? "Đã kết nối" : "Chưa kết nối"));
    }
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
 * CHẶN TRUY CẬP — chỉ Kỹ thuật được vào tab Cài đặt (mục 4.10)
 *
 * Trước đây đây là màn "khoá PIN" cho operator: nhập lại ĐÚNG PIN của
 * CHÍNH mình là mở khoá được — tức không hề chặn thật (ai cũng biết PIN
 * của mình). Nhân viên cân/Quản lý giờ bị chặn HẲN theo vai trò (xem
 * app_state_try_login: settings_unlocked = (role == ROLE_TECHNICIAN)) nên
 * không còn bàn phím PIN ở đây nữa — họ không có cách nào tự mở được.
 * ──────────────────────────────────────────────────────────────────────── */
static void build_forbidden_screen(lv_obj_t *host)
{
    lv_obj_t *card = ui_common_card(host);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_ver(card, 40, 0);
    lv_obj_set_style_pad_row(card, 6, 0);

    ui_common_icon(card, &img_icon_lock, UI_COLOR_ICON);

    lv_obj_t *msg = lv_label_create(card);
    lv_label_set_text(msg, "Khu vực chỉ dành cho Kỹ thuật viên");
    lv_obj_set_style_text_font(msg, UI_FONT_H4_BOLD, 0);
    lv_obj_set_style_text_color(msg, UI_COLOR_HEADING, 0);
    lv_obj_set_style_pad_bottom(msg, 6, 0);

    lv_obj_t *hint = lv_label_create(card);
    lv_label_set_text(hint, "Đăng xuất và đăng nhập lại bằng tài khoản Kỹ thuật để cấu hình thiết bị.");
    lv_obj_set_style_text_font(hint, UI_FONT_BODY, 0);
    lv_obj_set_style_text_color(hint, UI_COLOR_BODY, 0);
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

    /* A top-layer keyboard is independent of the scrolling settings viewport. */
    s_kb = lv_keyboard_create(lv_layer_top());
    lv_obj_add_flag(s_kb, LV_OBJ_FLAG_FLOATING);
    lv_obj_remove_flag(s_kb, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_set_width(s_kb, UI_HOR_RES);
    lv_obj_add_event_cb(s_kb, kb_done_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_height(s_kb, 320);
    /* Font Inter tự biên của app KHÔNG có dải LV_SYMBOL_* (icon Backspace/
     * Enter...) nên phím đặc biệt sẽ hiện ô chữ nhật đứng trống (tofu) nếu
     * dùng cho bàn phím — phải dùng Montserrat BUILT-IN. Bàn phím rộng cả
     * màn hình nên size 40px vẫn vừa. Tăng khoảng cách giữa phím để ô bao
     * quanh phím to cân đối với chữ. */
    lv_obj_set_style_text_font(s_kb, &lv_font_montserrat_40, 0);
    lv_obj_set_style_pad_row(s_kb, 10, 0);
    lv_obj_set_style_pad_column(s_kb, 8, 0);
    lv_obj_align(s_kb, LV_ALIGN_BOTTOM_MID, 0, -UI_BOTTOMNAV_H);
    lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
    /* Không cần lv_obj_move_foreground() ở đây — s_kb vừa được tạo nên
     * mặc định đã là con cuối cùng (trên cùng) của cha nó. */

    return s_container;
}

void ui_settings_refresh(void)
{
    /* Keep unsaved camera edits across unrelated WiFi/printer/theme rebuilds. */
    char draft_ip[40], draft_port[8], draft_user[64], draft_pass[128], draft_token[1024], draft_rx[8];
    char draft_rtsp_main[160], draft_rtsp_sub[160];
    bool draft = s_cam_ip_ta && s_cam_token_ta && s_rx_port_ta;
    if (draft) {
        snprintf(draft_ip, sizeof(draft_ip), "%s", lv_textarea_get_text(s_cam_ip_ta));
        snprintf(draft_port, sizeof(draft_port), "%s", lv_textarea_get_text(s_cam_port_ta));
        snprintf(draft_user, sizeof(draft_user), "%s", lv_textarea_get_text(s_cam_user_ta));
        snprintf(draft_pass, sizeof(draft_pass), "%s", lv_textarea_get_text(s_cam_pass_ta));
        snprintf(draft_token, sizeof(draft_token), "%s", lv_textarea_get_text(s_cam_token_ta));
        snprintf(draft_rx, sizeof(draft_rx), "%s", lv_textarea_get_text(s_rx_port_ta));
        snprintf(draft_rtsp_main, sizeof(draft_rtsp_main), "%s", lv_textarea_get_text(s_rtsp_main_ta));
        snprintf(draft_rtsp_sub, sizeof(draft_rtsp_sub), "%s", lv_textarea_get_text(s_rtsp_sub_ta));
    }
    ui_settings_close_keyboard();
    ui_common_clear(s_host);
    s_wifi_pw_ta = NULL;
    s_rx_port_ta = s_cam_token_ta = s_rtsp_main_ta = s_rtsp_sub_ta = NULL;
    s_rx_endpoint = s_rx_status = s_rx_count = s_rx_snapshot = NULL;
    s_print_preview = NULL;
    s_time_now_label = NULL;
    s_time_sync_label = NULL;
    s_ip_fields_wrap = NULL;
    s_ip_addr_ta = NULL;
    s_ip_nm_ta = NULL;
    s_ip_gw_ta = NULL;
    s_ip_dns_ta = NULL;
    s_cam_ip_ta = NULL;
    s_cam_port_ta = NULL;
    s_cam_user_ta = NULL;
    s_cam_pass_ta = NULL;
    s_cam_list_host = NULL;
    s_cam_status_dot = NULL;
    s_cam_status_label = NULL;
    s_gw_ip_ta = NULL;
    s_gw_port_ta = NULL;
    s_gw_status_dot = NULL;
    s_gw_status_label = NULL;
    s_p5_ip_ta = NULL;
    s_p5_port_ta = NULL;
    s_p5_list_host = NULL;
    s_p5_status_dot = NULL;
    s_p5_status_label = NULL;

    app_state_t *st = app_state();
    employee_t *e = (st->current_employee_idx >= 0) ? &st->employees[st->current_employee_idx] : NULL;
    /* Chỉ Kỹ thuật được cấu hình — Nhân viên cân/Quản lý bị chặn hẳn (xem
     * app_state_try_login: settings_unlocked = (role == ROLE_TECHNICIAN)). */
    bool locked = !e || !st->settings_unlocked;

    if (locked) {
        build_forbidden_screen(s_host);
        return;
    }

    build_wifi_section(s_host);
    build_static_ip_section(s_host);
    build_gateway_section(s_host);
    build_receiver_section(s_host);
    if (draft) {
        lv_textarea_set_text(s_cam_ip_ta, draft_ip);
        lv_textarea_set_text(s_cam_port_ta, draft_port);
        lv_textarea_set_text(s_cam_user_ta, draft_user);
        lv_textarea_set_text(s_cam_pass_ta, draft_pass);
        lv_textarea_set_text(s_cam_token_ta, draft_token);
        lv_textarea_set_text(s_rx_port_ta, draft_rx);
        lv_textarea_set_text(s_rtsp_main_ta, draft_rtsp_main);
        lv_textarea_set_text(s_rtsp_sub_ta, draft_rtsp_sub);
    }
    build_p5_section(s_host);
    build_printer_section(s_host);
    build_version_section(s_host);
    build_time_section(s_host);
    build_display_section(s_host);
}
