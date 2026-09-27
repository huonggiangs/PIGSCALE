idf_component_register(
    SRCS
        "main.c"
        "display/display_driver.c"
        "ui/ui_theme.c"
        "ui/ui_main.c"
        "ui/ui_orders.c"
        "ui/ui_history.c"
        "ui/ui_report.c"
        "ui/ui_settings.c"
        "ui/ui_calibration.c"
        "ui/ui_nav.c"
        "sensors/ads131m08.c"
        "sensors/angle_sensor.c"
        "weighing/weight_logic.c"
        "network/wifi_manager.c"
    INCLUDE_DIRS
        "."
        "display"
        "ui"
        "sensors"
        "weighing"
        "network"
    REQUIRES
        driver
        esp_timer
        esp_wifi
        esp_eth
        nvs_flash
        freertos
        esp_psram
        lvgl
        esp_lvgl_port
)

# Bật C11 để dùng designated initializers
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu11 -Wno-unused-variable)
