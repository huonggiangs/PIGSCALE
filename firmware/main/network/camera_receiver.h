#pragma once
#include "camera_protocol.h"
#include "esp_err.h"

typedef struct {
    char ip[40];
    uint16_t https_port;
    uint16_t receive_port;
    char user[64];
    char password[128];
    char rtsp_main[160];
    char rtsp_sub[160];
    char token[1024];
} camera_config_t;

typedef struct {
    bool running;
    bool applying;
    uint16_t listening_port;
    esp_err_t server_error;
    uint32_t sequence;
    camera_count_event_t latest;
    int64_t received_us;
    bool snapshot_busy;
    esp_err_t snapshot_error;
    int snapshot_http_status;
    uint32_t snapshot_sequence;
    camera_count_event_t snapshot_event;
    size_t snapshot_size;
} camera_receiver_status_t;

/* Init once after NVS/netif. Worker owns server lifecycle and HTTPS requests. */
esp_err_t camera_receiver_init(void);
void camera_receiver_get_config(camera_config_t *out);
esp_err_t camera_receiver_save_config(const camera_config_t *config);
void camera_receiver_get_status(camera_receiver_status_t *out);
/* Caller owns returned JPEG (free). Metadata belongs to this exact image. */
uint8_t *camera_receiver_copy_snapshot(size_t *size, uint16_t *width, uint16_t *height,
                                      camera_count_event_t *event);
void camera_receiver_endpoint(char *out, size_t size);
