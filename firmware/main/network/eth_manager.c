/**
 * @file eth_manager.c
 * @brief Ethernet PoE — LAN8720 RMII + esp_netif + DHCP
 *
 * Sơ đồ:
 *   app_main → eth_manager_init()
 *             → esp_eth_mac_new_esp32()   (EMAC trong ESP32-P4)
 *             → esp_eth_phy_new_lan87xx() (PHY LAN8720 ngoài)
 *             → esp_eth_driver_install()
 *             → esp_netif_new(ETH default)
 *             → esp_netif_attach()
 *             → esp_eth_start()
 *
 * Sau khi DHCP cấp IP → ETH_STATE_GOT_IP, thông tin lưu vào s_ip_info.
 *
 * RMII CLK:  dùng GPIO50 làm RMII_CLK_IN (cấu hình trong sdkconfig.defaults:
 *             CONFIG_ETH_RMII_CLK_INPUT=y
 *             CONFIG_ETH_RMII_CLK_IN_GPIO=50)
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_eth.h"
#include "esp_eth_mac.h"
#include "esp_eth_phy.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_mac.h"

#include "eth_manager.h"

static const char *TAG = "eth_mgr";

/* ── Module state ──────────────────────────────────────────────────────────── */
static esp_eth_handle_t  s_eth_handle = NULL;
static esp_netif_t      *s_netif      = NULL;
static volatile eth_state_t s_state   = ETH_STATE_STOPPED;
static eth_ip_info_t     s_ip_info    = {0};

/* ── Event handlers ────────────────────────────────────────────────────────── */
static void eth_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        s_state = ETH_STATE_CONNECTED;
        ESP_LOGI(TAG, "Ethernet: cáp đã cắm / link UP");
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        s_state = ETH_STATE_DISCONNECTED;
        memset(&s_ip_info, 0, sizeof(s_ip_info));
        ESP_LOGW(TAG, "Ethernet: mất kết nối / link DOWN");
        break;
    case ETHERNET_EVENT_START:
        s_state = ETH_STATE_STARTED;
        ESP_LOGI(TAG, "Ethernet driver đã khởi động");
        break;
    case ETHERNET_EVENT_STOP:
        s_state = ETH_STATE_STOPPED;
        ESP_LOGI(TAG, "Ethernet driver đã dừng");
        break;
    default:
        break;
    }
}

static void ip_event_handler(void *arg,
                              esp_event_base_t event_base,
                              int32_t event_id,
                              void *event_data)
{
    if (event_id == IP_EVENT_ETH_GOT_IP) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)event_data;
        s_state = ETH_STATE_GOT_IP;

        esp_ip4addr_ntoa(&ev->ip_info.ip,      s_ip_info.ip,      sizeof(s_ip_info.ip));
        esp_ip4addr_ntoa(&ev->ip_info.netmask, s_ip_info.netmask, sizeof(s_ip_info.netmask));
        esp_ip4addr_ntoa(&ev->ip_info.gw,      s_ip_info.gw,      sizeof(s_ip_info.gw));

        ESP_LOGI(TAG, "Ethernet IP: %s  mask: %s  gw: %s",
                 s_ip_info.ip, s_ip_info.netmask, s_ip_info.gw);
    } else if (event_id == IP_EVENT_ETH_LOST_IP) {
        memset(&s_ip_info, 0, sizeof(s_ip_info));
        s_state = ETH_STATE_CONNECTED;
        ESP_LOGW(TAG, "Ethernet: mất địa chỉ IP");
    }
}

