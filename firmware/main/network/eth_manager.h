#pragma once
/**
 * @file eth_manager.h
 * @brief Ethernet PoE (LAN8720 RMII) manager cho ESP32-P4-WIFI6-POE-ETH
 *
 * Tính năng:
 *  - Init EMAC + PHY (LAN8720 / lan87xx)
 *  - DHCP client tự động
 *  - Event callbacks: IP_GOT, DISCONNECTED
 *  - Expose trạng thái qua eth_manager_get_state()
 */

#include "esp_err.h"
#include "driver/gpio.h"
#include "esp_netif.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Trạng thái Ethernet ────────────────────────────────────────────────── */
typedef enum {
    ETH_STATE_STOPPED = 0,
    ETH_STATE_STARTED,
    ETH_STATE_CONNECTED,
    ETH_STATE_GOT_IP,
    ETH_STATE_DISCONNECTED,
} eth_state_t;

/* ── Cấu hình ───────────────────────────────────────────────────────────── */
typedef struct {
    gpio_num_t  mdc_gpio;       /*!< MDC  (ví dụ: GPIO31) */
    gpio_num_t  mdio_gpio;      /*!< MDIO (ví dụ: GPIO52) */
    gpio_num_t  phy_rst_gpio;   /*!< PHY reset GPIO (-1 nếu không có) */
    int32_t     phy_addr;       /*!< PHY MDIO address (0–31, thường = 1) */
} eth_manager_config_t;

/* ── Thông tin IP ───────────────────────────────────────────────────────── */
typedef struct {
    char ip[16];
    char netmask[16];
    char gw[16];
} eth_ip_info_t;

/**
 * @brief Khởi tạo Ethernet, attach netif, bật DHCP
 * @param cfg   Cấu hình EMAC/PHY GPIO
 * @return ESP_OK nếu thành công
 */
esp_err_t eth_manager_init(const eth_manager_config_t *cfg);

/**
 * @brief Lấy trạng thái hiện tại
 */
eth_state_t eth_manager_get_state(void);

/**
 * @brief Lấy thông tin IP sau khi kết nối
 * @param[out] info  Bộ nhớ được điền bởi hàm
 * @return ESP_OK nếu đang có IP, ESP_ERR_INVALID_STATE nếu chưa có
 */
esp_err_t eth_manager_get_ip(eth_ip_info_t *info);

/**
 * @brief Dừng & giải phóng tài nguyên Ethernet
 */
esp_err_t eth_manager_deinit(void);

#ifdef __cplusplus
}
#endif
