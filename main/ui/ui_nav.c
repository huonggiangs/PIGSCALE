/**
 * @file ui_nav.c
 * @brief Bottom navigation bar — 5 tabs
 *        Active tab: vàng (filled square), inactive: xám
 *        Tab click → lv_scr_load_anim() sang màn hình tương ứng
 */

#include "ui_nav.h"
#include "ui_theme.h"
#include "ui_main.h"
#include "ui_orders.h"
#include "ui_history.h"
#include "ui_report.h"
#include "ui_settings.h"
#include "esp_log.h"

static const char *TAG = "UI_NAV";

/* Tab definitions */
typedef struct {
    const char *icon;
    const char *label;
} nav_tab_t;

static const nav_tab_t k_tabs[] = {
    { LV_SYMBOL_DOWNLOAD,    "CÂN"       },
    { LV_SYMBOL_LIST,        "ĐƠN HÀNG"  },
    { LV_SYMBOL_REFRESH,     "LỊCH SỬ"   },
    { LV_SYMBOL_CHART,       "BÁO CÁO"   },
    { LV_SYMBOL_SETTINGS,    "CÀI ĐẶT"   },
};
#define NAV_TAB_COUNT  (sizeof(k_tabs)/sizeof(k_tabs[0]))

static lv_obj_t *s_tab_items[NAV_TAB_COUNT];
static int s_active_tab = 0;

static void tab_click_cb(lv_event_t *e);

/* ---------------------------------------------------------- */
void ui_nav_init(void)
{
    ESP_LOGI(TAG, "Nav init");
}

void ui_nav_build(lv_obj_t *parent, int active_tab)
{
    s_active_tab = active_tab;

    lv_obj_t *nav = lv_obj_create(parent);
    lv_obj_set_size(nav, UI_SCREEN_W, UI_NAV_H);
    lv_obj_align(nav, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(nav, UI_COLOR_NAV_BG, 0);
    lv_obj_set_style_bg_opa(nav, UI_OPA_FULL, 0);
    lv_obj_set_style_border_width(nav, 0, 0);
    lv_obj_set_style_border_side(nav, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(nav, UI_COLOR_BORDER, 0);
    lv_obj_set_style_radius(nav, 0, 0);
    lv_obj_set_style_pad_all(nav, 0, 0);
    lv_obj_set_scrollbar_mode(nav, LV_SCROLLBAR_MODE_OFF);

    lv_obj_set_flex_flow(nav, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(nav, LV_FLEX_ALIGN_SPACE_AROUND,
                               LV_FLEX_ALIGN_CENTER,
                               LV_FLEX_ALIGN_CENTER);

    for (int i = 0; i < (int)NAV_TAB_COUNT; i++) {
        lv_obj_t *item = lv_obj_create(nav);
        lv_obj_set_size(item, UI_SCREEN_W / NAV_TAB_COUNT, UI_NAV_H);
        lv_obj_set_style_pad_all(item, 0, 0);
        lv_obj_set_style_border_width(item, 0, 0);
        lv_obj_set_scrollbar_mode(item, LV_SCROLLBAR_MODE_OFF);

        bool is_active = (i == active_tab);

        if (is_active) {
            lv_obj_set_style_bg_color(item, UI_COLOR_ACCENT, 0);
            lv_obj_set_style_bg_opa(item, UI_OPA_FULL, 0);
            lv_obj_set_style_radius(item, UI_RADIUS_SM, 0);
        } else {
            lv_obj_set_style_bg_color(item, UI_COLOR_NAV_BG, 0);
            lv_obj_set_style_bg_opa(item, UI_OPA_FULL, 0);
            lv_obj_set_style_radius(item, 0, 0);
        }

        lv_obj_set_flex_flow(item, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(item, LV_FLEX_ALIGN_CENTER,
                                     LV_FLEX_ALIGN_CENTER,
                                     LV_FLEX_ALIGN_CENTER);

        lv_obj_t *lbl_icon = lv_label_create(item);
        lv_label_set_text(lbl_icon, k_tabs[i].icon);
        lv_obj_set_style_text_color(lbl_icon,
            is_active ? lv_color_hex(0x1A1A1A) : UI_COLOR_TEXT_SEC, 0);
        lv_obj_set_style_text_font(lbl_icon, UI_FONT_MEDIUM, 0);

        lv_obj_t *lbl_text = lv_label_create(item);
        lv_label_set_text(lbl_text, k_tabs[i].label);
        lv_obj_set_style_text_color(lbl_text,
            is_active ? lv_color_hex(0x1A1A1A) : UI_COLOR_TEXT_SEC, 0);
        lv_obj_set_style_text_font(lbl_text, UI_FONT_TINY, 0);

        s_tab_items[i] = item;

        lv_obj_add_event_cb(item, tab_click_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);
    }
}

void ui_nav_set_active(int tab_index)
{
    s_active_tab = tab_index;
}

/* ---------------------------------------------------------- */
static void tab_click_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx == s_active_tab) return;   /* uốn lại tab đang active */

    ESP_LOGI(TAG, "Switch to tab %d", idx);

    lv_obj_t *next_scr = NULL;

    switch (idx) {
        case 0:
            if (!ui_main_get_screen()) ui_main_screen_init();
            next_scr = ui_main_get_screen();
            break;
        case 1:
            if (!ui_orders_get_screen()) ui_orders_screen_init();
            next_scr = ui_orders_get_screen();
            break;
        case 2:
            if (!ui_history_get_screen()) ui_history_screen_init();
            next_scr = ui_history_get_screen();
            break;
        case 3:
            if (!ui_report_get_screen()) ui_report_screen_init();
            next_scr = ui_report_get_screen();
            break;
        case 4:
            if (!ui_settings_get_screen()) ui_settings_screen_init();
            next_scr = ui_settings_get_screen();
            break;
        default:
            return;
    }

    if (next_scr) {
        lv_scr_load_anim(next_scr, LV_SCR_LOAD_ANIM_FADE_ON, 180, 0, false);
    }

    ui_nav_set_active(idx);
}
