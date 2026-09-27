#pragma once
/**
 * @file ui_theme.h
 * @brief Design tokens — Cân Máy Xúc V3
 *        Dark industrial theme, yellow accent
 *        Căn cứ screen01.png
 */

#include "lvgl.h"

/* ============================================================
 * COLORS
 * ============================================================ */
#define UI_COLOR_BG           lv_color_hex(0x1A1A1A)   /* nền toàn app */
#define UI_COLOR_CARD         lv_color_hex(0x242424)   /* nền card/panel */
#define UI_COLOR_ACCENT       lv_color_hex(0xF5C800)   /* vàng chủ đạo */
#define UI_COLOR_ACCENT_DIM   lv_color_hex(0xB89500)   /* vàng tối (pressed) */
#define UI_COLOR_TEXT_PRI     lv_color_hex(0xFFFFFF)   /* chữ trắng */
#define UI_COLOR_TEXT_SEC     lv_color_hex(0x888888)   /* chữ xám */
#define UI_COLOR_TEXT_DIM     lv_color_hex(0x555555)   /* chữ rất tối */
#define UI_COLOR_BORDER       lv_color_hex(0x333333)   /* đường viền */
#define UI_COLOR_PROGRESS_BG  lv_color_hex(0x333333)   /* thanh progress track */
#define UI_COLOR_STATUS_WAIT  lv_color_hex(0x3A3A3A)   /* badge CHỜ XỬ LÝ */
#define UI_COLOR_NAV_BG       lv_color_hex(0x111111)   /* nền bottom nav */
#define UI_COLOR_NAV_ACTIVE   lv_color_hex(0xF5C800)   /* icon active */

/* ============================================================
 * OPACITY
 * ============================================================ */
#define UI_OPA_FULL    LV_OPA_COVER
#define UI_OPA_NONE    LV_OPA_TRANSP

/* ============================================================
 * FONTS — SVN Gilroy Việt hóa
 * ============================================================ */
/*
 * Compile font với lv_font_conv (cần Node.js):
 *   npm install -g lv_font_conv
 *   lv_font_conv --font SVNGilroy-Bold.ttf \
 *     -r 0x0020-0x007E,0x00C0-0x024F,0x1EA0-0x1EF9 \
 *     --size 20 --format lvgl --bpp 4 -o main/ui/fonts/font_gilroy_20.c
 *   (lặp cho size 22, 26, 32, 44, 48, 96)
 *
 * Đặt tất cả .c vào main/ui/fonts/ và thêm vào CMakeLists.txt:
 *   target_sources(${COMPONENT_TARGET} PRIVATE
 *       "ui/fonts/font_gilroy_20.c" ... )
 *
 * Fallback: nếu chưa có file font, dùng Montserrat bằng cách
 * comment block #ifdef bên dưới.
 */

#ifdef CONFIG_UI_FONT_GILROY
/* SVN Gilroy — khai báo extern từ font_gilroy_XX.c */
LV_FONT_DECLARE(font_gilroy_20)
LV_FONT_DECLARE(font_gilroy_22)
LV_FONT_DECLARE(font_gilroy_26)
LV_FONT_DECLARE(font_gilroy_32)
LV_FONT_DECLARE(font_gilroy_44)
LV_FONT_DECLARE(font_gilroy_48)
LV_FONT_DECLARE(font_gilroy_96)

#define UI_FONT_TINY        &font_gilroy_20   /* label nhỏ, badge   */
#define UI_FONT_SMALL       &font_gilroy_22   /* sub-label          */
#define UI_FONT_NORMAL      &font_gilroy_26   /* body text          */
#define UI_FONT_MEDIUM      &font_gilroy_32   /* header, btn label  */
#define UI_FONT_LARGE       &font_gilroy_44   /* stats, order value */
#define UI_FONT_XLARGE      &font_gilroy_48   /* screen title       */
#define UI_FONT_WEIGHT_NUM  &font_gilroy_96   /* số cân lớn         */

#else
/* Fallback: LVGL Montserrat (enable trong lv_conf.h:
 *   LV_FONT_MONTSERRAT_20/22/26/32/44/48 = 1) */
#define UI_FONT_TINY        &lv_font_montserrat_20
#define UI_FONT_SMALL       &lv_font_montserrat_22
#define UI_FONT_NORMAL      &lv_font_montserrat_26
#define UI_FONT_MEDIUM      &lv_font_montserrat_32
#define UI_FONT_LARGE       &lv_font_montserrat_44
#define UI_FONT_XLARGE      &lv_font_montserrat_48
#define UI_FONT_WEIGHT_NUM  &lv_font_montserrat_48
#endif

/* ============================================================
 * DIMENSIONS  — Target: 800×1280 portrait (≈ 1.6× so với 480×800)
 * ============================================================ */
#define UI_SCREEN_W       800
#define UI_SCREEN_H      1280
#define UI_HEADER_H        96   /* 60 × 1.6 */
#define UI_ORDER_CARD_H   130   /* 80 × 1.6 */
#define UI_WEIGHT_CARD_H  512   /* 320 × 1.6 */
#define UI_STATS_H        144   /* 90 × 1.6 */
#define UI_BTN_H          112   /* 70 × 1.6 */
#define UI_NAV_H          128   /* 80 × 1.6 */

#define UI_PAD_SM          12   /* 8  × 1.5 */
#define UI_PAD_MD          24   /* 16 × 1.5 */
#define UI_PAD_LG          36   /* 24 × 1.5 */
#define UI_RADIUS_SM        8
#define UI_RADIUS_MD       18
#define UI_RADIUS_LG       28

/* ============================================================
 * STYLE HELPERS — gọi ui_theme_init() một lần để populate
 * ============================================================ */
extern lv_style_t style_screen;
extern lv_style_t style_card;
extern lv_style_t style_label_pri;
extern lv_style_t style_label_sec;
extern lv_style_t style_label_accent;
extern lv_style_t style_btn_primary;
extern lv_style_t style_btn_primary_pressed;

/**
 * @brief Khởi tạo tất cả style — gọi sau lvgl_port_init(), trước khi build UI
 */
void ui_theme_init(void);
