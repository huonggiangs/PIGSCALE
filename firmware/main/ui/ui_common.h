#pragma once
/**
 * @file ui_common.h
 * @brief Các widget dùng chung giữa nhiều màn hình (card, badge, nút, PIN...).
 */

#include "lvgl.h"
#include "app_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Xoá toàn bộ con của 1 container (dùng khi "rebuild" 1 tab) */
void ui_common_clear(lv_obj_t *cont);

/* Card nền trắng, bo góc, viền nhạt — theo --color-surface / --color-border */
lv_obj_t *ui_common_card(lv_obj_t *parent);

/* Badge dạng pill: nền màu + chữ màu, dùng cho trạng thái/loại phiếu */
lv_obj_t *ui_common_badge(lv_obj_t *parent, const char *text, lv_color_t bg, lv_color_t fg);

/* Nút bấm chữ + màu nền tuỳ chỉnh, trả về nút (label truy cập qua lv_obj_get_child) */
lv_obj_t *ui_common_button(lv_obj_t *parent, const char *text, lv_color_t bg, lv_color_t fg,
                            const lv_font_t *font);

/* Nút dạng viền (outline), nền trong suốt/nhạt — dùng cho hành động phụ / nguy hiểm */
lv_obj_t *ui_common_button_outline(lv_obj_t *parent, const char *text, lv_color_t border,
                                    lv_color_t fg, const lv_font_t *font);

/* Chấm tròn trạng thái kết nối (xanh/cam/đỏ theo link_state_t) */
lv_obj_t *ui_common_status_dot(lv_obj_t *parent, link_state_t state);
lv_color_t ui_common_link_color(link_state_t state);

/* Icon ảnh (A8) đã tô màu sẵn qua image-recolor */
lv_obj_t *ui_common_icon(lv_obj_t *parent, const void *src, lv_color_t color);

/* Modal overlay: tạo nền mờ tối phủ toàn màn hình + hộp trắng giữa màn hình.
 * Trả về con trỏ tới HỘP TRẮNG (nơi caller thêm nội dung); *out_overlay nhận
 * con trỏ overlay (dùng để đóng bằng ui_common_modal_close). */
lv_obj_t *ui_common_modal_open(lv_obj_t **out_overlay, lv_coord_t w, lv_coord_t h);
void ui_common_modal_close(lv_obj_t *overlay);

/* Toast nền tím nhạt, tự ẩn sau ~2.2s, gắn ở đầu 1 container (vd. vùng nội dung) */
void ui_common_toast(lv_obj_t *host, const char *text);

/* Hàng chấm PIN (4 chấm), tô đầy theo số ký tự đã nhập */
lv_obj_t *ui_common_pin_dots(lv_obj_t *parent, int filled_count, bool light_bg);

/* Bàn phím số 3x4 (1-9, trống, 0, xoá). digit_cb nhận ký tự '0'-'9'; backspace_cb khi bấm ⌫ */
typedef void (*ui_keypad_digit_cb_t)(char digit, void *user_data);
typedef void (*ui_keypad_backspace_cb_t)(void *user_data);
lv_obj_t *ui_common_keypad(lv_obj_t *parent, ui_keypad_digit_cb_t digit_cb,
                            ui_keypad_backspace_cb_t backspace_cb, void *user_data,
                            bool light_bg);

/* Định dạng số kiểu Việt Nam: "1.128,5" (1 chữ số thập phân) — theo handoff mục 2 */
void ui_fmt_weight_kg(char *out, size_t out_len, float kg);
/* Số nguyên không phân tách: "4", "-48" */
void ui_fmt_int(char *out, size_t out_len, int v);

#ifdef __cplusplus
}
#endif
