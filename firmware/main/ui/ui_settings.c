#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
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
static bool s_wifi_show_all;   /* true = hiện đủ danh sách; false = thu gọn khi đã kết nối */

/* ── Gateway: tìm camera trong mạng ──────────────────────────────────────── */
static lv_obj_t *s_cam_ip_ta;      /* ô "Địa chỉ IP camera" — Chọn từ danh sách tìm được sẽ điền vào đây */
static lv_obj_t *s_cam_list_host;  /* danh sách camera tìm được — ẩn cho tới khi bấm Tìm kiếm */

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
 * GATEWAY (mục 4.10)
 * ──────────────────────────────────────────────────────────────────────── */
static void gateway_check_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_shell_toast("Đang kết nối Gateway/Camera... (demo)");
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

/* Danh sách camera "tìm thấy" mô phỏng — chưa có giao thức quét thật
 * (ONVIF/mDNS) trong app_state.h. */
typedef struct { const char *name; const char *ip; } demo_camera_t;
static const demo_camera_t k_demo_cameras[] = {
    { "Camera cổng vào",  "192.168.1.21" },
    { "Camera khu cân",   "192.168.1.22" },
    { "Camera bãi xuất",  "192.168.1.23" },
};
#define DEMO_CAMERA_COUNT ((int)(sizeof(k_demo_cameras) / sizeof(k_demo_cameras[0])))

static void cam_pick_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= DEMO_CAMERA_COUNT || !s_cam_ip_ta) return;
    lv_textarea_set_text(s_cam_ip_ta, k_demo_cameras[idx].ip);
    ui_shell_toast("Đã chọn camera — kiểm tra tài khoản rồi bấm Kết nối");
}

static void cam_search_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (!s_cam_list_host) return;
    ui_shell_toast("Đang tìm camera trong mạng... (demo)");

    /* Cập nhật TẠI CHỖ (không gọi ui_settings_refresh) để không mất nội
     * dung đang gõ dở ở Token/tài khoản camera — cùng cách printer preview
     * toggle đang làm với s_print_preview. */
    ui_common_clear(s_cam_list_host);
    for (int i = 0; i < DEMO_CAMERA_COUNT; i++) {
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
        lv_label_set_text(name, k_demo_cameras[i].name);
        lv_obj_set_style_text_font(name, UI_FONT_BODY_BOLD, 0);
        lv_obj_set_style_text_color(name, UI_COLOR_HEADING, 0);
        lv_obj_t *ip = lv_label_create(info);
        lv_label_set_text(ip, k_demo_cameras[i].ip);
        lv_obj_set_style_text_font(ip, UI_FONT_XS, 0);
        lv_obj_set_style_text_color(ip, UI_COLOR_BODY, 0);

        lv_obj_t *pick_btn = ui_common_button_outline(row, "Chọn", UI_COLOR_BORDER, UI_COLOR_PRIMARY, UI_FONT_BODY_BOLD);
        lv_obj_add_event_cb(pick_btn, cam_pick_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
    lv_obj_clear_flag(s_cam_list_host, LV_OBJ_FLAG_HIDDEN);
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

    /* Tìm camera trong mạng */
    make_field_label(card, "Tìm camera trong mạng");
    lv_obj_t *search_row = lv_obj_create(card);
    lv_obj_remove_style_all(search_row);
    lv_obj_set_flex_flow(search_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(search_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(search_row, 8, 0);
    lv_obj_set_width(search_row, LV_PCT(100));
    lv_obj_set_height(search_row, LV_SIZE_CONTENT);
    lv_obj_clear_flag(search_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *search_ta = make_text_field(search_row, "Lọc theo tên/IP (tuỳ chọn)...");
    lv_obj_set_flex_grow(search_ta, 1);
    lv_obj_t *search_btn = ui_common_button(search_row, "Tìm kiếm", UI_COLOR_PRIMARY, lv_color_white(), UI_FONT_BODY_BOLD);
    lv_obj_add_event_cb(search_btn, cam_search_cb, LV_EVENT_CLICKED, NULL);

    s_cam_list_host = lv_obj_create(card);
    lv_obj_remove_style_all(s_cam_list_host);
    lv_obj_set_flex_flow(s_cam_list_host, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_width(s_cam_list_host, LV_PCT(100));
    lv_obj_set_height(s_cam_list_host, LV_SIZE_CONTENT);
    lv_obj_clear_flag(s_cam_list_host, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_cam_list_host, LV_OBJ_FLAG_HIDDEN);

    make_field_label(card, "Địa chỉ IP camera");
    s_cam_ip_ta = make_text_field(card, "Chọn camera ở trên hoặc nhập tay...");

    make_field_label(card, "Tên đăng nhập");
    make_text_field(card, "Tên đăng nhập camera...");

    make_field_label(card, "Mật khẩu");
    make_password_row(card, "Mật khẩu camera...");

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
    if (!s_time_now_label || !s_time_sync_label) return;

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
    s_time_now_label = NULL;
    s_time_sync_label = NULL;
    s_cam_ip_ta = NULL;
    s_cam_list_host = NULL;

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
    build_gateway_section(s_host);
    build_p5_section(s_host);
    build_printer_section(s_host);
    build_version_section(s_host);
    build_time_section(s_host);
    build_display_section(s_host);
}
