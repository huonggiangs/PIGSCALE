#include "ui_common.h"
#include "ui_theme.h"
#include <stdio.h>
#include <string.h>

void ui_common_clear(lv_obj_t *cont)
{
    lv_obj_clean(cont);
}

lv_obj_t *ui_common_card(lv_obj_t *parent)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_style_bg_color(card, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, UI_RADIUS_CARD, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, UI_COLOR_BORDER, 0);
    lv_obj_set_style_pad_all(card, 14, 0);
    lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    return card;
}

lv_obj_t *ui_common_badge(lv_obj_t *parent, const char *text, lv_color_t bg, lv_color_t fg)
{
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_style_bg_color(b, bg, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(b, UI_RADIUS_BADGE, 0);
    lv_obj_set_style_pad_hor(b, 10, 0);
    lv_obj_set_style_pad_ver(b, 4, 0);
    lv_obj_set_height(b, LV_SIZE_CONTENT);
    lv_obj_set_width(b, LV_SIZE_CONTENT);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(b);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, fg, 0);
    lv_obj_set_style_text_font(lbl, UI_FONT_XS, 0);
    return b;
}

lv_obj_t *ui_common_button(lv_obj_t *parent, const char *text, lv_color_t bg, lv_color_t fg,
                            const lv_font_t *font)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_style_bg_color(btn, bg, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_height(btn, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_ver(btn, 14, 0);
    lv_obj_set_style_pad_hor(btn, 18, 0);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, fg, 0);
    lv_obj_set_style_text_font(lbl, font ? font : UI_FONT_BODY_BOLD, 0);
    lv_obj_center(lbl);
    return btn;
}

lv_obj_t *ui_common_button_outline(lv_obj_t *parent, const char *text, lv_color_t border,
                                    lv_color_t fg, const lv_font_t *font)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, border, 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_height(btn, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_ver(btn, 12, 0);
    lv_obj_set_style_pad_hor(btn, 16, 0);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, fg, 0);
    lv_obj_set_style_text_font(lbl, font ? font : UI_FONT_BODY_BOLD, 0);
    lv_obj_center(lbl);
    return btn;
}

lv_color_t ui_common_link_color(link_state_t state)
{
    switch (state) {
        case LINK_OK:   return UI_COLOR_SUCCESS;
        case LINK_WEAK: return UI_COLOR_WARNING;
        default:        return UI_COLOR_DANGER;
    }
}

lv_obj_t *ui_common_status_dot(lv_obj_t *parent, link_state_t state)
{
    lv_obj_t *dot = lv_obj_create(parent);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, 10, 10);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, ui_common_link_color(state), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    return dot;
}

lv_obj_t *ui_common_icon(lv_obj_t *parent, const void *src, lv_color_t color)
{
    lv_obj_t *img = lv_image_create(parent);
    lv_image_set_src(img, src);
    lv_obj_set_style_image_recolor(img, color, 0);
    lv_obj_set_style_image_recolor_opa(img, LV_OPA_COVER, 0);
    return img;
}

