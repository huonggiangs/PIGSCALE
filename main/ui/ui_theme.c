/**
 * @file ui_theme.c
 * @brief Style init cho toàn app
 */

#include "ui_theme.h"

lv_style_t style_screen;
lv_style_t style_card;
lv_style_t style_label_pri;
lv_style_t style_label_sec;
lv_style_t style_label_accent;
lv_style_t style_btn_primary;
lv_style_t style_btn_primary_pressed;

void ui_theme_init(void)
{
    /* ---- Màn hình ---- */
    lv_style_init(&style_screen);
    lv_style_set_bg_color(&style_screen, UI_COLOR_BG);
    lv_style_set_bg_opa(&style_screen, UI_OPA_FULL);
    lv_style_set_border_width(&style_screen, 0);
    lv_style_set_pad_all(&style_screen, 0);

    /* ---- Card ---- */
    lv_style_init(&style_card);
    lv_style_set_bg_color(&style_card, UI_COLOR_CARD);
    lv_style_set_bg_opa(&style_card, UI_OPA_FULL);
    lv_style_set_border_color(&style_card, UI_COLOR_BORDER);
    lv_style_set_border_width(&style_card, 1);
    lv_style_set_radius(&style_card, UI_RADIUS_MD);
    lv_style_set_pad_all(&style_card, UI_PAD_MD);

    /* ---- Label chính (trắng) ---- */
    lv_style_init(&style_label_pri);
    lv_style_set_text_color(&style_label_pri, UI_COLOR_TEXT_PRI);
    lv_style_set_text_font(&style_label_pri, UI_FONT_NORMAL);

    /* ---- Label phụ (xám) ---- */
    lv_style_init(&style_label_sec);
    lv_style_set_text_color(&style_label_sec, UI_COLOR_TEXT_SEC);
    lv_style_set_text_font(&style_label_sec, UI_FONT_SMALL);

    /* ---- Label accent (vàng) ---- */
    lv_style_init(&style_label_accent);
    lv_style_set_text_color(&style_label_accent, UI_COLOR_ACCENT);
    lv_style_set_text_font(&style_label_accent, UI_FONT_NORMAL);

    /* ---- Nút chính (vàng) ---- */
    lv_style_init(&style_btn_primary);
    lv_style_set_bg_color(&style_btn_primary, UI_COLOR_ACCENT);
    lv_style_set_bg_opa(&style_btn_primary, UI_OPA_FULL);
    lv_style_set_border_width(&style_btn_primary, 0);
    lv_style_set_radius(&style_btn_primary, UI_RADIUS_LG);
    lv_style_set_text_color(&style_btn_primary, lv_color_hex(0x1A1A1A));
    lv_style_set_text_font(&style_btn_primary, UI_FONT_MEDIUM);

    /* ---- Nút chính pressed ---- */
    lv_style_init(&style_btn_primary_pressed);
    lv_style_set_bg_color(&style_btn_primary_pressed, UI_COLOR_ACCENT_DIM);
    lv_style_set_translate_y(&style_btn_primary_pressed, 2);
}
