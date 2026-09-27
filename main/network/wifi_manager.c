/**
 * @file wifi_manager.c
 * @brief Wi-Fi init (STA mode) — kết nối từ NVS credentials
 *        TODO: provisioning qua BLE hoặc màn hình CÀI ĐẶT
 */
#include "wifi_manager.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "freertos/event_groups.h"

static const char *TAG = "WIFI";

void wifi_manager_init(void)
{
    ESP_LOGI(TAG, "Wi-Fi init (STA) — stub, configure SSID/pass via menuconfig");
    /* TODO: full implementation */
}