lv_obj_t *ui_common_modal_open(lv_obj_t **out_overlay, lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *overlay = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, UI_HOR_RES, UI_VER_RES);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_50, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(overlay);

    lv_obj_t *box = lv_obj_create(overlay);
    lv_obj_set_style_bg_color(box, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(box, UI_RADIUS_CARD, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_pad_all(box, 20, 0);
    lv_obj_set_size(box, w, h);
    lv_obj_center(box);

    if (out_overlay) *out_overlay = overlay;
    return box;
}

void ui_common_modal_close(lv_obj_t *overlay)
{
    if (overlay) lv_obj_delete(overlay);
}

/* ── Toast ───────────────────────────────────────────────────────────────── */
static void toast_timer_cb(lv_timer_t *t)
{
    lv_obj_t *toast = (lv_obj_t *)lv_timer_get_user_data(t);
    if (toast) lv_obj_delete(toast);
    lv_timer_delete(t);
}

void ui_common_toast(lv_obj_t *host, const char *text)
{
    lv_obj_t *toast = lv_obj_create(host);
    lv_obj_remove_style_all(toast);
    lv_obj_set_style_bg_color(toast, UI_COLOR_PRIMARY_SOFT, 0);
    lv_obj_set_style_bg_opa(toast, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(toast, 10, 0);
    lv_obj_set_style_pad_hor(toast, 16, 0);
    lv_obj_set_style_pad_ver(toast, 10, 0);
    lv_obj_set_width(toast, LV_SIZE_CONTENT);
    lv_obj_set_height(toast, LV_SIZE_CONTENT);
    lv_obj_align(toast, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_clear_flag(toast, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(toast);

    lv_obj_t *lbl = lv_label_create(toast);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, UI_COLOR_PRIMARY_HOVER, 0);
    lv_obj_set_style_text_font(lbl, UI_FONT_BODY_BOLD, 0);

    lv_timer_t *timer = lv_timer_create(toast_timer_cb, 2200, toast);
    lv_timer_set_repeat_count(timer, 1);
}

/* ── PIN dots ────────────────────────────────────────────────────────────── */
lv_obj_t *ui_common_pin_dots(lv_obj_t *parent, int filled_count, bool light_bg)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, 16, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < 4; i++) {
        lv_obj_t *dot = lv_obj_create(row);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, 20, 20);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        if (i < filled_count) {
            lv_obj_set_style_bg_color(dot, light_bg ? UI_COLOR_PRIMARY : UI_COLOR_WHITE, 0);
            lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        } else {
            lv_obj_set_style_bg_opa(dot, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(dot, 2, 0);
            lv_obj_set_style_border_color(dot, light_bg ? UI_COLOR_BORDER : UI_COLOR_ON_DARK_MUTED, 0);
        }
    }
    return row;
}

/* ── Bàn phím số ─────────────────────────────────────────────────────────── */
typedef struct {
    ui_keypad_digit_cb_t digit_cb;
    ui_keypad_backspace_cb_t backspace_cb;
    void *user_data;
    char digit; /* 0 nếu là phím xoá */
} keypad_key_ctx_t;

static void keypad_key_event_cb(lv_event_t *e)
{
    keypad_key_ctx_t *ctx = (keypad_key_ctx_t *)lv_event_get_user_data(e);
    if (!ctx) return;
    if (ctx->digit == 0) {
        if (ctx->backspace_cb) ctx->backspace_cb(ctx->user_data);
    } else {
        if (ctx->digit_cb) ctx->digit_cb(ctx->digit, ctx->user_data);
    }
}

static void keypad_key_delete_cb(lv_event_t *e)
{
    keypad_key_ctx_t *ctx = (keypad_key_ctx_t *)lv_event_get_user_data(e);
    lv_free(ctx);
}

static lv_obj_t *make_key(lv_obj_t *parent, const char *label, const lv_font_t *font,
                           bool light_bg, char digit,
                           ui_keypad_digit_cb_t digit_cb, ui_keypad_backspace_cb_t backspace_cb,
                           void *user_data)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, 86, 86);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    if (light_bg) {
        lv_obj_set_style_bg_color(btn, UI_COLOR_MIST_100, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    } else {
        lv_obj_set_style_bg_color(btn, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_10, 0);
    }

    if (label) {
        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, label);
        lv_obj_set_style_text_font(lbl, font, 0);
        lv_obj_set_style_text_color(lbl, light_bg ? UI_COLOR_HEADING : lv_color_white(), 0);
        lv_obj_center(lbl);
    }

    keypad_key_ctx_t *ctx = lv_malloc(sizeof(keypad_key_ctx_t));
    ctx->digit_cb = digit_cb;
    ctx->backspace_cb = backspace_cb;
    ctx->user_data = user_data;
    ctx->digit = digit;
    lv_obj_add_event_cb(btn, keypad_key_event_cb, LV_EVENT_CLICKED, ctx);
    lv_obj_add_event_cb(btn, keypad_key_delete_cb, LV_EVENT_DELETE, ctx);
    return btn;
}

lv_obj_t *ui_common_keypad(lv_obj_t *parent, ui_keypad_digit_cb_t digit_cb,
                            ui_keypad_backspace_cb_t backspace_cb, void *user_data,
                            bool light_bg)
{
    lv_obj_t *grid = lv_obj_create(parent);
    lv_obj_remove_style_all(grid);
    lv_obj_set_size(grid, 3 * 86 + 2 * 14, 4 * 86 + 3 * 14);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(grid, 14, 0);
    lv_obj_set_style_pad_column(grid, 14, 0);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

    const char *labels[12] = {"1","2","3","4","5","6","7","8","9", NULL, "0", LV_SYMBOL_BACKSPACE};
    const char digits[12]  = {'1','2','3','4','5','6','7','8','9', 0, '0', 0};
    for (int i = 0; i < 12; i++) {
        bool is_backspace = (i == 11);
        const lv_font_t *f = is_backspace ? &lv_font_montserrat_14 : UI_FONT_H2_BOLD;
        if (i == 9) {
            /* ô trống */
            lv_obj_t *spacer = lv_obj_create(grid);
            lv_obj_remove_style_all(spacer);
            lv_obj_set_size(spacer, 86, 86);
            continue;
        }
        make_key(grid, labels[i], f, light_bg, is_backspace ? 0 : digits[i],
                 is_backspace ? NULL : digit_cb, is_backspace ? backspace_cb : NULL, user_data);
    }
    return grid;
}

/* ── Định dạng số kiểu Việt Nam ──────────────────────────────────────────── */
void ui_fmt_weight_kg(char *out, size_t out_len, float kg)
{
    /* 1 chữ số thập phân, dấu phẩy thập phân + dấu chấm phân cách nghìn */
    long whole = (long)(kg);
    int frac = (int)((kg - (float)whole) * 10.0f + 0.5f);
    if (frac >= 10) { frac -= 10; whole += 1; }
    if (frac < 0) frac = 0;

    char whole_buf[24];
    snprintf(whole_buf, sizeof(whole_buf), "%ld", whole);
    int len = (int)strlen(whole_buf);

    char grouped[32];
    int gi = 0;
    for (int i = 0; i < len; i++) {
        if (i > 0 && (len - i) % 3 == 0) grouped[gi++] = '.';
        grouped[gi++] = whole_buf[i];
    }
    grouped[gi] = '\0';

    snprintf(out, out_len, "%s,%d", grouped, frac);
}

void ui_fmt_int(char *out, size_t out_len, int v)
{
    snprintf(out, out_len, "%d", v);
}
