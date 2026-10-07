#include "camera_receiver.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_http_server.h"
#include "esp_http_client.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"

#define MAX_JSON (16 * 1024)
#define MAX_JPEG (512 * 1024)
#define LOCK() xSemaphoreTake(s_lock, portMAX_DELAY)
#define UNLOCK() xSemaphoreGive(s_lock)
static SemaphoreHandle_t s_lock;
static TaskHandle_t s_worker;
static httpd_handle_t s_server;
static camera_config_t s_config;
static camera_receiver_status_t s_status;
static uint32_t s_config_generation;
static uint8_t *s_jpeg;
static uint16_t s_width, s_height;
static const char *TAG = "CAM_RX";

static bool config_valid(const camera_config_t *cfg)
{
    struct in_addr addr;
    if (!cfg || !memchr(cfg->ip, 0, sizeof(cfg->ip)) || !inet_aton(cfg->ip, &addr) ||
        !addr.s_addr || addr.s_addr == INADDR_BROADCAST || !cfg->https_port ||
        !cfg->receive_port || !memchr(cfg->user, 0, sizeof(cfg->user)) ||
        !memchr(cfg->password, 0, sizeof(cfg->password)) ||
        !memchr(cfg->rtsp_main, 0, sizeof(cfg->rtsp_main)) || !memchr(cfg->rtsp_sub, 0, sizeof(cfg->rtsp_sub)) ||
        !memchr(cfg->token, 0, sizeof(cfg->token))) return false;
    if ((cfg->rtsp_main[0] && strncmp(cfg->rtsp_main, "rtsp://", 7) && strncmp(cfg->rtsp_main, "rtsps://", 8)) ||
        (cfg->rtsp_sub[0] && strncmp(cfg->rtsp_sub, "rtsp://", 7) && strncmp(cfg->rtsp_sub, "rtsps://", 8))) return false;
    /* Tokens become HTTP header values; reject CR/LF and other control bytes. */
    for (const unsigned char *p = (const unsigned char *)cfg->token; *p; ++p)
        if (*p < 33 || *p > 126) return false;
    return true;
}

static esp_err_t receive_count(httpd_req_t *req)
{
    /* One configured camera: reject other LAN sources before reading the body.
     * The supplied API does not specify webhook authentication. */
    struct sockaddr_in peer;
    socklen_t peer_len = sizeof(peer);
    struct in_addr allowed;
    LOCK();
    inet_aton(s_config.ip, &allowed);
    uint32_t generation = s_config_generation;
    bool applying = s_status.applying;
    UNLOCK();
    if (applying) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Applying configuration");
        return ESP_FAIL;
    }
    if (getpeername(httpd_req_to_sockfd(req), (struct sockaddr *)&peer, &peer_len) != 0 ||
        peer.sin_family != AF_INET || peer.sin_addr.s_addr != allowed.s_addr) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Unexpected camera IP");
        return ESP_FAIL;
    }
    char type[64];
    if (httpd_req_get_hdr_value_str(req, "Content-Type", type, sizeof(type)) != ESP_OK ||
        strncasecmp(type, "application/json", 16) || (type[16] && type[16] != ';')) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Expected application/json");
        return ESP_FAIL;
    }
    if (!req->content_len || req->content_len > MAX_JSON) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Body must be 1..16384 bytes");
        return ESP_FAIL;
    }
    char *body = malloc(req->content_len + 1);
    if (!body) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_FAIL;
    }
    size_t done = 0;
    int64_t deadline = esp_timer_get_time() + 5000000;
    while (done < req->content_len) {
        int n = httpd_req_recv(req, body + done, req->content_len - done);
        if (n <= 0 || esp_timer_get_time() > deadline) {
            free(body);
            httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, "Incomplete body");
            return ESP_FAIL;
        }
        done += n;
    }
    body[done] = 0;
    camera_count_event_t event = {0};
    bool valid = camera_parse_count(body, done, &event);
    free(body);
    if (!valid) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid count event (schema_version/device.id/timestamp/count)");
    LOCK();
    if (generation != s_config_generation) {
        UNLOCK();
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Configuration changed; retry");
    }
    bool same_device = s_status.sequence && !strcmp(event.device_id, s_status.latest.device_id);
    bool stale = same_device && event.timestamp < s_status.latest.timestamp;
    bool duplicate = same_device && event.timestamp == s_status.latest.timestamp && event.count == s_status.latest.count;
    if (!stale && !duplicate) {
        s_status.latest = event;
        s_status.received_us = esp_timer_get_time();
        ++s_status.sequence;
    }
    UNLOCK();
    if (!stale && !duplicate) xTaskNotifyGive(s_worker);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, stale ? "{\"ok\":true,\"ignored\":\"stale\"}" :
                                   duplicate ? "{\"ok\":true,\"duplicate\":true}" : "{\"ok\":true}");
}

