/**
 * @file main.c
 * @brief Cân Máy Xúc V3 — Entry point
 *        Board: ESP32-P4-WIFI6-POE-ETH
 *        Framework: ESP-IDF v5.3+, LVGL v9.x
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_psram.h"

#include "display_driver.h"
#include "ui_main.h"
#include "ui_nav.h"
#include "weight_logic.h"
#include "wifi_manager.h"

static const char *TAG = "MAYXUC_V3";

void app_main(void)
{
    ESP_LOGI(TAG, "=== CAN MAY XUC V3 BOOT ===");
    ESP_LOGI(TAG, "Heap: %lu bytes free", esp_get_free_heap_size());

    /* 1. NVS init */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* 2. PSRAM check */
    if (esp_psram_is_initialized()) {
        ESP_LOGI(TAG, "PSRAM: %zu KB available", esp_psram_get_size() / 1024);
    } else {
        ESP_LOGW(TAG, "PSRAM not detected — running from internal RAM");
    }

    /* 3. Display + LVGL init */
    display_driver_init();

    /* 4. Build UI */
    ui_nav_init();          // bottom nav bar
    ui_main_screen_init();  // màn hình CÂN (active mặc định)

    /* 5. Weight sensor & logic */
    weight_logic_init();

    /* 6. Network (background task) */
    wifi_manager_init();

    ESP_LOGI(TAG, "System ready. LVGL running.");

    /* Main loop — không làm gì, LVGL chạy trong task riêng */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "Heap free: %lu bytes", esp_get_free_heap_size());
    }
}