/* ── eth_manager_init ──────────────────────────────────────────────────────── */
esp_err_t eth_manager_init(const eth_manager_config_t *cfg)
{
    ESP_RETURN_ON_FALSE(cfg, ESP_ERR_INVALID_ARG, TAG, "cfg NULL");

    /* ── 1. Đăng ký event handlers ─────────────────────────────────────── */
    ESP_RETURN_ON_ERROR(
        esp_event_handler_register(ETH_EVENT,  ESP_EVENT_ANY_ID, eth_event_handler, NULL),
        TAG, "register ETH event"
    );
    ESP_RETURN_ON_ERROR(
        esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, ip_event_handler, NULL),
        TAG, "register IP event"
    );
    ESP_RETURN_ON_ERROR(
        esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_LOST_IP, ip_event_handler, NULL),
        TAG, "register IP_LOST event"
    );

    /* ── 2. EMAC (MAC trong chip ESP32-P4) ──────────────────────────────── */
    eth_mac_config_t mac_cfg = ETH_MAC_DEFAULT_CONFIG();
    mac_cfg.sw_reset_timeout_ms = 100;

    eth_esp32_emac_config_t emac_cfg = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    emac_cfg.smi_gpio.mdc_num  = cfg->mdc_gpio;
    emac_cfg.smi_gpio.mdio_num = cfg->mdio_gpio;
    /* RMII CLK được cấu hình qua sdkconfig (CLK_IN GPIO50) */

    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&emac_cfg, &mac_cfg);
    ESP_RETURN_ON_FALSE(mac, ESP_FAIL, TAG, "esp_eth_mac_new_esp32 failed");

    /* ── 3. PHY (LAN8720 / lan87xx) ─────────────────────────────────────── */
    eth_phy_config_t phy_cfg = ETH_PHY_DEFAULT_CONFIG();
    phy_cfg.phy_addr         = cfg->phy_addr;
    phy_cfg.reset_gpio_num   = cfg->phy_rst_gpio;

    esp_eth_phy_t *phy = esp_eth_phy_new_lan87xx(&phy_cfg);
    ESP_RETURN_ON_FALSE(phy, ESP_FAIL, TAG, "esp_eth_phy_new_lan87xx failed");

    /* ── 4. Cài đặt Ethernet driver ─────────────────────────────────────── */
    esp_eth_config_t eth_cfg = ETH_DEFAULT_CONFIG(mac, phy);
    ESP_RETURN_ON_ERROR(
        esp_eth_driver_install(&eth_cfg, &s_eth_handle),
        TAG, "esp_eth_driver_install failed"
    );

    /* ── 5. Tạo netif và gắn vào driver ─────────────────────────────────── */
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    s_netif = esp_netif_new(&netif_cfg);
    ESP_RETURN_ON_FALSE(s_netif, ESP_FAIL, TAG, "esp_netif_new failed");

    /* Gắn Ethernet driver vào netif qua glue layer */
    void *eth_netif_glue = esp_eth_new_netif_glue(s_eth_handle);
    ESP_RETURN_ON_FALSE(eth_netif_glue, ESP_FAIL, TAG, "esp_eth_new_netif_glue failed");

    ESP_RETURN_ON_ERROR(
        esp_netif_attach(s_netif, eth_netif_glue),
        TAG, "esp_netif_attach failed"
    );

    /* ── 6. Bật Ethernet ────────────────────────────────────────────────── */
    ESP_RETURN_ON_ERROR(
        esp_eth_start(s_eth_handle),
        TAG, "esp_eth_start failed"
    );

    ESP_LOGI(TAG, "Ethernet PoE LAN8720 init OK (MDC=%d MDIO=%d PHY_ADDR=%"PRId32")",
             cfg->mdc_gpio, cfg->mdio_gpio, cfg->phy_addr);
    return ESP_OK;
}

/* ── eth_manager_get_state / get_ip ────────────────────────────────────────── */
eth_state_t eth_manager_get_state(void)
{
    return s_state;
}

esp_err_t eth_manager_get_ip(eth_ip_info_t *info)
{
    ESP_RETURN_ON_FALSE(info, ESP_ERR_INVALID_ARG, TAG, "info NULL");
    if (s_state != ETH_STATE_GOT_IP) {
        return ESP_ERR_INVALID_STATE;
    }
    memcpy(info, &s_ip_info, sizeof(eth_ip_info_t));
    return ESP_OK;
}

/* ── eth_manager_deinit ─────────────────────────────────────────────────────── */
esp_err_t eth_manager_deinit(void)
{
    if (s_eth_handle) {
        esp_eth_stop(s_eth_handle);
        esp_eth_driver_uninstall(s_eth_handle);
        s_eth_handle = NULL;
    }
    if (s_netif) {
        esp_netif_destroy(s_netif);
        s_netif = NULL;
    }
    esp_event_handler_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, eth_event_handler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_ETH_GOT_IP, ip_event_handler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_ETH_LOST_IP, ip_event_handler);
    s_state = ETH_STATE_STOPPED;
    return ESP_OK;
}
