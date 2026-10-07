## CMakeLists.txt — PIG WEIGH firmware (main component)
## Nguồn: hardware bring-up tái sử dụng từ dự án CÂN MÁY XÚC V3 (display/, touch/,
## network/) + toàn bộ UI mới theo esp32p4-lvgl-handoff.md (ui/).
## Lưu ý: ui/ui_main.c(.h) là UI CŨ (máy xúc) — KHÔNG đưa vào SRCS, giữ lại trong
## cây thư mục chỉ để tham khảo lịch sử, không được biên dịch/liên kết.

file(GLOB UI_FONT_SRCS "ui/fonts/*.c")
file(GLOB UI_ICON_SRCS "ui/icons/*.c")

idf_component_register(
    SRCS
        "main.c"
        "display/display_driver.c"
        "touch/gt9271.c"
        "network/eth_manager.c"
        "network/wifi_manager.c"
        "network/camera_client.c"
        "network/camera_receiver.c"
        "network/camera_protocol.c"
        "network/rtsp_player.c"
        "ui/app_state.c"
        "ui/ui_common.c"
        "ui/ui_shell.c"
        "ui/ui_login.c"
        "ui/ui_orders.c"
        "ui/ui_weighing.c"
        "ui/ui_alerts.c"
        "ui/ui_history.c"
        "ui/ui_settings.c"
        "ui/ui_station_modal.c"
        "ui/ui_help.c"
        ${UI_FONT_SRCS}
        ${UI_ICON_SRCS}
    INCLUDE_DIRS
        "."
        "display"
        "touch"
        "network"
        "ui"
    REQUIRES
        esp_lcd
        esp_lcd_touch
        espressif__esp_lcd_touch_gt911
        waveshare__esp_lcd_jd9365_10_1
        esp_eth
        esp_netif
        esp_event
        esp_wifi
        espressif__esp_wifi_remote
        esp_http_client
        esp_http_server
        json
        esp_timer
        esp_lvgl_port
        esp_hw_support
        driver
        freertos
        nvs_flash
        log
        esp_system
        heap
        esp_rtsp_service
        esp_h264
)

# Cac font do lv_font_conv sinh ra dung "#include lvgl/lvgl.h" khi khong co
# LV_LVGL_H_INCLUDE_SIMPLE. Component lvgl__lvgl chi dua lvgl.h o thu muc goc
# nen phai bat macro nay de fallback ve "#include lvgl.h".
target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE)