static esp_err_t start_server(uint16_t port)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = port;
    cfg.ctrl_port = port == 32768 ? 32769 : 32768;
    cfg.stack_size = 8192;
    cfg.max_open_sockets = 2;
    cfg.lru_purge_enable = true;
    cfg.recv_wait_timeout = 2;
    cfg.send_wait_timeout = 2;
    esp_err_t err = httpd_start(&s_server, &cfg);
    if (err != ESP_OK) return err;
    httpd_uri_t uri = {.uri = "/api/device-data", .method = HTTP_POST, .handler = receive_count};
    err = httpd_register_uri_handler(s_server, &uri);
    if (err != ESP_OK) { httpd_stop(s_server); s_server = NULL; }
    return err;
}

static esp_err_t snapshot_header_cb(esp_http_client_event_t *event)
{
    if (event->event_id == HTTP_EVENT_ON_HEADER && !strcasecmp(event->header_key, "Content-Type"))
        snprintf((char *)event->user_data, 64, "%s", event->header_value);
    return ESP_OK;
}

static void fetch_snapshot(const camera_config_t *cfg, uint32_t generation,
                           const camera_count_event_t *event, uint32_t sequence)
{
    esp_err_t err = ESP_ERR_INVALID_STATE;
    int status = 0;
    uint8_t *jpeg = NULL;
    size_t used = 0;
    uint16_t width = 0, height = 0;
    esp_http_client_handle_t client = NULL;
    char type[64] = {0};
    if (!cfg->token[0]) goto finish;
    char url[112];
    char auth[sizeof(cfg->token) + 8];
    snprintf(url, sizeof(url), "https://%s:%u/api/v1/media/snapshot", cfg->ip, cfg->https_port);
    snprintf(auth, sizeof(auth), "Bearer %s", cfg->token);
    esp_http_client_config_t http = {
        .url = url, .timeout_ms = 4000, .disable_auto_redirect = true,
        .skip_cert_common_name_check = true,
        .event_handler = snapshot_header_cb, .user_data = type,
    };
    client = esp_http_client_init(&http);
    if (!client) { err = ESP_ERR_NO_MEM; goto finish; }
    err = esp_http_client_set_header(client, "Authorization", auth);
    if (err != ESP_OK) goto finish;
    err = esp_http_client_open(client, 0);
    if (err != ESP_OK) goto finish;
    int64_t length = esp_http_client_fetch_headers(client);
    status = esp_http_client_get_status_code(client);
    if (length < -1 || status != 200 || strncasecmp(type, "image/jpeg", 10) ||
        (type[10] && type[10] != ';') || length > MAX_JPEG) { err = ESP_ERR_INVALID_RESPONSE; goto finish; }
    jpeg = malloc(MAX_JPEG);
    if (!jpeg) { err = ESP_ERR_NO_MEM; goto finish; }
    int64_t deadline = esp_timer_get_time() + 8000000;
    while (used < MAX_JPEG && !esp_http_client_is_complete_data_received(client)) {
        int n = esp_http_client_read(client, (char *)jpeg + used, (MAX_JPEG - used > 4096) ? 4096 : MAX_JPEG - used);
        if (n < 0 || esp_timer_get_time() > deadline) { err = ESP_ERR_TIMEOUT; goto finish; }
        if (!n) break;
        used += n;
    }
    if (!esp_http_client_is_complete_data_received(client) ||
        !camera_jpeg_dimensions(jpeg, used, &width, &height)) { err = ESP_ERR_INVALID_RESPONSE; goto finish; }
    err = ESP_OK;
finish:
    if (client) esp_http_client_cleanup(client);
    LOCK();
    if (generation == s_config_generation) {
        s_status.snapshot_busy = false;
        s_status.snapshot_error = err;
        s_status.snapshot_http_status = status;
        if (err == ESP_OK) {
            free(s_jpeg);
            s_jpeg = jpeg;
            jpeg = NULL;
            s_width = width; s_height = height;
            s_status.snapshot_size = used;
            s_status.snapshot_sequence = sequence;
            s_status.snapshot_event = *event;
        }
    }
    UNLOCK();
    free(jpeg);
}

