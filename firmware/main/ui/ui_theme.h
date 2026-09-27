#pragma once
/**
 * @file ui_theme.h
 * @brief Design tokens (màu sắc, font, kích thước) — lấy nguyên từ
 *        _ds/anio-design-system.../tokens/colors.css và typography.css
 *        của file thiết kế "Cân heo - Bảng điều khiển 10.1.dc.html".
 *
 * KHÔNG hardcode màu/số trực tiếp trong các file ui_*.c — luôn dùng hằng số
 * ở đây để toàn bộ giao diện đồng nhất và dễ chỉnh sửa về sau.
 */

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ────────────────────────────────────────────────────────────────────────
 * MÀU SẮC — nguồn: tokens/colors.css (anio-design-system)
 * ──────────────────────────────────────────────────────────────────────── */

/* base palette */
#define UI_COLOR_PURPLE_500   lv_color_hex(0x5F60B9)  /* --anio-purple-500 (primary) */
#define UI_COLOR_PURPLE_600   lv_color_hex(0x3F3FA6)  /* --anio-purple-600 (primary hover/pressed) */
#define UI_COLOR_PURPLE_100   lv_color_hex(0xEFEFF8)  /* --anio-purple-100 (primary soft bg) */
#define UI_COLOR_INK_900      lv_color_hex(0x1C1F34)  /* --anio-ink-900 (heading / nền tối) */
#define UI_COLOR_SLATE_500    lv_color_hex(0x6C757D)  /* --anio-slate-500 (chữ phụ) */
#define UI_COLOR_MIST_100     lv_color_hex(0xF6F7F9)  /* --anio-mist-100 (nền trang) */
#define UI_COLOR_LINE_200     lv_color_hex(0xEBEBEB)  /* --anio-line-200 (viền) */
#define UI_COLOR_WHITE        lv_color_hex(0xFFFFFF)  /* --anio-white (nền card) */
#define UI_COLOR_GOLD_400     lv_color_hex(0xFFBD00)  /* --anio-gold-400 (accent) */
#define UI_COLOR_GOLD_100     lv_color_hex(0xFFF07C)  /* --anio-gold-100 (secondary soft) */
#define UI_COLOR_GREEN_500    lv_color_hex(0x3CAE5C)
#define UI_COLOR_GREEN_600    lv_color_hex(0x39B54A)  /* --color-success */
#define UI_COLOR_RED_500      lv_color_hex(0xFB2F2F)
#define UI_COLOR_RED_600      lv_color_hex(0xEA2F2F)  /* --color-danger */
#define UI_COLOR_RED_OUTLINE  lv_color_hex(0xF24141)
#define UI_COLOR_ORANGE_500   lv_color_hex(0xFC7F3A)  /* --color-warning */
#define UI_COLOR_ORANGE_700   lv_color_hex(0xA8542A)
#define UI_COLOR_ICON_MUTED   lv_color_hex(0x9CA1A6)

/* semantic aliases — dùng các tên này trong code UI */
#define UI_COLOR_PRIMARY        UI_COLOR_PURPLE_500
#define UI_COLOR_PRIMARY_HOVER  UI_COLOR_PURPLE_600
#define UI_COLOR_PRIMARY_SOFT   UI_COLOR_PURPLE_100
#define UI_COLOR_HEADING        UI_COLOR_INK_900
#define UI_COLOR_BODY           UI_COLOR_SLATE_500
#define UI_COLOR_BG             UI_COLOR_MIST_100
#define UI_COLOR_SURFACE        UI_COLOR_WHITE
#define UI_COLOR_BORDER         UI_COLOR_LINE_200
#define UI_COLOR_ICON           UI_COLOR_ICON_MUTED
#define UI_COLOR_ACCENT         UI_COLOR_GOLD_400

#define UI_COLOR_SUCCESS         UI_COLOR_GREEN_600
#define UI_COLOR_SUCCESS_BG      lv_color_hex(0xEAF9EE)
#define UI_COLOR_DANGER          UI_COLOR_RED_600
#define UI_COLOR_DANGER_BG       lv_color_hex(0xFDEAEA)
#define UI_COLOR_WARNING         UI_COLOR_ORANGE_500
#define UI_COLOR_WARNING_BG      lv_color_hex(0xFFF1E8)
#define UI_COLOR_INFO_SOFT       UI_COLOR_PURPLE_100
#define UI_COLOR_INFO            UI_COLOR_PURPLE_500

/* text trên nền tối (màn đăng nhập) */
#define UI_COLOR_ON_DARK_MUTED  lv_color_hex(0xB9BBDA)
#define UI_COLOR_ON_DARK_ERROR  lv_color_hex(0xFFB4B4)
#define UI_COLOR_ON_DARK_HINT   lv_color_hex(0x8F92B8)

/* ────────────────────────────────────────────────────────────────────────
 * FONT — Inter (đủ dấu tiếng Việt), biên dịch trong main/ui/fonts/
 * (thay cho font-secondary Work Sans của thiết kế gốc, vì Work Sans không
 *  phủ đủ Unicode tiếng Việt — xem esp32p4-lvgl-handoff.md mục 1)
 * Kích thước theo đúng type-scale trong tokens/typography.css.
 * ──────────────────────────────────────────────────────────────────────── */
