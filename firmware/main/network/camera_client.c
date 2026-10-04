/**
 * @file camera_client.c
 * @brief Xem camera_client.h — chỉ kiểm tra camera có đáp ứng HTTP trên mạng.
 */
#include "camera_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_client.h"
#include "esp_log.h"

static const char *TAG = "CAM_CLIENT";

static volatile bool                  s_busy   = false;
static volatile camera_check_result_t s_result = CAMERA_CHECK_UNKNOWN;

typedef struct {
    char     ip[40];
    uint16_t port;
} check_task_arg_t;

static void check_task(void *arg)
{
    check_task_arg_t *a = (check_task_arg_t *)arg;

    /* Camera Vivoo này phục vụ Web UI qua HTTPS (xác nhận trực tiếp qua
     * trình duyệt — địa chỉ camera hiện "https"), KHÔNG phải HTTP thường
     * như ghi trong tài liệu (có thể do cấu hình riêng của thiết bị này).
     * Chứng chỉ gần như chắc chắn tự ký (self-signed, camera LAN nội bộ)
     * nên bỏ qua xác minh chuỗi chứng chỉ + tên CN — xem
     * CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY trong sdkconfig.defaults. */
    char url[64];
    snprintf(url, sizeof(url), "https://%s:%u/", a->ip, (unsigned)a->port);

    esp_http_client_config_t cfg = {
        .url                        = url,
        .timeout_ms                 = 4000,  /* TLS handshake chậm hơn TCP thường */
        .method                     = HTTP_METHOD_GET,
        .skip_cert_common_name_check = true,  /* kết nối bằng IP, cert CN sẽ không khớp */
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_err_t err = esp_http_client_perform(client);

    /* Bất kỳ phản hồi HTTP nào (kể cả 401/404) đều chứng tỏ camera CÓ TRÊN
     * MẠNG và đang chạy dịch vụ HTTP — đó là tất cả những gì ta có thể xác
     * nhận một cách trung thực khi chưa có tài liệu API/đăng nhập thật của
     * Vivoo (xem ghi chú trong camera_client.h). */
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "Camera %s:%u phan hoi HTTP %d — co tren mang", a->ip, (unsigned)a->port, status);
        s_result = CAMERA_CHECK_REACHABLE;
    } else {
        ESP_LOGW(TAG, "Camera %s:%u khong phan hoi (%s)", a->ip, (unsigned)a->port, esp_err_to_name(err));
        s_result = CAMERA_CHECK_UNREACHABLE;
    }

    esp_http_client_cleanup(client);
    free(a);
    s_busy = false;
    vTaskDelete(NULL);
}

void camera_client_test_async(const char *ip, uint16_t port)
{
    if (!ip || !ip[0] || s_busy) return;

    check_task_arg_t *a = malloc(sizeof(*a));
    if (!a) return;
    snprintf(a->ip, sizeof(a->ip), "%s", ip);
    a->port = port;

    s_busy = true;
    if (xTaskCreate(check_task, "cam_check", 4096, a, 4, NULL) != pdPASS) {
        s_busy = false;
        free(a);
    }
}

bool camera_client_is_busy(void) { return s_busy; }
camera_check_result_t camera_client_get_result(void) { return s_result; }