static void receiver_worker(void *arg)
{
    (void)arg;
    uint32_t applied_generation = 0, fetched_sequence = 0;
    for (;;) {
        LOCK();
        camera_config_t cfg = s_config;
        uint32_t generation = s_config_generation;
        UNLOCK();
        if (generation != applied_generation) {
            if (s_server) { httpd_stop(s_server); s_server = NULL; }
            esp_err_t err = start_server(cfg.receive_port);
            LOCK();
            s_status.running = err == ESP_OK;
            s_status.listening_port = err == ESP_OK ? cfg.receive_port : 0;
            s_status.server_error = err;
            if (generation == s_config_generation) s_status.applying = false;
            UNLOCK();
            applied_generation = generation;
            fetched_sequence = 0;
            ESP_LOGI(TAG, "Receiver port %u: %s", cfg.receive_port, esp_err_to_name(err));
        }
        LOCK();
        uint32_t sequence = s_status.sequence;
        camera_count_event_t event = s_status.latest;
        bool fetch = sequence && sequence != fetched_sequence && generation == s_config_generation;
        if (fetch) s_status.snapshot_busy = true;
        UNLOCK();
        if (fetch) {
            fetch_snapshot(&cfg, generation, &event, sequence);
            fetched_sequence = sequence;
            continue;
        }
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
}

esp_err_t camera_receiver_init(void)
{
    if (s_lock) return ESP_ERR_INVALID_STATE;
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) return ESP_ERR_NO_MEM;
    snprintf(s_config.ip, sizeof(s_config.ip), "192.168.1.10");
    s_config.https_port = 443;
    s_config.receive_port = 8080;
    snprintf(s_config.user, sizeof(s_config.user), "admin");
    /* KHÔNG đưa mật khẩu thật vào mã nguồn (rò rỉ credential qua Git/firmware
     * phát hành — xem mục Bảo mật trong docs/REVIEW_TONG_THE). Để trống: bắt
     * buộc nhập qua Cài đặt > Camera (lưu vào NVS, không vào source code). */
    s_config.password[0] = '\0';
    snprintf(s_config.rtsp_main, sizeof(s_config.rtsp_main), "rtsp://192.168.1.10:554/live/0");
    snprintf(s_config.rtsp_sub, sizeof(s_config.rtsp_sub), "rtsp://192.168.1.10:554/live/1");
    nvs_handle_t nvs;
    if (nvs_open("camera", NVS_READONLY, &nvs) == ESP_OK) {
        camera_config_t saved;
        size_t size = sizeof(saved);
        if (nvs_get_blob(nvs, "config_v2", &saved, &size) == ESP_OK && size == sizeof(saved) && config_valid(&saved)) s_config = saved;
        nvs_close(nvs);
    }
    s_config_generation = 1;
    s_status.applying = true;
    if (xTaskCreate(receiver_worker, "camera_rx", 10240, NULL, 4, &s_worker) != pdPASS) {
        s_status.applying = false;
        s_status.server_error = ESP_ERR_NO_MEM;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void camera_receiver_get_config(camera_config_t *out)
{
    if (!s_lock) { memset(out, 0, sizeof(*out)); return; }
    LOCK(); *out = s_config; UNLOCK();
}

esp_err_t camera_receiver_save_config(const camera_config_t *cfg)
{
    if (!config_valid(cfg)) return ESP_ERR_INVALID_ARG;
    if (!s_worker) return ESP_ERR_INVALID_STATE;
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("camera", NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;
    err = nvs_set_blob(nvs, "config_v2", cfg, sizeof(*cfg));
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    if (err != ESP_OK) return err;
    LOCK();
    s_config = *cfg;
    ++s_config_generation;
    memset(&s_status, 0, sizeof(s_status));
    s_status.applying = true;
    free(s_jpeg); s_jpeg = NULL;
    UNLOCK();
    xTaskNotifyGive(s_worker);
    return ESP_OK;
}

void camera_receiver_get_status(camera_receiver_status_t *out)
{
    if (!s_lock) { memset(out, 0, sizeof(*out)); out->server_error = ESP_ERR_NO_MEM; return; }
    LOCK(); *out = s_status; UNLOCK();
}

uint8_t *camera_receiver_copy_snapshot(size_t *size, uint16_t *width, uint16_t *height, camera_count_event_t *event)
{
    if (!s_lock) return NULL;
    LOCK();
    uint8_t *copy = s_jpeg ? malloc(s_status.snapshot_size) : NULL;
    if (copy) {
        memcpy(copy, s_jpeg, s_status.snapshot_size);
        *size = s_status.snapshot_size; *width = s_width; *height = s_height;
        *event = s_status.snapshot_event;
    }
    UNLOCK();
    return copy;
}

void camera_receiver_endpoint(char *out, size_t size)
{
    camera_receiver_status_t status;
    camera_receiver_get_status(&status);
    /* Prefer the interface routed towards the configured camera. */
    camera_config_t cfg;
    camera_receiver_get_config(&cfg);
    struct in_addr camera;
    inet_aton(cfg.ip, &camera);
    esp_netif_ip_info_t chosen = {0};
    for (esp_netif_t *n = esp_netif_next_unsafe(NULL); n; n = esp_netif_next_unsafe(n)) {
        esp_netif_ip_info_t ip;
        if (!esp_netif_is_netif_up(n) || esp_netif_get_ip_info(n, &ip) != ESP_OK || !ip.ip.addr) continue;
        if (!chosen.ip.addr) chosen = ip;
        if ((ip.ip.addr & ip.netmask.addr) == (camera.s_addr & ip.netmask.addr)) { chosen = ip; break; }
    }
    if (!chosen.ip.addr) snprintf(out, size, "Chưa có địa chỉ IP mạng");
    else snprintf(out, size, "http://" IPSTR ":%u/api/device-data", IP2STR(&chosen.ip),
                  status.running ? status.listening_port : cfg.receive_port);
}