LV_FONT_DECLARE(font_inter_12);
LV_FONT_DECLARE(font_inter_14);
LV_FONT_DECLARE(font_inter_16);
LV_FONT_DECLARE(font_inter_18);
LV_FONT_DECLARE(font_inter_20);
LV_FONT_DECLARE(font_inter_22);
LV_FONT_DECLARE(font_inter_26);
LV_FONT_DECLARE(font_inter_30);
LV_FONT_DECLARE(font_inter_38);
LV_FONT_DECLARE(font_inter_48);
LV_FONT_DECLARE(font_inter_bold_12);
LV_FONT_DECLARE(font_inter_bold_14);
LV_FONT_DECLARE(font_inter_bold_16);
LV_FONT_DECLARE(font_inter_bold_18);
LV_FONT_DECLARE(font_inter_bold_20);
LV_FONT_DECLARE(font_inter_bold_22);
LV_FONT_DECLARE(font_inter_bold_26);
LV_FONT_DECLARE(font_inter_bold_30);
LV_FONT_DECLARE(font_inter_bold_38);
LV_FONT_DECLARE(font_inter_bold_48);

#define UI_FONT_XS         (&font_inter_12)       /* text-xs (badge nhỏ) */
#define UI_FONT_BODY       (&font_inter_14)       /* text-body / text-small / h6 */
#define UI_FONT_BODY_BOLD  (&font_inter_bold_14)  /* button / label đậm 14px */
#define UI_FONT_H5         (&font_inter_16)
#define UI_FONT_H5_BOLD    (&font_inter_bold_16)
#define UI_FONT_H4         (&font_inter_18)
#define UI_FONT_H4_BOLD    (&font_inter_bold_18)
#define UI_FONT_H3         (&font_inter_20)
#define UI_FONT_H3_BOLD    (&font_inter_bold_20)
#define UI_FONT_H2_BOLD    (&font_inter_bold_22)
#define UI_FONT_STATION    (&font_inter_bold_26)  /* tên trạm cân màn đăng nhập */
#define UI_FONT_BRAND      (&font_inter_bold_30)  /* "PIG WEIGH" */
#define UI_FONT_H1_BOLD    (&font_inter_bold_38)
#define UI_FONT_DISPLAY    (&font_inter_bold_48)  /* số khối lượng lớn */

/* ────────────────────────────────────────────────────────────────────────
 * ẢNH / ICON — biên dịch từ assets/icons/ *.svg (main/ui/icons/)
 * Icon dạng A8 (chỉ alpha) — tô màu lúc chạy bằng
 * lv_obj_set_style_image_recolor()/_opa(). Logo là ảnh màu ARGB8888.
 * ──────────────────────────────────────────────────────────────────────── */
LV_IMAGE_DECLARE(img_icon_nav_orders);
LV_IMAGE_DECLARE(img_icon_nav_weighing);
LV_IMAGE_DECLARE(img_icon_nav_suggestions);
LV_IMAGE_DECLARE(img_icon_nav_history);
LV_IMAGE_DECLARE(img_icon_nav_settings);
LV_IMAGE_DECLARE(img_icon_refresh);
LV_IMAGE_DECLARE(img_icon_check);
LV_IMAGE_DECLARE(img_icon_lock);
LV_IMAGE_DECLARE(img_icon_camera_off);
LV_IMAGE_DECLARE(img_icon_wifi_signal);
LV_IMAGE_DECLARE(img_logo_pig_weigh);
LV_IMAGE_DECLARE(img_camera_01_frame);

/* ────────────────────────────────────────────────────────────────────────
 * BỐ CỤC — canvas thiết kế gốc 800×1024, panel vật lý 800×1280.
 * Chiều rộng khớp sẵn (800=800). Chiều cao được "scale đều" theo đúng
 * mục 3 của handoff (không đổi tỉ lệ TỪNG vùng): hệ số =1280/1024=1.25,
 * áp dụng cho chiều cao 4 vùng cố định; vùng nội dung nhận phần còn lại.
 * ──────────────────────────────────────────────────────────────────────── */
#define UI_SCALE_Y            1.25f
#define UI_HOR_RES            800
#define UI_VER_RES             1280

#define UI_STATUSBAR_H        80   /* gốc 64 * 1.25 */
#define UI_SUBBAR_H            50   /* gốc 40 * 1.25 */
#define UI_BOTTOMNAV_H        100   /* gốc 80 * 1.25 */
#define UI_CONTENT_Y          (UI_STATUSBAR_H + UI_SUBBAR_H)
#define UI_CONTENT_H          (UI_VER_RES - UI_STATUSBAR_H - UI_SUBBAR_H - UI_BOTTOMNAV_H)

#define UI_PAD_SCREEN          16
#define UI_RADIUS_CARD         12
#define UI_RADIUS_BADGE        999

#ifdef __cplusplus
}
#endif
