#include <stdint.h>
#include "ui_alerts.h"
#include "ui_theme.h"
#include "ui_common.h"
#include "app_state.h"
#include <stdio.h>
#include <string.h>

static lv_obj_t *s_container;
static lv_obj_t *s_list_host;

static void ack_btn_cb(lv_event_t *e)
{
    const char *id = (const char *)lv_event_get_user_data(e);
    app_state_ack_alert(id);
    ui_alerts_refresh();
}

static void badge_for_level(lv_obj_t *parent, alert_level_t level)
{
    switch (level) {
        case ALERT_CRITICAL: ui_common_badge(parent, "CRITICAL", UI_COLOR_DANGER_BG, UI_COLOR_DANGER); break;
        case ALERT_WARNING:  ui_common_badge(parent, "WARNING", UI_COLOR_WARNING_BG, UI_COLOR_ORANGE_700); break;
        default:              ui_common_badge(parent, "INFO", UI_COLOR_INFO_SOFT, UI_COLOR_INFO); break;
    }
}

static void build_alert_card(lv_obj_t *parent, alert_t *a)
{
    lv_obj_t *card = ui_common_card(parent);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 6, 0);

    lv_obj_t *head = lv_obj_create(card);
    lv_obj_remove_style_all(head);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(head, 8, 0);
    lv_obj_set_width(head, LV_PCT(100));
    lv_obj_set_height(head, LV_SIZE_CONTENT);
    lv_obj_clear_flag(head, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(head);
    lv_label_set_text(title, a->title);
    lv_obj_set_style_text_font(title, UI_FONT_H4_BOLD, 0);
    lv_obj_set_style_text_color(title, UI_COLOR_HEADING, 0);
    badge_for_level(head, a->level);

    lv_obj_t *desc = lv_label_create(card);
    lv_label_set_text(desc, a->desc);
    lv_label_set_long_mode(desc, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(desc, LV_PCT(100));
    lv_obj_set_style_text_font(desc, UI_FONT_BODY, 0);
    lv_obj_set_style_text_color(desc, UI_COLOR_BODY, 0);

    /* dòng hành động "→ ..." — mũi tên dùng font Montserrat built-in (→ không
     * nằm trong dải Unicode đã nhúng của font_inter_*), tách khỏi label chữ. */
    lv_obj_t *action_row = lv_obj_create(card);
    lv_obj_remove_style_all(action_row);
    lv_obj_set_flex_flow(action_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(action_row, 6, 0);
    lv_obj_set_width(action_row, LV_PCT(100));
    lv_obj_set_height(action_row, LV_SIZE_CONTENT);
    lv_obj_clear_flag(action_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *arrow = lv_label_create(action_row);
    lv_label_set_text(arrow, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_font(arrow, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(arrow, UI_COLOR_PRIMARY, 0);
    lv_obj_t *action = lv_label_create(action_row);
    lv_label_set_text(action, a->action);
    lv_label_set_long_mode(action, LV_LABEL_LONG_WRAP);
    lv_obj_set_flex_grow(action, 1);
    lv_obj_set_style_text_font(action, UI_FONT_BODY_BOLD, 0);
    lv_obj_set_style_text_color(action, UI_COLOR_PRIMARY, 0);

    lv_obj_t *ack_btn = ui_common_button(card, a->acknowledged ? "Đã xem" : "Đánh dấu đã xem",
                                          a->acknowledged ? UI_COLOR_MIST_100 : UI_COLOR_PRIMARY_SOFT,
                                          a->acknowledged ? UI_COLOR_ICON : UI_COLOR_PRIMARY, UI_FONT_BODY_BOLD);
    lv_obj_set_style_pad_top(ack_btn, 4, 0);
    if (a->acknowledged) {
        lv_obj_add_state(ack_btn, LV_STATE_DISABLED);
    } else {
        /* a->id trỏ vào bộ nhớ tĩnh của app_state() (singleton, không bao giờ
         * giải phóng) nên dùng trực tiếp làm user_data là an toàn. */
        lv_obj_add_event_cb(ack_btn, ack_btn_cb, LV_EVENT_CLICKED, a->id);
    }
}

lv_obj_t *ui_alerts_create(lv_obj_t *parent)
{
    s_container = lv_obj_create(parent);
    lv_obj_remove_style_all(s_container);
    lv_obj_set_size(s_container, UI_HOR_RES, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(s_container, UI_PAD_SCREEN, 0);
    lv_obj_set_flex_flow(s_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_container, 12, 0);
    lv_obj_clear_flag(s_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(s_container);
    lv_label_set_text(title, "Đề xuất và cảnh báo");
    lv_obj_set_style_text_font(title, UI_FONT_H2_BOLD, 0);
    lv_obj_set_style_text_color(title, UI_COLOR_HEADING, 0);

    s_list_host = lv_obj_create(s_container);
    lv_obj_remove_style_all(s_list_host);
    lv_obj_set_flex_flow(s_list_host, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_list_host, 12, 0);
    lv_obj_set_width(s_list_host, LV_PCT(100));
    lv_obj_set_height(s_list_host, LV_SIZE_CONTENT);
    lv_obj_clear_flag(s_list_host, LV_OBJ_FLAG_SCROLLABLE);

    return s_container;
}

void ui_alerts_refresh(void)
{
    ui_common_clear(s_list_host);
    app_state_t *st = app_state();
    if (st->alert_count == 0) {
        lv_obj_t *empty = lv_label_create(s_list_host);
        lv_label_set_text(empty, "Không có cảnh báo nào.");
        lv_obj_set_style_text_color(empty, UI_COLOR_BODY, 0);
        lv_obj_set_style_text_font(empty, UI_FONT_BODY, 0);
        return;
    }
    for (int i = 0; i < st->alert_count; i++) {
        build_alert_card(s_list_host, &st->alerts[i]);
    }
}
